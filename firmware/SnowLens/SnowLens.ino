/*
 * SnowLens – ski goggle display firmware
 * Target: ATmega328P-AU @ 16 MHz (Arduino framework, "Arduino Uno" board setting)
 *
 * Reads speed and time from a NEO-6M GPS module, temperature from a BMP280
 * and the battery voltage, and shows everything on a 1.51" transparent
 * SSD1306 OLED mounted inside the goggle lens.
 *
 * Required libraries (Arduino Library Manager):
 *   - U8g2              (olikraus)
 *   - Adafruit BMP280
 *   - TinyGPSPlus       (Mikal Hart)
 *
 * Note: this file was reconstructed from the code listing in the project
 * documentation (appendix 9.7). Only formatting and comments were added;
 * the logic is unchanged.
 *
 * Author: Lorenz Alber – TFO "Oskar von Miller" Meran, 2024/25
 */

#include <SPI.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>

// ---------------------------------------------------------------- Pins ----
#define OLED_DC      9
#define OLED_RESET   8
#define OLED_CS      10
#define BATTERY_PIN  A3
#define RXPin        2      // GPS TX -> MCU RX
#define TXPin        3      // GPS RX <- MCU TX
#define LedPin       4      // status LED

// ------------------------------------------------------------- Objects ----
// Display is mounted rotated, so the picture is turned by 270° (U8G2_R3).
U8G2_SSD1306_128X64_NONAME_1_4W_HW_SPI display(U8G2_R3, OLED_CS, OLED_DC, OLED_RESET);

SoftwareSerial gpsSerial(RXPin, TXPin);
TinyGPSPlus gps;

Adafruit_BMP280 tempSensor;
bool tempSensorWorking = false;

// ------------------------------------------------------------- Helpers ----
// Rough LiPo state of charge from the terminal voltage.
uint8_t getBatteryPercentage(float voltage)
{
  if (voltage >= 4.2)  return 100;
  if (voltage >= 4.1)  return 90;
  if (voltage >= 4.0)  return 80;
  if (voltage >= 3.9)  return 70;
  if (voltage >= 3.8)  return 60;
  if (voltage >= 3.7)  return 50;
  if (voltage >= 3.6)  return 40;
  if (voltage >= 3.5)  return 30;
  if (voltage >= 3.4)  return 20;
  if (voltage >= 3.3)  return 10;
  if (voltage >= 3.25) return 5;
  return 0;
}

// --------------------------------------------------------------- Setup ----
void setup()
{
  Serial.begin(9600);
  gpsSerial.begin(9600);
  pinMode(LedPin, OUTPUT);

  display.begin();
  display.enableUTF8Print();

  // Boot screen
  display.firstPage();
  do {
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(0, 10, "Initializing...");
    display.drawFrame(0, 0, display.getWidth(), display.getHeight());
  } while (display.nextPage());
  delay(1000);

  // Temperature sensor (I2C address 0x76 or 0x77)
  Wire.begin();
  tempSensorWorking = tempSensor.begin(0x76) || tempSensor.begin(0x77);

  display.firstPage();
  do {
    display.setFont(u8g2_font_6x10_tf);
    if (tempSensorWorking) {
      display.drawStr(0, 10, "Temp sensor online");
    } else {
      display.drawStr(0, 10, "Temp sensor offline");
    }
  } while (display.nextPage());
  delay(1000);

  display.firstPage();
  do {
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(0, 10, "GPS Weather Display");
    display.drawStr(0, 25, "Starting up...");
  } while (display.nextPage());
  delay(1000);
}

// ---------------------------------------------------------------- Loop ----
void loop()
{
  digitalWrite(LedPin, HIGH);
  bool newData = false;

  // Feed NMEA data into TinyGPSPlus for 500 ms
  for (unsigned long start = millis(); millis() - start < 500;)
  {
    while (gpsSerial.available())
    {
      char c = gpsSerial.read();
      if (gps.encode(c))
        newData = true;
    }
  }

  // Battery voltage (5 V ADC reference)
  float batteryVoltage = (analogRead(BATTERY_PIN) / 1023.0) * 5.0;
  uint8_t batteryPercentage = getBatteryPercentage(batteryVoltage);

  // Temperature; -8 °C offset compensates self-heating of the electronics
  float temperature = 0.0;
  float RealTemp = 0.0;
  if (tempSensorWorking) {
    temperature = tempSensor.readTemperature();
    RealTemp = temperature - 8;
  }

  // Draw screen (page buffer mode)
  display.firstPage();
  do {
    display.setFont(u8g2_font_6x10_tf);

    // Battery
    char buffer[32];
    dtostrf(batteryVoltage, 4, 2, buffer);
    char batteryPctStr[32];
    sprintf(batteryPctStr, "%u%%", batteryPercentage);
    display.drawStr(24, 34, batteryPctStr);

    display.drawStr(4, 94, "  km/h");

    // Time (UTC + 2 h for Italy, summer time)
    if (gps.time.isValid())
    {
      char timeStr[32];
      int italyHour = (gps.time.hour() + 2) % 24;
      sprintf(timeStr, " %02d:%02d:%02d",
              italyHour, gps.time.minute(), gps.time.second());
      display.drawStr(0, 22, timeStr);
    }

    // Temperature
    if (tempSensorWorking) {
      dtostrf(RealTemp, 4, 1, buffer);
      char tempStr[32];
      sprintf(tempStr, " %s*C", buffer);
      display.drawStr(4, 46, tempStr);
    } else {
      display.drawStr(0, 46, "Temp: ERROR");
    }

    // Speed (large font)
    display.setFont(u8g2_font_10x20_tf);
    if (gps.speed.isValid())
    {
      float speedKmh = gps.speed.kmph();
      dtostrf(speedKmh, 5, 1, buffer);
      char speedStr[32];
      sprintf(speedStr, "%s", buffer);
      display.drawStr(0, 80, speedStr);
    }
    else
    {
      display.drawStr(8, 80, " N.D.");
    }
  } while (display.nextPage());

  delay(100);
}
