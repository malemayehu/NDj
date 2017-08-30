#include <Wire.h>
#include <MPU9250.h>
#include <U8g2lib.h>
#include <Wifi_S08_v2.h>
#include <SPI.h>
#include <math.h>
#include <Snooze_6s08.h>
#include <SIXS08_util.h>


#define SCREEN U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI
#define SPI_CLK 14
#define ARRAY_SIZE 200
#define SAMPLE_FREQ 8000                           // Hz, telephone sample rate
#define SAMPLE_DURATION 3                          // sec
#define NUM_SAMPLES SAMPLE_FREQ*SAMPLE_DURATION    // number of of samples
#define ENC_LEN (NUM_SAMPLES + 2 - ((NUM_SAMPLES + 2) % 3)) / 3 * 4  // Encoded length of clip
#define API_KEY AIzaSyAoWHhpQixMaNuOtRhi1yIpNXL2ffICQTI
#define TRANSFER_BAUD 38400 // adjust baud here
#define HWSERIAL Serial3
#define OVERFLOW_ARRAY 2500

/* LIGHT VARIABLES */
elapsedMillis lightTimer;
int lightSwitchTime = 1000;
int green = 2;
int red = 3;
int blue = 4;
String currentLight = "FFF";
String lightMode = "flash";
//////////////////////



//Advisors:
//Joe and Mark
//mchoun95@mit.edu/ gmail

void receive_serial_data();
void send_serial_data(String input[]);
void pretty_print(int startx, int starty, String input, int fwidth, int fheight, int spacing, SCREEN &display);
String send_to_server(String request_type, float array[]);
void filter_response(String input);
void send_serial_string(String input);
void smooth_data(float *ain, float *aout, int m);
void update_array(float *ar, float newData);
void gesture_scan();
void  toggle_slave_pause();
void slave_next();
void slave_prev();
void slave_random();
void reinitialize();
float find_peak(float *ar, int b1, int b2);



// Global constants

/* CONSTANTS */
const char PREFIX[] = "{\"config\":{\"encoding\":\"MULAW\",\"sampleRate\":8000},\"audio\": {\"content\":\"";
const char SUFFIX[] = "\"}}";
const int PREFIX_SIZE = sizeof(PREFIX);
const int SUFFIX_SIZE = sizeof(SUFFIX);
const int BIT_DEPTH = 16;
const int LED_PIN = 13;               // Teensy LED Pin
const int BUTTON_PIN1 = 23;             // pushbutton
const int BUTTON_PIN2 = 22;             // pushbutton
const int BUTTON_PIN3 = 21;             // pushbutton
//const int BUTTON_PIN4 = 20;             // pushbutton

const int SAMPLE_INTERVAL = 25;       // time in-between IMU readings
const int SCREEN_HEIGHT = 64;         // OLED dimensions
const int SCREEN_WIDTH = 128;
const int FONT_HEIGHT = 11;           // Font dimensions
const int FONT_WIDTH = 6;
const int LINE_SPACING = 0;
const int NUM_GESTURES = 3;
const String gesture_names[] = {"skip", "rewind", "shuffle"};
const int Y_INC = FONT_HEIGHT + LINE_SPACING;
const String KERBEROS = "rodbay";  // your kerberos
const String GPATH = "/6S08dev/" + KERBEROS + "/final/sb3.py";
const String IPATH = "/6S08dev/" + KERBEROS + "/final/sb1.py";
const String IESC = "iesc-s2.mit.edu";
const int NUMTRACKS = 255;
//const int AUDIO_IN = A3;
const int SLEEP_DELAY = 15000;
const int STATE_DELAY = 5000;
const bool S08 = false;// toggle Internet connection


// variables

int data_index = 0;
int state = -1;
String MAC = "";
String out = "";
int gesture_num = 0;
int tracknum = 0;
String dataStr = "";
String index_str = "";
int resp_tracknum = 0;
float mag_accel;                      // acceleration magnitude
int MIN_TIME = 200;
float THRESHOLD = 1.2;
int track_index = 0;
bool paused = false;
int volume = 5;
int cswitch = 0;
bool volchange = false;


elapsedMillis init_timer;
elapsedMillis time_since_update = 0;  // time since last screen update
elapsedMillis wait_time = 0;                    // time since last step
elapsedMillis sleep_timer = 0;
elapsedMillis time_since_sample = 0;  // time since last screen update

//char speech_data[PREFIX_SIZE + ENC_LEN + SUFFIX_SIZE];
String unsortedTracks[NUMTRACKS];
int sortedTracks[NUMTRACKS] = {-1};
float accel_data[ARRAY_SIZE] = {0};   // initialize array holding accel data
float data[ARRAY_SIZE];





ESP8266 wifi = ESP8266(0, true);
SCREEN oled(U8G2_R2, 10, 15, 16);
MPU9250 imu;
PowerMonitor pm(oled, imu, wifi, 0x0F);


