#include <Arduino.h>
#include <U8g2lib.h>
#include <SD.h>
#include <SPI.h>
#include <FS.h>
#include <Wire.h>
#include <chrono>

// Soil Moisture Logger: Sensor reading + OLED display + SD card data logging with deep sleep
// Hardware: ESP32-C3 + capacitive sensor (ADC0) + SSD1306 display + SD card


// ===== File Configuration =====
const String FILE_PATH = "/soil.csv"; // CSV file: timestamp;raw_adc;moisture_%

// ===== Timing Configuration =====
const auto INTERVAL_WRITE_SD = std::chrono::minutes(5); // Write to SD every 5 minutes

const bool USE_DEEP_SLEEP = true; // Enable for power efficiency
const auto TIME_TO_SLEEP = (INTERVAL_WRITE_SD + std::chrono::seconds(1));

// ===== Hardware Configuration =====
const uint8_t DISPLAY_I2C_ADDRESS = 0x3C; // SSD1306 OLED display

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// Preserved in RTC memory across deep sleep cycles
RTC_DATA_ATTR std::chrono::milliseconds timeOffset = std::chrono::milliseconds(0);
RTC_DATA_ATTR auto timeLastSdWrite = std::chrono::steady_clock::now();

bool sdCardMounted = false;
bool displayConnected = false;


void u8g2_prepare()
{
	// Configure font and display settings for OLED
	u8g2.setFont(u8g2_font_6x10_tf);
	u8g2.setFontRefHeightExtendedText();
	u8g2.setDrawColor(1);
	u8g2.setFontPosTop();
	u8g2.setFontDirection(0);
}

// Append text to SD card file
void appendFile(fs::FS& fs, const char* path, const char* message)
{
	log_d("Appending to file: %s", path);

	File file = fs.open(path, FILE_APPEND);

	if (!file)
	{
		log_e("Failed to open file for appending");
		return;
	}

	if (file.print(message))
	{
		log_d("Message appended");
	}
	else
	{
		log_e("Append failed");
	}

	file.close();
}

// Check if I2C OLED display is connected
bool initDisplay()
{
	bool connected = false;

	if (!displayConnected)
		Wire.begin();

	Wire.beginTransmission(DISPLAY_I2C_ADDRESS);
	const uint8_t result = Wire.endTransmission();

	connected = (result == 0);

	if (connected && !displayConnected)
		u8g2.begin();
	else if (!connected)
		Wire.end();

	return connected;
}

// Initialize SD card and log info
bool mountSdCard()
{
	bool result = false;

	if (sdCardMounted || SD.begin())
	{
		uint8_t cardType = SD.cardType();

		if (cardType != CARD_NONE && cardType != CARD_UNKNOWN)
		{
			String strType = "UNKNOWN";

			if (cardType == CARD_MMC)
				strType = "MMC";
			else if (cardType == CARD_SD)
				strType = "SDSC";
			else if (cardType == CARD_SDHC)
				strType = "SDHC";

			log_d("SD Card Type: %s", strType.c_str());

			const uint64_t sizeUsedMiB = (SD.usedBytes() / 1024 / 1024);
			const uint64_t sizeTotalMiB = (SD.totalBytes() / 1024 / 1024);

			log_d("Used Space: %llu / %llu MiB", sizeUsedMiB, sizeTotalMiB);

			if (SD.totalBytes() > 0)
				result = true;
		}
		else
		{
			log_w("No SD Card attached!");
		}
	}
	else
	{
		log_e("SD Card Mount Failed!");
	}

	if (!result)
		SD.end();

	return result;
}

// Initialize display and SD card; use LED as fallback indicator
void initDevices()
{
	displayConnected = initDisplay();
	sdCardMounted = mountSdCard();

	log_i("Display Connected: %d", displayConnected);
	log_i("SD-Card Mounted:   %d", sdCardMounted);

	if (!displayConnected)
	{
		pinMode(BUILTIN_LED, OUTPUT);
		digitalWrite(BUILTIN_LED, LOW);
	}
}

// Format time as HH:MM:SS
String getFormattedTime(const std::chrono::steady_clock::time_point& time)
{
	const auto secsSinceEpoch = std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count();

	const auto hours = (secsSinceEpoch / 3600);
	const auto minutes = (secsSinceEpoch / 60) % 60;
	const auto seconds = secsSinceEpoch % 60;

	const String strHours = (hours < 10) ? "0" + String(hours) : String(hours);
	const String strMinutes = (minutes < 10) ? "0" + String(minutes) : String(minutes);
	const String strSeconds = (seconds < 10) ? "0" + String(seconds) : String(seconds);

	return (strHours + ":" + strMinutes + ":" + strSeconds);
}

