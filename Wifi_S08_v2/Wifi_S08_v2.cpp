//Author: Daniel Mendelsohn
//Editor: Mark Chounlakone
//Written for 6.S08 Spring 2016
//Edited for 6.S08 Spring 2017


#include "Wifi_S08_v2.h"
#include <WString.h>
#include <Arduino.h>

ESP8266 * ESP8266::_instance;

// Substrings to look for from AT command responses
const char ESP8266::READY[] = "ready";
const char ESP8266::OK[] = "OK";
const char ESP8266::OK_PROMPT[] = "OK\r\n>";
const char ESP8266::SEND_OK[] = "SEND OK";
const char ESP8266::ERROR[] = "ERROR";
const char ESP8266::FAIL[] = "FAIL";
const char ESP8266::STATUS[] = "STATUS:";
const char ESP8266::ALREADY_CONNECTED[] = "ALREADY CONNECTED";
const char ESP8266::HTML_START[] = "<html>";
const char ESP8266::HTML_END[] = "</html>";
const char ESP8266::SEND_FAIL[] = "SEND FAIL";
const char ESP8266::CLOSED[] = "CLOSED";
const char ESP8266::UNLINK[] = "UNLINK";

// Constructors and init method
ESP8266::ESP8266() {
  init(0, false);
}
ESP8266::ESP8266(bool verboseSerial) {
  init(0, verboseSerial);
}
ESP8266::ESP8266(int mode) {
  init(mode, false);
}
ESP8266::ESP8266(int mode, bool verboseSerial) {
  init(mode, verboseSerial);
}

void ESP8266::init(int mode, bool verboseSerial) {
  _instance = this;
  serialYes = verboseSerial;
  state = IDLE;
  stateAP = AWAITCLIENT;
  ESPmode = mode;


  if (ESPmode == 0){     //Station mode
    hasRequest = false;
    responseReady = false;
    connected = false;
    dataReady = false;
    doAutoConn = true;
    newNetworkInfo = false;
    MAC = "";
    reqReconn = false;

    receiveCount = 0;
    transmitCount = 0;

    ssid[0] = '\0';
    password[0] = '\0';
    response[0] = '\0';

    // Default initialization of request_p, to avoid NULL pointer exception
    request_p = (volatile Request *)malloc(sizeof(Request));
    request_p->domain[0] = '\0';
    request_p->path[0] = '\0';
    request_p->data[0] = '\0';
    request_p->data[DATASIZE] = '\0';
    request_p->data_ref = NULL;
    request_p->data_len = 0;
    request_p->data_offset = 0;
    request_p->port = 0;
    request_p->type = GET_REQ;
    request_p->auto_retry = false;
    request_p->ssl = false;
    request_p->big = false;
  }
  else if(ESPmode == 1){  //Access Point mode
    hasRequest = true;
    responseReady = false;
    newNetworkInfo = false;
    serverStatus = false;
    dataReady = false;

    receiveCount = 0;
    transmitCount = 0;

    ssid[0] = '\0';
    password[0] = '\0';
    response[0] = '\0';

    // Default initialization of request_p, to avoid NULL pointer exception
    requestAP_p = (volatile RequestAP *)malloc(sizeof(RequestAP));
    requestAP_p->path[0] = '\0';
    requestAP_p->data[0] = '\0';
    requestAP_p->data[DATASIZE] = '\0';
    requestAP_p->typeAP = GET_REQ;

    // Default initialization of pages
    storedPages = (volatile Pages *)malloc(sizeof(Pages));
    stringToVolatileArray(DEF_DIR, storedPages->directory, NUMBEROFPAGES*PAGESIZE);
    stringToVolatileArray(DEF_HTML, storedPages->html, NUMBEROFPAGES*HTMLSTORAGE);

    for(int i = 1;i < NUMBEROFPAGES; i++){
      stringToVolatileArray("\0", storedPages->directory+i*PAGESIZE, NUMBEROFPAGES*PAGESIZE);
      stringToVolatileArray("\0", storedPages->html+i*HTMLSTORAGE, NUMBEROFPAGES*HTMLSTORAGE);
    }
  }
}

void ESP8266::begin() {
  delay(500);
  emptyRx();
  if (serialYes) {
    Serial.begin(115200);
    //while (!Serial);  //Loop until Serial is initialized
    Serial.flush();
  }
  wifiSerial.begin(115200);
  while (!wifiSerial); //Loop until wifiSerial is initialized
  if (checkPresent()) {
    Serial.print("6.S08 Wifi Library Loaded: V");
    Serial.println(ESP_VERSION);
    if(ESPmode == 0){
      reset();
      getMACFromDevice();
      wifiSerial.println(AT_CIPSSLSIZE);
      if (!waitForTarget(OK, AT_TIMEOUT)) Serial.println("SSL Buffer Size Failed");
      emptyRx();
    }
    else if(ESPmode == 1){
      startAP();
    }
  }
  enableTimer();
}

bool ESP8266::isConnected() {
  return connected;
}

void ESP8266::connectWifi(String id, String pass) {
  if (id == "") {
    if (serialYes) {
      Serial.println("The empty string is not a valid SSID");
    }
    return;
  }
  bool idOk = stringToVolatileArray(id, ssid, SSIDSIZE);
  bool passOk = stringToVolatileArray(pass, password, PASSWORDSIZE);
  if (serialYes) {
    if (!idOk) {
      Serial.println("Given SSID is too long");
    }
    if (!passOk) {
      Serial.println("Given password is too long");
    }
  }
  if (idOk && passOk) {
    newNetworkInfo = true;
  }
}

bool ESP8266::isBusy() {
  return hasRequest;
}