void setup_angle() {
  Serial.print("Check IMU:");
  char c = imu.readByte(MPU9250_ADDRESS, WHO_AM_I_MPU9250);
  Serial.print("MPU9250: "); Serial.print("I AM "); Serial.print(c, HEX);
  Serial.print(" I should be "); Serial.println(0x73, HEX);
  if (c == 0x73) {
    imu.MPU9250SelfTest(imu.selfTest);
    imu.initMPU9250();
    imu.calibrateMPU9250(imu.gyroBias, imu.accelBias);
    imu.initMPU9250();
    imu.initAK8963(imu.factoryMagCalibration);
  } // if (c == 0x73)
  else
  {
    while (1) Serial.println("NOT FOUND"); // Loop forever if communication doesn't happen
  }
  imu.getAres();
  imu.getGres();
  imu.getMres();
}

void get_angle(float&x, float&y) {
  imu.readAccelData(imu.accelCount);
  x = imu.accelCount[0] * imu.aRes;
  y = imu.accelCount[1] * imu.aRes;
}

void lights(int r, int g, int b) {
  digitalWrite(red, !r);
  digitalWrite(green, !g);
  digitalWrite(blue, !b);
  String x,y,z;
  if(r == HIGH)
    x = "T";
  else
    x = "F";
  if(g == HIGH)
    y = "T";
  else
    y = "F";
  if(b == HIGH)
    z = "T";
  else
    z = "F";
  currentLight = x + y + z;
}

void lights() {
  if(lightMode == "flash") {
    if(currentLight == "TFF") {
      lights(LOW, HIGH, LOW);
    } else if(currentLight == "FTF") {
      lights(LOW, LOW, HIGH);
    } else {
      lights(HIGH,LOW,LOW);
    }
  } else if(lightMode == "fade") {
    if(currentLight == "TTF") {
      lights(LOW, HIGH, HIGH);
    } else if(currentLight == "FTT") {
      lights(HIGH, LOW, HIGH);
    } else {
      lights(HIGH, HIGH, LOW);
    }
  }
}

class Button {
    int state;
    int flag;
    elapsedMillis t_since_change; //timer since switch changed value
    elapsedMillis t_since_state_2; //timer since entered state 2 (reset to 0 when entering state 0)
    unsigned long debounce_time;
    unsigned long long_press_time;
    int pin;
    bool button_pressed;
  public:
    Button(int p) {
      state = 0;
      pin = p;
      t_since_change = 0;
      t_since_state_2 = 0;
      debounce_time = 10;
      long_press_time = 1000;
      button_pressed = 0;
    }
    void read() {
      bool button_state = digitalRead(pin);  // true if HIGH, false if LOW
      button_pressed = !button_state; // Active-low logic is hard, inverting makes our lives easier.
    }
    int update() {
      read();
      flag = 0;
      if (state == 0) { // Unpressed, rest state
        if (button_pressed) {
          state = 1;
          t_since_change = 0;
        }
      } else if (state == 1) { //Tentative pressed
        if (!button_pressed) {
          state = 0;
          t_since_change = 0;
        } else if (t_since_change >= debounce_time) {
          state = 2;
          t_since_state_2 = 0;
        }
      } else if (state == 2) { // Short press
        if (!button_pressed) {
          state = 4;
          t_since_change = 0;
        } else if (t_since_state_2 >= long_press_time) {
          state = 3;
        }
      } else if (state == 3) { //Long press
        if (!button_pressed) {
          state = 4;
          t_since_change = 0;
        }
      } else if (state == 4) { //Tentative unpressed
        if (button_pressed && t_since_state_2 < long_press_time) {
          state = 2; // Unpress was temporary, return to short press
          t_since_change = 0;
        } else if (button_pressed && t_since_state_2 >= long_press_time) {
          state = 3; // Unpress was temporary, return to long press
          t_since_change = 0;
        } else if (t_since_change >= debounce_time) { // A full button1 push is complete
          state = 0;
          if (t_since_state_2 < long_press_time) { // It is a short press
            flag = 1;
          } else {  // It is a long press
            flag = 2;
          }
        }
      }
      return flag;
    }
};

Button button1(BUTTON_PIN1);
Button button2(BUTTON_PIN2);
Button button3(BUTTON_PIN3);
//Button button4(BUTTON_PIN4);



