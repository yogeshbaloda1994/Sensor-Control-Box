////////////////////////////////////////////////////////////////////////////////////////////////////
// Program Includes
////////////////////////////////////////////////////////////////////////////////////////////////////
#include <Arduino.h>
#include <StateMachine.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <CD22.h>
#include "FreeStack.h"
#include "SdFat.h"


////////////////////////////////////////////////////////////////////////////////////////////////////
// Program Macros
////////////////////////////////////////////////////////////////////////////////////////////////////
// Set _DEBUG_ to true for verbose output on USB Serial Port
bool _DEBUG_ = true;
#define CD22_BAUD CD22_SERIAL_BAUD_230400


////////////////////////////////////////////////////////////////////////////////////////////////////
// Devices Class Instantiation
////////////////////////////////////////////////////////////////////////////////////////////////////
StateMachine machine = StateMachine();
CD22 mySensor;
LiquidCrystal_I2C lcd(0x27, 16, 2);  // set the LCD address to 0x27 for a 16 chars and 2 line display


////////////////////////////////////////////////////////////////////////////////////////////////////
// Enumerations and structs
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef enum _user_time_period {
  LOGGER_USER_TIME_PERIOD_5_SEC = 5,
  LOGGER_USER_TIME_PERIOD_10_SEC = 10,
  LOGGER_USER_TIME_PERIOD_15_SEC = 15,
  LOGGER_USER_TIME_PERIOD_30_SEC = 30,
  LOGGER_USER_TIME_PERIOD_1_MIN = 60,
  LOGGER_USER_TIME_PERIOD_2_MIN = 120,
  LOGGER_USER_TIME_PERIOD_3_MIN = 180,
  LOGGER_USER_TIME_PERIOD_4_MIN = 240,
  LOGGER_USER_TIME_PERIOD_5_MIN = 300,
  LOGGER_USER_TIME_PERIOD_10_MIN = 600,
  LOGGER_USER_TIME_PERIOD_15_MIN = 900,
  LOGGER_USER_TIME_PERIOD_20_MIN = 1200,
  LOGGER_USER_TIME_PERIOD_30_MIN = 1800,
  LOGGER_USER_TIME_PERIOD_1_HOUR = 3600
} User_Time_Period;

struct data_t {
  int16_t distanceReading;
};


////////////////////////////////////////////////////////////////////////////////////////////////////
// Global Variables
////////////////////////////////////////////////////////////////////////////////////////////////////
bool flag_InitFinished = false;
bool flag_SensorPresent = false;
bool flag_CommunicationError = false;
bool flag_UnknownError = false;
bool flag_LaserState = true;
bool flag_UserSettingsSet = false;
bool flag_RecomSettingsSet = false;
bool flag_PreExperimentCheck = false;
bool flag_DevSettingsSet = false;
bool flag_PostExperimentCheck = false;
bool flag_ExperimentRunning = false;
bool flag_SDCardError = false;
bool flag_RateTooFast = false;
bool flag_SDErrorDisplayed = false;

uint16_t User_SampleRate = 200;  // Hz
const uint32_t LOG_INTERVAL_USEC = 1000000.0 / User_SampleRate;
User_Time_Period User_TimePeriod = LOGGER_USER_TIME_PERIOD_1_MIN;

////////////////////////////////////////////////////////////////////////////////////////////////////
// SD Card defines, macros and globals
////////////////////////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
// This program was designed for exFAT but will support FAT16/FAT32.
// Note: Uno will not support SD_FAT_TYPE = 3.
// SD_FAT_TYPE = 0 for SdFat/File as defined in SdFatConfig.h,
// 1 for FAT16/FAT32, 2 for exFAT, 3 for FAT16/FAT32 and exFAT.
#define SD_FAT_TYPE 2

/*
  Change the value of SD_CS_PIN if you are using SPI and
  your hardware does not use the default value, SS.
  Common values are:
  Arduino Ethernet shield: pin 4
  Sparkfun SD shield: pin 8
  Adafruit SD shields and modules: pin 10
*/

// SDCARD_SS_PIN is defined for the built-in SD on some boards.
#ifndef SDCARD_SS_PIN
const uint8_t SD_CS_PIN = SS;
#else   // SDCARD_SS_PIN
// Assume built-in SD is used.
const uint8_t SD_CS_PIN = SDCARD_SS_PIN;
#endif  // SDCARD_SS_PIN

// FIFO SIZE - 512 byte sectors.  Modify for your board.
#ifdef __AVR_ATmega328P__
// Use 512 bytes for 328 boards.
#define FIFO_SIZE_SECTORS 1
#elif defined(__AVR__)
// Use 2 KiB for other AVR boards.
#define FIFO_SIZE_SECTORS 4
#else  // __AVR_ATmega328P__
// Use 8 KiB for non-AVR boards.
#define FIFO_SIZE_SECTORS 16
#endif  // __AVR_ATmega328P__

// Preallocate 1GiB file.
const uint32_t PREALLOCATE_SIZE_MiB = 1024UL;

