//outputs CSV data that can be opened by Excel, time is stored as serial number accurate to ~1second
//WIFI webserver to dynamically download data

#include "WifiSettings.h"
#include "utils.h"
#include "ioMux.h"
#include "sensors.h"
#include <avr/pgmspace.h>
#include <MemoryFree.h>

//Needed for SD card
//Uses hardware SPI on D10,D11,D12,D13
//LOGINTERVAL is in seconds- takes about 1 second to do a log
#include <SPI.h>
#include <SD.h>

const char JSON_EXT[] PROGMEM = ".JS";
const char FILENAME[] PROGMEM = "%04d%02d%02d.JS";
const char XML_DATE[] PROGMEM = "%04d-%02d-%02d";
const char XML_TIME[] PROGMEM = "%02d:%02d:%02d";
const char XML_INT[] PROGMEM = "%d";

const char HTTP_HEADER_GET[] PROGMEM = "GET ";
const char HTTP_HEADER_PROTOCOL[] PROGMEM = "HTTP/";
const char URL_FILE_INDEX_1[] PROGMEM = "index.htm";
const char URL_FILE_INDEX_2[] PROGMEM = "index.html";
const char URL_FILE_LIST[] PROGMEM = "list.html";
const char URL_FILE_DELETE[] PROGMEM = "rm/";
const char URL_WIFI_STATUS[] PROGMEM = "wiFiStatus.html";
const char URL_API_OUTSIDE[] PROGMEM = "api/outside";
const char URL_API_WATERTANK[] PROGMEM = "api/waterTank";

const char INDEX_LINE[] PROGMEM = "<li><a href=\"%s\">Download %s</a></li>\n";

#define JSON_START F("[\r\n")
#define JSON_END F("\r\n]")
#define JSON_NULL F("null")
#define COMMA F(",")

// Every 10 minutes.
#define LOG_INTERVAL (10L * 60L * 1000L)
#define WIFI_LOG_INTERVAL (60L * 60L * 1000L)
#define WIFI_RESET_INTERVAL (24L * 60L * 60L * 1000L)


#define LIGHTPIN A0

#define TEMP_A_PIN A0
#define TEMP_B_PIN A1
#define TEMP_C_PIN A2
#define TEMP_D_PIN A3

//Needed for RTC
//Uses SCL and SDA aka A4 and A5
#include <Wire.h>
#include <RTClib.h>

RTC_DS1307 rtc;

//LED pins- LED1 is diagnostic, LED2 is card in use (flickers during write)
#define LED1 4
#define LED2 3
long sampleTime;      //keeps track of logging interval
long wifiLogTime;      //keeps track of logging interval
long wifiResetTime;      //keeps track of logging interval
int status=0;         //to display status info on webpage
unsigned long lastSampleTimeUnix = 0;

//for DHT11 interface
//#define DHT11PIN 8
#define DHT11PIN 2


// UltraSonic module
#define USONIC_1_TRIG D_X
#define USONIC_1_ECHO D_Y
#define USONIC_2_TRIG D_X
#define USONIC_2_ECHO D_Y
#define USMAX 3000  //??
#define USTIMEOUT_2M   11600  // Timeout for 2m
#define US_TIMEOUT_2_5M 14500  // Timeout for 2.5m

#define WIFI Serial
#define WIFI_DEBUG 1

#define AT_CMD_RECV F("+IPD,")
#define AT_CMD_SEND F("AT+CIPSEND=")
#define AT_CMD_CLOSE F("AT+CIPCLOSE=")
#define AT_CMD_WIFI_STATUS F("AT+CIPSTATUS")
#define AT_CMD_FIRMWARE F("AT+GMR")
#define AT_CMD_DISCONNECT F("AT+CWQAP")
#define AT_CMD_STATION_IP F("AT+CWLIF")
#define AT_CMD_AP_PASSIVE_SCAN F("AT+CWLAP=,,,1,,")
#define AT_CMD_PING_AP F("AT+PING=\"192.168.0.1\"")
#define AT_CMD_AP_STATUS F("AT+CWJAP_CUR?")
#define AT_CMD_GET_SLEEP_MODE F("AT+SLEEP?")
#define AT_CMD_GET_FREE_RAM F("AT+SYSRAM?")

#define AT_CMD_GET_RECEIVE_MODE F("AT+CIPRECVMODE?")
#define AT_CMD_GET_STATION_LIST_MODE F("AT+CWLIF")

#define AT_CMD_GET_MODE_CFG_FLASH F("AT+CWMODE_DEF?")
#define AT_CMD_SET_MODE_STATION_FLASH F("AT+CWMODE_DEF=1")
#define AT_CMD_SET_MODE_SOFT_AP_STATION_FLASH F("AT+CWMODE_DEF=3")

#define AT_CMD_GET_IP_CFG F("AT+CIPSTA_CUR?")
#define AT_CMD_GET_IP_CFG_FLASH F("AT+CIPSTA_DEF?")
#define AT_CMD_GET_AP_CFG_FLASH F("AT+CWJAP_DEF?")
#define AT_CMD_SET_AUTO_CONN_FLASH F("AT+CWAUTOCONN=1")

#define AT_REPLY_OK F("OK")
#define AT_REPLY_ERROR F("ERROR")
#define AT_REPLY_SEND_OK F("SEND OK")
#define AT_REPLY_READY F("ready")
//#define AT_REPLY_GOT_IP F("WIFI GOT IP")
#define AT_READY_TO_SEND F(">")