void setup() {
  Serial.begin(115200);
  Serial3.begin(TRANSFER_BAUD);

  Serial.println("boot");

  Wire.begin();
  SPI.setSCK(14);
  pinMode(LED_PIN, OUTPUT);            // Set up output LED
  pinMode(BUTTON_PIN1, INPUT_PULLUP);   // Button pullup
  pinMode(BUTTON_PIN2, INPUT_PULLUP);   // Button pullup
  pinMode(BUTTON_PIN3, INPUT_PULLUP);   // Button pullup
//  pinMode(BUTTON_PIN4, INPUT_PULLUP);   // Button pullup

  pinMode(red, OUTPUT);
  pinMode(blue, OUTPUT);
  pinMode(green, OUTPUT);
  
  oled.begin();               // Screen startup
  oled.setFont(u8g2_font_profont11_mf);

  imu.initMPU9250();          // IMU startup
  imu.MPU9250SelfTest(imu.selfTest);
  imu.calibrateMPU9250(imu.gyroBias, imu.accelBias);
  imu.getAres();


  for(int i = 0; i < NUMTRACKS; i ++){
  unsortedTracks[i]= "";
  sortedTracks[i]= -1;}

  oled.clearBuffer();         // Wifi startup
  oled.setCursor(0, Y_INC);
  oled.print("connecting to wifi...");
  oled.sendBuffer();
  wifi.begin();
  if(S08){
  wifi.connectWifi("6s08", "iesc6s08");}
else{
  wifi.connectWifi("MIT", "");}
  MAC = wifi.getMAC();

  
while(!wifi.isConnected());// while loop was making it freeze
init_timer = 0;
pinMode(13, OUTPUT);
}


///////////////////////////////////////////////////////////////////LOOP/////////////////////////////////////////////////////////////////////////////

//TODO: Make a tuturial that people can flip through