void ESP8266::sendRequest(int type, String domain, int port, String path, String data) {
  sendRequest(type, domain, port, path, data, false);
}

void ESP8266::sendRequest(int type, String domain, int port, String path, String data, bool auto_retry) {
  RequestType _type;
  if (type == GET) {
    _type = GET_REQ;
  } else if (type == POST) {
    _type = POST_REQ;
  } else {
    Serial.println("Error: Request type must be GET or POST");
    return;
  }
  if (domain.length() > DOMAINSIZE - 1 ||
      path.length() > PATHSIZE - 1 ||
      data.length() > DATASIZE - 1) {
    Serial.println("Domain or path or data is too long");
  } else if (!hasRequest) { // Only send request if one isn't pending already
    disableTimer();
    domain.toCharArray((char *)request_p->domain, DOMAINSIZE);
    path.toCharArray((char *)request_p->path, PATHSIZE);
    data.toCharArray((char *)request_p->data, DATASIZE);
    request_p->port = port;
    request_p->type = _type;
    request_p->auto_retry = auto_retry;
    request_p->ssl = false;
    request_p->data_ref = NULL;
    request_p->data_offset = 0;
    request_p->big = false;
    hasRequest = true;
    responseReady = false;
    enableTimer();
    //benchmark = millis();
  } else if (serialYes) {
    Serial.println("Could not make request; one is already in progress");
  }
}

void ESP8266::sendBigRequest(String domain, int port, String path, const char* data) {
  RequestType _type = POST_REQ;
  if (domain.length() > DOMAINSIZE - 1 ||
      path.length() > PATHSIZE - 1) {
    Serial.println("Domain or path is too long");
  } else if (!hasRequest) { // Only send request if one isn't pending already
    disableTimer();
    domain.toCharArray((char *)request_p->domain, DOMAINSIZE);
    path.toCharArray((char *)request_p->path, PATHSIZE);
    request_p->port = port;
    request_p->type = _type;
    request_p->ssl = port == 443;
    request_p->auto_retry = false;
    request_p->data_ref = (volatile char*) data;
    request_p->data_offset = 0;
    request_p->data_len = strlen(data);
    request_p->big = true;
    hasRequest = true;
    responseReady = false;
    enableTimer();
    //benchmark = millis();
  } else if (serialYes) {
    Serial.println("Could not make request; one is already in progress");
  }
}

void ESP8266::clearRequest() {
  disableTimer();
  if (serialYes && hasRequest) {
    Serial.println("Cleared in-progress request");
  }
  hasRequest = false;
  enableTimer();
}

bool ESP8266::hasResponse() {
  return responseReady;
}

bool ESP8266::hasData() {
  return dataReady;
}

String ESP8266::getData() {
  if(!hasRequest){
    disableTimer();
    if(ESPmode == 1){
      String d = "";
      d = ((char *)requestAP_p->data);
      requestAP_p->data[0] = '\0';
      hasRequest = true;
      dataReady = false;
      enableTimer();
      return d;
    }
    else{
      if(serialYes){
        Serial.println("getData() is not supported in client mode.");
      }
      enableTimer();
      return "\0";
    }
  }
  else{
    return"\0";
  }
}

String ESP8266::getResponse() {
  disableTimer();
  String r = "";
  if (responseReady) {
    //benchmark = millis() - benchmark;
    //Serial.println(benchmark);
    r = ((char *)response);
    response[0] = '\0';
    responseReady = false; // after getting response, hasResponse() is false
  } else if (serialYes) {
    Serial.println("No response ready");
  }
  enableTimer();
  return r;
}

String ESP8266::getMAC() {
  return MAC;
}

String ESP8266::getVersion() {
  return ESP_VERSION;
}

String ESP8266::getStatus() {
  switch (state) {
    case IDLE:
    case CIPSTATUS:
      return "Idle";
      break;
    case CWJAP:
      return "Connecting to wifi";
      break;
    case CIPSTART:
    case CIPSEND:
    case DATAOUT:
      return "Sending to server";
    case AWAITRESPONSE:
      return "Waiting for server response";
  }
  return "Status Unknown";
}

bool ESP8266::restore() {
  disableTimer();
  bool ok = true;
  emptyRx();
  wifiSerial.println(AT_RESTORE);
  ok = ok && waitForTarget(READY, RESTORE_TIMEOUT);
  ok = ok && reset();
  enableTimer();
  return ok;
}

bool ESP8266::startAP(){
  disableTimer();
  bool ok = true;
  emptyRx();
  wifiSerial.println(AT_CWMODE_AP);
  ok = ok && waitForTarget(OK, CWMODE_TIMEOUT);
  if (serialYes) {
    if (ok) {
      Serial.println("CW_MODE: 2");
    } else {
      Serial.println("CW_MODE SET ERROR");
    }
    Serial.flush();
  }
  emptyRx();
  wifiSerial.println(AT_RST);
  ok = ok && waitForTarget(OK, RST_TIMEOUT);
  if (serialYes) {
    if (ok) {
      Serial.println("Reset successful");
    } else {
      Serial.println("WARNING: Reset unsuccesful");
    }
    Serial.flush();
  }
  enableTimer();
  return ok;
}