#define HTTPINDEX_START F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<!DOCTYPE html>\r\n<html><body>\r\n<h2>WIFI DATALOGGER</h2><br>Click <a href=\"list.html\">list</a> to see the files that are availble to download.<br><br>\r\n")
#define HTTPINDEX_START_2 F("<h3>API</h3><ul><li><a href=\"api/waterTank\">Water tank levels</a></li><li><a href=\"api/outside\">Outside climate</a></li></ul><br><h3>Status</h3><ul><li><a href=\"wiFiStatus.html\">WiFi Status</a></li></li></ul><br>\r\n")
#define HTTPLIST F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<!DOCTYPE html>\r\n<html><body>\r\n<h2>WIFI DATALOGGER</h2><br>Choose a file:<ul>\r\n")
#define HTTPLIST_2 F("</ul>")
#define HTTPWIFI_STATUS_1 F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<!DOCTYPE html>\r\n<html><body>\r\n<h2>WIFI STATUS</h2><h4>Status</h4><br><samp>\r\n")
#define HTTPWIFI_STATUS_2 F("</samp>\r\n")
#define HTTPWIFI_STATUS_3 F("<br><h4>Firmware Version</h4><samp>\r\n")
#define HTTPWIFI_STATUS_4 F("<br><h4>AP Station Status</h4><samp>\r\n")
//#define HTTPWIFI_STATUS_5 F("<br><h4>Sleep Mode</h4><samp>\r\n")
#define HTTPWIFI_STATUS_6 F("<br><h4>ESP-13 Free RAM</h4><samp>\r\n")
#define HTTP404 F("HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<!DOCTYPE html>\r\n<html><body>\r\n<h3>File not found</h3><br><a href=\"index.htm\">Back to index...</a><br></body></html>")
#define HTTPJSON F("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n")
#define HTTPTEXT F("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n")
#define STATUSTITLE F("<br>Status (zero is no error): ")

const char STATUS_TEXT[] PROGMEM = "<br>Status (zero is no error): %d<br>Current time: %04d-%02d-%02d %02d:%02d:%02d<br>";
const char STATUS_TEXT2[] PROGMEM = "Last sample taken at: %04d-%02d-%02d %02d:%02d:%02d<br>Free memory: %d<br>";
#define HTMLEND F("</body></html>")
#define HTMLBR F("<br>")

const char API_WATER_TANK_DATA[] PROGMEM = "{ \"waterTank\": { \"emptySpaceA\": %d, \"emptySpaceB\": %d}}";
const char API_OUTSIDE_CLIMATE_DATA[] PROGMEM = "{ \"outside\": { \"temperature\": %d, \"humidity\": %d, \"lightLevel\": %d}}";

const char LOG_MSG_TEMP_VALUE[] PROGMEM = "Analog temp%d = %d, ";

// sketch needs ~450 bytes local space to not crash
//#define PKTSIZE 129
//#define PKTSIZE 257
#define PKTSIZE 161
#define SBUFSIZE 20
char fname[SBUFSIZE]="";    //filename
char currentFilename[SBUFSIZE]="";    //filename
int cxn=-1;                 //connection number
char pktbuf[PKTSIZE]="";    //to consolidate data for sending


void setup() {
  WIFI.begin(115200);   //start serial port for WIFI
/*
  selectDigitalDevice(0);
  usonicSetup(USONIC_1_TRIG, USONIC_1_ECHO); //set up ultrasonic sensor

  selectDigitalDevice(1);
  usonicSetup(USONIC_2_TRIG, USONIC_2_ECHO); //set up ultrasonic sensor

  setupDHT11(DHT11PIN);           //start temp/humidity sensor
*/
  pinMode(D_A, OUTPUT);
  pinMode(D_B, OUTPUT);
  pinMode(D_DISABLE, OUTPUT);

  pinMode(LED2,OUTPUT);
  digitalWrite(LED2,HIGH);      //turn on LED to show card in use
  if(!SD.begin(10)){            //SD card not responding
    errorflash(1);              //flash error code for card not found
  }

  if (!rtc.begin()) {           // rtc not responding
    errorflash(2);              //flash error code for RTC not found
  }
  digitalWrite(LED2,LOW);      //turn on LED to show card in use

  wifiinit();           //send starting settings- find AP, connect and set to station mode only, start server

  DateTime compileTime = DateTime(F(__DATE__), F(__TIME__));
  DateTime now = rtc.now();
  // The 90*60 is a bit of a half arsed attempt to not set the date during summer DST
  if( compileTime.secondstime() > (now.secondstime() + (90*60))) {
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(compileTime);
  }

  if (!rtc.isrunning()) {       //rtc not running- use ds1307 example to load current time
    errorflash(3);              //flash error code for RTC not running
  }

  File fhandle = openDataFile();
  if( ! fhandle) {
    errorflash(4);              //flash error code for RTC not running
  }
  fhandle.close(); //then close the file

  sampleTime = millis() + 10000;   //Wait for 10 seconds before the first sample.
  wifiLogTime = millis() + 10000;
  wifiResetTime = millis() + WIFI_RESET_INTERVAL;
}