void loop()
{
  //button flag values
  int flag1 = button1.update();
  int flag2 = button2.update();
  int flag3 = button3.update();

//soft reset hold all 3 buttons
  if(flag1 == flag2 == flag3 == 2){
    reinitialize();
  }
  if(flag1 == flag3 == 2 && flag2 ==0 && state < 3){ // Calibrate new gestures to 
    state = 7;
  }

  //keeps indices in sync
  if(state != -1 && HWSERIAL.available()){ // update the current track number
    Serial.println("Track update");
    if(HWSERIAL.peek() == '@'){
      String temp = HWSERIAL.readStringUntil('$');
      Serial.println(temp);
      track_index = temp.substring(1,temp.indexOf('\n')).toInt();
  }
  }

  if(lightTimer >= lightSwitchTime) {
    lights();
    lightTimer = 0;
  }

// update volume from button presses
  if(state != -1 && flag1 == 1 && flag3 == 0){ 
  if(volume < 10) volume++;
  volchange = true;
  }
  else if(state != -1 && flag1 == 0 && flag3 == 1){
  if(volume > 0) volume--;
  volchange = true;

  }

  if(volchange){
    slave_vol();
  }


  if (state == -1) {   // init state
    if(init_timer > 5000){ //check for connection
          Serial3.write(1);
          init_timer = 0;
    }

    if (Serial3.available() > 0) {
      oled.clearBuffer();
      pretty_print(0, 2 * Y_INC, "Received from Slave", 6, 8, 2, oled);
      oled.sendBuffer();
      Serial.println("Received from Slave");
      receive_serial_data();
      Serial.println(dataStr);
      oled.clearBuffer();
      pretty_print(0, 2 * Y_INC, "STEP", 6, 8, 2, oled);
      oled.sendBuffer();
      int anchor1 = 15;
      int anchor2 = 0;
      String delim = "*";
      // for our own record (mapping song title to number if the array transfers don't work)
      for ( int i = 0; i < tracknum; i++) {
        anchor2 = dataStr.indexOf(delim, anchor1);
        if (anchor2 == -1) {
          unsortedTracks[i] = dataStr.substring(anchor1);
          tracknum = i + 1;
          break;
        }
        else {
          unsortedTracks[i] = dataStr.substring(anchor1 , anchor2);
          anchor1 = anchor2 + 1;
        }
      }
      if(dataStr.length() <= 1800){
        Serial.println(dataStr);
      wifi.sendRequest(POST, IESC, 80, IPATH, dataStr, false);}
      else{
        char request_array[OVERFLOW_ARRAY];
        dataStr.toCharArray(request_array, OVERFLOW_ARRAY);
        wifi.sendBigRequest(IESC, 80, IPATH, request_array);
        //wifi.sendBigRequest(POST, IESC, 80, IPATH, dataStr);        
      }
      delay(100);
      while (!wifi.hasResponse()); // wait for response
      }
    if (wifi.hasResponse()) {
      oled.clearBuffer();
      pretty_print(0, 2 * Y_INC, "Sending to Slave", 6, 8, 2, oled);
      oled.sendBuffer();
      Serial.println("step");
      String response = wifi.getResponse();
      response = response.substring(7, response.indexOf("</html>") -1);
      Serial.println(response);
      oled.clearBuffer();
      pretty_print(0, 2 * Y_INC, "Sending to Slave", 6, 8, 2, oled);
      oled.sendBuffer();
      filter_response(response);
      delay(500);
      send_serial_string(index_str);
      state = 0;
    }

  oled.clearBuffer();
  oled.drawStr(0,15,"init");
  oled.sendBuffer();
  sleep_timer = 0;

  } else if (state == 0) {   // STATE 0 Base state ( play state), wifi on
  if(sleep_timer > SLEEP_DELAY){
     state = 1;
     pm.setPowerMode("wifi", 0, 1000000); // rlly big number
    }
    delay(100);
    gesture_scan();
    String pauseString = "";
     oled.clearBuffer();
    oled.setCursor(0, Y_INC);
    oled.print("Now Playing: ");
    oled.setCursor(0, 2 * Y_INC);
    oled.print(unsortedTracks[sortedTracks[track_index]]);
    if(paused){ pauseString = "Double tap to pause";} else {pauseString = "Double tap to play";}
    oled.setCursor(0, 3 * Y_INC);
    oled.print(pauseString);
    pretty_print(0, 4 * Y_INC, "Long press for gesture controls", 6, 8, 2, oled);
    oled.sendBuffer();
    if(flag2 == 2) {
      state = 2;
      pm.setPowerMode("wifi", 1, 0);  
      if(!wifi.isConnected()){
      if(S08){ wifi.connectWifi("6s08", "iesc6s08");} else{ wifi.connectWifi("MIT", "");}}
    }

  } else if (state == 1) {  // STATE 1, wifi SLEEP
        delay(100);

    if (flag2 == 2) { // If long press transition to Gestures
      state = 2;
      pm.setPowerMode("wifi", 1, 0);
      if(!wifi.isConnected()){
      if(S08){ wifi.connectWifi("6s08", "iesc6s08");} else{ wifi.connectWifi("MIT", "");}}
      init_timer = 0;
    } 
    gesture_scan();
    String pauseString = "";
    oled.clearBuffer();
    oled.setCursor(0, Y_INC);
    oled.print("Now Playing: ");
    oled.setCursor(0, 2 * Y_INC);
    oled.print(unsortedTracks[sortedTracks[track_index]]);
    if(paused){ pauseString = "Double tap to pause";} else {pauseString = "Double tap to play";}
    oled.setCursor(0, 3 * Y_INC);
    oled.print(pauseString);
    pretty_print(0, 4 * Y_INC, "Long press for gesture controls", 6, 8, 2, oled);
    oled.sendBuffer();
  } else if (state == 2) {  // recording vestibule. If this segment times out then return to base state
        delay(100);

    if (flag2 == 1) {
      state = 3;
      data_index = 0;
    }
    else if (flag2 == 2 && flag1 == flag3 == 0) { // long press to return to base state
      state = 0;
      sleep_timer = 0;
    }
    if(init_timer > STATE_DELAY){ // return to base state
      state = 0;
      sleep_timer = 0;
    }
    oled.clearBuffer();
    oled.setCursor(0, Y_INC);
    oled.print("Controls:");
    oled.setCursor(0, 2 * Y_INC);
    oled.print("Shake right: skip next");
    oled.setCursor(0, 3 * Y_INC);
    oled.print("Shake left: skip prev");
    oled.setCursor(0, 4 * Y_INC);
    oled.print("Shake it up: shuffle");
    oled.setCursor(0, 5 * Y_INC);
    oled.print("Short press to proceed");
    oled.sendBuffer();
  } else if (state == 3) { // Recording for get.
    if (flag2 == 1 || flag2 == 2) {
      state = 4;
    }
    else {
      imu.readAccelData(imu.accelCount);
      time_since_sample = 0;
      imu.ay = imu.accelCount[1] * imu.aRes;
      data[data_index] = imu.ay;
      data_index++;
      if (data_index >= ARRAY_SIZE) {
        state = 4;
        init_timer = 0;
      }
      while (time_since_sample <= SAMPLE_INTERVAL);
    }
    digitalWrite(LED_PIN, HIGH);
    oled.clearBuffer();
    oled.setCursor(0, Y_INC);
    oled.print("Shake left or right: ");
    oled.setCursor(0, 2 * Y_INC);
    oled.print("Recording gesture...");
    oled.setCursor(0, 3 * Y_INC);
    oled.print("Press button to stop");
    oled.sendBuffer();
    init_timer = 0;
    sleep_timer = 0;
  } else if (state == 4) { // Reject or accept gesture
    if (flag2 == 1) {
      state = 5;
    } else if (flag2 == 2 && flag1 == flag3 == 0) {
      state = 2;
      data_index = 0;
    }
    if(init_timer > STATE_DELAY){ // skip to the next state
      state = 5;
    }
     // we are reusing variables here to save space
    digitalWrite(LED_PIN, LOW);
    oled.clearBuffer();
    oled.setCursor(0, Y_INC);
    oled.print("Controls: ");
    oled.setCursor(0, 2 * Y_INC);
    oled.print("Short press to skip");
    oled.setCursor(0, 3 * Y_INC);
    oled.print("Long press to retry");
    oled.sendBuffer();
  } else if (state == 5) {  // Upload gesture
        delay(100);

    oled.clearBuffer();
    oled.setCursor(0, Y_INC);
    oled.print("Controls: ");
    oled.setCursor(0, 2 * Y_INC);
    oled.print("sending...");
    oled.sendBuffer();
    out = send_to_server("get", data); // send and compare to server
    oled.clearBuffer();
    oled.setCursor(0, 10);
    oled.print("Controls: ");
    pretty_print(0, 2 * Y_INC, out, 6, 8, 2, oled);
    oled.sendBuffer();
    delay(1500);
    if(out.toLowerCase().indexOf("skip") != -1){ // choose a method
      state = 6;
      cswitch = 1;
    }
    else if(out.toLowerCase().indexOf("rewind") != -1){
      state = 6;
      cswitch = 2;
    } else if(out.toLowerCase().indexOf("shuffle") != -1){
      state = 6;
      cswitch = 3;
    }// add new commands here
  } else if (state == 6) {  // execute commands
  String msg = "";
    switch (cswitch) {
            case 0:
      msg = "ERROR, no command received";
      break;
            case 1:
      msg = "Skipping to next track \n";
      slave_next();
      break;
            case 2:
      slave_prev();
      msg = "Skipping to last track \n";
      break;
            case 3:
      slave_random();
      msg = "Shuffle to random song";
      break;}

    pretty_print(0, Y_INC, msg, 6, 8, 2, oled); // update method call to display
    delay(STATE_DELAY/4);
    cswitch = 0;
    state = 0;
    sleep_timer = 0;

  } else if (state == 7) {  // upload gestures to Server loop
     if (flag1 > 0) {// scroll through gestures
      gesture_num = (gesture_num + 1) % NUM_GESTURES;
    } if (flag3 > 0) {
      gesture_num = (gesture_num - 1) % NUM_GESTURES;
    }
    if (flag2 == 1) {
      state = 8;
      data_index = 0;
    }
    else if (flag2 == 2 && flag1 == flag3 == 0) { // return to base state
      state = 0;
    }
    if(init_timer > STATE_DELAY){ // return to base state
      state = 0;
      sleep_timer = 0;
    }
    oled.clearBuffer();
    oled.setCursor(0, Y_INC);
    oled.print("Calibrate: " + gesture_names[gesture_num]);
    oled.setCursor(0, 2 * Y_INC);
    oled.print("Short press to return");
    oled.setCursor(0, 3 * Y_INC);
    oled.print("Long press to record");
    oled.setCursor(0, 4 * Y_INC);
    oled.print("Volume buttons to scroll");
    oled.sendBuffer();
  } else if (state == 8) {  // Record gesture == state 3
    if (flag2 == 1 || flag2 == 2) {
      state = 9;
    } else {
      imu.readAccelData(imu.accelCount);
      time_since_sample = 0;
      imu.ay = imu.accelCount[1] * imu.aRes;
      data[data_index] = imu.ay;
      data_index++;
      if (data_index >= ARRAY_SIZE) {
        state = 9;
      }
      while (time_since_sample <= SAMPLE_INTERVAL);
    }
    digitalWrite(LED_PIN, HIGH);
    oled.clearBuffer();
    oled.setCursor(0, Y_INC);
    oled.print("Calibrate: ");
    oled.setCursor(0, 2 * Y_INC);
    oled.print("recording gesture...");
    oled.setCursor(0, 3 * Y_INC);
    oled.print("press to stop");
    oled.sendBuffer();
    init_timer = 0;
  } else if (state == 9) {  // Accept or reject gesture == state 4
  if (flag2 == 1) {
      state = 10;
    } else if (flag2 == 2 && flag1 == flag3 == 0) {
      state = 7;
      init_timer = 0;
    }
    if(init_timer > STATE_DELAY){ // skip to the next state
      state = 10;
    }
     // we are reusing variables here to save space
    digitalWrite(LED_PIN, LOW);
    oled.clearBuffer();
    oled.setCursor(0, Y_INC);
    oled.print("Controls: ");
    oled.setCursor(0, 2 * Y_INC);
    oled.print("Short press to accept");
    oled.setCursor(0, 3 * Y_INC);
    oled.print("Long press to retry");
    oled.sendBuffer();
    ///////////////////////////////////////
  } else if (state == 10) { // Send to server and store in database, displays response from server

    digitalWrite(LED_PIN, LOW);
    oled.clearBuffer();
    oled.setCursor(0, Y_INC);
    oled.print("Calibrate: ");
    oled.setCursor(0, 2 * Y_INC);
    oled.print("Uploading...");
    delay(1000);
    oled.sendBuffer();
    out = send_to_server("post", data);
    oled.clearBuffer();
    oled.setCursor(0, Y_INC);
    oled.print("Calibrate: ");
    pretty_print(0, 2 * Y_INC, out, 6, 8, 2, oled);
    oled.sendBuffer();
    delay(1500);

    state = 0;
    sleep_timer = 0;
  } 


}