// Update OLED display with current measurement
void writeDataToDisplay(const std::chrono::steady_clock::time_point& time, double valRaw, float valPercent)
{
	u8g2.clearBuffer();
	u8g2_prepare();

	const int xPosValTime = 48;
	const int xPosValRaw = 84;
	const int xPosValPercent = 84;
	const int xPosValSd = 72;

	const int yRowHeight = 12;
	const int yRowSpacing = 8;

	const String formattedTime = getFormattedTime(time);

	int yPos = 0;

	// ===== Display Time =====
	u8g2.drawStr(0, yPos, "Time:");
	u8g2.drawStr(xPosValTime, yPos, formattedTime.c_str());

	yPos += yRowHeight;
	yPos += yRowSpacing;

	// ===== Display Raw ADC Value =====
	u8g2.drawStr(0, yPos, "Value Raw:");
	u8g2.drawStr(xPosValRaw, yPos, String(valRaw, 0).c_str());

	yPos += yRowHeight;

	// ===== Display Soil Moisture in Percent =====
	u8g2.drawStr(0, yPos, "Moisture [%]:");
	u8g2.drawStr(xPosValPercent, yPos, String(valPercent, 1).c_str());

	yPos += yRowHeight;
	yPos += yRowSpacing;

	// ===== Display SD Card Mount Status =====
	const String sdStatus = sdCardMounted ? "YES" : "NO";
	u8g2.drawStr(0, yPos, "SD Mounted:");
	u8g2.drawStr(xPosValSd, yPos, sdStatus.c_str());

	u8g2.sendBuffer();
}

// Append measurement to CSV on SD card (timestamp;raw_value;percent)
void writeDataToSdCard(const std::chrono::steady_clock::time_point& time, double valRaw, float valPercent)
{
	const auto secsSinceEpoch = std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count();
	String strLine = String(secsSinceEpoch) + ";" + String(valRaw, 0) + ";" + String(valPercent, 1);

	log_d("Appending line to SD: %s", strLine.c_str());

	strLine += "\n";
	appendFile(SD, FILE_PATH.c_str(), strLine.c_str());
}

// Read ADC with averaging to reduce noise
double getAverageAdcValue(uint8_t pin, uint16_t numSamples, uint16_t delayMs)
{
	double sum = 0.0;

	for (uint16_t i = 0; i < numSamples; i++)
	{
		sum += analogRead(pin);
		delay(delayMs);
	}

	return (sum / numSamples);
}

// Take measurement and update display/SD card if needed
void takeMeasurement(bool forceSdWrite = false)
{
	// Convert ADC (0=wet to 4095=dry) to percentage (100%=wet to 0%=dry)
	const double valRaw = getAverageAdcValue(0, 10, 100);
	const float valPercent = (100.0f - (valRaw / 4095.0f) * 100.0f);

	const auto timeNow = (std::chrono::steady_clock::now() + timeOffset);

	if (displayConnected)
		writeDataToDisplay(timeNow, valRaw, valPercent);

	if (sdCardMounted)
	{
		if (forceSdWrite || timeNow - timeLastSdWrite >= INTERVAL_WRITE_SD)
		{
			writeDataToSdCard(timeNow, valRaw, valPercent);
			timeLastSdWrite = timeNow;
		}
	}
}

void setup()
{
	Serial.begin(115200);
	delay(1000);

	log_i("========== START ==========");

	// ===== Initialize Peripherals (Display & SD-Card) =====
	initDevices();

	// ===== Initialize Analog-to-Digital Converter (ADC) =====
	getAverageAdcValue(0, 5, 10);

	if (USE_DEEP_SLEEP)
	{
		takeMeasurement(false);

		const auto sleepTime = std::chrono::duration_cast<std::chrono::microseconds>(TIME_TO_SLEEP).count();
		esp_sleep_enable_timer_wakeup(sleepTime);

		log_i("Entering deep sleep mode...");

		// ===== Calculate time offset for wakeup =====
		// (Keeps system time synchronized across deep sleep cycles)
		const auto timeNow = std::chrono::steady_clock::now();
		timeOffset += std::chrono::duration_cast<std::chrono::milliseconds>(timeNow.time_since_epoch());
		timeOffset += TIME_TO_SLEEP;

		if (!displayConnected)
			digitalWrite(BUILTIN_LED, HIGH);

		esp_deep_sleep_start();
	}
}

void loop()
{
	if (!USE_DEEP_SLEEP)
	{
		// ===== Initialize Peripherals =====
		initDevices();

		takeMeasurement(false);
	}

	delay(1000);
}
