#include <Wire.h>
#include <U8g2lib.h>
#include <Wifi_S08_v2.h>
#include <SPI.h>
#include <math.h> 

#define SCREEN U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI
#define SPI_CLK 14
#define ARRAY_SIZE 200
#define MasterSerial Serial3

const int NUMTRACKS = 255;
String masterTracks[NUMTRACKS];




// Global constants
const int LED_PIN = 13;               // Teensy LED Pin
const int BUTTON_PIN = 9;             // pushbutton
const int SAMPLE_INTERVAL = 25;       // time in-between IMU readings
const int SCREEN_HEIGHT = 64;         // OLED dimensions
const int SCREEN_WIDTH = 128;
const int FONT_HEIGHT = 11;           // Font dimensions
const int FONT_WIDTH = 6;
const int LINE_SPACING = 0;
const int NUM_GESTURES = 4;
const String gesture_names[] = {"skip", "rewind"};
const int Y_INC = FONT_HEIGHT + LINE_SPACING;
const String KERBEROS = "rodbay";  // your kerberos
const String GPATH = "/6S08dev/" + KERBEROS + "/final/sb3.py";
const String IPATH = "/";// ADD PATH NAME HERE #################################
const String IESC = "iesc-s2.mit.edu";


// Global variables
elapsedMillis time_since_sample = 0;  // time since last screen update
float data[ARRAY_SIZE];
int data_index = 0;
int state = -1;
String MAC = "";
String out = "";
int gesture_num = 0;