/////////////////////////////////////////////////////////////////////////SLAVE FXNS/////////////////////////////////////////////////////////////////////////////
void toggle_slave_pause(){
  Serial3.write("I");
  if(paused){
    paused = false;
  } else{
    paused = true;
  }
}
void slave_next(){
  Serial3.write("N");
while(!Serial3.available()); // wait for response
}
void slave_prev(){
  Serial3.write("P");
while(!Serial3.available());
}
void slave_random(){
  Serial3.write("R");
while(!Serial3.available());
}
void slave_vol(){
  Serial3.write("V");
  Serial3.print(volume);
}

void reinitialize(){ // reinitialize all variables of importance
  pm.setPowerMode("wifi", 1, 0);
  if(!wifi.isConnected()){
      if(S08){ wifi.connectWifi("6s08", "iesc6s08");} else{ wifi.connectWifi("MIT", "");}}
  init_timer = 0;
  sleep_timer = 0;
  paused = false;
  Serial3.print("Z");
  state = -1;
  for(int i = 0; i < NUMTRACKS; i ++){
  unsortedTracks[i]= "";
  sortedTracks[i]= -1;}
  for(int i = 0; i < ARRAY_SIZE; i++){
  accel_data[i] = 0;   // initialize array holding accel data
  data[i] = 0;}

}