bool ESP8266::startserver(String netName, String pass){
  disableTimer();
  delay(500);
  if (netName == "") {
    if (serialYes) {
      Serial.println("The empty string is not a valid SSID");
    }
    return false;
  }
  bool idOk = stringToVolatileArray(netName, ssid, SSIDSIZE);
  bool passOk = stringToVolatileArray(pass, password, PASSWORDSIZE);
  if (serialYes) {
    if (!idOk) {
      Serial.println("Given SSID is too long");
    }
    if (!passOk) {
      Serial.println("Given password is too long");
    }
  }
  if (idOk && passOk) {
    newNetworkInfo = true;
  }
  bool ok = true;
  emptyRx();
    wifiSerial.print(AT_CWSAP_SET);
    String s = (char *)ssid;
    String p = (char *)password;
  wifiSerial.println('\"' + s + '\"' + ',' + '\"' + p + '\"' + ',' + "1" + ',' + "4");
  ok = ok && waitForTarget(OK, CWSAP_TIMEOUT);
  emptyRx();
  wifiSerial.println(AT_CIPMUX);
  ok = ok && waitForTarget(OK, CIPMUX_TIMEOUT);
  emptyRx();
  wifiSerial.println(AT_CIPSERVER);
  ok = ok && waitForTarget(OK, CIPSERVER_TIMEOUT);
  emptyRx();
  wifiSerial.println(AT_CIPAP_SET);
  ok = ok && waitForTarget(OK, CIPAP_TIMEOUT);
  if (serialYes) {
    if (ok) {
      Serial.println("Server Started");
      serverStatus = true;
    } else {
      Serial.println("ERROR Starting Server");
    }
    Serial.flush();
  }
  enableTimer();
  return ok;
}

bool ESP8266::setServer(){
  delay(500);
  bool ok = true;
  emptyRx();
    wifiSerial.print(AT_CWSAP_SET);
    String s = (char *)ssid;
    String p = (char *)password;
  wifiSerial.println('\"' + s + '\"' + ',' + '\"' + p + '\"' + ',' + "1" + ',' + "4");
  ok = ok && waitForTarget(OK, CWSAP_TIMEOUT);
  emptyRx();
  wifiSerial.println(AT_CIPMUX);
  ok = ok && waitForTarget(OK, CIPMUX_TIMEOUT);
  emptyRx();
  wifiSerial.println(AT_CIPSERVER);
  ok = ok && waitForTarget(OK, CIPSERVER_TIMEOUT);
  emptyRx();
  wifiSerial.println(AT_CIPAP_SET);
  ok = ok && waitForTarget(OK, CIPAP_TIMEOUT);
  if (serialYes) {
    if (ok) {
      Serial.println("Server Started");
      serverStatus = true;
    } else {
      Serial.println("ERROR Starting Server");
    }
    Serial.flush();
  }
  return ok;
}

void ESP8266::setPage(String directory, String html){
  if(hasRequest){
    disableTimer();
    if (directory.length() > PAGESIZE){
      if(serialYes){
        Serial.println();
        Serial.print("Directory name too long.");
      }
    }
    else if (pageExists(directory)){
      pageStore(storedPages->directory, directory, html);
      if(serialYes){
        Serial.println();
        Serial.println("Page set.");
      }
    }
    else if(pagesAvailable()){
      pageCreate(storedPages->directory,directory);
      pageStore(storedPages->directory, directory, html);
      if(serialYes){
        Serial.println();
        Serial.print("Page created.");
      }
    }
    else{
      if(serialYes){
        Serial.println();
        Serial.println("No more pages can be set");
      }
    }
    enableTimer();
  }
}

//creates the page
void ESP8266::pageCreate(volatile char arr[], String directory){
  for(int i = 0; i < NUMBEROFPAGES; i++){
    String cmp = arr[i*PAGESIZE];
    if(cmp == "\0"){
      for(unsigned int c = 0;c < directory.length()+1;c++){
        arr[i*PAGESIZE + c] = directory[c];
      }
      break;
    }
  }

}

//stores the html with the coresponding directory
void ESP8266::pageStore(volatile char arr[], String dir, String html){
  for(int i = 0; i < NUMBEROFPAGES; i++){
    String cmp = arr[i*PAGESIZE];
    if(cmp == dir){
      for(unsigned int c = 0;c < html.length()+1;c++){
        storedPages->html[i*HTMLSTORAGE+c] = html[c];
      }
    }
  }
}

//checks if a page exists
bool ESP8266::pageExists(String directory){
  for(int i = 0; i < NUMBEROFPAGES; i++){
    String cmp = (char *)(storedPages->directory+i*PAGESIZE);
    if(cmp == directory){
      return true;
    }
  }
  if(serialYes){
    Serial.println();
    Serial.println("Page does not Exist.");
  }
  return false;
}

//checks if more pages can be stored
bool ESP8266::pagesAvailable(){
  for(int i = 0; i < NUMBEROFPAGES; i++){
    String cmp = (char *)(storedPages->directory+i*PAGESIZE);
    if(cmp == "\0"){
      return true;
    }
  }
  if(serialYes){
    Serial.println();
    Serial.println("Page array is full.");
  }
  return false;
}

bool ESP8266::reset() {
  disableTimer();
  bool ok = true;
  emptyRx();
  wifiSerial.println(AT_CWAUTOCONN);
  ok = ok && waitForTarget(OK, CWAUTOCONN_TIMEOUT);
  emptyRx();
  wifiSerial.println(AT_CWMODE);
  ok = ok && waitForTarget(OK, CWMODE_TIMEOUT);
  emptyRx();
  wifiSerial.println(AT_RST);
  ok = ok && waitForTarget(READY, RST_TIMEOUT);
  if (serialYes) {
    if (ok) {
      Serial.println("Reset successful");
    } else {
      Serial.println("WARNING: Reset unsuccesful");
    }
    Serial.flush();
  }
  enableTimer();
  return ok;
}