void loop() {

  if(millis()>sampleTime){   //if it's been more than logging interval

   setupDHT11(D_Y);           //start temp/humidity sensor
   selectDigitalDevice(2);
   dht11Response_t dht11Response = readDHT11(D_Y);     //fetch data to log

   usonicSetup(USONIC_1_TRIG, USONIC_1_ECHO); //set up ultrasonic sensor
   selectDigitalDevice(0);
   delay(100);

    int distanceA=usonicRead(USONIC_1_TRIG, USONIC_1_ECHO, US_TIMEOUT_2_5M); //distance in cm, time out at 11600us or 2m maximum range

   usonicSetup(USONIC_1_TRIG, USONIC_1_ECHO); //set up ultrasonic sensor
   selectDigitalDevice(1);
   delay(100);
    
    int distanceB=usonicRead(USONIC_2_TRIG, USONIC_2_ECHO, US_TIMEOUT_2_5M); //distance in cm, time out at 11600us or 2m maximum range

    int light = -1;
    //light=analogRead(LIGHTPIN);     //get light sensor data
    float temperatureA = 10.1F;
    float temperatureB = 10.2F;
    float temperatureC = 10.3F;
    float temperatureD = 10.4F;
//    float temperatureA = readThermister(TEMP_A_PIN);
//    float temperatureB = readThermister(TEMP_B_PIN);
//    float temperatureC = readThermister(TEMP_C_PIN);
//    float temperatureD = readThermister(TEMP_D_PIN);
    //Serial.print(F("DistanceA=")); Serial.println(distanceA);
    //Serial.print(F("DistanceB=")); Serial.println(distanceB);
    //Serial.print(F("tempA=")); Serial.println(temperatureA);
    //Serial.print(F("tempB=")); Serial.println(temperatureB);
    //Serial.print(F("tempC=")); Serial.println(temperatureC);
    //Serial.print(F("tempD=")); Serial.println(temperatureD);
    //Serial.print(F("DHTtemp=")); Serial.println(DHTtemp);
    //Serial.print(F("DHThum=")); Serial.println(DHThum);

    logtocard(temperatureA, temperatureB, temperatureC, temperatureD, distanceA, distanceB, dht11Response, light);          //log it

//    sampleTime+=(LOGINTERVAL*10L);        //add interval, so interval is precise
    sampleTime+=LOG_INTERVAL;        //add interval, so interval is precise
  }
  
  checkwifi();  

}




void errorflash(int n){           //non-recoverable error, flash code on LED1
  //WIFI.print(F("error flash:"));
  //WIFI.println(n);
  status=n;                       //set status for webpage
  pinMode(LED1,OUTPUT);
  while(1){                       //do until reset
    for(int i=0;i<n;i++){         //flash n times
      digitalWrite(LED1,HIGH);
      delay(500);
      digitalWrite(LED1,LOW);
      delay(300);      
      checkwifi();                //wifi services still available during fault
    }
    delay(1000);                  //pause and repeat
  }  
}


File openDataFile(){      //put the column headers you would like here, if you don't want headers, comment out the line below
  DateTime now = rtc.now();                   //capture time
  sprintf_P(fname, FILENAME, now.year(), now.month(), now.day());
  if( strlen(currentFilename) == 0) {
    strcpy(currentFilename,fname);
  }
  if( strcmp(fname,currentFilename) != 0) {
    File currenFile = SD.open(currentFilename, FILE_WRITE);
    currenFile.print(JSON_END);
    currenFile.close();
    strcpy(currentFilename,fname);
  }

  return SD.open(fname, FILE_WRITE);
}

void logtocard(float temperatureA, float temperatureB, float temperatureC, float temperatureD, int distanceA, int distanceB,
    dht11Response_t dht11Response, int lightLevel){
  unsigned long timestamp;
  DateTime now = rtc.now();                   //capture time
  lastSampleTimeUnix = now.unixtime();
  digitalWrite(LED2,HIGH);                    //turn on LED to show card in use
  delay(200);                                 //a bit of warning that card is being accessed, can be reduced if faster sampling needed
  
  File fhandle = openDataFile();
  if(!fhandle){                     // Cannot open the file, raise an error.
    errorflash(5);
  }

  unsigned long s;
  s=fhandle.size();             //get file size
  if(!s){                       //if file is empty, add column headers
    fhandle.print(JSON_START);
  } else {
    fhandle.println(COMMA);
  }
    
  // Timestamp format ISO 8601: 2017-12-26T07:44:19+10
  fhandle.print(F(" {\r\n  \"sampleTime\": \"")); //write timestamp to card, integer part, converted to Excel time serial number with resolution of ~1 second
  sprintf_P(pktbuf, XML_DATE, now.year(), now.month(), now.day());
  fhandle.print(pktbuf);
  fhandle.print(F("T"));
  sprintf_P(pktbuf, XML_TIME, now.hour(),now.minute(),now.second());
  fhandle.print(pktbuf);
  fhandle.println(F("+10\","));

  fhandle.print(F("  \"outside\": { \"temperature\": "));
  if(dht11Response.status){fhandle.print(dht11Response.temperature);}else{fhandle.print(JSON_NULL);}         //put data if valid otherwise blank (will be blank cell in Excel)
  fhandle.print(F(", \"humidity\": "));
  if(dht11Response.status){fhandle.print(dht11Response.humidity);}else{fhandle.print(JSON_NULL);}          //put data if valid otherwise blank
  fhandle.print(F(", \"lightLevel\": "));
  fhandle.print(lightLevel);                       //put data (can't validate analog input)
  fhandle.println(F("},"));

  fhandle.print(F("  \"inside\": { \"temperatureA\": "));
  fhandle.print(temperatureA);
  fhandle.print(F(", \"temperatureB\": "));
  fhandle.print(temperatureB);
  fhandle.print(F(", \"temperatureC\": "));
  fhandle.print(temperatureC);
  fhandle.print(F(", \"temperatureD\": "));
  fhandle.print(temperatureD);
  fhandle.println(F("},"));

  fhandle.print(F("  \"waterTank\": { \"emptySpaceA\": "));
  fhandle.print(distanceA);
  fhandle.print(F(", \"emptySpaceB\": "));
  fhandle.print(distanceB);
  fhandle.println(F("}"));

  if(!fhandle.print(F(" }"))){                     //if we can't write data, there's a problem with card (probably full)
    fhandle.close();                          //close file to save the data we have
    errorflash(6);                            //error code
  }
  fhandle.close();                            //close file so data is saved
  digitalWrite(LED2,LOW);                     //turn off LED card to show card closed
}