// Try max SPI clock for an SD. Reduce SPI_CLOCK if errors occur.
#define SPI_CLOCK SD_SCK_MHZ(50)

// Try to select the best SD card configuration.
#if HAS_SDIO_CLASS
#define SD_CONFIG SdioConfig(FIFO_SDIO)
#elif ENABLE_DEDICATED_SPI
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SPI_CLOCK)
#else  // HAS_SDIO_CLASS
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI, SPI_CLOCK)
#endif  // HAS_SDIO_CLASS

// Save SRAM if 328.
#ifdef __AVR_ATmega328P__
#include "MinimumSerial.h"
MinimumSerial MinSerial;
#define Serial MinSerial
#endif  // __AVR_ATmega328P__


////////////////////////////////////////////////////////////////////////////////////////////////////
// /// SD Card Functions
////////////////////////////////////////////////////////////////////////////////////////////////////

// Replace logRecord(), printRecord(), and ExFatLogger.h for your sensors.
void logRecord(data_t* data, uint16_t overrun) {
  if (overrun) {
    // Add one since this record has no data. Could add overrun field.
    overrun++;
  } else {
    data->distanceReading = mySensor.readDistanceRaw();
  }
}
//------------------------------------------------------------------------------
void printRecord(Print* pr, data_t* data) {
  static uint32_t nr = 0;
  if (!data) {
    pr->print(F("LOG_INTERVAL_USEC,"));
    pr->println(LOG_INTERVAL_USEC);
    pr->print(F("rec#"));
    pr->print(F(",distanceReading"));
    pr->println();
    nr = 0;
    return;
  }

  pr->print(nr++);
  pr->write(',');
  pr->print(data->distanceReading);
  pr->println();
}

void printRecordUserUnits(Print* pr, data_t* data) {
  static uint32_t nr = 0;
  if (!data) {
    pr->print(F("Log Interval in micro-seconds,"));
    pr->println(LOG_INTERVAL_USEC);
    pr->print(F("Sensor Reading divider for mm,"));
    pr->println(uint8_t(1000.0/mySensor.least_count));   
    pr->print(F("Index, "));
    pr->print(F("Raw Reading, ")); 
    pr->print(F("Milli-Seconds, "));
    pr->print(F("mm"));
    pr->println();
    nr = 0;
    return;
  }
  // Raw Reading
  pr->print(nr);
  pr->write(',');
  pr->print(data->distanceReading);
  pr->print(',');

  // User Units
  pr->print((nr++)*LOG_INTERVAL_USEC/1000.0);
  pr->write(',');
  pr->print((mySensor.least_count*data->distanceReading)/1000.0);
  pr->println();
}
//==============================================================================
const uint64_t PREALLOCATE_SIZE = (uint64_t)PREALLOCATE_SIZE_MiB << 20;
// Max length of file name including zero byte.
#define FILE_NAME_DIM 40
// Max number of records to buffer while SD is busy.
const size_t FIFO_DIM = 512 * FIFO_SIZE_SECTORS / sizeof(data_t);

#if SD_FAT_TYPE == 0
typedef SdFat sd_t;
typedef File file_t;
#elif SD_FAT_TYPE == 1
typedef SdFat32 sd_t;
typedef File32 file_t;
#elif SD_FAT_TYPE == 2
typedef SdExFat sd_t;
typedef ExFile file_t;
#elif SD_FAT_TYPE == 3
typedef SdFs sd_t;
typedef FsFile file_t;
#else  // SD_FAT_TYPE
#error Invalid SD_FAT_TYPE
#endif  // SD_FAT_TYPE

sd_t sd;

file_t binFile;
file_t csvFile;
// You may modify the filename.  Digits before the dot are file versions.
char binName[] = "CD22Logger00.bin";
//------------------------------------------------------------------------------
#if USE_RTC
#if USE_RTC == 1
RTC_DS1307 rtc;
#elif USE_RTC == 2
RTC_DS3231 rtc;
#elif USE_RTC == 3
RTC_PCF8523 rtc;
#else  // USE_RTC == type
#error USE_RTC type not implemented.
#endif  // USE_RTC == type

// Call back for file timestamps.  Only called for file create and sync().
void dateTime(uint16_t* date, uint16_t* time, uint8_t* ms10) {
  DateTime now = rtc.now();

  // Return date using FS_DATE macro to format fields.
  *date = FS_DATE(now.year(), now.month(), now.day());

  // Return time using FS_TIME macro to format fields.
  *time = FS_TIME(now.hour(), now.minute(), now.second());

  // Return low time bits in units of 10 ms.
  *ms10 = now.second() & 1 ? 100 : 0;
}
#endif  // USE_RTC