String ESP8266::sendCustomCommand(String command, unsigned long timeout) {
  disableTimer();
  emptyRx();
  wifiSerial.println(command);
  String customResponse = "";
  unsigned long startTime = millis();
  while (millis() - startTime < timeout) {
    if (wifiSerial.available()) {
      char c = wifiSerial.read();
      customResponse = customResponse + String(c);
    }
  }
  enableTimer();
  return customResponse;
}

bool ESP8266::isAutoConn() {
  return doAutoConn;
}

void ESP8266::setAutoConn(bool value) {
  doAutoConn = value;
}

int ESP8266::getTransmitCount() {
  return transmitCount;
}

void ESP8266::resetTransmitCount() {
  transmitCount = 0;
}

int ESP8266::getReceiveCount() {
  return receiveCount;
}

void ESP8266::resetReceiveCount() {
  receiveCount = 0;
}

//// PRIVATE FUNCTIONS (Non-ISR only)
void ESP8266::enableTimer() {
  if (ESPmode == 0){
    timer.begin(ESP8266::handleInterrupt, INTERRUPT_MICROS);
  }
  else if (ESPmode == 1){
    timer.begin(ESP8266::handleInterruptAP, INTERRUPT_MICROS_AP);
  }
}

void ESP8266::disableTimer() {
  timer.end();
}

// Check if ESP8266 is present, this
bool ESP8266::checkPresent() {
  emptyRx();
  wifiSerial.println(AT_BASIC);
  bool ok = waitForTarget(OK, AT_TIMEOUT);
  if (serialYes) {
    if (ok) {
      Serial.println("ESP8266 present");
    } else {
      Serial.println("ESP8266 not present");
    }
    Serial.flush();
  }
  return ok;
}

// Blocking function (with timeout) to get MAC address of ESP8266
void ESP8266::getMACFromDevice() {
  MAC = "";
  wifiSerial.println(AT_CIPAPMAC);  //Send MAC query
  unsigned long start = millis();
  bool foundMacStart = false;
  while (millis() - start < MAC_TIMEOUT) {
    if (wifiSerial.available() > 0) {
      char c = wifiSerial.read();
      if (serialYes) {
        Serial.print(c);
      }
      if (foundMacStart) {
        MAC = String(MAC+c);
        if (MAC.length() >= MACSIZE) {
          unsigned long timeLeft = MAC_TIMEOUT - millis() + start;
          if (waitForTarget(OK,timeLeft)){ //Wait for rest of message
            return; //MAC now holds the MAC address
          } else {
            break; // timeout before "OK" received
          }
        }
      } else if (c == '"') {
        foundMacStart = true;
      }
    }
  }
  if (serialYes) {
    Serial.println("MAC address request timed out");
  }
}

// Empty wifi serial buffer
void ESP8266::emptyRx() {
  while (wifiSerial.available() > 0) {
    wifiSerial.read();
  }
}

// Wait until target received over wifiSerial, or timeout as elapsed
// Return whether target was received before the timeout
bool ESP8266::waitForTarget(const char *target, unsigned long timeout) {
  String resp = "";
  unsigned long start = millis();
  if (serialYes) {
    Serial.println();
    Serial.print("Waiting for ");
    Serial.println(target);
  }
  while (millis() - start < timeout) {
    if (wifiSerial.available() > 0) {
      char c = wifiSerial.read();
      resp = String(resp+c);
      if (resp.endsWith(target)) {
        if (serialYes) {
          Serial.println(); // New line
        }
        return true;
      }
    }
  }
  return false;
}


bool ESP8266::stringToVolatileArray(String str, volatile char arr[], uint32_t len) {
  if (str.length() >= (len - 1)) { //string is too long
    return false;
  }
  for (uint32_t i = 0; i < str.length(); i++) {
    arr[i] = str[i];
  }
  arr[str.length()] = '\0';
  return true;
}

//// PRIVATE FUNCTIONS (ISR - no String class allowed)
// Static handler calls singleton instance's handler
void ESP8266::handleInterrupt(void) {
    _instance->processInterrupt();
}

void ESP8266::handleInterruptAP(void) {
    _instance->processInterruptAP();
}