void dorequest(int messageLength){
//  unsigned long t;                     //timeout
  int p=0;                             //pointer to getreq position
  int f=1;                             //flag to tell if first line or not
  int crcount=0;                       //if we get two CR/LF in a row, request is complete
  char lengthStr[20];

  clearPktbuf();
  int bytesRead = WIFI.readBytes(pktbuf, min( messageLength, sizeof(pktbuf) - 1));
  //sprintf(lengthStr, "\nbytesRead %d", bytesRead);
  //WIFI.println(lengthStr);
  //WIFI.print(F("doReq: "));
  //WIFI.println(pktbuf);

  // Read all the remaining characters off the serial port
  unsigned long timeout = 1000 + millis();
  while(bytesRead < messageLength && millis() < timeout) {
    if(WIFI.read() != -1) {
      bytesRead++; 
    }
  }

  strcpy_PF( lengthStr, HTTP_HEADER_PROTOCOL);
  if( ! strContains(pktbuf, lengthStr)) {
    wifiResetTime -= (WIFI_RESET_INTERVAL / 5);
    return;
  }

  crcount = strContains(pktbuf, "\r");

  fname[0]=0;                                              //blank
  if( strncmp_P(pktbuf, HTTP_HEADER_GET, strlen_P(HTTP_HEADER_GET)) == 0) {
    //If 'GET ' at the start, so change flag to0
    crcount=1;
    parseFileName(pktbuf);                                                       //complete request found, extract name of requested file
  }

  if(fname[0]==0 || strcmp_P(fname,URL_FILE_INDEX_1) == 0 || strcmp_P(fname,URL_FILE_INDEX_2) == 0){
    servePage();
    sendStatus();
    crcount=0;                                      //serve index page, reset crcount on fileserve
  } else if(strcmp_P(fname, URL_FILE_LIST) == 0){
    listPage();
    sendStatus();
    crcount=0;                                      //serve index page, reset crcount on fileserve
  } else if(strcmp_P(fname, URL_WIFI_STATUS) == 0){
    displayWiFiStatus();
    sendStatus();
    crcount=0;                                      //serve index page, reset crcount on fileserve
  } else if(strcmp_P(fname, URL_API_WATERTANK) == 0){
    getWaterTankLevels();
    crcount=0;
  } else if(strcmp_P(fname, URL_API_OUTSIDE) == 0){
    getOutsideClimate();
    crcount=0;
  } else if(strncmp_P(fname, URL_FILE_DELETE, strlen_P(URL_FILE_DELETE)) == 0) {
    deleteFile(fname + strlen_P(URL_FILE_DELETE));
    listPage();
    sendStatus();
    crcount=0;                                      //serve index page, reset crcount on fileserve
  } else {
    crcount = sendFile(fname);                       //serve entire data file
  }

  if(crcount) {serve404();sendStatus();}             //no valid file served => 404 error

  delay(100);
  WIFI.print(AT_CMD_CLOSE);                         //close
  WIFI.println(cxn);  
  wiFiWaitForOkReply();                             //disconnect
  cxn=-1;                                           //clear for next connection
}


void parseFileName(char *requestLine){
  int t=0;                   
  fname[0]=0;                                       //blank
  int p=5;                                          //start after 'GET /'
  while((requestLine[p]!=' ')                       //use ? to separate fields, ' ' to end
        &&(requestLine[p])
        &&(requestLine[p]!='?')){                   //terminate on space or end of string or ?
    fname[t]=requestLine[p];
    t++;
    fname[t]=0;                                     //add to fname
    p++;
    if( !(t<SBUFSIZE)) {                            //check bounds
      return;
    }
  }
}