//------------------------------------------------------------------------------
#define error(s) sd.errorHalt(&Serial, F(s))
#define dbgAssert(e) ((e) ? (void)0 : error("assert " #e))
//-----------------------------------------------------------------------------
// Convert binary file to csv file.
void binaryToCsv() {
  uint8_t lastPct = 0;
  uint32_t t0 = millis();
  data_t binData[FIFO_DIM];

  if (!binFile.seekSet(512)) {
    Serial.println("binFile.seek failed");
    flag_SDCardError = true;
    return;
  }
  uint32_t tPct = millis();
  printRecordUserUnits(&csvFile, nullptr);
  while (binFile.available()) {
    int nb = binFile.read(binData, sizeof(binData));
    if (nb <= 0) {
      Serial.println("read binFile failed");
      flag_SDCardError = true;
      return;
    }
    size_t nr = nb / sizeof(data_t);
    for (size_t i = 0; i < nr; i++) {
      printRecordUserUnits(&csvFile, &binData[i]);
    }

    if ((millis() - tPct) > 1000) {
      uint8_t pct = binFile.curPosition() / (binFile.fileSize() / 100);
      if (pct != lastPct) {
        tPct = millis();
        lastPct = pct;
        Serial.print(pct, DEC);
        Serial.println('%');
        csvFile.sync();
      }
    }
    if (Serial.available()) {
      break;
    }
  }
  csvFile.close();
  Serial.print(F("Done: "));
  Serial.print(0.001 * (millis() - t0));
  Serial.println(F(" Seconds"));
}

//------------------------------------------------------------------------------
void clearSerialInput() {
  uint32_t m = micros();
  do {
    if (Serial.read() >= 0) {
      m = micros();
    }
  } while (micros() - m < 10000);
}

//-------------------------------------------------------------------------------
bool createBinFile() {
  binFile.close();
  while (sd.exists(binName)) {
    char* p = strchr(binName, '.');
    if (!p) {
      Serial.println("no dot in filename");
      return false;
    }
    while (true) {
      p--;
      if (p < binName || *p < '0' || *p > '9') {
        Serial.println("Can't create file name");
        return false;
      }
      if (p[0] != '9') {
        p[0]++;
        break;
      }
      p[0] = '0';
    }
  }
  if (!binFile.open(binName, O_RDWR | O_CREAT)) {
    Serial.println("open binName failed");
    return false;
  }
  Serial.println(binName);
  if (!binFile.preAllocate(PREALLOCATE_SIZE)) {
    Serial.println("preAllocate failed");
    return false;
  }

  Serial.print(F("preAllocated: "));
  Serial.print(PREALLOCATE_SIZE_MiB);
  Serial.println(F(" MiB"));

  return true;
}

//-------------------------------------------------------------------------------
bool createCsvFile() {
  char csvName[FILE_NAME_DIM];
  if (!binFile.isOpen()) {
    Serial.println(F("No current binary file"));
    return false;
  }

  // Create a new csvFile.
  binFile.getName(csvName, sizeof(csvName));
  char* dot = strchr(csvName, '.');
  if (!dot) {
    Serial.println("no dot in filename");
    return false;
  }
  strcpy(dot + 1, "csv");
  if (!csvFile.open(csvName, O_WRONLY | O_CREAT | O_TRUNC)) {
    Serial.println("open csvFile failed");
    return false;
  }
  clearSerialInput();
  Serial.print(F("Writing: "));
  Serial.println(csvName);
  for(uint8_t i= 0; i<*dot; i++)
  {
    lcd.setCursor(i, 1);
    lcd.print(csvName[i]);
  }

  return true;
}