// Main interrupt handler, ISR activity follows an FSM pattern
void ESP8266::processInterrupt() {
  switch (state) {
    case IDLE:
      {
      bool autoCheck = doAutoConn
        && (millis() - lastConnectionCheck > CONNCHECK_TIMEOUT || reqReconn);
      if (ssid[0] != '\0' && (newNetworkInfo || autoCheck)) {
        // If we have an SSID, and it's new (or it's time to refresh),
        // then check network connection and reconnect if needed
        emptyRxAndBuffer();
        wifiSerial.println(AT_CIPSTATUS);
        timeoutStart = millis();
        newNetworkInfo = false;
        state = CIPSTATUS;
      } else if (connected && hasRequest) { // Process the request
        emptyRxAndBuffer();
        if (request_p->ssl)
          wifiSerial.print(AT_CIPSTART_SSL);
        else
          wifiSerial.print(AT_CIPSTART);
        wifiSerial.print("\"");
        wifiSerial.print((char *)request_p->domain);
        wifiSerial.print("\",");
        wifiSerial.println(request_p->port);
        timeoutStart = millis();
        responseReady = false;
        state = CIPSTART;
      }
      reqReconn = false;
      }
      break;
    case CIPSTATUS:
      if (isTargetInResp(OK)) {
        int status = getStatusFromResp();
        if (status == -1) {
          if (serialYes) {
            Serial.println("Couldn't determine connection status");
          }
          lastConnectionCheck = millis();
          connected = false;
          reqReconn = true;
          emptyRxAndBuffer();
          state = IDLE;
        } else if (status == 2 || status == 3 || status == 4) {
          lastConnectionCheck = millis();
          connected = true;
          emptyRxAndBuffer();
          state = IDLE; // Connection ok, return to idle
        } else {
          if (serialYes) {
            Serial.println("Not connected, attempting to connect");
          }
          connected = false;
          emptyRxAndBuffer();
          wifiSerial.print(AT_CWJAP);
          wifiSerial.print("\"");
          wifiSerial.print((char *)ssid);
          wifiSerial.print("\",\"");
          wifiSerial.print((char *)password);
          wifiSerial.println("\"");
          timeoutStart = millis();
          state = CWJAP;
        }
      } else if (isTargetInResp(ERROR)) {
        if (serialYes) {
          Serial.println(debugCount);
          debugCount = 0;
          Serial.println("\nCouldn't determine connection status");
        }
        lastConnectionCheck = millis();
        connected = false;
        reqReconn = true;
        emptyRxAndBuffer();
        state = IDLE;
      } else if (millis() - timeoutStart > CIPSTATUS_TIMEOUT) {
        if (serialYes) {
          Serial.println("\nCIPSTATUS timed out");
        }
        lastConnectionCheck = millis();
        connected = false;
        reqReconn = true;
        emptyRxAndBuffer();
        state = IDLE; // Hopefully it'll work next time
      }
      break;
    case CWJAP:
      if (isTargetInResp(OK)) {
        lastConnectionCheck = millis(); //Connection succeeded
        connected = true;
        emptyRxAndBuffer();
        state = IDLE;
      } else if (isTargetInResp(FAIL)) {
        lastConnectionCheck = millis();
        emptyRxAndBuffer();
        state = IDLE;
      } else if (isTargetInResp(ERROR)) { //This shouldn't happen
        if (serialYes) {
          Serial.println("\nMalformed CWJAP instruction");
        }
        lastConnectionCheck = millis();
        emptyRxAndBuffer();
        state = IDLE;
      } else if (millis() - timeoutStart > CWJAP_TIMEOUT) {
        if (serialYes) {
          Serial.println("\nCWJAP instruction timed out");
        }
        lastConnectionCheck = millis();
        emptyRxAndBuffer();
        state = IDLE;
      }
      break;
    case CIPSTART:
      if (isTargetInResp(CLOSED)) {
        Serial.println("Connect failed, retrying...");
        timeoutStart = millis();
        state = IDLE;
      } else if ((isTargetInResp(ERROR) && isTargetInResp(ALREADY_CONNECTED))
          || isTargetInResp(OK)) {
        //Compute the length of the request
        int len = DATASIZE;
        // if (request_p->big) {  // large request
        //     len = DATASIZE;
        // }
        // else {
        //   len = strlen((char *)request_p->domain)
        //     + strlen((char *)request_p->path)
        //     + strlen((char *)request_p->data);
        //   char portString[8];
        //   char dataLenString[8];
        //   sprintf(portString, "%d", request_p->port);
        //   sprintf(dataLenString, "%d", strlen((char *)request_p->data));
        //   len += strlen(portString);
        //   if (request_p->type==GET_REQ) {
        //     len += HTTP_GET_FIXED_LEN;
        //   } else {
        //     len += strlen(dataLenString);
        //     len += HTTP_POST_JSON_FIXED_LEN;
        //   }
        //   if (request_p->data_offset != NULL) {
        //     len = strlen((char *)request_p->domain)
        //       + strlen((char *)request_p->path);
        //     len += strlen(portString);
        //     len += HTTP_JSON_CHUNKED_FIXED_LEN;
        //   }
        // }
        emptyRxAndBuffer();
        wifiSerial.print(AT_CIPSEND);
        wifiSerial.println(len);
        timeoutStart = millis();
        state = CIPSEND;
      } else if (isTargetInResp(ERROR)) {
        if (serialYes) {
          Serial.println("Could not make TCP connection");
        }
        emptyRxAndBuffer();
        hasRequest = request_p->auto_retry;
        state = IDLE;
      } else if (millis() - timeoutStart > CIPSTART_TIMEOUT) {
        if (serialYes) {
          Serial.println("TCP connection attempt timed out");
        }
        emptyRxAndBuffer();
        hasRequest = request_p->auto_retry;
        state = IDLE;
      }
      break;
    case CIPSEND:
      if (isTargetInResp(OK_PROMPT)) {
        emptyRxAndBuffer();
        if (request_p->big) { // only posts
          if (request_p->data_offset == 0) {
            wifiSerial.print(HTTP_POST);
            wifiSerial.print((char *)request_p->path);
            wifiSerial.print(HTTP_0);
            wifiSerial.print((char *)request_p->domain);
            wifiSerial.print(":");
            wifiSerial.print(request_p->port);
            wifiSerial.print(HTTP_JSON);
            wifiSerial.print(HTTP_CHUNKED);
            wifiSerial.print(HTTP_END);
            wifiSerial.print("\\0");
            state = CIPSTART;
            request_p->data_offset = -1;
          } else {
            if (request_p->data_offset == -1) request_p->data_offset = 0;
            char chunkString[8];
            int remaining = request_p->data_len - request_p->data_offset;
            if (remaining >= DATASIZE-7) {
              sprintf((char *)request_p->data, "%X\r\n", DATASIZE-7);
              memcpy((void *)request_p->data+5, (void *)request_p->data_ref+request_p->data_offset, DATASIZE-7);
              request_p->data[2046]='\r';
              request_p->data[2047]='\n';
              request_p->data_offset += DATASIZE-7;
              state = CIPSTART;
              Serial.println((char *)request_p->data);
              wifiSerial.print((char *) request_p->data);
              wifiSerial.print("\\0");
            } else if (remaining > 0) {
              sprintf(chunkString, "%X", remaining);
              int chunk_len = strlen(chunkString);
              sprintf((char *)request_p->data, "%X\r\n", remaining);
              memcpy((void *)request_p->data+chunk_len+2, (void *)request_p->data_ref+request_p->data_offset, remaining);
              request_p->data[remaining+chunk_len+2] = '\r';
              request_p->data[remaining+chunk_len+3] = '\n';
              request_p->data[remaining+chunk_len+4] = '\0';
              request_p->data_offset += remaining;
              Serial.println((char *)request_p->data);
              wifiSerial.print((char *) request_p->data);
              wifiSerial.print("\\0");
              state = CIPSTART;
            } else {
              wifiSerial.print("0\r\n\r\n");
              wifiSerial.print("\\0");
              state = DATAOUT;
            }
          }
          timeoutStart = millis();
        } else {
          if (request_p->type == GET_REQ) {
            wifiSerial.print(HTTP_GET);
            wifiSerial.print((char *)request_p->path);
            wifiSerial.print("?");
            wifiSerial.print((char *)request_p->data); //URL params
            wifiSerial.print(HTTP_0);
            wifiSerial.print((char *)request_p->domain);
            wifiSerial.print(":");
            wifiSerial.print(request_p->port);
            wifiSerial.print(HTTP_END);
            wifiSerial.print("\\0");
            if (serialYes) {
              Serial.print(HTTP_GET);
              Serial.print((char *)request_p->path);
              Serial.print("?");
              Serial.print((char *)request_p->data); //URL params
              Serial.print(HTTP_0);
              Serial.print((char *)request_p->domain);
              Serial.print(":");
              Serial.print(request_p->port);
              Serial.println(HTTP_END);
            }
          } else {
            wifiSerial.print(HTTP_POST);
            wifiSerial.print((char *)request_p->path);
            wifiSerial.print(HTTP_0);
            wifiSerial.print((char *)request_p->domain);
            wifiSerial.print(":");
            wifiSerial.print(request_p->port);
            wifiSerial.print(HTTP_1);
            wifiSerial.print(strlen((char *)request_p->data));
            wifiSerial.print(HTTP_2);
            wifiSerial.print(HTTP_END);
            wifiSerial.print((char *)request_p->data);
            wifiSerial.print("\\0");
            if (serialYes) {
              Serial.print(HTTP_POST);
              Serial.print((char *)request_p->path);
              Serial.print(HTTP_0);
              Serial.print((char *)request_p->domain);
              Serial.print(":");
              Serial.print(request_p->port);
              Serial.print(HTTP_1);
              Serial.print(strlen((char *)request_p->data));
              Serial.print(HTTP_2);
              Serial.print(HTTP_END);
              Serial.println((char *)request_p->data);
            }
          }
          state = DATAOUT;
        }
      } else if (isTargetInResp(ERROR)) {
        if (serialYes) {
          Serial.println("CIPSEND command failed");
        }
        wifiSerial.println(AT_CIPCLOSE);
        hasRequest = request_p->auto_retry;
        state = IDLE;
      } else if (millis() - timeoutStart > CIPSEND_TIMEOUT) {
        if (serialYes) {
          Serial.println("CIPSEND command timed out");
        }
        wifiSerial.println(AT_CIPCLOSE);
        hasRequest = request_p->auto_retry;
        state = IDLE;
      }
      break;
    case DATAOUT:
      if (isTargetInResp(SEND_OK)) {
        emptyRxAndBuffer();
        timeoutStart = millis();
        transmitCount++; // ESP8266 has successfully sent request out into the world
        state = AWAITRESPONSE;
        benchmark = millis();
      } else if (isTargetInResp(ERROR)) {
        emptyRxAndBuffer();
        if (serialYes) {
          Serial.println("Problem sending HTTP data");
        }
        wifiSerial.println(AT_CIPCLOSE);
        hasRequest = request_p->auto_retry;
        state = IDLE;
      } else if (millis() - timeoutStart > DATAOUT_TIMEOUT) {
        emptyRxAndBuffer();
        if (serialYes) {
          Serial.println("Timeout while confirming HTTP send");
        }
        wifiSerial.println(AT_CIPCLOSE);
        hasRequest = request_p->auto_retry;
        state = IDLE;
      } else if (isTargetInResp(SEND_FAIL)){
        emptyRxAndBuffer();
        if (serialYes) {
          Serial.println("Failed to send HTTP");
        }
        wifiSerial.println(AT_CIPCLOSE);
        hasRequest = request_p->auto_retry;
        state = IDLE;
      }
      break;
    case AWAITRESPONSE:
      if (isTargetInResp(HTML_END)) {
        benchmark = millis() - benchmark;
        getStringFromResp(HTML_START, HTML_END, (char *)response);
        if (serialYes) {
          Serial.println("Got HTTP response!");
          Serial.print("Response speed: ");
          Serial.println(benchmark);
        }
        wifiSerial.println(AT_CIPCLOSE);
        hasRequest = false; //We're done with this request
        responseReady = true;
        receiveCount++; // ESP8266 has successfully received a response from the web
        debugCount++;
        emptyRxAndBuffer();
        state = IDLE;
      } else if (isTargetInResp("\",")) {
        getStringFromResp("transcript", "\",", (char *)response);
        wifiSerial.println(AT_CIPCLOSE);
        hasRequest = false; //We're done with this request
        responseReady = true;
        receiveCount++; // ESP8266 has successfully received a response from the web
        debugCount++;
        emptyRxAndBuffer();
      }
      else if (millis() - timeoutStart > HTTP_TIMEOUT /*&& !isTargetInResp(IPD)*/) {
        if (serialYes) {
          Serial.println(debugCount);
          debugCount = 0;
          Serial.println("HTTP timeout");
        }
        wifiSerial.println(AT_CIPCLOSE);
        hasRequest = request_p->auto_retry;
        emptyRxAndBuffer();
        state = IDLE;
      } else if (isTargetInResp(CLOSED)){
        hasRequest = request_p->auto_retry;
        emptyRxAndBuffer();
        state = IDLE;
      }
      break;
  }
}

