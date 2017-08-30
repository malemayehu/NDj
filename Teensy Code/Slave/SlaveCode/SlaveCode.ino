// Simple advanced MP3 file player example
//
// Requires the audio shield:
//   http://www.pjrc.com/store/teensy3_audio.html
//   or dac / pwm / SD shield
//
// Example sketch by Dnstje.nl
// Act like a real MP3/AAC player up to 255 songs.
// Buttons at port 2 3 4, feel free to remap them.
// I don't have a Audioboard yet, just a SDcard breakout and using the onboard DAC and PWM outputs.
//
// This sketch will also using: EEPROM, Bounce library, Serial, SD.
//
// Sending char's to serial port to change tracks.
// P = previous track, N = Next track, S = Stop track, O = Play track I = Pause track, K = list tracklist, C = current track, R = Random track selection.
//
//
//
// MP3/AAC library code by Frank Boesing.


#define AUDIO_BOARD // if using audioboard 
//#define PWMDAC // if using DAC and PWM as output.  (dac ~ pin 14 = left ch, PWM ~ pin 3 ~ 4 + resistor dac right ch)

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
//#include <Bounce.h> //Buttons
//#include <EEPROM.h> // store last track
#include <play_sd_mp3.h> //mp3 decoder
#include <play_sd_aac.h> // AAC decoder
#include <SD.h>

#define SDCARD_CS_PIN    10
#define SDCARD_MOSI_PIN  7
#define SDCARD_SCK_PIN   14
#define BUTTON1 4  //NEXT
#define BUTTON2 3  //Play Pause
#define BUTTON3 2  //PREV 
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI
#define SPI_CLK 14
#define TRANSFER_BAUD 38400 // adjust baud here
//#define SlaveSerial Serial1



//Bounce bouncer1 = Bounce(BUTTON1, 50);
//Bounce bouncer2 = Bounce(BUTTON2, 50);
//Bounce bouncer3 = Bounce(BUTTON3, 50);


const int DELAY_CONST = 500;
const int chipSelect = 10;  // if using another pin for SD card CS.
const int NUMTRACKS = 80;
int track = 0;
int tracknum;
int trackext[NUMTRACKS]; // 0= nothing, 1= mp3, 2= aac, 3= wav.
String tracklist[NUMTRACKS];
//String toSend[NUMTRACKS];
int autoDJ[NUMTRACKS] = { -1};
boolean debugging = false;
File SRoot;
//SdFile MRoot;
char playthis[16];
boolean trackchange;
boolean paused;
String song_names = "";
boolean resetting = false;
//SdFat sd;
//SdFat SD;

AudioPlaySdWav           playWav1;
AudioPlaySdMp3           playMp31;
AudioPlaySdAac           playAac1;
#ifdef PWMDAC
AudioOutputAnalog        dac1;
AudioOutputPWM           pwm1;
#endif

#ifdef AUDIO_BOARD
AudioOutputI2S           i2s1;
#endif

AudioMixer4              mixleft;
AudioMixer4              mixright;
//mp3
AudioConnection          patch1(playMp31, 0, mixleft, 0);
AudioConnection          patch2(playMp31, 1, mixright, 0);
//aac

AudioConnection          patch1a(playAac1, 0, mixleft, 1);
AudioConnection          patch2a(playAac1, 1, mixright, 1);

AudioConnection          patchCord3(playWav1, 0, i2s1, 0);
AudioConnection          patchCord4(playWav1, 1, i2s1, 1);

#ifdef AUDIO_BOARD
AudioConnection          patch5(mixleft, 0, i2s1, 0);
AudioConnection          patch6(mixright, 0, i2s1, 1);
AudioControlSGTL5000     sgtl5000_1;     //xy=240,153
#endif