void sendStatus(){                                   //to show logger status
  DateTime now = rtc.now();

  //DateTime now = rtc.isrunning() ? rtc.now() : DateTime(dummy_t);
  //uint32_t dummy_t = 0;
  //DateTime now = DateTime(dummy_t);

  WIFI.print(AT_CMD_SEND);                           //send data
  WIFI.print(cxn);                                   //to client
  WIFI.print(F(","));        
  sprintf_P(pktbuf, STATUS_TEXT, status, now.year(), now.month(), now.day(), now.hour(),now.minute(),now.second());
  WIFI.println(strlen(pktbuf));            //data has length, needs to be same as string below, plus 1 for status
  delay(50);
  WIFI.print(pktbuf);
  wiFiWaitForSendOkReply();

  int lsYear   = 0;
  int lsMonth  = 0;
  int lsDay    = 0;
  int lsHour   = 0;
  int lsMinute = 0;
  int lsSecond = 0;
  if( lastSampleTimeUnix > 0) {
    DateTime lastSampleTime = DateTime( lastSampleTimeUnix);
    lsYear   = lastSampleTime.year();
    lsMonth  = lastSampleTime.month();
    lsDay    = lastSampleTime.day();
    lsHour   = lastSampleTime.hour();
    lsMinute = lastSampleTime.minute();
    lsSecond = lastSampleTime.second();
  }

  sprintf_P(pktbuf, STATUS_TEXT2, lsYear, lsMonth, lsDay, lsHour, lsMinute, lsSecond, freeMemory());
  wiFiSendStart(cxn, strlen(pktbuf) + strlen_PF(HTMLEND));
  WIFI.print(pktbuf);
  WIFI.print(HTMLEND);
  wiFiWaitForSendOkReply();
}

void servePage(){                                     //for serving a page of data
  WIFIsenddata(HTTPINDEX_START, cxn);
  WIFIsenddata(HTTPINDEX_START_2, cxn);
}

void listPage(){                                     //for serving a page of data
  WIFIsenddata(HTTPLIST, cxn);

//  debugWiFi(F("lf1 fm;"));
//  sprintf_P(pktbuf, XML_INT, freeMemory());
//  debugWiFi(pktbuf);

//  File root = SD.open("/",FILE_READ);
//  root.seek(0);
  File root = SD.open("/");
//  debugWiFi(F("lf2 fm;"));
//  sprintf_P(pktbuf, XML_INT, freeMemory());
//  debugWiFi(pktbuf);
 
  int sendLength = -1;
  clearPktbuf();

  while (true) {
    File entry =  root.openNextFile();

    if (! entry) {
//      WIFI.print(F("-nf"));
      // no more files
      break;
    }
//    WIFI.print(F("-1:"));
//    WIFI.println(entry.name());

    if (! entry.isDirectory()) {
      // files have sizes, directories do not
      //<a href=\"DATA.CSV\">Download DATA.CSV</a><br>
      sprintf_P(&pktbuf[strlen(pktbuf)], INDEX_LINE, entry.name(), entry.name());
      if( sendLength == -1) {
        sendLength = sizeof(pktbuf) - (strlen(pktbuf) + 20);
      } else if (strlen(pktbuf) > sendLength) {
        WIFIsenddata(pktbuf,cxn);
        clearPktbuf();
      }
    }
    entry.close();
  }
  root.close();

  if( strlen(pktbuf) > 0) {
    WIFIsenddata(pktbuf, cxn);
  }

  WIFIsenddata(HTTPLIST_2, cxn);
}

void deleteFile(char* filename) {
  bool result = SD.remove(filename);
}

void displayWiFiStatus() {
  WIFIsenddata(HTTPWIFI_STATUS_1, cxn);

  int len = strlen_PF(AT_REPLY_OK) + 1;
  char replyStr[len];
  strcpy_PF(replyStr,AT_REPLY_OK);

  WIFI.println(AT_CMD_WIFI_STATUS);
  logStatusWeb(replyStr);

  WIFIpurge();              //clear incoming buffer
  WIFIsenddata( pktbuf, cxn);
  WIFIsenddata( HTTPWIFI_STATUS_2, cxn);

  WIFIsenddata( HTTPWIFI_STATUS_3, cxn);
  WIFI.println(AT_CMD_FIRMWARE);
  WIFIsenddata( HTTPWIFI_STATUS_2, cxn);

  WIFIsenddata( HTTPWIFI_STATUS_4, cxn);
  WIFI.println(AT_CMD_AP_STATUS);
  logStatusWeb(replyStr);
  WIFIsenddata( HTTPWIFI_STATUS_2, cxn);

  WIFIsenddata( HTTPWIFI_STATUS_6, cxn);
  WIFI.println(AT_CMD_GET_FREE_RAM);
  logStatusWeb(replyStr);
  WIFIsenddata( HTTPWIFI_STATUS_2, cxn);
}

void logStatusWeb(const char* replyStr){   //command c (nocrlf needed), returns true if response r received, otherwise times out
  int len = strlen_PF(HTMLBR) + 1;
  char htmlBr[len];
  strcpy_PF(htmlBr,HTMLBR);

  unsigned long timeout = 2000 + millis();
  clearPktbuf();
  while(millis()< timeout && !strContains(pktbuf, replyStr)){          //until timeout
    while(WIFI.available()){
      int c = WIFI.read();
      if(c != -1){             //response good
        if(c== '\n') {
          if( strlen(pktbuf) < (PKTSIZE - 5)){
            addtobuffer(pktbuf,PKTSIZE,htmlBr);      //add to buffer
          }
        } else {
          addtobuffer(pktbuf,PKTSIZE,c);      //add to buffer
        }
      }
    }
  }
  WIFIpurge();              //clear incoming buffer
  WIFIsenddata( pktbuf, cxn);
}

void serve404(){                                   //for serving a 404 page. i.e. noting found
  WIFIsenddata( HTTP404, cxn);
}