///////////////////////////////////////////////////////////////////UTIL/////////////////////////////////////////////////////////////////////////////


void gesture_scan(){
  imu.readAccelData(imu.accelCount);
  // calculate acceleration in g's and store in ax, ay, and az
  imu.ax = (float)imu.accelCount[0]*imu.aRes;
  imu.ay = (float)imu.accelCount[1]*imu.aRes;
  imu.az = (float)imu.accelCount[2]*imu.aRes;

	// calculate the acceleration magnitude
	mag_accel = sqrt(pow(imu.ax,2) + pow(imu.ay,2) + pow(imu.az,2));
	
		// shift new data into array
		update_array(accel_data, mag_accel);

		// smooth the data with moving-average filter
		float smoothed_data[ARRAY_SIZE] = {0};	
		smooth_data(accel_data, smoothed_data, 3);// adjust the order if taps arent being found

		// look for peak, if peak is above threshold AND enough time has passed 
		// since last step, then this is a new step
		float peak = find_peak(smoothed_data, -1 ,-1);    //EDIT THIS FUNCTION BELOW
    float p[5] = {0};
    int arr_div = ARRAY_SIZE/5;
    int counter = 0;
   if(peak >= THRESHOLD && sleep_timer >= SLEEP_DELAY){
     state = 0;
    sleep_timer = 0;
    for(int i = 0; i < 5; i++){
      if(i != 4){
      p[i]= find_peak(smoothed_data, arr_div*i, arr_div*(i+1));}
      else{
        p[i] = find_peak(smoothed_data, arr_div*i, -2);
      }
    }
    for(int i = 0; i < 5; i ++){
      if(p[i] > THRESHOLD){
        counter++;
      }
    }
    if(counter > 2){ // if there are 2 taps
      sleep_timer = 0;
      toggle_slave_pause();
    }
   }
}


int* parse_int_arr(String input, String delim, int& arrlen){
      int anchor1 = 0;
      int anchor2;
      int temparr[tracknum];

      // for our own record (mapping song title to number if the array transfers don't work)
      for ( int i = 0; i < NUMTRACKS; i++) {
        anchor2 = input.indexOf(delim, anchor1);
        if (anchor2 == -1) {
          temparr[i] = input.substring(anchor1).toInt();
          arrlen = i;
          break;
        }
        else {
          temparr[i] = input.substring(anchor1, anchor2).toInt();
          anchor1 = anchor2;
        }
      }
            return temparr;

}
String* parse_str_arr(String input, String delim, int& arrlen){
      int anchor1 = 0;
      int anchor2;
      String temparr[tracknum];

      // for our own record (mapping song title to number if the array transfers don't work)
      for ( int i = 0; i < NUMTRACKS; i++) {
        anchor2 = input.indexOf(delim, anchor1);
        if (anchor2 == -1) {
          temparr[i] = input.substring(anchor1);
          arrlen = i;
          break;
        }
        else {
          temparr[i] = input.substring(anchor1, anchor2);
          anchor1 = anchor2;
        }
      }
      return temparr;
}

String send_to_server(String request_type, float array[]) {
  String string_array = "";         // Formulate data into a string
  for (int i = 0; i < data_index - 1; i++) {
    string_array += String(array[i]) + ",";
  }
  string_array += String(array[data_index - 1]);  // add last piece of data

  if (request_type == "post") {
    String data = "kerberos=" + KERBEROS      // Formulate request string
                  + "&gesture_name=" + gesture_names[gesture_num]
                  + "&gesture=" + string_array;
    wifi.sendRequest(POST, IESC, 80, GPATH, data);
  } else if (request_type == "get") {
    String data = "kerberos=" + KERBEROS      // Formulate request string
                  + "&gesture=" + string_array;
    wifi.sendRequest(GET, IESC, 80, GPATH, data);
  }
  while (!wifi.hasResponse());
  String raw_string = wifi.getResponse();
  return raw_string.substring(6, raw_string.indexOf("</html>"));
}