//-------------------------------------------------------------------------------
bool logData() {
  int32_t delta;  // Jitter in log time.
  int32_t maxDelta = 0;
  uint32_t maxLogMicros = 0;
  uint32_t maxWriteMicros = 0;
  size_t maxFifoUse = 0;
  size_t fifoCount = 0;
  size_t fifoHead = 0;
  size_t fifoTail = 0;
  uint16_t overrun = 0;
  uint16_t maxOverrun = 0;
  uint32_t totalOverrun = 0;
  uint32_t fifoBuf[128 * FIFO_SIZE_SECTORS];
  data_t* fifoData = (data_t*)fifoBuf;

  // Write dummy sector to start multi-block write.
  if(!(sizeof(fifoBuf) >= 512))
  {
      Serial.println("Size of FIFO Buffer >= 512");
      return false;
  }
  memset(fifoBuf, 0, sizeof(fifoBuf));
  if (binFile.write(fifoBuf, 512) != 512) {
    Serial.println("write first sector failed");
    return false;
  }
  clearSerialInput();
  Serial.print(F("Logging for "));
  Serial.print(uint16_t(User_TimePeriod));
  Serial.println(F(" Seconds"));

  // Wait until SD is not busy.
  while (sd.card()->isBusy()) {
  }

  // Start time for log file.
  uint32_t m = millis();

  // Time to log next record.
  uint32_t logTime = micros();

  unsigned long startMillis = millis();
  unsigned long currentMillis;

  while (true) {

    currentMillis = millis();

    // Time for next data record.
    logTime += LOG_INTERVAL_USEC;

    // Wait until time to log data.
    delta = micros() - logTime;
    if (delta > 0) {
      Serial.print(F("delta: "));
      Serial.println(delta);
      Serial.println("Rate too fast");
      flag_RateTooFast = true;
      return false;
    }
    while (delta < 0) {
      delta = micros() - logTime;
    }

    if (fifoCount < FIFO_DIM) {
      uint32_t m = micros();
      logRecord(fifoData + fifoHead, overrun);
      m = micros() - m;
      if (m > maxLogMicros) {
        maxLogMicros = m;
      }
      fifoHead = fifoHead < (FIFO_DIM - 1) ? fifoHead + 1 : 0;
      fifoCount++;
      if (overrun) {
        if (overrun > maxOverrun) {
          maxOverrun = overrun;
        }
        overrun = 0;
      }
    } else {
      totalOverrun++;
      overrun++;
      if (overrun > 0XFFF) {
        error("too many overruns");
      }
    }
    // Save max jitter.
    if (delta > maxDelta) {
      maxDelta = delta;
    }
    // Write data if SD is not busy.
    if (!sd.card()->isBusy()) {
      size_t nw = fifoHead > fifoTail ? fifoCount : FIFO_DIM - fifoTail;
      // Limit write time by not writing more than 512 bytes.
      const size_t MAX_WRITE = 512 / sizeof(data_t);
      if (nw > MAX_WRITE) nw = MAX_WRITE;
      size_t nb = nw * sizeof(data_t);
      uint32_t usec = micros();
      if (nb != binFile.write(fifoData + fifoTail, nb)) {
        Serial.println("write binFile failed");
        return false;
      }
      usec = micros() - usec;
      if (usec > maxWriteMicros) {
        maxWriteMicros = usec;
      }
      fifoTail = (fifoTail + nw) < FIFO_DIM ? fifoTail + nw : 0;
      if (fifoCount > maxFifoUse) {
        maxFifoUse = fifoCount;
      }
      fifoCount -= nw;
      if (uint16_t((currentMillis - startMillis)/1000) >= uint16_t(User_TimePeriod)) {
        break;
      }
    }
  }
  Serial.print(F("\nLog time: "));
  Serial.print(0.001 * (millis() - m));
  Serial.println(F(" Seconds"));
  binFile.truncate();
  binFile.sync();
  Serial.print(("File size: "));
  // Warning cast used for print since fileSize is uint64_t.
  Serial.print((uint32_t)binFile.fileSize());
  Serial.println(F(" bytes"));
  Serial.print(F("totalOverrun: "));
  Serial.println(totalOverrun);
  Serial.print(F("FIFO_DIM: "));
  Serial.println(FIFO_DIM);
  Serial.print(F("maxFifoUse: "));
  Serial.println(maxFifoUse);
  Serial.print(F("maxLogMicros: "));
  Serial.println(maxLogMicros);
  Serial.print(F("maxWriteMicros: "));
  Serial.println(maxWriteMicros);
  Serial.print(F("Log interval: "));
  Serial.print(LOG_INTERVAL_USEC);
  Serial.print(F(" micros\nmaxDelta: "));
  Serial.print(maxDelta);
  Serial.println(F(" micros"));

  return true;
}

//-----------------------------------------------------------------------------
void printData() {
  if (!binFile.isOpen()) {
    Serial.println(F("No current binary file"));
    return;
  }
  // Skip first dummy sector.
  if (!binFile.seekSet(512)) {
    Serial.println("seek failed");
    flag_SDCardError = true;
    return;
  }
  clearSerialInput();
  Serial.println(F("type any character to stop\n"));
  delay(1000);
  printRecord(&Serial, nullptr);
  while (binFile.available() && !Serial.available()) {
    data_t record;
    if (binFile.read(&record, sizeof(data_t)) != sizeof(data_t)) {
      Serial.println("read binFile failed");
      flag_SDCardError = true;
      return;
    }
    printRecord(&Serial, &record);
  }
}