int sendFile(char *filename){             //for providing a csv document to download
  File dataFile = SD.open(filename);     // Open the file and see if it exists.
  if (! dataFile) {
    return 1;
  }

  strcpy_P(pktbuf,JSON_EXT);
  bool appendJsonTail = false;
  if( strContains( filename, pktbuf)){
    appendJsonTail = true;
    WIFIsenddata( HTTPJSON, cxn);
  } else {
    WIFIsenddata( HTTPTEXT, cxn);
  }

  pktbuf[0]=0;                           //empty buffer

  dataFile.seek(0);
  while (dataFile.available()) {         //send it all
    char c=dataFile.read();
    if(appendJsonTail && c == ']') {
      appendJsonTail = false;
    }
    addtobuffer(pktbuf,PKTSIZE,c);      //add to buffer
    if(strlen(pktbuf)>PKTSIZE-2){       //if buffer full
      WIFIsenddata(pktbuf,cxn);         //send data
      pktbuf[0]=0;                      //empty buffer      
    }    
  }
  if(pktbuf[0]){                        //send data if any left in buffer
    WIFIsenddata(pktbuf,cxn);         //send data
  }
  dataFile.close();                      //close file
  if(appendJsonTail) {
    strcpy_PF(pktbuf, JSON_END);
    WIFIsenddata(pktbuf,cxn);
  }

  return 0;
}

void getWaterTankLevels(){                                     //Returns JSON with the current levels of the water tanks.
  
   usonicSetup(USONIC_1_TRIG, USONIC_1_ECHO); //set up ultrasonic sensor
   selectDigitalDevice(1);
   delay(100);
   int distanceA=usonicRead(USONIC_1_TRIG, USONIC_1_ECHO, US_TIMEOUT_2_5M);

  
   usonicSetup(USONIC_2_TRIG, USONIC_2_ECHO); //set up ultrasonic sensor
   selectDigitalDevice(1);
   delay(100);
   int distanceB=usonicRead(USONIC_2_TRIG, USONIC_2_ECHO, US_TIMEOUT_2_5M);

  sprintf_P(pktbuf, API_WATER_TANK_DATA, distanceA, distanceB);
  wiFiSendStart(cxn, strlen_PF(HTTPJSON)+strlen(pktbuf));
  WIFI.print(HTTPJSON);                  //send HTTP header for csv data type, file content to follow
  WIFI.print(pktbuf);
  wiFiWaitForSendOkReply();
}

void getOutsideClimate(){                      //Returns JSON with the current tempreture, humidity etc.

   setupDHT11(D_Y);           //start temp/humidity sensor
   selectDigitalDevice(2);
   delay(500);
   dht11Response_t dht11Response = readDHT11(D_Y);     //fetch data to log
  
  //dht11Response_t dht11Response = readDHT11(DHT11PIN); //returns status 1 on ok, 0 on fail (eg checksum, data not received)
  int light = -1;
  //int light=analogRead(LIGHTPIN);                //get light sensor data

  sprintf_P(pktbuf, API_OUTSIDE_CLIMATE_DATA, dht11Response.temperature, dht11Response.humidity, light);

  wiFiSendStart(cxn, strlen_PF(HTTPJSON)+strlen(pktbuf));
  WIFI.print(HTTPJSON);                  //send HTTP header for csv data type, file content to follow
  WIFI.print(pktbuf);
  wiFiWaitForSendOkReply();
}


void wifiinit(){
  WIFIcmd(F("ATE0"),AT_REPLY_OK,1000);                                     //turn echo off

  delay(1000);

  if( !checkWifiConfig(AT_CMD_GET_MODE_CFG_FLASH, F("CWMODE_DEF:3"), AT_REPLY_OK)) {
    WIFIcmd(AT_CMD_SET_MODE_SOFT_AP_STATION_FLASH, AT_REPLY_OK, 1000);                     //Set the default to a station
    //WIFIcmd(AT_CMD_SET_MODE_STATION_FLASH, AT_REPLY_OK, 1000);                     //Set the default to a station
  }

  // Set default ip address
  if( !checkWifiConfig(AT_CMD_GET_IP_CFG_FLASH, WIFI_IP, AT_REPLY_OK)) {
    WIFIcmd(AT_CMD_SET_IP, AT_REPLY_OK, 10000);
  }
  
  bool apSet = false;
  for( int cnt = 0; cnt < 5 && ! apSet; cnt++) {
    if( checkWifiConfig(AT_CMD_GET_AP_CFG_FLASH, WIFI_SSID, AT_REPLY_OK)) {
      apSet = true;
    } else {
      delay(1000);
    }
  }
  if( !apSet) {
    WIFIcmd(F("AT+CWQAP"), AT_REPLY_OK, 5000);                     //exit any AP's
    WIFIcmd(AT_CMD_SET_AUTO_CONN_FLASH, AT_REPLY_OK, 1000);        //Set autoconnect
    WIFIcmd(AT_CMD_SET_WIFI_SSID, AT_REPLY_OK, 10000);         //Set the default to a station

    WIFIcmd(F("AT+RST"), AT_REPLY_READY, 10000);                   //reset
    delay(5000);
  }

  wifiConfigServer();


}

void wifiResetServer() {
  WIFIcmd(F("AT+RST"), AT_REPLY_READY, 10000);
  delay(10000);
  wifiConfigServer();
}

