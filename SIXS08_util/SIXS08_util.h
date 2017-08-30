#ifndef SIXS08_UTIL_H
#define SIXS08_UTIL_H

#include <Arduino.h>

#ifndef GPSSerial
#define GPSSerial Serial3
#endif

#ifndef SCREEN
#define SCREEN U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI
#endif

class PowerMonitor {
  const float BATTERY_CAPACITY = 350;         // battery capacity in mA-h
  const float BATTERY_VOLTAGE = 3.7;          // V
  const float TEENSY_ON_CURRENT = 34;         // Teensy sleep currents (mA)
  const float TEENSY_SLEEP_CURRENT = 0.61;
  const float TEENSY_DEEPSLEEP_CURRENT = 0.0086;
  const float TEENSY_SUPPLY_VOLTAGE = 3.3;

  // Byte sequence to turn GPS ON and OFF
  const uint8_t GPSoff[16] = {0xB5, 0x62, 0x02, 0x41, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x4D, 0x3B};
  const uint8_t GPSon[16] = {0xB5, 0x62, 0x02, 0x41, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x4C, 0x37};

  // Current-sensing resistor parameters
  const int CURRENT_SENSE_PIN_HI = A7;    // Analog in pins connected to current-sensing resistor
  const int CURRENT_SENSE_PIN_LO = A6;
  const float CURRENT_SENSE_RESISTOR = 2; // Value of current-sensing resistor


  // Variables that hold state of the various components
  uint8_t state_teensy, state_gps, state_wifi, state_oled, state_imu;
  bool iswifi, isgps, isoled, isimu;    // do these exist (1) or not (0)
  float energyConsumed;                 // running total of energy consumed, in mJ
  float batteryCapacity;                // calculated battery capacity, in mJ
  float percentEnergyLeft;              // percent of battery energy left
  unsigned long timestamp;              // time of last update (ms)
  unsigned long teensy_sleep_time;      // duration for teensy to sleep (ms)
  unsigned long wifi_sleep_time;        // duration for wifi to sleep (ms)
  elapsedMillis wifi_sleep_timer;       // timer to check how long wifi has slept (ms)
  elapsedMillis time_since_start;       // timer for total elapsed time (ms)

  SCREEN& oled;             // references to peripherals
  MPU9250& imu;
  ESP8266& wifi;

  SnoozeTimer timer;                           //sleep timer
  SnoozeBlock config;  

  public:
    PowerMonitor(SCREEN&, MPU9250&, ESP8266&, uint8_t);
    void updateEnergy();
    void setPowerMode(String, uint8_t, unsigned long);
    void setPowerMode(String, uint8_t);
    float readPower();
    float getEnergyConsumed();
    float getBatteryLeft();
    float timeLeft();
    void displayPower();

  private:
    void sendUBX(const uint8_t *, uint8_t);
};

#endif