//Main interrupt handler for Access Point Mode
void ESP8266::processInterruptAP(){
  switch(stateAP) {
    case RESET:
      {
      if(isTargetInResp(ERROR)){
        if (serialYes){
          Serial.println("Server Disconnected");
          Serial.println("Attempting to restart the server");
        }
        setServer();
        if(serverStatus){
        stateAP = AWAITCLIENT;
        }
      }
      else{
        stateAP = AWAITCLIENT;
        timeoutStart = millis();
        serverStatus = true;
      }
      break;
      }
    case AWAITCLIENT:
      {
      // get link id
      if(getStringFromResp("IPD",":",(char *)response)){
        hasRequest = true;
        String resp = (char *)response;

            linkID = int(resp[4]) - 48;

            if (serialYes){

              Serial.println();
              Serial.print("linkID: ");
              Serial.println(linkID);
              Serial.print("resp: ");
              Serial.println(resp);

          }
          timeoutStart = millis();
          first = true;
            stateAP = AWAITREQUEST;
            responseReady = true;
        }
      else if(getStringFromResp("PD",":",(char *)response)){
            hasRequest = true;
            String resp = (char *)response;

            linkID = int(resp[3]) - 48;

            if (serialYes){

              Serial.println();
              Serial.print("linkID: ");
              Serial.println(linkID);
              Serial.print("resp: ");
              Serial.println(resp);

          }
          timeoutStart = millis();
          first = true;
            stateAP = AWAITREQUEST;
            responseReady = true;
        }
        /*else if(millis() - timeoutStart > CHECK_TIMEOUT || serverStatus == false){
          emptyRxAndBuffer();
          wifiSerial.println(AT_CWSAP_GET);
          stateAP = RESET;
        }*/
      else {
          if (serialYes and first){
            first = false;
            Serial.println();
            Serial.println("ESP8266 AWAITCLIENT");
          }
        }
      break;
      }
      case AWAITREQUEST:
       {
      if (getStringFromResp("POST", "Host",(char *)response)){
        String resp = (char *)response;
        requestAP_p->typeAP = POST_REQ;
        requestParse(resp);
        timeoutStart = millis();
        first = true;
        stateAP = SENDRESPONSE;
      }
      else if (getStringFromResp("GET", "Host",(char *)response)){
        String resp = (char *)response;
        requestAP_p->typeAP = GET_REQ;
        requestParse(resp);
        timeoutStart = millis();
        first = true;
        stateAP = SENDRESPONSE;
      }
      else if ((millis()-timeoutStart) > AWAITREQUEST_TIMEOUT){
        if (serialYes){
          Serial.println();
          Serial.println("Received an incomplete request");
        }
        stateAP = AWAITCLIENT;
      }
    break;
    }
      case SENDRESPONSE:
      {
        if(first){
          wifiSerial.print(AT_CIPSEND);
        wifiSerial.print(linkID);
          findPage();
          first = false;
        }
        if(isTargetInResp(OK_PROMPT)){
          emptyRxAndBuffer();
          servePage();
          if (serialYes){
            Serial.println();
            Serial.println("Got prompt");
          }
          timeoutStart = millis();
          first = true;
          stateAP = DATAOUTAP;
        }
        else if(isTargetInResp(ERROR)){
          emptyRxAndBuffer();
          stateAP = CLOSE;
        }
        else if (millis() - timeoutStart > SENDRESPONSE_TIMEOUT){
          timeoutStart = millis();
          stateAP = CLOSE;
          if (serialYes){
            Serial.println();
            Serial.println("CIPSEND TIMEOUT");
          }
        }
      break;
      }
      case DATAOUTAP:
      {
      if(isTargetInResp(SEND_OK)){
          emptyRxAndBuffer();
        timeoutStart = millis();
          stateAP = CLOSE;
          if (serialYes){
            Serial.println();
            Serial.println("CIPSEND COMPLETE");
          }
        }
        else if (millis() - timeoutStart > CIPSEND_TIMEOUT){
          emptyRxAndBuffer();
          first = true;
          stateAP = AWAITCLIENT;
          timeoutStart = millis();
          if (serialYes){
            Serial.println();
            Serial.println("CIPSEND ERROR");
          }
        }
      break;
      }
      case CLOSE:
      {
        if(isTargetInResp(CLOSED)){
          emptyRxAndBuffer();
          timeoutStart = millis();
          stateAP = AWAITCLIENT;
        }
        if(isTargetInResp(OK)){
          emptyRxAndBuffer();
          timeoutStart = millis();
          stateAP = AWAITCLIENT;
        }
        if(isTargetInResp(UNLINK)){
          emptyRxAndBuffer();
          timeoutStart = millis();
          stateAP = AWAITCLIENT;
        }
        else if (millis() - timeoutStart > CLOSE_TIMEOUT){
          wifiSerial.print(AT_CIPCLOSE_AP);
          wifiSerial.println(linkID);
          timeoutStart = millis();
        }
        hasRequest = false;
      }

  }
}