void wifiConfigServer() {
  WIFIcmd(F("AT+CIPMUX=1"),AT_REPLY_OK,2000);                              //MUX on (needed for server)
  WIFIcmd(F("AT+CIPSERVERMAXCONN=1"),AT_REPLY_OK,2000);                    //Set the maximum number of connections.
  WIFIcmd(F("AT+CIPSERVER=1,80"),AT_REPLY_OK,2000);                        //server on
  WIFIcmd(F("AT+CIPSTO=60"),AT_REPLY_OK,2000);                             //disconnect after x time if no data
}

bool checkWifiConfig(const __FlashStringHelper *commandF, const __FlashStringHelper *successStrF, const __FlashStringHelper *endResponseF){

  WIFI.println(commandF);

  char endResponse[strlen_PF(endResponseF) + 1];
  strcpy_PF(endResponse,endResponseF);

  char successStr[strlen_PF(successStrF) + 1];
  strcpy_PF(successStr,successStrF);

  unsigned long timeout = millis() + 2000;
  clearPktbuf();
  while(millis() < timeout && !strContains(pktbuf, endResponse)){          //until timeout
    while(WIFI.available()){
      int c = WIFI.read();
      if(c != -1){             //response good
        if(c== '\n') {
          if( strContains(pktbuf, successStr)) {
            WIFIpurge();
            return true;
          }
          clearPktbuf();
        } else {
          addtobuffer(pktbuf,PKTSIZE,c);      //add to buffer
        }
      }
    }
  }

  return false;
}

bool checkWifiConfigEcho(const __FlashStringHelper *commandF, const __FlashStringHelper *successStrF, const __FlashStringHelper *endResponseF){

  WIFI.println(commandF);

  char endResponse[strlen_PF(endResponseF) + 1];
  strcpy_PF(endResponse,endResponseF);

  char successStr[strlen_PF(successStrF) + 1];
  strcpy_PF(successStr,successStrF);

  unsigned long timeout = millis() + 2000;
  clearPktbuf();
  while(millis() < timeout && !strContains(pktbuf, endResponse)){          //until timeout
    while(WIFI.available()){
      int c = WIFI.read();
      if(c != -1){             //response good
        if(c == '\n') {
          WIFI.print(F("resp- "));
          WIFI.println(pktbuf);
          if( strContains(pktbuf, successStr)) {
            WIFIpurge();
            return true;
          }
          clearPktbuf();
        } else {
          addtobuffer(pktbuf,PKTSIZE,c);      //add to buffer
        }
      }
    }
  }
  WIFIpurge();
          WIFI.print(F("TO resp- "));
          WIFI.println(pktbuf);
  WIFI.println(F("TO-"));

  return false;
}

void checkwifi(){
  if( !WIFI.available()) {
    return;
  }

  clearPktbuf();
  int messageLength = -1;
  unsigned long timeout = millis() + 1000;
  bool readData = true;
  
  while(readData && timeout > millis()){                    //check if any data from WIFI
    if( WIFI.available()) {
       int b = WIFI.read();

       // If there is a newline then the data is trash, not a message.
       if(b == '\n') {
         clearPktbuf();
         return;
       }

      addtobuffer(pktbuf,PKTSIZE,b);      //add to buffer
      //WIFI.print(F("xxx; "));
      //WIFI.println(pktbuf);

      if(b == ':') {
        //WIFI.print(F("xxx; "));
        //WIFI.println(pktbuf);
      
        char recvCmd[strlen_PF(AT_CMD_RECV) + 1];
        strcpy_PF(recvCmd,AT_CMD_RECV);
        if(strContains(pktbuf, recvCmd)) {
          // Just received data command: +IPD,0,n:    
          int i = strlen(recvCmd);         
        
          // Get the link id.
          if( !isDigit(pktbuf[i])) {
            cxn = -1;
            return;
          }

          cxn = atoi(&pktbuf[i]);
          //WIFI.print(F("Cxn; "));
          //WIFI.println(cxn);

          // Get the length of the message
          i++; // move past the comma ,
          while(pktbuf[i] != 0 && pktbuf[i] != ',') {
            i++;
          }
          i++; // move past the comma ,
          if( !isDigit(pktbuf[i])) {
            cxn = -1;
            return;
          }
          messageLength = atoi(&pktbuf[i]);
          //WIFI.print(F("messageLength; "));
          //WIFI.println(messageLength);

          // Read all the data
          digitalWrite(LED2,HIGH);
          dorequest(messageLength);
          digitalWrite(LED2,LOW);
          clearPktbuf();
          readData = false;
        }
      }   
    }
  }
}

bool readLine(unsigned long timeoutTime){
  clearPktbuf();
  int messageLength = -1;

  while(timeoutTime > millis()){                    //check if any data from WIFI
    while(WIFI.available()){                        //check if any data from WIFI
      int b = WIFI.read(); 

      if(b == '\n' && strlen(pktbuf) > 0) {
        return true;
      } else if( b != '\r') {
        addtobuffer(pktbuf,PKTSIZE,b);      //add to buffer
      }
    }
  }

  return false;
}

