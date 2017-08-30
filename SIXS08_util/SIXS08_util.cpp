#include <WString.h>
#include <Arduino.h>
#include <Snooze_6s08.h>
#include <MPU9250.h>
#include <U8g2lib.h>
#include <Wifi_S08_v2.h>
#include <SPI.h>
#include <Wire.h>
#include "SIXS08_util.h"


/* Constructor. Takes in one 8-bit input, c, that determines which
components are being used in our system. We assume Teensy is always present!
Other components each take 1 bit, as shown below:   
      c: 0 0 0 0 OLED IMU GPS WIFI
      So 00001101 = 0x0D would mean GPS is not present */   
PowerMonitor::PowerMonitor(SCREEN& display, MPU9250& imu,  ESP8266& wifi, uint8_t c) :  oled(display), imu(imu), wifi(wifi)
  {
    config = SnoozeBlock(timer);

    energyConsumed = 0;
    batteryCapacity = BATTERY_CAPACITY * BATTERY_VOLTAGE * 3600; // mJoules
    percentEnergyLeft = 100;            // Start out at 100%
    timestamp = 0;
    time_since_start = 0;
    wifi_sleep_timer = 0;
    wifi_sleep_time = 0;
    teensy_sleep_time = 0;

    iswifi = isgps = isoled = isimu = false;    // Assume no components
    if (c | 0x01 << 0) iswifi = true;           // Add components depending on bits in c
    if (c | 0x01 << 1) isgps = true;
    if (c | 0x01 << 2) isimu = true;    
    if (c | 0x01 << 3) isoled = true;

    // initially, all components are on
    state_teensy = 2;   //0=deepsleep, 1 = sleep, 2 = normal
    state_gps = 1;      //0=sleep, 1 = on
    state_wifi = 1;     //0=sleep, 1 = on aka modem-sleep
    state_oled = 1;     //0=sleep, 1 = on
    state_imu = 1;      //0=sleep, 1 = on
  }


/* Determine the energy consumed since the last update. Updates 
class variables energyConsumed and percentEnergyLeft */
void PowerMonitor::updateEnergy() {
  unsigned long dT = millis() - timestamp;  // Time since last update
  float pw = 0;                             // power usage during this interval
  
  if (state_teensy==2) {          // if Teensy's awake, measure power directly
    pw = readPower();
  } else if (state_teensy==1) {   // else, estimate power
    pw = TEENSY_SLEEP_CURRENT * TEENSY_SUPPLY_VOLTAGE;
  } else if (state_teensy==0) {
    pw = TEENSY_DEEPSLEEP_CURRENT * TEENSY_SUPPLY_VOLTAGE;
  }

  float deltaEnergy = dT / 1000.0 * pw;       // increment of energy since last timestamp
  energyConsumed += deltaEnergy;              // mJ 
  percentEnergyLeft -= deltaEnergy / batteryCapacity * 100;

  // also check wifi timer in case wifi was put to sleep and has now woken up
  if (!state_wifi) {                          
    if (wifi_sleep_timer >= wifi_sleep_time) { 
      state_wifi = 1;     // update wifi state to ON
    }
  }

  timestamp = millis();                       // update new timestamp
}
  
/* Set power mode of component 
Inputs:
  Arduino String p: can be "teensy", "wifi"
  8-bit unsigned integer m: value depends on component */
void PowerMonitor::setPowerMode(String p, uint8_t m, unsigned long t){
  // Before changing power mode, update the energy consumed for last interval
  updateEnergy();   

  // Teensy modes are 2: Normal, 1: Sleep, 0: DeepSleep
  if (p=="teensy") {
    if (m != state_teensy) {    // Only set mode if different from current state
      if (t != teensy_sleep_time) {   // update sleep time if different than before
        teensy_sleep_time = t;
        timer.setTimer(teensy_sleep_time);
      }
      if (m==1) {
        state_teensy = m;
        Snooze.sleep(config);   // This is blocking, so will pause here
        updateEnergy();         // update energy again to capture energy during sleep
        state_teensy = 2;         
        } 
      else if (m==0) {
        state_teensy = m;
        Snooze.deepSleep(config); // This is blocking, so will pause here
        updateEnergy();
        state_teensy = 2;
      }
    }
  }

  // Wifi modes are 1: On, 0: DeepSleep
  else if (iswifi && (p=="wifi")) {
    if (m == 0) {
      if (t != wifi_sleep_time) {   // update sleep time if different than before
        wifi_sleep_time = t;
      }
      String response = wifi.sendCustomCommand(String("AT+GSLP=" + String(wifi_sleep_time)),100);
      wifi_sleep_timer = 0;
    }
    state_wifi = m;
  }    
}


/* Set power mode of OLED, IMU, GPS
Inputs:
  Arduino String p: can be "oled", "imu", "gps"
  8-bit unsigned integer m: 1 (wake) or 0 (sleep) */
void PowerMonitor::setPowerMode(String p, uint8_t m){
  // Before changing power mode, update the energy consumed for last interval
  updateEnergy();   

  // OLED modes are 1: Normal, 0: Sleep
  if (isoled && (p=="oled")) {
    if (m != state_oled) {
      if (m==1) {
        oled.setPowerSave(0);       // u8g2 power save mode enabled
      } else if (m==0) {
        oled.setPowerSave(1);       // disabled
      }
      state_oled = m;
    }
  }

  // GPS modes are 1: On, 0: Sleep
  else if (isgps && (p=="gps")) {
    if (m != state_gps) {
      if (m==0) {
        sendUBX(GPSoff, sizeof(GPSoff)/sizeof(uint8_t));
      } else if (m==1) {
        sendUBX(GPSon, sizeof(GPSon)/sizeof(uint8_t));        
      }
      state_gps = m;
    }     
  }

  //  IMU modes are 1: On, 0: Sleep
  else if (isimu && (p=="imu")) {
    if (m != state_imu) {
      if (m==0) {
        imu.sleep();
      } else if (m==1) {
        imu.wake();
      }
    }
  }
}


// Read the instataneous system power via the current-sensing resistor
float PowerMonitor::readPower() {
  int v2 = analogRead(CURRENT_SENSE_PIN_HI); 
  float v_p = v2 * 2 * 3.3 / 4096;
  int v1 = analogRead(CURRENT_SENSE_PIN_LO);
  float v_m = v1 * 2 * 3.3 / 4096;
  float i = (v_p - v_m) / CURRENT_SENSE_RESISTOR;
  return  1000 * v_m * i;    // mW
}

// Returns energyConsumed for use by outside world
float PowerMonitor::getEnergyConsumed() {
  return energyConsumed;
}

// Returns percentEnergyLeft for use by outside world
float PowerMonitor::getBatteryLeft() {
  return percentEnergyLeft;
}

// Send byte to GPS to power ON and OFF
void PowerMonitor::sendUBX(const uint8_t *MSG, uint8_t len) {
  for(int i=0; i<len; i++) {
    GPSSerial.write(MSG[i]);
  }
}

// Estimate the time (in minutes) left based on usage to-date.
float PowerMonitor::timeLeft() {
  float min_since_start = time_since_start / (1000.0 * 60);
  float total_lifetime_min = min_since_start / (1 - 0.01*getBatteryLeft());
  return total_lifetime_min - min_since_start;
}

void PowerMonitor::displayPower(){
	float pleft = getBatteryLeft();
	oled.drawFrame(104,3, 20, 8);
	oled.drawBox(124,5,2,4);
	int covered  = int((20.0)*pleft/100.0);
	oled.drawBox(104,3,covered,8);
}