//finds the page to serve and sends the length of the page as a CIPSEND parameter
void ESP8266::findPage(){
  //check if the requested path has an assigned page
  String s= (char *)requestAP_p->path;
  if(pageExists(s) == false){
    //if not set the page to the default
    s = "default";
    stringToVolatileArray(s, requestAP_p->path, PATHSIZE);
  }

  String h = getPage(s);

  //count the length of the html
  char c = ' ';
  int len = 0;
  while (c!='\0'){
    c = h[len];
    len++;
  }
  len--;
  wifiSerial.print(",");
  wifiSerial.println(len);
  return;
}

//gets the html for the given directory
String ESP8266::getPage(String dir){
  for(int i = 0; i < NUMBEROFPAGES; i++){
    String cmp = (char *)(storedPages->directory+i*PAGESIZE);
    if(cmp == dir){
      return (char *)(storedPages->html+i*HTMLSTORAGE);
    }
  }
  if(serialYes){
    Serial.println("There was an error getting the page requested.");
  }
  return (char *)storedPages->html;
}

//serves the page requested
void ESP8266::servePage(){
  String s = (char *)requestAP_p->path;
  String h = getPage(s);
  //String html = "<html>\n<title>It works!</title>\n<body>\n<h1>Congrats</h1>\n<p>You have successfully interneted.</p>\n</body>\n</html>";
  wifiSerial.println(h);
}