bool readLineEcho(unsigned long timeoutTime){
  clearPktbuf();
  int messageLength = -1;

  while(timeoutTime > millis()){                    //check if any data from WIFI
    while(WIFI.available()){                        //check if any data from WIFI
      int b = WIFI.read(); 

      if(b == '\n' && strlen(pktbuf) > 0) {
    WIFI.print(F("SUC RL = "));
    WIFI.println(pktbuf);
        return true;
      } else if( b != '\r') {
        addtobuffer(pktbuf,PKTSIZE,b);      //add to buffer
      }
    }
  }
    WIFI.print(F("TO RL = "));
    WIFI.println(pktbuf);

  return false;
}

int WIFIcmd(const __FlashStringHelper *c, const __FlashStringHelper *reply, long timeout){   //command c (nocrlf needed), returns true if response r received, otherwise times out
  WIFI.println(c);
  return wiFiWaitForReplyLine(reply, timeout);
}

int WIFIcmd(const char* c, const __FlashStringHelper *reply, long timeout){   //command c (nocrlf needed), returns true if response r received, otherwise times out
  WIFI.println(c);
  return wiFiWaitForReplyLine(reply, timeout);
}

int WIFIcmdEcho(const __FlashStringHelper *c, const __FlashStringHelper *reply, long timeout){   //command c (nocrlf needed), returns true if response r received, otherwise times out
  WIFI.println(c);
  return wiFiWaitForReplyLineEcho(reply, timeout);
}

int WIFIcmdEcho(const char* c, const __FlashStringHelper *reply, long timeout){   //command c (nocrlf needed), returns true if response r received, otherwise times out
  WIFI.println(c);
  return wiFiWaitForReplyLineEcho(reply, timeout);
}

void WIFIpurge(){                         //empty serial buffer
  while(WIFI.available()){
    int b = WIFI.read();
  }
}

void WIFIpurge(int length ){                         //empty serial buffer
  unsigned long timeout = millis() + 1000;
  
  while(length > 0 && timeout > millis()){
    int b = WIFI.read();
    if( b >= 0) {
      length--;
    }
  }
}

void WIFIsenddata(char* d, int client){   //send data to client
  wiFiSendStart(client, strlen(d));
  WIFI.print(d);                          //data
  wiFiWaitForSendOkReply();
}

void WIFIsenddata(const __FlashStringHelper *d, int client){   //send data to client
  wiFiSendStart(client, strlen_PF(d));
  WIFI.print(d);                          //data
  wiFiWaitForSendOkReply();
}

void wiFiSendStart(int client, int len){   //Start send data to WiFi
  WIFI.print(F("AT+CIPSEND="));            //send data command
  WIFI.print(client);                      //to client
  WIFI.print(F(","));
  WIFI.println(len);                       //data has length, needs to be same as string below
  wiFiWaitForReadyToSend();                //Wait for the message that WiFi unit is ready for the data.
}


int wiFiWaitForReadyToSend(){
  return wiFiWaitForReply(AT_READY_TO_SEND, false, 3000);
}

int wiFiWaitForOkReply(){
  return wiFiWaitForReplyLine(AT_REPLY_OK, 5000);
}

int wiFiWaitForSendOkReply(){
  return wiFiWaitForReply(AT_REPLY_SEND_OK, true, 2000);
}

int wiFiWaitForReply(const __FlashStringHelper *replyStrF, bool purge, unsigned long timeout){
  int len = strlen_PF(replyStrF) + 1;
  char replyStr[len];
  strcpy_PF(replyStr,replyStrF);

  timeout+=millis();

  while(millis()<timeout){          //until timeout
    while(WIFI.available()){
      if(WIFI.find(replyStr)){             //response good
        if(purge) {
          WIFIpurge();              //clear incoming buffer
        }
        return 1;
      }
    }
  }

  if(purge) {
    WIFIpurge();                            //clear incoming buffer
  }
  return 0;       //response not found
}

int wiFiWaitForReplyLine(const __FlashStringHelper *replyStrF, unsigned long timeout){
  int len = strlen_PF(replyStrF) + 1;
  char replyStr[len];
  strcpy_PF(replyStr,replyStrF);

  char errorStr[len];
  strcpy_PF(replyStr,replyStrF);

  timeout+=millis();

  while(readLine(timeout)){
    if(strContains(pktbuf, replyStr)) {
      return 1;
    }
  }

  return 0;       //response not found
}

int wiFiWaitForReplyLineEcho(const __FlashStringHelper *replyStrF, unsigned long timeout){
  int len = strlen_PF(replyStrF) + 1;
  char replyStr[len];
  strcpy_PF(replyStr,replyStrF);

  char errorStr[len];
  strcpy_PF(replyStr,replyStrF);

  timeout+=millis();

  while(readLine(timeout)){
    if(strContains(pktbuf, replyStr)) {
      WIFI.print(F("SUC resp- "));
      WIFI.println(pktbuf);
      return 1;
    }
      WIFI.print(F("resp ln- "));
      WIFI.println(pktbuf);
  }

  WIFI.print(F("TO resp- "));
  WIFI.println(pktbuf);
  return 0;       //response not found
}



// =================================================================
// == 
// == General utility functions.
// == 
// =================================================================

void displayFreeMemory(const __FlashStringHelper *msg) {
  Serial.print(F("freeMemory("));
  Serial.print(msg);
  Serial.print(F(")="));
  Serial.println(freeMemory()); 
}

void clearPktbuf(){
  memset(pktbuf, 0, sizeof(pktbuf));
}