ESP8266 wifi = ESP8266(0, true);
SCREEN oled(U8G2_R2, 10, 15, 16);


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
        } else if (t_since_change >= debounce_time) { // A full button push is complete
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

Button button(BUTTON_PIN);


void setup() {
  Serial.begin(115200);
  Serial3.begin(115200);

  Serial.println("boot");
  Wire.begin();
  SPI.setSCK(14);
  pinMode(LED_PIN, OUTPUT);            // Set up output LED
  pinMode(BUTTON_PIN, INPUT_PULLUP);   // Button pullup
  
  oled.begin();               // Screen startup
  oled.setFont(u8g2_font_profont11_mf);

//  imu.initMPU9250();          // IMU startup
//  imu.MPU9250SelfTest(imu.selfTest);
//  imu.calibrateMPU9250(imu.gyroBias, imu.accelBias);
//  imu.getAres();

  oled.clearBuffer();         // Wifi startup
  oled.setCursor(0, Y_INC);
  oled.print("connecting to wifi...");
  oled.sendBuffer();
  wifi.begin();
  wifi.connectWifi("6s08", "iesc6s08");
  MAC = wifi.getMAC();
  while(!wifi.isConnected());


  pinMode(13, OUTPUT);

}




void loop()
{
  int flag = button.update();
  if (state == -1) {   // init state
  oled.clearBuffer();
  oled.setCursor(0,0);
  oled.drawStr(0,15,"init");
  oled.sendBuffer();

    if (Serial3.available() > 0) {
      oled.clearBuffer();
      oled.print("Received from Slave");
      oled.sendBuffer();

      Serial.println("Received from Slave");
      String dataStr = "";
      receive_serial_data(dataStr);
      delay(250);
      wifi.sendRequest(POST, IESC, 80, IPATH, dataStr);
      //  ET.close(); this is not a thing
      delay(100);
      while (!wifi.hasResponse());
      }
    if (wifi.hasResponse()) {
          Serial.println("step");

      String temp = wifi.getResponse();
      temp = temp.substring(6, temp.indexOf("</html>"));
      int anchor1 = 0;
      int anchor2;
      // for our own record (mapping song title to number if the array transfers don't work)
      for ( int i = 0; i < NUMTRACKS; i++) {
        anchor2 = temp.indexOf(",", anchor1);
        if (anchor2 == -1) {
          masterTracks[i] = temp.substring(anchor1);
        }
        else {
          masterTracks[i] = temp.substring(anchor1, anchor2);
          anchor1 = anchor2;
        }
      }
      
      oled.clearBuffer();
      oled.print("Sending to Slave");
      oled.sendBuffer();
      delay(500);
      send_serial_data(masterTracks);
      state = 0;
    }

  } else if (state == 0) {   // Base state
    oled.clearBuffer();
    oled.setCursor(0, Y_INC);
    oled.print("Short press to PLAY");
    oled.setCursor(0, 2 * Y_INC);
    oled.print("Long press to LEARN");
    oled.sendBuffer();
    if (flag == 1) {
      state = 7;
    } else if (flag == 2) {
      state = 1;
    }
  } else if (state == 1) {  // LEARN: Pick gesture to learn
    if (flag == 1) {
      gesture_num = (gesture_num + 1) % NUM_GESTURES;
    } else if (flag == 2) {
      state = 2;
    }
    oled.clearBuffer();
    oled.setCursor(0, Y_INC);
    oled.print("LEARN: " + gesture_names[gesture_num]);
    oled.setCursor(0, 2 * Y_INC);
    oled.print("short press to change");
    oled.setCursor(0, 3 * Y_INC);
    oled.print("long press to accept");
    oled.sendBuffer();
  } else if (state == 2) {  // LEARN: Wait to record
    if (flag == 1) {
      state = 3;
      data_index = 0;
    }
    else if (flag == 2) {
      state = 0;
    }
    oled.clearBuffer();
    oled.setCursor(0, Y_INC);
    oled.print("LEARN: " + gesture_names[gesture_num]);
    oled.setCursor(0, 2 * Y_INC);
    oled.print("short press to record");
    oled.setCursor(0, 3 * Y_INC);
    oled.print("long press to restart");
    oled.sendBuffer();
  } else if (state == 3) { // LEARN: Recording
    if (flag == 1 || flag == 2) {
      state = 4;
    }
    else {
//      imu.readAccelData(imu.accelCount);
//      time_since_sample = 0;
//      imu.ax = imu.accelCount[0] * imu.aRes;
//      data[data_index] = imu.ax;
      data_index++;
      if (data_index >= ARRAY_SIZE) {
        state = 4;
      }
      while (time_since_sample <= SAMPLE_INTERVAL);
    }
    digitalWrite(LED_PIN, HIGH);
    oled.clearBuffer();
    oled.setCursor(0, Y_INC);
    oled.print("LEARN");
    oled.setCursor(0, 2 * Y_INC);
    oled.print("recording gesture...");
    oled.setCursor(0, 3 * Y_INC);
    oled.print("press to stop");
    oled.sendBuffer();
  } else if (state == 4) {  // LEARN: Reject or accept gesture
    if (flag == 1) {
      state = 5;
    } else if (flag == 2) {
      state = 6;
    }
    digitalWrite(LED_PIN, LOW);
    oled.clearBuffer();
    oled.setCursor(0, Y_INC);
    oled.print("LEARN");
    oled.setCursor(0, 2 * Y_INC);
    oled.print("short press to reject");
    oled.setCursor(0, 3 * Y_INC);
    oled.print("long press to accept");
    oled.sendBuffer();
  } else if (state == 5) {  // LEARN: Reject
    state = 2;
    oled.clearBuffer();
    oled.setCursor(0, Y_INC);
    oled.print("LEARN");
    oled.setCursor(0, 2 * Y_INC);
    oled.print("reject");
    oled.sendBuffer();
    delay(1000);
  } else if (state == 6) {  // LEARN: Accept & upload
    state = 1;
    oled.clearBuffer();
    oled.setCursor(0, Y_INC);
    oled.print("LEARN");
    oled.setCursor(0, 2 * Y_INC);
    oled.print("accept...sending...");
    oled.sendBuffer();
    out = send_to_server("post", data);
    oled.clearBuffer();
    oled.setCursor(0, 10);
    oled.print("LEARN");
    pretty_print(0, 2 * Y_INC, out, 6, 8, 2, oled);
    oled.sendBuffer();
    delay(2000);
  } else if (state == 7) {  // Play
    if (flag == 1) {
      state = 8;
      data_index = 0;
    }
    else if (flag == 2) {
      state = 0;
    }
    oled.clearBuffer();
    oled.setCursor(0, Y_INC);
    oled.print("PLAY");
    oled.setCursor(0, 2 * Y_INC);
    oled.print("short press to record");
    oled.setCursor(0, 3 * Y_INC);
    oled.print("long press to go back");
    oled.sendBuffer();
  } else if (state == 8) {  // PLAY: Read
    if (flag == 1 || flag == 2) {
      state = 9;
    } else {
//      imu.readAccelData(imu.accelCount);
//      time_since_sample = 0;
//      imu.ax = imu.accelCount[0] * imu.aRes;
//      data[data_index] = imu.ax;
      data_index++;
      if (data_index >= ARRAY_SIZE) {
        state = 9;
      }
      while (time_since_sample <= SAMPLE_INTERVAL);
    }
    digitalWrite(LED_PIN, HIGH);
    oled.clearBuffer();
    oled.setCursor(0, Y_INC);
    oled.print("PLAY");
    oled.setCursor(0, 2 * Y_INC);
    oled.print("recording gesture...");
    oled.setCursor(0, 3 * Y_INC);
    oled.print("press to stop");
    oled.sendBuffer();
  } else if (state == 9) {  // PLAY: send gesture to server
    digitalWrite(LED_PIN, LOW);
    oled.clearBuffer();
    oled.setCursor(0, Y_INC);
    oled.print("PLAY");
    oled.setCursor(0, 2 * Y_INC);
    oled.print("comparing...");
    delay(1000);
    oled.sendBuffer();
    out = send_to_server("get", data);
    state = 10;
  } else if (state == 10) { // PLAY: display match results
    if (flag == 1 || flag == 2) {
      state = 7;
    }
    oled.clearBuffer();
    oled.setCursor(0, Y_INC);
    oled.print("PLAY");
    pretty_print(0, 2 * Y_INC, out, 6, 8, 2, oled);
    oled.sendBuffer();
  }
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

void receive_serial_data(String& data){
  int counter = 0;
  bool recieving = true;
  while(recieving){
    Serial.println("receiving");
  String temp = Serial3.readStringUntil('\n'); // could be ser3
  masterTracks[counter] = temp;
  if(Serial3.peek() == '$'){ // $ to end transmission.
    Serial3.println('$');
    Serial3.flush();
    recieving = false;
    break;
  }
  else if(Serial3.peek() == '+'){
    Serial3.read();
    masterTracks[counter] = temp + Serial3.readStringUntil('\n');
  }
  counter++;
}
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