void setup() {
  Serial.begin(115200); // change this if necessary
  Serial1.begin(TRANSFER_BAUD);

#ifdef AUDIO_BOARD
  sgtl5000_1.enable();
  sgtl5000_1.volume(0.5);

  SPI.setMOSI(7);
  SPI.setSCK(14);
#endif
  //setup pins with pullups
  pinMode(BUTTON1, INPUT_PULLUP);
  pinMode(BUTTON3, INPUT_PULLUP);
  pinMode(BUTTON2, INPUT_PULLUP);
  // reads the last track what was playing.
 // EEPROM.write(0, 0);
 // track = EEPROM.read(0);

  Serial.println("boot");





  // Audio connections require memory to work.  For more
  // detailed information, see the MemoryAndCpuUsage example
  AudioMemory(16);
  //put the gain a bit lower, some MP3 files will clip otherwise.
  //  mixleft.gain(0,0.9);
  //  mixright.gain(0,0.9);

  //Start SD card
  if (!(SD.begin(chipSelect))) {
    // stop here, but print a message repetitively
    while (1) {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }
  //Starting to index the SD card for MP3/AAC.
  SRoot = SD.open("/");
  init_sd();

  if (debugging) { // Print out all variables
    Serial.println("AutoDJ: ");
    for (int i = 0; i < NUMTRACKS; i++) {
      if (autoDJ[i] == -1) break;
      Serial.print(autoDJ[i] + ", ");
    }
    Serial.print("Tracklist: ");
    for (int i = 0; i < NUMTRACKS; i++) {
      Serial.print(tracklist[i] + ", ");
    }
    Serial.println("Song names: ");
    Serial.print(song_names);
    delay(1000);
  }

}



void reset() {
  // hit dat reset button
  //stop whats playing
  if (trackext[track] == 1) playMp31.stop();
  if (trackext[track] == 2) playAac1.stop();
  if (trackext[track] == 3) playWav1.stop();
  resetting  = true;
}

void reinitialize() {
  resetting = false;
  for (int i = 0; i < NUMTRACKS; i ++) {
    track = 0;
    tracklist[i] = "";
    trackext[i] = 0;
    autoDJ[i] = 0;
  }
  // we want to exit to loop so we will trigger a boolean and then start this
  SRoot = SD.open("/");
  if (debugging) { // Print out all variables
    Serial.println("AutoDJ: ");
    for (int i = 0; i < NUMTRACKS; i++) {
      if (autoDJ[i] == -1) break;
      Serial.print(autoDJ[i] + ", ");
    }
    Serial.print("Tracklist: ");
    for (int i = 0; i < NUMTRACKS; i++) {
      Serial.print(tracklist[i] + ", ");
    }
    Serial.println("Song names: ");
    Serial.print(song_names);
    delay(1000);
  }
}


void loop() {
  if (resetting) {
    reinitialize();
    delay(500);
  }
  play();

}
void play() {

  Serial.println();
  Serial.println(tracknum);
  Serial.println("tracknum");
  delay(5);
  Serial.println(track);
  delay(5);

  Serial.println(trackext[autoDJ[track]]);
  delay(5);

  Serial.println(tracklist[autoDJ[track]]);


  if (trackext[track] == 1) {
    Serial.println("MP3" );
    playFileMP3(playthis);
  } else if (trackext[autoDJ[track]] == 2) {
    Serial.println("AAC");
    playFileAAC(playthis);
  } else if (trackext[autoDJ[track]] == 3) {
    Serial.println("Wav");
    delay(500);
    playFileWav(playthis);
  }



  if (trackchange == true) { //when your track is finished, go to the next one. when using buttons, it will not skip twice.
    nexttrack();
  }
  delay(100);
}





void init_sd() {
  String titles = "";
  SRoot = SD.open("/");
  //  MRoot = sd.open("/", O_READ);
  Serial1.begin(TRANSFER_BAUD);
  String data = "";

  printDirectory(SRoot, titles);

  delay(200);


  for ( int h = 0 ; h < tracknum ; h++ )
  {
    for (int l = tracknum; l > 0 ; l--)
    {
      if (tracklist[l] == "" && trackext[l] == 0) {
        break;
      }
      if (tracklist[l - 1] > tracklist[l])
      {
        String temp1 = tracklist[l];
        tracklist[l - 1] = tracklist[l];
        tracklist[l] = temp1;
        int temp2 = trackext[l - 1];
        trackext[l - 1] = trackext[l];
        trackext[l] = temp2;
      }
    }
  }
  Serial.println("List out tracklist");

  for (int i = 0; i < tracknum; i++) {
    Serial.println(tracklist[i]);
  }
  delay(200);

  Serial1.begin(TRANSFER_BAUD);
  while (!Serial1.available());
 // Serial1.read();
  Serial.println(song_names);

  send_serial_string(song_names);

  Serial.println("sent data");

  while (!Serial1.available());
  receive_serial_data(data);
  data = data.substring(1);
  Serial.println(data);

  int anchor1 = 0;
  int anchor2 = 0;
  String delim = ",";


  // for our own record (mapping song title to number if the array transfers don't work)
  for ( int i = 0; i < tracknum; i++) {
    anchor2 = data.indexOf(delim, anchor1);
    if (anchor2 == -1) {
      tracknum = i;
      break;
      //
      //          autoDJ[i] = data.substring(anchor1).toInt();
      //           Serial.println(autoDJ[i]);
    }
    else {
      autoDJ[i] = data.substring(anchor1 , anchor2).toInt();
      Serial.println(autoDJ[i]);
      anchor1 = anchor2 + 1;
    }
  }
  tracklist[autoDJ[track]].toCharArray(playthis, sizeof(tracklist[autoDJ[track]]));
  Serial.println(sizeof(tracklist[autoDJ[track]]));
  Serial.print("wrote Name: "); Serial.println(playthis);
  Serial.println("List out tracklist");

  for (int i = 0; i < tracknum; i++) {
    Serial.println(tracklist[i]);
  }
  Serial.println("List out autoDJ");

  for (int i = 0; i < tracknum; i++) {
    Serial.println(tracklist[autoDJ[i]]);
  }

}

void printDirectory(File dir, String& file_list) {
  while (true) {
    File entry =  dir.openNextFile();
    if (!entry) {
      // no more files
      //Serial.println("**nomorefiles**");
      break;
    }

    if (!entry.isDirectory()) {

      String curfile = entry.name(); //parse file contents to build title strings
      if (curfile == "SDINIT.TXT") {
        Serial.println("found sdinit");
        int counter = 0;
        while (entry.available()) {
          Serial.println("printing init");
          String line = entry.readStringUntil('\n');
          //          toSend[counter] = line;
          song_names += line + "\n";

          counter++;
        }
        song_names += "$";

        Serial.println(song_names);
      }

      file_list += curfile + "~~";
      Serial.println(curfile + "check");
      //look for MP3 or AAC files
      int m = curfile.lastIndexOf(".MP3");// + curfile.lastIndexOf(".mp3");
      int a = curfile.lastIndexOf(".AAC");// + curfile.lastIndexOf(".aac");
      int w = curfile.lastIndexOf(".WAV");// + curfile.lastIndexOf(".wav");

      // if returned results is more then 0 add them to the list.
      if (m > 0 || a > 0 || w > 0) {

        tracklist[tracknum] = curfile;
        if (m > 0) trackext[tracknum] = 1;
        if (a > 0) trackext[tracknum] = 2;
        if (w > 0) trackext[tracknum] = 3;
        Serial.print(tracknum);
        tracknum++;
        //      if(w > 0){ trackext[tracknum] = 3;}
        //Serial.print(m);
      }
      // close
      entry.close();
      //   }
    }
    if (track > tracknum) {
      //if it is too big, reset to 0
//      EEPROM.write(0, 0);
      track = 0;
    }
  }
}



///////////////////////////////SD Audio Funcitons

void playFileWav(const char *filename)
{
  Serial.print("Playing file: ");
  Serial.print(track);
  Serial.print(" - ");
  Serial.println(filename);
  trackchange = true; //auto track change is allowed.
  // Start playing the file.  This sketch continues to
  // run while the file plays.
//  EEPROM.write(0, track); //meanwhile write the track position to eeprom address 0
  // Start playing the file.  This sketch continues to
  // run while the file plays.
  playWav1.play(filename);

  // A brief delay for the library read WAV info
  delay(25);

  // Simply wait for the file to finish playing.
  while (playWav1.isPlaying()) {
//    controls();// doesn't really work for wav files
    serialcontrols();
  }
  // uncomment these lines if you audio shield
  // has the optional volume pot soldered
  //float vol = analogRead(15);
  //vol = vol / 1024;
  // sgtl5000_1.volume(vol);
}


void playFileMP3(const char *filename)
{
  Serial.println("step");
  delay(DELAY_CONST);
  Serial.print("Playing file: ");
  Serial.print(track);
  Serial.print(" - ");
  Serial.println(filename);
  trackchange = true; //auto track change is allowed.
  // Start playing the file.  This sketch continues to
  // run while the file plays.
//  EEPROM.write(0, track); //meanwhile write the track position to eeprom address 0
  //  updateTimer = 0;


  playMp31.play(filename);



  // Simply wait for the file to finish playing.
  while (playMp31.isPlaying()) {
    // update controls!
//    controls();
    serialcontrols();
  }
}

void playFileAAC(const char *filename)
{
  Serial.print("Playing file: ");
  Serial.print(track);
  Serial.print(" - ");
  Serial.println(filename);
  trackchange = true; //auto track change is allowed.
  // Start playing the file.  This sketch continues to
  // run while the file plays.
//  EEPROM.write(0, track); //meanwhile write the track position to eeprom address 0
  //    updateTimer = 0;
  playAac1.play(filename);


  // Simply wait for the file to finish playing.
  while (playAac1.isPlaying()) {
    // update controls!
//    controls();
    serialcontrols();
  }
}
//
//bool controls() {
//  bool outpt = false;
//  bouncer1.update();
//  bouncer2.update();
//  bouncer3.update();
//  if ( bouncer1.fallingEdge()) {
//    outpt = true;
//    nexttrack();
//  }
//  if ( bouncer2.fallingEdge()) {
//    outpt = true;
//    pausetrack();
//  }
//  if ( bouncer3.fallingEdge()) {
//    prevtrack();
//    outpt = true;
//  }
//  return outpt;
//}

void serialcontrols() {
  if (Serial.available()) {
    byte incomingByte = Serial.read();

    switch (incomingByte) {

      case 78:     //Next track  N
        nexttrack();
        break;

      case 80:     //Previous track P
        prevtrack();
        break;

      case 73:     //pause track I
        pausetrack();
        break;

      case 75:     //List tracklist K
        //list our tracklist over serial
        for (int i = 0; i < tracknum; i++) {
          Serial.print(i);
          Serial.print("\t");
          Serial.print(tracklist[autoDJ[i]]);
          Serial.print("\t");
          Serial.println(trackext[i]);
        }
        break;

      case 67: //current track C
        Serial.print("current track = ");
        Serial.print(track);
        Serial.print(" - ");
        Serial.println(tracklist[autoDJ[track]]);
        break;
      case 82: //random track
        randomtrack();
        break;

      case 90: // reset Z
        Serial.println("resetting");
        reset();
        break;


      case 86: // volume change V
        int vol = Serial1.readString().toInt();
        sgtl5000_1.volume(0.1 * vol);
        Serial.println("vol change: " + String (vol));
        delay(2); // This may break the code
        break;
    }
    incomingByte = 0;
  }
}



void nexttrack() {
  Serial.println("Next track!");
  trackchange = false; // we are doing a track change here, so the auto trackchange will not skip another one.
  if (trackext[track] == 1) playMp31.stop();
  if (trackext[track] == 2) playAac1.stop();
  if (trackext[track] == 3) playWav1.stop();
  track++;
  Serial.println("stopped Music");
  if (track >= tracknum) { // keeps in tracklist.
    track = 0;
    delay(50);
  }
  Serial1.begin(TRANSFER_BAUD);
  send_serial_string("@" + String(track));
    Serial.print("@" + String(track));

delay(100);
  tracklist[autoDJ[track]].toCharArray(playthis, sizeof(tracklist[autoDJ[track]])); //since we have to convert String to Char will do this
  Serial.print("wrote Name: "); Serial.println(playthis);
}

void prevtrack() {
  Serial.println("Previous track! ");
  trackchange = false; // we are doing a track change here, so the auto trackchange will not skip another one.
  if (trackext[track] == 1) playMp31.stop();
  if (trackext[track] == 2) playAac1.stop();
  if (trackext[track] == 3) playWav1.stop();

  track--;
  Serial.println("current track num: " + String(track));
  if (track < 0) { // keeps in tracklist.
    track = tracknum - 1;
    delay(50);
  }

  Serial1.begin(TRANSFER_BAUD);
  send_serial_string("@" + String(track));
      Serial.print("@" + String(track));

delay(100);

  tracklist[autoDJ[track]].toCharArray(playthis, sizeof(tracklist[autoDJ[track]])); //since we have to convert String to Char will do this
  Serial.print("wrote Name: "); Serial.println(playthis);

}

void pausetrack() {
  if (paused) {
    Serial.println("Playing!");
  }
  else {
    Serial.println("Paused!");
  }
  paused = playMp31.pause(!paused);
  //  paused = playWav1.pause(!paused);
}


void randomtrack() {
  Serial.println("Random track!");
  trackchange = false; // we are doing a track change here, so the auto trackchange will not skip another one.
  if (trackext[track] == 1) playMp31.stop();
  if (trackext[track] == 2) playAac1.stop();
  if (trackext[track] == 3) playWav1.stop();

  track = random(tracknum);

  Serial1.begin(TRANSFER_BAUD);
  send_serial_string("@" + String(track));
      Serial.print("@" + String(track));

  delay(100);

  tracklist[autoDJ[track]].toCharArray(playthis, sizeof(tracklist[autoDJ[track]])); //since we have to convert String to Char will do this
}

void receive(int numBytes) {}

////////////////////////////////////////////////////////////////////////////////////SERIAL///////////////////////////////////////////////////////
void receive_serial_data(String& data) {
  int counter = 0;
  bool recieving = true;
  String temp;
  while (recieving) {
    temp += Serial1.readStringUntil('\n'); // could be ser3

    if (Serial1.peek() == '$') { // $ to end transmission.
      Serial1.println('$');
      Serial1.flush();
      recieving = false;
      data = temp;
      break;
    }
    else if (Serial1.peek() == '+') {
      Serial1.read();
      tracklist[counter] = temp + Serial1.readStringUntil('\n');
    }
    counter++;
  }
}

void send_serial_data(String input[]) {
  for (int i = 0; i < tracknum; i++) {
    delay(2);
    if (input[i] == "") {
      break;
    }
    else if (sizeof(input[i]) < Serial1.availableForWrite()) {
      Serial1.print(input[i] + '\n');
      Serial.print(input[i] + '\n');
    }
    else
    {
      int loops = sizeof(input[i]) / Serial1.availableForWrite() + 1; // create a scaling factor for the size of each chunk
      int strLen  = input[i].length();
      String msg = "";
      int anchor  = 0;
      for (int j = 0; j < loops; j ++) { //
        if (j != loops - 1) {
          msg = input[i].substring(strLen / loops * j, (strLen / loops) * (j + 1));
          Serial1.print(msg + '\n+');
          Serial1.flush();
          Serial.println("Split: " + msg);
        }
        else {
          msg = input[i].substring(strLen / loops * j + 1);
          Serial1.print("Split: " + msg + '\n');
          Serial1.flush();
          Serial.print(input[i] + '\n');

        }
      }
    }
  }
  Serial1.print('$'); // end transmission char
}

void send_serial_string(String input) {
  if (input.length() < 1900) {
    Serial1.print(input + "\n$");
  }
  else if (input.length() >= 1900) {
    int loops = sizeof(input) / Serial1.availableForWrite() + 1; // create a scaling factor for the size of each chunk
    int strLen  = input.length();
    String msg = "";
    int anchor  = 0;
    for (int j = 0; j < loops; j ++) { //
      if (j != loops - 1) {
        msg = input.substring(strLen / loops * j, (strLen / loops) * (j + 1));
        Serial1.print(msg + "\n+");
        Serial1.flush();
      }
      else {
        msg = input.substring(strLen / loops * j + 1);
        Serial1.print(msg + "\n$");
        Serial1.flush();
      }
    }
  }
}


