// Author: Daniel Mendelsohn
// Written for 6.S08, Spring 2016
//
// This library provides non-blocking web connectivity via an ESP8266 chip.
//
// In order for the library to function properly, you will need to edit the
// file 'serial1.c' and change the value of the macro RX_BUFFER_SIZE from 64
// to something larger, like 1024

#ifndef Wifi_S08_v2_H
#define Wifi_S08_v2_H
#endif

#define ESP_VERSION "2.1"
#define wifiSerial Serial1

#define GET 0
#define POST 1

// Sizes of character arrays
#define BUFFERSIZE 4096
#define RESPONSESIZE 4096
#define MACSIZE 17
#define SSIDSIZE 32
#define PASSWORDSIZE 64
#define DOMAINSIZE 256
#define PATHSIZE 256
#define DATASIZE 2048
#define NUMBEROFPAGES 8
#define PAGESIZE 64
#define HTMLSTORAGE 1024

// Timing constants
#define INTERRUPT_MICROS 1000
#define INTERRUPT_MICROS_AP 500
#define AT_TIMEOUT 1000
#define MAC_TIMEOUT 1000
#define CWMODE_TIMEOUT 1000
#define CWAUTOCONN_TIMEOUT 1000
#define RST_TIMEOUT 7000
#define RESTORE_TIMEOUT 7000
#define CONNCHECK_TIMEOUT 10000
#define CIPSTATUS_TIMEOUT 5000
#define CWJAP_TIMEOUT 15000
#define CIPSTART_TIMEOUT 15000
#define CIPSEND_TIMEOUT 2000
#define DATAOUT_TIMEOUT 5000
#define HTTP_TIMEOUT 10000
#define SENDRESPONSE_TIMEOUT 300
#define CLOSE_TIMEOUT 100
#define AWAITREQUEST_TIMEOUT 1000
#define CWSAP_TIMEOUT 5000
#define CIPMUX_TIMEOUT 5000
#define CIPSERVER_TIMEOUT 5000
#define CIPAP_TIMEOUT 5000
#define CHECK_TIMEOUT 1000

// AT Commands, some of which require appended arguments
#define AT_BASIC "AT"
#define AT_CWMODE "AT+CWMODE_DEF=1"
#define AT_CWAUTOCONN "AT+CWAUTOCONN=0"
#define AT_RST "AT+RST"
#define AT_RESTORE "AT+RESTORE"
#define AT_CIPAPMAC "AT+CIPAPMAC?"
#define AT_CIPSTATUS "AT+CIPSTATUS"
#define AT_CWJAP "AT+CWJAP_DEF="
#define AT_CIPSTART "AT+CIPSTART=\"TCP\","
#define AT_CIPSTART_SSL "AT+CIPSTART=\"SSL\","
#define AT_CIPSSLSIZE "AT+CIPSSLSIZE=4096"
#define AT_CIPSEND "AT+CIPSENDEX="
#define AT_CIPCLOSE "AT+CIPCLOSE"

// AT Commands, for access point setup and operation
#define AT_CWMODE_AP "AT+CWMODE_DEF=2"
#define AT_CWSAP_SET "AT+CWSAP="
#define AT_CWSAP_GET "AT+CWSAP?"
#define AT_CIPAP_SET "AT+CIPAP=\"192.168.4.1\""
#define AT_CIPAP_GET "AT+CIPAP?"
#define AT_CIPMUX "AT+CIPMUX=1"
#define AT_CIPSERVER "AT+CIPSERVER=1,80"
#define AT_CWDHCP "AT+CWDHCP=0,0"
#define AT_CWLIF "AT+CWLIF"
#define IPD "+IPD"
#define AT_CIPCLOSE_AP "AT+CIPCLOSE="


// default html page to display
#define DEF_DIR "default,"
#define DEF_HTML "<html>\n<title>Page Error</title>\n<body>\n<h1>Page not set</h1>\n<p>The page you requested was not found</p>\n</body>\n</html>"

#define HTTP_POST "POST "
#define HTTP_GET "GET "
#define HTTP_0 " HTTP/1.1\r\nHost: "
#define HTTP_1 "\r\nAccept:*/*\r\nContent-Length: "
#define HTTP_CHUNKED "\r\nAccept:*/*\r\nTransfer-Encoding:chunked"
#define HTTP_2 "\r\nContent-Type: application/x-www-form-urlencoded"
#define HTTP_JSON "\r\nContent-Type:application/json"
#define HTTP_END "\r\n\r\n"

//macros for length of boilerplate part of GET and POST requests
//-3 offset to ignore null terminators, +4 offset for "?" and ":" and \r\n
#define HTTP_GET_FIXED_LEN sizeof(HTTP_GET)+sizeof(HTTP_0)+sizeof(HTTP_END)-3+2
//-5 offset to ignore null terminators, +3 offset for ":", and /r/n
#define HTTP_POST_FIXED_LEN sizeof(HTTP_POST)+sizeof(HTTP_0)+sizeof(HTTP_1)\
  +sizeof(HTTP_2)+sizeof(HTTP_END)-5+3
#define HTTP_POST_JSON_FIXED_LEN sizeof(HTTP_POST)+sizeof(HTTP_0)+sizeof(HTTP_1)\
  +sizeof(HTTP_2)+sizeof(HTTP_END)-5+3