void pretty_print(int startx, int starty, String input, int fwidth, int fheight, int spacing, SCREEN &display) {
  int x = startx;
  int y = starty;
  String temp = "";
  for (int i = 0; i < input.length(); i++) {
    if (fwidth * temp.length() <= (SCREEN_WIDTH - fwidth - x)) {
      if (input.charAt(i) == '\n') {
        display.setCursor(x, y);
        display.print(temp);
        y += (fheight + spacing);
        temp = "";
        if (y > SCREEN_HEIGHT) break;
      } else {
        temp.concat(input.charAt(i));
      }
    } else {
      display.setCursor(x, y);
      display.print(temp);
      temp = "";
      y += (fheight + spacing);
      if (y > SCREEN_HEIGHT) break;
      if (input.charAt(i) != '\n') {
        temp.concat(input.charAt(i));
      } else {
        display.setCursor(x, y);
        y += (fheight + spacing);
        if (y > SCREEN_HEIGHT) break;
      }
    }
    if (i == input.length() - 1) {
      display.setCursor(x, y);
      display.print(temp);
    }
  }
}
////////////////////////////////////////////////////////////////////////////////////SERIAL///////////////////////////////////////////////////////
//mchoun95@mit.edu/ gmail
void receive_serial_data(){
  int counter = 0;
  bool receiving = true;
  String builder = "get&Song_names=";
  while(receiving){
  String temp = Serial3.readStringUntil('\n'); // could be ser3
  Serial.println(temp);
  unsortedTracks[counter] = temp;
  delay(1);
  if(Serial3.peek() == '$'){ // $ to end transmission.
    Serial3.println('$');
    Serial3.flush();
    receiving = false;
    break;
  }
  else if(Serial3.peek() == '+'){
    Serial3.read();
    unsortedTracks[counter] = temp + Serial3.readStringUntil('\n');
  }
  counter++;
}
for(int i = 0; i <= counter; i++){
  if(i == counter){
    builder += unsortedTracks[i];
    break;
  }
  Serial.print(unsortedTracks[i]);
  builder += unsortedTracks[i] + "*";
}
dataStr = builder;
Serial.println(dataStr);
tracknum = counter;
}

void send_serial_data(String input[]){
    for(int i = 0; i < NUMTRACKS; i++){
            delay(2);
      if(sizeof(input[i]) < Serial3.availableForWrite()){
        Serial3.print(input[i]+'\n');
      }
      else
      {
        int loops = sizeof(input[i])/Serial3.availableForWrite() + 1; // create a scaling factor for the size of each chunk
        int strLen  = input[i].length();
        String msg = "";
        int anchor  = 0;
        for(int j = 0; j < loops; j ++){ // 
        if(j != loops - 1){
        msg = input[i].substring(strLen/loops*j, (strLen/loops)*(j+1));
        Serial3.print(msg + '\n+');
        Serial3.flush();}
        else{
          msg = input[i].substring(strLen/loops*j+1);
          Serial3.print(msg + '\n');
          Serial3.flush();
        }
        }
      }
    }
    Serial3.print('$'); // end transmission char
}

void send_serial_string(String input){
  if(input.length() < 1900){
    Serial3.print(input + "\n$");
  }
   else if(input.length() >= 1900){
        int loops = sizeof(input)/Serial3.availableForWrite() + 1; // create a scaling factor for the size of each chunk
        int strLen  = input.length();
        String msg = "";
        int anchor  = 0;
        for(int j = 0; j < loops; j ++){ // 
        if(j != loops - 1){
        msg = input.substring(strLen/loops*j, (strLen/loops)*(j+1));
        Serial3.print(msg + '\n');
        Serial3.flush();}
        else{
          msg = input.substring(strLen/loops*j+1);
          Serial3.print(msg + "\n$");
          Serial3.flush();
        }
        }  }
}

void filter_response(String input){
  Serial.println("Step");
  String outpt[tracknum]; // parse_str_arr(input, ",", resp_tracknum);

      int anchor1 = 0;
      int anchor2 = 0;
      String delim = ",";

      // for our own record (mapping song title to number if the array transfers don't work)
      Serial.println(input);
      for ( int i = 0; i < tracknum; i++) {
        anchor2 = input.indexOf(delim, anchor1);
        if (anchor2 == -1) {
          outpt[i] = input.substring(anchor1);
          tracknum = i + 1;
          Serial.println("Tracknum = " + String(i+1));
          break;
        }
        else {
          outpt[i] = input.substring(anchor1 , anchor2);
          Serial.println("A1: " + String(anchor1) + " A2: " + String(anchor2));

          anchor1 = anchor2 + 2;
        }
      }

  
  Serial.println("Step");
  for(int i = 0; i < tracknum; i++){
    Serial.println("Step " + String(i));
    if(outpt[i].indexOf("\\r") != -1){
      outpt[i] = outpt[i].substring(1, outpt[i].indexOf("\\r")); 
    }
    else{
            outpt[i] = outpt[i].substring(1, outpt[i].indexOf("'", 2)); 
    }
        Serial.println(outpt[i]);
  }
  int counter = 0;
  for(int i = 0; i < tracknum; i++){ // loop through all tracks we need to fix this up a little
  outpt[i].toLowerCase();
  if(outpt[i] == "") continue;
    for(int j = 0; j < tracknum + 10; j++){ // assumes we've lost 10 songs
      String tempStr = unsortedTracks[j];
      if(tempStr == "") continue;
      tempStr.toLowerCase();
          Serial.println("MAS: " + tempStr + " out: " + outpt[i]);
      if(tempStr.indexOf(outpt[i]) != -1){ // if a song is found in the outpt match to songs
        index_str += String(j) + ",";
        sortedTracks[counter]= j;
        counter++;
        }

      }
    }
    for(int i = 0; i < tracknum + 10; i++){ // add the indices of all the unsorted tracks to the end of the list
      bool counted = false;
      for(int j = 0; j < tracknum + 10; j++){
          if(sortedTracks[j] == i){
            counted = true;
          }
      }
      if(!counted && sortedTracks[i]!= -1){
        Serial.println("i: " + String(i) + " Val: " + sortedTracks[i]);
        index_str += String(i) + ",";
      }
    }
      Serial.println(index_str);
}