//------------------------------------------------------------------------------
void printUnusedStack() {
#if HAS_UNUSED_STACK
  Serial.print(F("\nUnused stack: "));
  Serial.println(UnusedStack());
#endif  // HAS_UNUSED_STACK
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// User Defined Functions
////////////////////////////////////////////////////////////////////////////////////////////////////
bool checkComm() {
  CD22_SensorType senType = mySensor.CheckSensorType();
  if ((senType == CD22_SENSOR_15_485) || (senType == CD22_SENSOR_35_485) || (senType == CD22_SENSOR_100_485))
    return true;
  else
    return false;
}

void checkError() {
  flag_CommunicationError = true;
  if (mySensor.CheckSensorType() == CD22_SENSOR_NO_SENSOR)
    flag_UnknownError = false;
  else
    flag_UnknownError = true;
}

void forceUknownError() {
  flag_CommunicationError = true;
  flag_UnknownError = true;
}

void resetSensor()
{
  mySensor.LaserOFF();
  mySensor.releaseZeroReset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// State Logic Functions
////////////////////////////////////////////////////////////////////////////////////////////////////
void State_Init();
void State_SensorDetect();
void State_DeviceSettings();
void State_UnknownError();
void State_UserSettings();
void State_RecomSettings();
void State_PreExperiment();
void State_PostExperiment();
void State_RunExperiment();
void State_SDCardError();

////////////////////////////////////////////////////////////////////////////////////////////////////
// State Machine - Create States
////////////////////////////////////////////////////////////////////////////////////////////////////
State* Init = machine.addState(&State_Init);
State* SensorDetect = machine.addState(&State_SensorDetect);
State* DeviceSettings = machine.addState(&State_DeviceSettings);
State* UnkwnError = machine.addState(&State_UnknownError);
State* UserSettings = machine.addState(&State_UserSettings);
State* RecomSettings = machine.addState(&State_RecomSettings);
State* PreExperiment = machine.addState(&State_PreExperiment);
State* PostExperiment = machine.addState(&State_PostExperiment);
State* RunExperiment = machine.addState(&State_RunExperiment);
State* SDCardError = machine.addState(&State_SDCardError);


////////////////////////////////////////////////////////////////////////////////////////////////////
// State Machine - State Functinos
////////////////////////////////////////////////////////////////////////////////////////////////////
void State_Init()
{
  delay(500);

  if (_DEBUG_) {
    Serial.begin(9600);
    Serial.println("\n\nEntering State: Init");
    Serial.println("Starting Device");
  }

  flag_SDCardError = false;

  // initialize the lcd
  lcd.init();
  lcd.backlight();
  // Print a message to the LCD.
  lcd.setCursor(0, 0);
  lcd.print("Starting Device");
  lcd.setCursor(0, 1);
  lcd.print("@  ");
  lcd.setCursor(2, 1);
  lcd.print(String(CD22_BAUD));
  lcd.setCursor(11, 1);
  lcd.print("BAUD");

  delay(3000);

  FillStack();

#if !ENABLE_DEDICATED_SPI
  if (_DEBUG_)
  {
    Serial.println(
      F("\nFor best performance edit SdFatConfig.h\n"
        "and set ENABLE_DEDICATED_SPI nonzero"));
  }
#endif  // !ENABLE_DEDICATED_SPI

  if (_DEBUG_)
  {
    Serial.print(FIFO_DIM);
    Serial.println(F(" FIFO entries will be used."));
  }

  // Initialize SD.
  while (!sd.begin(SD_CONFIG))
  {
    if (_DEBUG_)
      Serial.println("No SD Card Detcted. Please insert SD Card");
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("No SD Card");
    lcd.setCursor(0, 1);
    lcd.print("Insert SD Card");
    delay(1000);
  }

  if (_DEBUG_)
      Serial.println("SD Card Detcted");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SD Card Detcted");

#if USE_RTC
  if (!rtc.begin()) {
    error("rtc.begin failed");
  }
  if (!rtc.isrunning()) {
    // Set RTC to sketch compile date & time.
    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    error("RTC is NOT running!");
  }
  // Set callback
  FsDateTime::setCallback(dateTime);
#endif  // USE_RTC

  delay(2000);

  flag_InitFinished = true;
}

void State_SensorDetect() {

  if (_DEBUG_)
    Serial.println("\n\nEntering State: SensorDetect");

  flag_SensorPresent = false;
  flag_CommunicationError = false;
  flag_LaserState = true;
  flag_UserSettingsSet = false;
  flag_RecomSettingsSet = false;
  flag_PreExperimentCheck = false;
  flag_DevSettingsSet = false;
  flag_PostExperimentCheck = false;
  flag_ExperimentRunning = false;
  flag_SDErrorDisplayed = false;

  lcd.clear();

  // Start device communication
  CD22_SensorType senType = mySensor.begin(CD22_SERIAL_TYPE_HW, CD22_SERIAL_BAUD_230400, 60);  // Last number is time waited for response before erroring out (0ms - 255ms)
  //Returns ideal center point (15mm, 35mm, 100mm) based on the sensor connected
  if (senType != CD22_SENSOR_NO_SENSOR) {
    flag_SensorPresent = true;

    if (_DEBUG_) {
      Serial.print("Sensor Detected: CD-");
      Serial.print(uint8_t(senType));
      Serial.println("-485");
    }

    lcd.setCursor(0, 0);
    lcd.print("Sensor Detected: ");
    lcd.setCursor(0, 1);

    switch (senType) {
      case CD22_SENSOR_15_485:
        lcd.print("CD22-15-485");
        break;
      case CD22_SENSOR_35_485:
        lcd.print("CD22-35-485");
        break;
      case CD22_SENSOR_100_485:
        lcd.print("CD22-100-485");
        break;
      default:
        checkError();
    }
  } else {
    flag_SensorPresent = false;

    if (_DEBUG_)
      Serial.println("No Sensor Detected. Please Check Sensor Conenctions");

    lcd.setCursor(0, 0);
    lcd.print("No CD22 Detected");
    lcd.setCursor(0, 1);
    lcd.print("Chk Wiring/Baud");
  }

  delay(2000);
}

void State_DeviceSettings() {

  if (_DEBUG_)
    Serial.println("\n\nEntering State: DeviceSettings");

  if (flag_LaserState) {
    if (!mySensor.LaserOFF())
      checkError();
    else {
      if (_DEBUG_)
        Serial.println("Turning Laser Off");

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Set Laser Off");
      delay(1000);

      flag_LaserState = false;
    }
  }

  if (!checkComm())
    checkError();

  // Print currently set Sample Rate & Currently set Averaging Setting
  CD22_SensorSampleTime SampleTime = mySensor.getSampleTime();
  CD22_SensorAvgNum Averaging = mySensor.getAveraging();

  if (_DEBUG_)
    Serial.print("Current Sample Time is ");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("S.Time: ");
  lcd.setCursor(0, 1);
  lcd.print("Avg: ");

  lcd.setCursor(10, 0);
  switch (SampleTime) {
    case CD22_SENSOR_SAMPLE_TIME_500US:
      lcd.print("500us");
      if (_DEBUG_)
        Serial.println("500us");
      break;
    case CD22_SENSOR_SAMPLE_TIME_1000US:
      lcd.print("1000us");
      if (_DEBUG_)
        Serial.println("1000us");
      break;
    case CD22_SENSOR_SAMPLE_TIME_2000US:
      lcd.print("2000us");
      if (_DEBUG_)
        Serial.println("2000us");
      break;
    case CD22_SENSOR_SAMPLE_TIME_4000US:
      lcd.print("4000us");
      if (_DEBUG_)
        Serial.println("4000us");
      break;
    case CD22_SENSOR_SAMPLE_TIME_AUTO:
      lcd.print("AUTO");
      if (_DEBUG_)
        Serial.println("AUTO");
      break;
  }

  if (_DEBUG_)
    Serial.print("Current Avergaing is set to ");

  lcd.setCursor(13, 1);
  switch (Averaging) {
    case CD22_SENSOR_AVG_NUM_1:
      lcd.print("1");
      if (_DEBUG_)
        Serial.println("1");
      break;
    case CD22_SENSOR_AVG_NUM_8:
      lcd.print("8");
      if (_DEBUG_)
        Serial.println("8");
      break;
    case CD22_SENSOR_AVG_NUM_64:
      lcd.print("64");
      if (_DEBUG_)
        Serial.println("64");
      break;
    case CD22_SENSOR_AVG_NUM_512:
      lcd.print("512");
      if (_DEBUG_)
        Serial.println("512");
      break;
  }

  delay(2000);
  flag_DevSettingsSet = true;
}

void State_UnknownError() {
  if (_DEBUG_) {
    Serial.println("\n\nEntering State: Uknown Error");
    Serial.println("Uknown Error");
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Uknown Error");

  delay(1000);
}

void State_UserSettings() {
  flag_PostExperimentCheck = false;
  //User Input

  if (_DEBUG_) {
    Serial.println("\n\nEntering State: UserSettings");
    Serial.print("Logging Sample Rate is ");
    Serial.print(String(User_SampleRate));
    Serial.println("Hz");
    Serial.print("Logging Time Period is set to: ");
    Serial.print(String(User_TimePeriod));
    Serial.println(" Seconds");
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("S.Time: ");
  lcd.setCursor(0, 1);
  lcd.print("Time: ");

  lcd.setCursor(14, 0);
  lcd.print("Hz");

  lcd.setCursor(10, 0);
  lcd.print(User_SampleRate);

  lcd.setCursor(10, 1);
  switch (User_TimePeriod) {
    case LOGGER_USER_TIME_PERIOD_5_SEC:
      lcd.print(" 5SEC");
      break;
    case LOGGER_USER_TIME_PERIOD_10_SEC:
      lcd.print("10SEC");
      break;
    case LOGGER_USER_TIME_PERIOD_15_SEC:
      lcd.print("15SEC");
      break;                
    case LOGGER_USER_TIME_PERIOD_30_SEC:
      lcd.print("30SEC");
      break;
    case LOGGER_USER_TIME_PERIOD_1_MIN:
      lcd.print(" 1MIN");
      break;
    case LOGGER_USER_TIME_PERIOD_2_MIN:
      lcd.print(" 2MIN");
      break;
    case LOGGER_USER_TIME_PERIOD_3_MIN:
      lcd.print(" 3MIN");
      break;
    case LOGGER_USER_TIME_PERIOD_4_MIN:
      lcd.print(" 4MIN");
      break;
    case LOGGER_USER_TIME_PERIOD_5_MIN:
      lcd.print(" 5MIN");
      break;
    case LOGGER_USER_TIME_PERIOD_10_MIN:
      lcd.print("10MIN");
      break;
    case LOGGER_USER_TIME_PERIOD_15_MIN:
      lcd.print("15MIN");
      break;
    case LOGGER_USER_TIME_PERIOD_20_MIN:
      lcd.print("20MIN");
      break;
    case LOGGER_USER_TIME_PERIOD_30_MIN:
      lcd.print("30MIN");
      break;
    case LOGGER_USER_TIME_PERIOD_1_HOUR:
      lcd.print("1HOUR");
      break;
  }

  delay(2000);
  flag_UserSettingsSet = true;  // Replace with actual conditions
}

void State_RecomSettings() {

  if (_DEBUG_)
    Serial.println("\n\nEntering State: RecomSettings");

  CD22_SensorSampleTime RecommendedSampleTime;
  CD22_SensorAvgNum RecommendedAveraging;
  float User_SampleTime_us = (1000000.0 / User_SampleRate);

  if (User_SampleTime_us < 2000) {
    RecommendedSampleTime = CD22_SENSOR_SAMPLE_TIME_500US;
    RecommendedAveraging = CD22_SENSOR_AVG_NUM_1;
  } else if ((User_SampleTime_us >= 2000) && (User_SampleTime_us < 4000)) {
    RecommendedSampleTime = CD22_SENSOR_SAMPLE_TIME_1000US;
    RecommendedAveraging = CD22_SENSOR_AVG_NUM_1;
  } else if ((User_SampleTime_us >= 4000) && (User_SampleTime_us < 8000)) {
    RecommendedSampleTime = CD22_SENSOR_SAMPLE_TIME_2000US;
    RecommendedAveraging = CD22_SENSOR_AVG_NUM_1;
  } else if ((User_SampleTime_us >= 8000) && (User_SampleTime_us < 16000)) {
    RecommendedSampleTime = CD22_SENSOR_SAMPLE_TIME_500US;
    RecommendedAveraging = CD22_SENSOR_AVG_NUM_8;
  } else if ((User_SampleTime_us >= 16000) && (User_SampleTime_us < 32000)) {
    RecommendedSampleTime = CD22_SENSOR_SAMPLE_TIME_1000US;
    RecommendedAveraging = CD22_SENSOR_AVG_NUM_8;
  } else if ((User_SampleTime_us >= 32000) && (User_SampleTime_us < 64000)) {
    RecommendedSampleTime = CD22_SENSOR_SAMPLE_TIME_2000US;
    RecommendedAveraging = CD22_SENSOR_AVG_NUM_8;
  } else if ((User_SampleTime_us >= 64000) && (User_SampleTime_us < 128000)) {
    RecommendedSampleTime = CD22_SENSOR_SAMPLE_TIME_4000US;
    RecommendedAveraging = CD22_SENSOR_AVG_NUM_8;
  } else if ((User_SampleTime_us >= 128000) && (User_SampleTime_us < 256000)) {
    RecommendedSampleTime = CD22_SENSOR_SAMPLE_TIME_1000US;
    RecommendedAveraging = CD22_SENSOR_AVG_NUM_64;
  } else if ((User_SampleTime_us >= 256000) && (User_SampleTime_us < 512000)) {
    RecommendedSampleTime = CD22_SENSOR_SAMPLE_TIME_2000US;
    RecommendedAveraging = CD22_SENSOR_AVG_NUM_64;
  } else if ((User_SampleTime_us >= 512000) && (User_SampleTime_us < 1024000)) {
    RecommendedSampleTime = CD22_SENSOR_SAMPLE_TIME_500US;
    RecommendedAveraging = CD22_SENSOR_AVG_NUM_512;
  } else if ((User_SampleTime_us >= 1024000) && (User_SampleTime_us < 2048000)) {
    RecommendedSampleTime = CD22_SENSOR_SAMPLE_TIME_1000US;
    RecommendedAveraging = CD22_SENSOR_AVG_NUM_512;
  } else {
    RecommendedSampleTime = CD22_SENSOR_SAMPLE_TIME_2000US;
    RecommendedAveraging = CD22_SENSOR_AVG_NUM_512;
  }

  // Change Sample Rate
  if (!mySensor.setSampleTime(RecommendedSampleTime))
    checkError();

  // Change Averaging Setting
  if (!mySensor.setAveraging(RecommendedAveraging))
    checkError();

  if (_DEBUG_)
    Serial.println("Recommended Settings Applied Successfully");

  flag_RecomSettingsSet = true;
}

void State_PreExperiment() {

  if (_DEBUG_)
    Serial.println("\n\nEntering State: PreExperiment");

  if (!flag_LaserState) {
    if (!mySensor.LaserON())
      checkError();
    else {
      if (_DEBUG_)
        Serial.println("Turning Laser On");

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Set Laser On");
      delay(1000);

      flag_LaserState = true;
    }
  }

  if (!mySensor.executeZeroReset())
    checkError();
  else {
    if (_DEBUG_)
      Serial.println("Setting Zero");

    lcd.setCursor(0, 1);
    lcd.print("Setting Zero");
    delay(1000);
  }

  delay(1000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Starting the");
  lcd.setCursor(0, 1);
  lcd.print("Experiment Now!");
  delay(1000);

  if (!checkComm())
    checkError();

  if (_DEBUG_)
    Serial.println("Starting Experiment Now");

  if(!createBinFile())
  {
    flag_SDCardError = true;
  }
  else
    flag_PreExperimentCheck = true;
}

void State_PostExperiment() {

  if (_DEBUG_)
    Serial.println("\n\nEntering State: PostExperiment");  

  mySensor.flush();

  lcd.clear();
  lcd.setCursor(0, 0);

  if (flag_LaserState)
  {
    if (!mySensor.LaserOFF())
    {
      checkError();

      if (_DEBUG_)
        Serial.println("Error turning Laser Off");

      lcd.print("Laser Off Fail");
      delay(1000);        
    }
    else {

      if (_DEBUG_)
        Serial.println("Turning Laser Off");


      lcd.print("Set Laser Off");
      delay(1000);

      flag_LaserState = false;
    }
  }
  else
  {
    forceUknownError();
  }

  lcd.setCursor(0, 1);
  if (!mySensor.releaseZeroReset())
  {
    checkError();
    if (_DEBUG_)
      Serial.println("Error Releasing Zero");

    lcd.print("Rel. Zero Fail");
    delay(1000);    
  }
  else {
    if (_DEBUG_)
      Serial.println("Releasing Zero");

    lcd.print("Releasing Zero");
    delay(1000);
  }

  if (!checkComm())
    checkError();

  lcd.clear();
  if (createCsvFile())
  {
    lcd.setCursor(0, 0);
    lcd.print("Creating CSV");
    binaryToCsv();
    flag_PostExperimentCheck = true;
    delay(2000);
  }
  else
  {
    flag_SDCardError = true;
    flag_PostExperimentCheck = false;
  }
}

void State_RunExperiment() {

  if (_DEBUG_)
    Serial.println("\n\nEntering State: RunExperiment");  

  if (_DEBUG_)
    Serial.println("Starting Experiment Now");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Running the");
  lcd.setCursor(0, 1);
  lcd.print("Experiment Now!");

  flag_ExperimentRunning = true;

  if(logData())
  {
  if (_DEBUG_)
    Serial.println("Experiment Finished");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Experiment");
  lcd.setCursor(0, 1);
  lcd.print("Done!");
  delay(1000);
  flag_ExperimentRunning = false;
  }
  else
  {
    flag_SDCardError = true;
  }
}

void State_SDCardError() {

  if (_DEBUG_)
    Serial.println("\n\nEntering State: SDCardError");

  resetSensor();

  if(flag_RateTooFast)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Rate Too Fast");
    lcd.setCursor(0, 1);
    lcd.print("Reduce S. Rate");
  }
  else
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SD Error: Try ");
    lcd.setCursor(0, 1);
    lcd.print("Format/Deleting");    
  }

  delay(2000);
  flag_SDErrorDisplayed = true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// State Machine - State Transition Functions
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsInitFinished(void) {
  return flag_InitFinished;
}
bool IsSensorPresent(void) {
  return flag_SensorPresent;
}
bool IsCommError(void) {
  return (flag_CommunicationError & !flag_UnknownError);
}
bool IsUnknownError(void) {
  return (flag_CommunicationError & flag_UnknownError);
}
bool IsUserSettingsSet(void) {
  return flag_UserSettingsSet;
}
bool IsUserRecomSettingsSet(void) {
  return flag_RecomSettingsSet;
}
bool IsPreExperimentCheckOK(void) {
  return flag_PreExperimentCheck;
}
bool IsDevSettingsSet(void) {
  return flag_DevSettingsSet;
}
bool IsPostExperimentCheckOK(void) {
  return flag_PostExperimentCheck;
}
bool IsExperimentingFinished(void) {
  return !flag_ExperimentRunning;
}

bool IsSDCardInErrorState(void) {
  return flag_SDCardError;
}
bool IsSDErrorDisplayed(void)
{
  return flag_SDErrorDisplayed;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Arduino Setup Function
////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {

  Init->addTransition(&IsInitFinished, SensorDetect);

  SensorDetect->addTransition(&IsSensorPresent, UserSettings);

  UserSettings->addTransition(&IsUserSettingsSet, RecomSettings);

  RecomSettings->addTransition(&IsCommError, SensorDetect);
  RecomSettings->addTransition(&IsUnknownError, UnkwnError);
  RecomSettings->addTransition(&IsUserRecomSettingsSet, DeviceSettings);

  DeviceSettings->addTransition(&IsCommError, SensorDetect);
  DeviceSettings->addTransition(&IsUnknownError, UnkwnError);
  DeviceSettings->addTransition(&IsDevSettingsSet, PreExperiment);

  PreExperiment->addTransition(&IsCommError, SensorDetect);
  PreExperiment->addTransition(&IsUnknownError, UnkwnError);
  PreExperiment->addTransition(&IsSDCardInErrorState, SDCardError);
  PreExperiment->addTransition(&IsPreExperimentCheckOK, RunExperiment);

  RunExperiment->addTransition(&IsSDCardInErrorState, SDCardError);
  RunExperiment->addTransition(&IsExperimentingFinished, PostExperiment);

  PostExperiment->addTransition(&IsCommError, SensorDetect);
  PostExperiment->addTransition(&IsUnknownError, UnkwnError);
  PostExperiment->addTransition(&IsSDCardInErrorState, SDCardError);  
  PostExperiment->addTransition(&IsPostExperimentCheckOK, UserSettings);

  SDCardError->addTransition(&IsSDErrorDisplayed, Init);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Arduino Loop Function
////////////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
  machine.run();
}