#define HTTP_JSON_CHUNKED_FIXED_LEN sizeof(HTTP_POST)+sizeof(HTTP_0)+sizeof(HTTP_CHUNKED)\
  +sizeof(HTTP_JSON)-4+3

#include <WString.h>
#include <Arduino.h>

class ESP8266 {
  public:
    ESP8266();
    ESP8266(bool verboseSerial);
    ESP8266(int mode);
    ESP8266(int mode, bool verboseSerial);
    void setPage(String directory, String html);
    void begin();
    bool isConnected();
    void connectWifi(String ssid, String password);
    bool startserver(String netName, String pass);
    bool isBusy();
    void sendRequest(int type, String domain, int port, String path,
        String data);
    void sendRequest(int type, String domain, int port, String path,
        String data, bool auto_retry);
    void sendBigRequest(String domain, int port, String path,
        const char* data);
    void clearRequest();
    int benchmark;
    bool hasResponse();
    String getResponse();
    String getMAC();
    String getVersion();
    String getStatus();
    bool restore();
    bool reset();
    String sendCustomCommand(String command, unsigned long timeout);
    bool isAutoConn();
    void setAutoConn(bool value);
    int getTransmitCount();
    void resetTransmitCount();
    int getReceiveCount();
    void resetReceiveCount();
    String getData();
    bool hasData();

  private:
    static ESP8266 * _instance; //Static instance of this singleton class

    //String constants for processing ESP8266 responses
    static char const READY[];
    static char const OK[];
    static char const OK_PROMPT[];
    static char const SEND_OK[];
    static char const ERROR[];
    static char const FAIL[];
    static char const STATUS[];
    static char const ALREADY_CONNECTED[];
    static char const HTML_START[];
    static char const HTML_END[];
    static char const SEND_FAIL[];
    static char const CLOSED[];
    static char const UNLINK[];

    // Private enums and structs
    enum RequestType {GET_REQ, POST_REQ};
    struct Request {
      volatile char domain[DOMAINSIZE];
      volatile char path[PATHSIZE];
      volatile char data[DATASIZE+1];
      volatile char *data_ref;
      volatile int  data_len;
      volatile long data_offset;
      volatile int port;
      volatile RequestType type;
      volatile bool auto_retry;
      volatile bool ssl;
      volatile bool big;
    };
    struct RequestAP {
      volatile RequestType typeAP;
      volatile char path[PATHSIZE];
      volatile char data[DATASIZE];
    };
    struct Pages {
      volatile char directory[NUMBEROFPAGES*PAGESIZE];
      volatile char html[NUMBEROFPAGES*HTMLSTORAGE];
    };
    enum State {
      IDLE, //When nothing is happening
      CIPSTATUS, //awaiting CIPSTATUS response
      CWJAP, //connecting to network
      CIPSTART, //awaiting CIPSTART response
      CIPSEND, //awaiting CIPSEND response
      DATAOUT, //awaiting "SEND OK" confirmation
      AWAITRESPONSE, //awaiting HTTP response
    };
    enum StateAP {
      RESET,
      AWAITCLIENT,
      AWAITREQUEST, //awaiting client request
      SENDRESPONSE, //return a response
      DATAOUTAP, //awaiting "SEND OK" confirmation
      CLOSE, //close the connection
    };

    // Functions for strictly non-ISR context
    void enableTimer();
    void disableTimer();
    void init(int mode, bool verboseSerial);
    bool checkPresent();
    bool startAP();
    void getMACFromDevice();
    bool waitForTarget(const char *target, unsigned long timeout);
    bool stringToVolatileArray(String str, volatile char arr[],
        uint32_t len);
    bool pagesAvailable();
    bool pageExists(String directory);
    String getPage(String dir);
    void pageCreate(volatile char arr[], String directory);
    void pageStore(volatile char arr[], String dir, String html);


    // Functions for ISR context
    static void handleInterrupt(void);
      static void handleInterruptAP(void);
    void processInterrupt();
      void processInterruptAP();
    bool isTargetInResp(const char target[]);
    bool getStringFromResp(const char *target, char *result);
    bool getStringFromResp(const char *startTarget, const char *endTarget,
        char *result);
    int getStatusFromResp(); //Only call if we got an OK CIPSTATUS resp
    void loadRx();
    void emptyRx();
    void emptyRxAndBuffer();
    void requestParse(String resp);
    void findPage();
    void servePage();
    bool setServer();


    // Non-ISR variables
    String MAC;
    IntervalTimer timer;
    int ESPmode;


    // Shared variables between user calls and interrupt routines
    volatile bool serialYes;
    volatile bool newNetworkInfo;
    volatile char ssid[SSIDSIZE];
    volatile char password[PASSWORDSIZE];
    volatile bool connected;
    volatile bool doAutoConn;
    volatile bool hasRequest;
    volatile Request *request_p;
    volatile bool responseReady;
    volatile char response[RESPONSESIZE];
    volatile int transmitCount;
    volatile int receiveCount;
    volatile bool reqReconn;

    //Shared variables for AP
    volatile bool dataReady;
    volatile int linkID;
    volatile Pages *storedPages;
    volatile RequestAP *requestAP_p;
    volatile int debugCount;
    volatile bool first;
    volatile bool serverStatus;

    // Variables for interrupt routines
    volatile State state;
    volatile StateAP stateAP;
    volatile unsigned long lastConnectionCheck;
    volatile unsigned long timeoutStart;
    volatile char inputBuffer[BUFFERSIZE];  // Serial input loaded here
};