// Parses the response and stores the path and data into requestAP_p struct
void ESP8266::requestParse(String resp){
  String pathtmp;
  String datatmp;
    char res[512];
    resp.toCharArray(res, 512);
    char* ptr = res;
    while(*ptr != ' '){
        ptr++;
      }
      ptr++;
    while(*ptr != '?' && *ptr != ' '){
      pathtmp += *ptr;
      ptr++;
      }
    while(*ptr != ' '){
      datatmp += *ptr;
      ptr++;
    }

    stringToVolatileArray(pathtmp, requestAP_p->path, PATHSIZE);

    if (datatmp != "\0"){
      stringToVolatileArray(datatmp, requestAP_p->data, DATASIZE);
    }
    if(serialYes){
      Serial.println();
      Serial.print("Path: ");
      Serial.println((char *)requestAP_p->path);
      Serial.print("Data: ");
      if (datatmp == "\0"){
        Serial.println("No Data");
      }
      else{
        Serial.println((char *)requestAP_p->data);
      }
    }
    dataReady = true;
}

// Returns true if and only if target is in inputBuffer
bool ESP8266::isTargetInResp(const char *target) {
  loadRx();
  return (strstr((char *)inputBuffer, target) != NULL);
}

// Looks for target in inputBuffer.  If target is found, loads all
// preceding characters (including the target itself) into result array and
// returns true, otherwise returns false.
bool ESP8266::getStringFromResp(const char *target, char *result) {
  loadRx();
  char *loc = strstr((char *)inputBuffer, target);
  if (loc != NULL) {
    int numChars = loc - inputBuffer + strlen(target);
    strncpy(result, (char *)inputBuffer, numChars);
    result[numChars] = '\0'; //Make sure we null terminate the result
    return true;
  } else {
    return false;
  }
}

// Looks for start and end targets in inputBuffer.  If both are found, and
// the start target is before the end target, this method loads the characters
// in between (including both targets) into result array and returns true.
// Otherwise, returns false.
bool ESP8266::getStringFromResp(const char*startTarget, const char*endTarget, char *result) {
  loadRx();
  char *startLoc = strstr((char *)inputBuffer, startTarget);
  char *endLoc = strstr((char *)inputBuffer, endTarget);
  if (startLoc != NULL && endLoc != NULL && startLoc < endLoc) {
    int numChars = endLoc - startLoc + strlen(endTarget);
    strncpy(result, startLoc, numChars);
    result[numChars] = '\0';  //Make sure we null terminate the result
    return true;
  } else {
    return false;
  }
}

// Looks for target in inputBuffer.  If target is found, we clear inputBuffer
// through and including that target and return true.  Otherwise, return false
/*bool ESP8266::clearStringFromResp(const char *target) {
  loadRx();
  char *loc = strstr((char *)inputBuffer, target);
  if (loc != NULL) {
    int numChars = loc - inputBuffer + strlen(target); //num chars to cut
    int newLen = strlen((char *)inputBuffer) - numChars;
    for (int i = 0; i < newLen; i++) {
      inputBuffer[i] = loc[i];
    }
    inputBuffer[newLen] = '\0';
    return true;
  } else {
    return false;
  }
}*/

// Looks for a valid response to CIPSTATUS and returns the integer status
// If an integer status can't be parsed from result, returns -1
int ESP8266::getStatusFromResp() {
  loadRx();
  char result[BUFFERSIZE] = {0};
  bool success = getStringFromResp(OK, result);
  if (success) {
    char *loc = strstr((char *)inputBuffer, STATUS); //Find "STATUS:"
    if (loc != NULL) {
      loc += strlen(STATUS);
      char c = loc[0]; //If next character is digit, return that number
      if (c >= '0' && c <= '9') {
        return c - '0';
      }
    }
  }
  return -1; //Could not find valid status int in inputBuffer
}

// Load wifi serial buffer into character array (inputBuffer)
void ESP8266::loadRx() {
  int buffIndex = strlen((char *)inputBuffer);
  while (wifiSerial.available() > 0 && buffIndex < BUFFERSIZE-1) {
      char c = wifiSerial.read();
      if (serialYes) {
        Serial.print(c);
      }
      inputBuffer[buffIndex] = c;
      inputBuffer[buffIndex+1] = '\0';
      buffIndex++;
  }
  if (buffIndex >= BUFFERSIZE -1) {
    if (serialYes) {
      Serial.println("WARNING: inputBuffer is full");
    }
  }
}

void ESP8266::emptyRxAndBuffer() {
  emptyRx();
  inputBuffer[0] = '\0';
}