// shift array once to left, add in new datapoint
void update_array(float *ar, float newData)
{
  int s = ARRAY_SIZE;
  for (int i=0; i<=s-2; i++) {
    ar[i] = ar[i+1];
  }
  ar[s-1] = newData;
}

float find_peak(float *ar, int b1, int b2)
{
  int s = ARRAY_SIZE;   // s is length of vector ar
  float p = 0;
  if(b2 > s){
    b2 = s-1;
  }
  if(b1 == b2 == -1){
  for (int i = 1; i<s-1; i++) {
    if ((ar[i] >= ar[i-1]) && (ar[i] >= ar[i+1])) {
      p = ar[i];
    }
  }} else if(b2 == -2){
      for (int i = b1; i<s-1; i++) {
    if ((ar[i] >= ar[i-1]) && (ar[i] >= ar[i+1])) {
      p = ar[i];
    }
  }
  }
  else{
  for (int i = b1; i<b2; i++) {
    if ((ar[i] >= ar[i-1]) && (ar[i] >= ar[i+1])) {
      p = ar[i];
    }
  }
  }
  return p;
}

// m point moving average filter of array ain
void smooth_data(float *ain, float *aout, int m)
{
  int s = ARRAY_SIZE;
  for (int n = 0; n < s ; n++) {
    int kmin = n>(m-1) ? n - m + 1: 0;
    aout[n] = 0;
  
    for (int k = kmin; k <= n; k++) {
      int d = m > n+1 ? n+1 : m;
      aout[n] += ain[k] / d;
    }
  }
}

//void send_serial_string(String input);
//void smooth_data(float *ain, float *aout, int m);
//void update_array(float *ar, float newData);
//void send_serial_data(String input[]);

///////////////////////////////////////////////////////////////////////////////MIC//////////////////////////////////////////////////////////////////////
// Record audio message
//void record_audio(bool spoof) {
//  if (spoof) {      //If SPOOFing, just delay a bit and then return
//    delay(1000);
//    return;
//  }
//  int sample_num = 0;    // counter for samples
//  int enc_index = 2;//PREFIX_SIZE - 1;  // index counter for encoded samples
//  float time_between_samples = 1000000 / SAMPLE_FREQ;
//  int value = 0;
//  int8_t raw_samples[3];   // 8-bit raw sample data array
//  char enc_samples[4];     // encoded sample data array
//
//  while (sample_num < NUM_SAMPLES) { //read in NUM_SAMPLES worth of audio data
//    time_since_sample = 0;
//    value = analogRead(AUDIO_IN);
//    raw_samples[sample_num % 3] = mulaw_encode(value - 24427);
//    sample_num++;
//    if (sample_num % 3 == 0) {
//      base64_encode(enc_samples, (char *) raw_samples, 3);
//      for (int i = 0; i < 4; i++) {
//        speech_data[enc_index + i] = enc_samples[i];
//      }
//      enc_index += 4;
//    }
//
//    // wait till next time to read
//    while (time_since_sample <= time_between_samples) delayMicroseconds(10);
//  }
//}
//
///* This code was obtained from
//  http://dystopiancode.blogspot.com/2012/02/pcm-law-and-u-law-companding-algorithms.html
//*/
//int8_t mulaw_encode(int16_t number)
//{
//  const uint16_t MULAW_MAX = 0x1FFF;
//  const uint16_t MULAW_BIAS = 33;
//  uint16_t mask = 0x1000;
//  uint8_t sign = 0;
//  uint8_t position = 12;
//  uint8_t lsb = 0;
//  if (number < 0)
//  {
//    number = -number;
//    sign = 0x80;
//  }
//  number += MULAW_BIAS;
//  if (number > MULAW_MAX)
//  {
//    number = MULAW_MAX;
//  }
//  for (; ((number & mask) != mask && position >= 5); mask >>= 1, position--)
//    ;
//  lsb = (number >> (position - 4)) & 0x0f;
//  return (~(sign | ((position - 5) << 4) | lsb));
//}



