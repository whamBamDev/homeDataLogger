//outputs CSV data that can be opened by Excel, time is stored as serial number accurate to ~1second
//WIFI webserver to dynamically download data

#include "WifiSettings.h"
#include <avr/pgmspace.h>
#include <MemoryFree.h>

//Needed for SD card
//Uses hardware SPI on D10,D11,D12,D13
//LOGINTERVAL is in seconds- takes about 1 second to do a log
#include <SPI.h>
#include <SD.h>

const char NTP_CONNECT_CMD[] PROGMEM = "AT+CIPSTART=0,\"UDP\",\"%s\",123";
const char TIME_FORMAT[] PROGMEM = "%04d-%02d-%02d %02d:%02d:%02d";

const char UTP0_HOST[] PROGMEM = "0.au.pool.ntp.org";
const char UTP0_IP[] PROGMEM = "203.100.61.10";
const char UTP1_HOST[] PROGMEM = "1.au.pool.ntp.org";
const char UTP1_IP[] PROGMEM = "54.252.129.186";
const char UTP2_HOST[] PROGMEM = "2.au.pool.ntp.org";
const char UTP2_IP[] PROGMEM = "144.48.166.166";
const char UTP3_HOST[] PROGMEM = "3.au.pool.ntp.org";
const char UTP3_IP[] PROGMEM = "203.19.252.4";

const char * const NTP_SERVERS[][2] PROGMEM = {
  {UTP1_HOST, UTP1_IP},
  {UTP0_HOST, UTP0_IP},
  {UTP2_HOST, UTP2_IP},
  {UTP3_HOST, UTP3_IP}
};

const char JSON_EXT[] PROGMEM = ".JS";
const char FILENAME[] PROGMEM = "%04d%02d%02d.JS";
const char XML_DATE[] PROGMEM = "%04d-%02d-%02d";
const char XML_TIME[] PROGMEM = "%02d:%02d:%02d";
const char XML_INT[] PROGMEM = "%d";

const char HTTP_HEADER_GET[] PROGMEM = "GET ";
const char URL_FILE_INDEX_1[] PROGMEM = "index.htm";
const char URL_FILE_INDEX_2[] PROGMEM = "index.html";
const char URL_FILE_LIST[] PROGMEM = "list.html";
const char URL_WIFI_STATUS[] PROGMEM = "wiFiStatus.html";
const char URL_API_WATERTANK[] PROGMEM = "api/waterTank";
const char URL_API_OUTSIDE[] PROGMEM = "api/outside";

const char INDEX_LINE[] PROGMEM = "<li><a href=\"%s\">Download %s</a></li>\n";

#define TEN_HOURS_IN_SECONDS (10 * 60 * 60)

#define JSON_START F("[\r\n")
#define JSON_END F("\r\n]")
#define JSON_NULL F("null")
#define COMMA F(",")

// Every 10 minutes.
#define LOGINTERVAL (10L * 60L * 1000L)
#define WIFILOGINTERVAL (30L * 60L * 1000L)


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
int status=0;         //to display status info on webpage
unsigned long lastSampleTimeUnix = 0;

int ntpStatus=0;      //to display status info of NTP calls on webpage
DateTime ntpLastLookupTime = NULL;

//for DHT11 interface
//#define DHT11PIN 8
#define DHT11PIN 2
struct dht11Response_t {
  int status;
  int temperature;
  int humidity;
} ;


// UltraSonic module
#define USONIC_1_TRIG 7
#define USONIC_1_ECHO 6
#define USONIC_2_TRIG 9
#define USONIC_2_ECHO 8
#define USMAX 3000  //??
#define USTIMEOUT_2M   11600  // Timeout for 2m
#define US_TIMEOUT_2_5M 14500  // Timeout for 2.5m

#define WIFI Serial
#define AT_CMD_SEND F("AT+CIPSEND=")
#define AT_CMD_CLOSE F("AT+CIPCLOSE=")
#define AT_CMD_WIFI_STATUS F("AT+CIPSTATUS")
#define AT_CMD_FIRMWARE F("AT+GMR")
#define AT_CMD_DISCONNECT F("AT+CWQAP")
#define AT_CMD_STATION_IP F("AT+CWLIF")
#define AT_CMD_AP_PASSIVE_SCAN F("AT+CWLAP=,,,1,,")
#define AT_CMD_AP_STATUS F("AT+CWJAP_CUR?")
#define AT_CMD_GET_SLEEP_MODE F("AT+SLEEP?")
#define AT_CMD_GET_FREE_RAM F("AT+SYSRAM?")


#define AT_REPLY_OK F("OK")
#define AT_REPLY_SEND_OK F("SEND OK")
#define AT_REPLY_READY F("ready")
#define AT_REPLY_GOT_IP F("WIFI GOT IP")
#define AT_READY_TO_SEND F(">")

//#define AT_READY_TO_SEND F("STATUS:4")

#define HTTPINDEX_START F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<!DOCTYPE html>\r\n<html><body>\r\n<h2>WIFI DATALOGGER</h2><br>Click <a href=\"list.html\">list</a> to see the files that are availble to download.<br><br>\r\n")
#define HTTPINDEX_START_2 F("<h3>API</h3><ul><li><a href=\"api/waterTank\">Water tank levels</a></li><li><a href=\"api/outside\">Outside climate</a></li></ul><br><h3>Status</h3><ul><li><a href=\"wiFiStatus.html\">WiFi Status</a></li></li></ul><br>\r\n")
#define HTTPLIST F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<!DOCTYPE html>\r\n<html><body>\r\n<h2>WIFI DATALOGGER</h2><br>Choose a file:<ul>\r\n")
#define HTTPLIST_2 F("</ul>")
#define HTTPWIFI_STATUS_1 F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<!DOCTYPE html>\r\n<html><body>\r\n<h2>WIFI STATUS</h2><h4>Status</h4><br><samp>\r\n")
#define HTTPWIFI_STATUS_2 F("</samp>\r\n")
#define HTTPWIFI_STATUS_3 F("<br><h4>Firmware Version</h4><samp>\r\n")
#define HTTPWIFI_STATUS_4 F("<br><h4>AP Station Status</h4><samp>\r\n")
#define HTTPWIFI_STATUS_5 F("<br><h4>Sleep Mode</h4><samp>\r\n")
#define HTTPWIFI_STATUS_6 F("<br><h4>ESP-13 Free RAM</h4><samp>\r\n")
#define HTTP404 F("HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<!DOCTYPE html>\r\n<html><body>\r\n<h3>File not found</h3><br><a href=\"index.htm\">Back to index...</a><br></body></html>")
#define HTTPJSON F("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n")
#define HTTPTEXT F("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n")
#define STATUSTITLE F("<br>Status (zero is no error): ")
// Too big for pktbuf buffer
//const char STATUS_TEXT[] PROGMEM = "<br>Status (zero is no error): %d<br>Current time: %04d-%02d-%02d %02d:%02d:%02d<br>NTP Status (zero is no error): %d - last check time: %04d-%02d-%02d %02d:%02d:%02d";
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

  usonicSetup(USONIC_1_TRIG, USONIC_1_ECHO); //set up ultrasonic sensor
  usonicSetup(USONIC_2_TRIG, USONIC_2_ECHO); //set up ultrasonic sensor

  wifiinit();           //send starting settings- find AP, connect and set to station mode only, start server
  setupDHT11(DHT11PIN);           //start temp/humidity sensor

  //getNtpTime();

  pinMode(LED2,OUTPUT);
  digitalWrite(LED2,HIGH);      //turn on LED to show card in use
  if(!SD.begin(10)){            //SD card not responding
    errorflash(1);              //flash error code for card not found
  }

  if (!rtc.begin()) {           // rtc not responding
    errorflash(2);              //flash error code for RTC not found
  }

  //WIFI.print(F("Compile time = "));
  //WIFI.print(F(__DATE__));
  //WIFI.print(F(" "));
  //WIFI.println(F(__TIME__));
  // Use the compile date/time, change to use NTP server one day
  DateTime compileTime = DateTime(F(__DATE__), F(__TIME__));
  DateTime now = rtc.now();
  // The 90*60 is a bit of a half arsed attempt to not set the date during summer DST
  if( compileTime.secondstime() > (now.secondstime() + (90*60))) {
    WIFI.println(F("--- 2"));
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
}

void loop() {
  if(millis()>sampleTime){   //if it's been more than logging interval
    dht11Response_t dht11Response = readDHT11(DHT11PIN);     //fetch data to log
    int distanceA=usonicRead(USONIC_1_TRIG, USONIC_1_ECHO, US_TIMEOUT_2_5M); //distance in cm, time out at 11600us or 2m maximum range
    int distanceB=usonicRead(USONIC_2_TRIG, USONIC_2_ECHO, US_TIMEOUT_2_5M); //distance in cm, time out at 11600us or 2m maximum range

    int light = -1;
    //light=analogRead(LIGHTPIN);     //get light sensor data
    float temperatureA = readThermister(TEMP_A_PIN);
    float temperatureB = readThermister(TEMP_B_PIN);
    float temperatureC = readThermister(TEMP_C_PIN);
    float temperatureD = readThermister(TEMP_D_PIN);
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
    sampleTime+=LOGINTERVAL;        //add interval, so interval is precise
  }
  checkwifi();  

  if(millis()>wifiLogTime){   //if it's been more than logging interval
    logWiFiStatusToCard();
    wifiLogTime += WIFILOGINTERVAL;
  }
}

double readThermister(int pin) {
  int rawADC;
  double temp;
  rawADC = analogRead(pin);
  temp = log(((10240000/rawADC) - 10000));
  temp = 1 / (0.001129148 + (0.000234125 + (0.0000000876741 * temp * temp ))* temp );
  temp = temp - 273.15; // Convert Kelvin to Celcius
  return temp;
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

void logWiFiStatusToCard(){
  unsigned long timestamp;
  DateTime now = rtc.now();                   //capture time
  lastSampleTimeUnix = now.unixtime();
  digitalWrite(LED2,HIGH);                    //turn on LED to show card in use
  delay(200);                                 //a bit of warning that card is being accessed, can be reduced if faster sampling needed
  
  strcpy_PF(pktbuf, F("WIFI.LOG"));
  File fhandle = SD.open(pktbuf, FILE_WRITE);
  if(!fhandle){                     // Cannot open the file, raise an error.
    errorflash(6);
  }

  unsigned long s;
    
  // Timestamp format ISO 8601: 2017-12-26T07:44:19+10
  fhandle.print(F("\r\n==================================================\r\n== Status Time: "));
  sprintf_P(pktbuf, XML_DATE, now.year(), now.month(), now.day());
  fhandle.print(pktbuf);
  fhandle.print(F("T"));
  sprintf_P(pktbuf, XML_TIME, now.hour(),now.minute(),now.second());
  fhandle.println(pktbuf);
  fhandle.print(F("Free Memory: "));
  sprintf_P(pktbuf, XML_INT, freeMemory());
  fhandle.println(pktbuf);

  fhandle.println(F("IP Connection Status"));
  clearPktbuf();

  int len = strlen_PF(AT_REPLY_OK) + 1;
  char replyStr[len];
  strcpy_PF(replyStr,AT_REPLY_OK);

  WIFI.println(AT_CMD_WIFI_STATUS);
  logResponseToFile( fhandle, replyStr);

  fhandle.println(F("AP Status"));
  WIFI.println(AT_CMD_AP_STATUS);
  logResponseToFile( fhandle, replyStr);

  fhandle.println(F("AP Free RAM"));
  WIFI.println(AT_CMD_GET_FREE_RAM);
  logResponseToFile( fhandle, replyStr);

  fhandle.close();                            //close file so data is saved
  digitalWrite(LED2,LOW);                     //turn off LED card to show card closed
}

void logResponseToFile( File fhandle, char *replyStr) {
  unsigned long timeout = 2000 + millis();
  clearPktbuf();
  while(millis()< timeout && !strContains(pktbuf, replyStr)){          //until timeout
    while(WIFI.available()){
      int c = WIFI.read();
      if(c != -1){             //response good
        addtobuffer(pktbuf,PKTSIZE,c);      //add to buffer
        if(c == '\n') {
          fhandle.print(pktbuf);
          clearPktbuf();
        }
      }
    }
  }
  WIFIpurge();              //clear incoming buffer
}

void setupDHT11( int pin){                    //set pin to output, set high for idle state
  pinMode( pin, OUTPUT);
  digitalWrite( pin, HIGH);
}

dht11Response_t readDHT11( int pin){        //returns status 1 on ok, 0 on fail (eg checksum, data not received)
  unsigned int n[83];           //to store bit times
  byte p=0;                     //pointer to current bit
  unsigned long t;              //time
  byte old=0;
  byte newd;
  dht11Response_t response;
  response.status = 0;
  response.temperature = -1;
  response.humidity = -1;
  
  for(int i=0;i<83;i++){n[i]=0;}
  digitalWrite(pin,LOW);   //start signal
  delay(25);
  digitalWrite(pin,HIGH);
  delayMicroseconds(20);
  pinMode(pin,INPUT);
  delayMicroseconds(40);
  t=micros()+10000L;
  while((micros()<t)&&(p<83)){    //read bits
    newd=digitalRead(pin);
    if(newd!=old){
      n[p]=micros();
      p++;
      old=newd;
    }
  }
  pinMode(pin,OUTPUT);      //reset for next cycle
  digitalWrite(pin,HIGH);
  if(p!=83){return response;}           //not a valid datastream
  byte data[5]={0,0,0,0,0};
  for(int i=0;i<40;i++){         //store data in array
    if(n[i*2+3]-n[i*2+2]>50){data[i>>3]=data[i>>3]|(128>>(i&7));}
  }
  byte k=0;     //checksum
  for(int i=0;i<4;i++){k=k+data[i];}
  if((k^data[4])){return response;}      //checksum error
  //DHTtemp=data[2];                //temperature
  //DHThum=data[0];                 //humidity
  response.status = 1;
  response.temperature = data[2];
  response.humidity = data[0];
  return response;                       //data valid
}

void errorflash(int n){           //non-recoverable error, flash code on LED1
  WIFI.print(F("error flash:"));
  WIFI.println(n);
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

void usonicSetup(int trigPin, int echoPin){
  pinMode(echoPin, INPUT);
  pinMode(trigPin, OUTPUT);
  digitalWrite(trigPin, LOW);
}


void dorequest(){
  unsigned long t;                     //timeout
  int p=0;                             //pointer to getreq position
  int f=1;                             //flag to tell if first line or not
  int crcount=0;                       //if we get two CR/LF in a row, request is complete
  clearPktbuf();
  
  pktbuf[0]=0;                         //clear string  
  t=millis()+1000;                     //wait up to a second for data, shouldn't take more than 125ms for 1460 byte MTU
  while((millis()<t)&&(crcount<2)){    //drop out if <CR><LF><CR><LF> seen or timeout
    if(WIFI.available()){
      int d;
      d=WIFI.read();
      if(d>31){                        //if an ASCII character
        crcount=0;                     //clear CR count
        if(f==1){                      //on first line
          if(p < sizeof(pktbuf)) {
            pktbuf[p]=d;               //add to GET buffer, but do not overrun the buffer.
            p++;
          }
        }
      }
      if(d==13){                       //if CR found increase CR count
        crcount++;
        f++;
      }      
    }
  }
  fname[0]=0;                                              //blank
  
  if( strncmp_P(pktbuf, HTTP_HEADER_GET, strlen_P(HTTP_HEADER_GET)) != 0){crcount=0;}   //no 'GET ' at the start, so change flag to cancel
  if(crcount==2){parseFileName(pktbuf);}                                                       //complete request found, extract name of requested file
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
  } else {
    crcount = sendFile(fname);                       //serve entire data file
  }

  if(crcount){serve404();sendStatus();}             //no valid file served => 404 error

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

  /*
  int ntpYear   = (&ntpLastLookupTime == NULL) ? ntpLastLookupTime.year() : 0;
  int ntpMonth  = (&ntpLastLookupTime == NULL) ? ntpLastLookupTime.month() : 0;
  int ntpDay    = (&ntpLastLookupTime == NULL) ? ntpLastLookupTime.day() : 0;
  int ntpHour   = (&ntpLastLookupTime == NULL) ? ntpLastLookupTime.hour() : 0;
  int ntpMinute = (&ntpLastLookupTime == NULL) ? ntpLastLookupTime.minute() : 0;
  int ntpSecond = (&ntpLastLookupTime == NULL) ? ntpLastLookupTime.second() : 0;
  */
  
  DateTime now = rtc.now();

  WIFI.print(AT_CMD_SEND);                           //send data
  WIFI.print(cxn);                                   //to client
  WIFI.print(F(","));        
  sprintf_P(pktbuf, STATUS_TEXT, status, now.year(), now.month(), now.day(), now.hour(),now.minute(),now.second());
// Full status was too big for pktbuf buffer.
//  sprintf_P(pktbuf, STATUS_TEXT, status, now.year(), now.month(), now.day(), now.hour(),now.minute(),now.second(),
//     ntpStatus, ntpYear, ntpMonth, ntpDay, ntpHour, ntpMinute, ntpSecond);
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

  File root = SD.open("/",FILE_READ);
  root.seek(0);

  int sendLength = -1;
  clearPktbuf();

  while (true) {
    File entry =  root.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
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
  logStatusWeb(replyStr);
  WIFIsenddata( HTTPWIFI_STATUS_2, cxn);

  WIFIsenddata( HTTPWIFI_STATUS_4, cxn);
  WIFI.println(AT_CMD_AP_STATUS);
  logStatusWeb(replyStr);
  WIFIsenddata( HTTPWIFI_STATUS_2, cxn);

  WIFIsenddata( HTTPWIFI_STATUS_5, cxn);
  WIFI.println(AT_CMD_GET_SLEEP_MODE);
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
  int distanceA=usonicRead(USONIC_1_TRIG, USONIC_1_ECHO, US_TIMEOUT_2_5M);
  int distanceB=usonicRead(USONIC_2_TRIG, USONIC_2_ECHO, US_TIMEOUT_2_5M);

  sprintf_P(pktbuf, API_WATER_TANK_DATA, distanceA, distanceB);
  wiFiSendStart(cxn, strlen_PF(HTTPJSON)+strlen(pktbuf));
  WIFI.print(HTTPJSON);                  //send HTTP header for csv data type, file content to follow
  WIFI.print(pktbuf);
  wiFiWaitForSendOkReply();
}

void getOutsideClimate(){                      //Returns JSON with the current tempreture, humidity etc.
  dht11Response_t dht11Response = readDHT11(DHT11PIN); //returns status 1 on ok, 0 on fail (eg checksum, data not received)
  int light = -1;
  //int light=analogRead(LIGHTPIN);                //get light sensor data

  sprintf_P(pktbuf, API_OUTSIDE_CLIMATE_DATA, dht11Response.temperature, dht11Response.humidity, light);

  wiFiSendStart(cxn, strlen_PF(HTTPJSON)+strlen(pktbuf));
  WIFI.print(HTTPJSON);                  //send HTTP header for csv data type, file content to follow
  WIFI.print(pktbuf);
  wiFiWaitForSendOkReply();
}


void wifiinit(){
  WIFIcmd(F("AT+RST"), AT_REPLY_READY, 5000);                    //reset
  WIFIcmd(F("AT+CWQAP"), AT_REPLY_OK, 5000);                     //exit any AP's

  WIFIcmd(WIFI_SSID, AT_REPLY_GOT_IP, 10000);                    //join AP
  WIFIcmd(F("ATE0"),AT_REPLY_OK,1000);                                     //turn echo off
  if( strlen_PF(SET_IP) > 0) {
    WIFIcmd(SET_IP,AT_REPLY_OK,5000);
  }
  WIFIcmd(F("AT+CWMODE=1"),AT_REPLY_OK,2000);                              //station mode only
//  WIFIcmd(F("AT+CWMODE=3"),AT_REPLY_OK,2000);                              //station mode only
  WIFIcmd(F("AT+CWHOSTNAME=\"donkey\""),AT_REPLY_OK,2000);                     //set the hostname
//  WIFIcmd(F("AT+MDNS=1,\"donkey\",\"iot\",80"),AT_REPLY_OK,2000);                     //set the hostname

  WIFIcmd(F("AT+CIPMUX=1"),AT_REPLY_OK,2000);                              //MUX on (needed for server)
  WIFIcmd(F("AT+CIPSERVER=1,80"),AT_REPLY_OK,2000);                        //server on
  WIFIcmd(F("AT+CIPSTO=10"),AT_REPLY_OK,2000);                             //disconnect after x time if no data
}

void checkwifi(){
  while(WIFI.available()){                    //check if any data from WIFI
    int d;
    d=WIFI.read();
    if((d>47)&&(d<58)&&(cxn<0)){cxn=d-48;}    //connection number, could come from IPD or CONNECT
    if(d==':'){                  //: means end of IPD data, content follows. LED on while busy
      digitalWrite(LED2,HIGH);
      dorequest();
      digitalWrite(LED2,LOW);
    }
  }
}

int WIFIcmd(const __FlashStringHelper *c, const __FlashStringHelper *reply, long timeout){   //command c (nocrlf needed), returns true if response r received, otherwise times out
  WIFI.println(c);
  return wiFiWaitForReply(reply, true, timeout);
}

int WIFIcmd(const char* c, const __FlashStringHelper *reply, long timeout){   //command c (nocrlf needed), returns true if response r received, otherwise times out
  WIFI.println(c);
  return wiFiWaitForReply(reply, true, timeout);
}

void WIFIpurge(){                         //empty serial buffer
  while(WIFI.available()){
    WIFI.read();
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
  return wiFiWaitForReply(AT_REPLY_OK, true, 5000);
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

int addtobuffer(char buf[], int bufsize, char str[]){      //add str to end of buf, limited by bufsize
  int p=0;
  int k=strlen(buf);
  while((k+p<bufsize-1)&&str[p]){                          //while there's room
    buf[p+k]=str[p];                                       //add character
    p++;
  }
  buf[p+k]=0;                                              //terminate array
  return p;                                                //number of characters added
}

int addtobuffer(char buf[], int bufsize, char str){       //add char to end of buf, limited by bufsize
  int k=strlen(buf);
  if(k<bufsize-1){                                        //if there's room for one more
    buf[k]=str;                                           //add it
    buf[k+1]=0;
    return 1;                                             //1 character added
  }
  return 0;
}

/*
int addtobuffer(char buf[], int bufsize, long n){      //add n as string to end of buf, limited by bufsize, longs will work with ints, uns ints, longs
  char str[15]="";                                     //temporary buffer for number
  if(n<0){str[0]='-';str[1]=0;n=-n;}                   //leading negative sign
  int lzero=0;
  long d=1000000000L;                                  //decade divider
  long j;
  while(d>0){                                          //for all digits
    j=(n/d)%10;                                        //find digit
    if(j){lzero=1;}                                    //non zero character found
    if(lzero||(d==1)){                                 //always show units and any after non-zero
      str[strlen(str)+1]=0;
      str[strlen(str)]=j+'0';                          //add a digit
    }
    d=d/10;                                            //next one
  }
  int p=0;
  int k=strlen(buf);
  while((k+p<bufsize-1)&&str[p]){
    buf[p+k]=str[p];
    p++;
  }
  buf[p+k]=0;                                           //terminate array
  return p;                                             //number of characters added  
}
*/
long usonicRead(int trigPin, int echoPin, long utimeout){ //utimeout is maximum time to wait for return in us
  long b;

  if(digitalRead(echoPin)==HIGH){return -1;} //if echo line is still low from last result, return 0;

  digitalWrite(trigPin, HIGH); //send trigger pulse
  delay(1);
  digitalWrite(trigPin, LOW);

  long utimer=micros();

  while((digitalRead(echoPin)==LOW)&&((micros()-utimer)<1000)){} //wait for pin state to change- return starts after 460us typically or timeout (eg if not connected)
  utimer=micros();

  while((digitalRead(echoPin)==HIGH)&&((micros()-utimer)<utimeout)){} //wait for pin state to change
  b=micros()-utimer;
 
  if(b==0){return -1;}
//  if(b==0){b=utimeout;}

  return b/58;
}

void displayFreeMemory(const __FlashStringHelper *msg) {
  Serial.print(F("freeMemory("));
  Serial.print(msg);
  Serial.print(F(")="));
  Serial.println(freeMemory()); 
}

void clearPktbuf(){
  memset(pktbuf, 0, sizeof(pktbuf));
}

void dirList(const __FlashStringHelper *msg) {
  Serial.print(F("------"));
  Serial.println(msg);
  displayFreeMemory(msg);

  File root = SD.open("/",FILE_READ);
  Serial.print(F("Root: "));
  Serial.println((root));
  root.seek(0);
  while (true) {
     File entry =  root.openNextFile();
     Serial.print(F("Entry: "));
     Serial.println((entry));
     if (! entry) {
       // no more files
       break;
     }
     Serial.print(F("-> "));
     Serial.println(entry.name());
     entry.close();
  }
  root.close();
}



int WIFIcmd2(const __FlashStringHelper *c,char* r,long tmout){   //command c (nocrlf needed), returns true if response r received, otherwise times out
  long t;
  WIFI.println(c);
//  Serial.println("");
  const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
  byte packetBuffer[ NTP_PACKET_SIZE+1]; 
  memset(packetBuffer, 0, NTP_PACKET_SIZE+1);

  int cnt=0;
  t=millis();
  while(millis()-t<tmout){          //until timeout
    while(WIFI.available()){
//      int d;
      
//      d=WIFI.read();
//  Serial.print(d);
//      if(cnt < NTP_PACKET_SIZE) packetBuffer[cnt++] = d;
//    Serial.print(cnt);
//    Serial.print(": ");
//    Serial.print( d);

      
      if(WIFI.find(r)){return 1;}   //response good
    }    
  }

//  String myString = String((char *)packetBuffer);
//  Serial.println(myString);
return 0;       //response not found
}

void getNtpTime() {
  ntpStatus=0;     //to display status info of NTP calls on webpage
  ntpLastLookupTime = rtc.now();
  char ipAddress[SBUFSIZE]="";   //for GET request

  int serverCount = sizeof NTP_SERVERS / sizeof NTP_SERVERS[0];
  for( int i = 0; i < serverCount; i++) {
    strcpy_P(ipAddress, (char*)pgm_read_word(&(NTP_SERVERS[i][1]))); 
    callNtpTimeServer(ipAddress);
    if(ntpStatus == 0) {
      return;
    }
  }
}

void callNtpTimeServer(char* ipAddress) {
  sprintf_P(pktbuf, NTP_CONNECT_CMD, ipAddress);
  int resp = WIFIcmd( pktbuf, AT_REPLY_OK, 10000);                                               //reset
//  int resp = WIFIcmd2(F("AT+CIPSTART=0,\"UDP\",\"129.250.35.250\",123"),ok,10000);                                               //reset

  if(!resp) {
    ntpStatus=1;
    return;
  }
  int cxn=0;

  const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

  memset(pktbuf, 0, NTP_PACKET_SIZE+1);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  pktbuf[0] = 0b11100011;   // LI, Version, Mode
  pktbuf[1] = 0;     // Stratum, or type of clock
  pktbuf[2] = 6;     // Polling Interval
  pktbuf[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  pktbuf[12]  = 49;
  pktbuf[13]  = 0x4E;
  pktbuf[14]  = 49;
  pktbuf[15]  = 52;  
  
  WIFI.print(F("AT+CIPSEND="));           //send data
  WIFI.print(cxn);                     //to client
  WIFI.print(F(","));              
  WIFI.println(NTP_PACKET_SIZE);                //data has length
  delay(50);
  WIFI.find(">");
  WIFI.write(pktbuf, NTP_PACKET_SIZE);                          //data
  delay(100);
  WIFI.find("SEND OK");

  memset(pktbuf, 0, NTP_PACKET_SIZE+1);
  int tcnt = 0;
  int cnt = 0;
  bool readData = false;

  // Timeout in 5 seconds
  int tmout = 5000;
  int t=millis();
  while(millis()-t<tmout && cnt < NTP_PACKET_SIZE){          //until timeout
    while(WIFI.available()) {
      int d=WIFI.read();
      tcnt++;
      if(readData) {
         pktbuf[cnt] = d;
         cnt++;
      } else {
        if(d == ':') {
          //Serial.println("data found");
          readData = true;
        }
      }
    }
  delay(500);
  }
 
  Serial.print(F("bytes read "));
  Serial.println( cnt);
  Serial.print(F("tbytes read "));
  Serial.println(tcnt);
  Serial.print(pktbuf[40]);
  Serial.print(F("  "));
  Serial.print(pktbuf[41]);
  Serial.print(F("  "));
  Serial.print(pktbuf[42]);
  Serial.print(F("  "));
  Serial.println(pktbuf[43]);
//  Serial.println( (char *) pktbuf);
//  String myString = String((char *)pktbuf);
//  Serial.println(myString);
//  Serial.println(F("End"));
  WIFIcmd(F("AT+CIPCLOSE=0"), AT_REPLY_OK, 5000);                                               //reset

  if(cnt < NTP_PACKET_SIZE) {
    ntpStatus=2;
    return;
  }
 
  unsigned long highWord = word(pktbuf[40], pktbuf[41]);
  unsigned long lowWord = word(pktbuf[42], pktbuf[43]);
  // combine the four bytes (two words) into a long integer
  // this is NTP time (seconds since Jan 1 1900):
  unsigned long secsSince1900 = highWord << 16 | lowWord;
  Serial.print(F("Seconds since Jan 1 1900 = "));
  Serial.println(secsSince1900);

  DateTime now = rtc.now();                   //capture time

  // now convert NTP time into everyday time:
  Serial.print("Unix time = ");
  // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
  const unsigned long seventyYears = 2208988800UL;

  const unsigned long tenHours = 36000UL;

  // subtract seventy years:
  unsigned long epoch = secsSince1900 - seventyYears;
  // print Unix time:
  Serial.println(epoch);

  Serial.print(F("cloc             "));
  sprintf_P(pktbuf, TIME_FORMAT, now.year(), now.month(), now.day(), now.hour(),now.minute(),now.second());
  //Serial.println(now.unixtime());
  Serial.println(pktbuf);
  DateTime compileTime = DateTime(epoch);
  Serial.print(F("ep unix          "));
  sprintf_P(pktbuf, TIME_FORMAT, compileTime.year(), compileTime.month(), compileTime.day(), compileTime.hour(),compileTime.minute(),compileTime.second());
     Serial.println(pktbuf);
     //Serial.println(compileTime.unixtime());

     compileTime = DateTime(secsSince1900);
     Serial.print(F("since 1900 unix  "));
     sprintf_P(pktbuf, TIME_FORMAT, compileTime.year(), compileTime.month(), compileTime.day(), compileTime.hour(),compileTime.minute(),compileTime.second());
     Serial.println(pktbuf);
     Serial.print(F("DateTime size = "));
     Serial.println(sizeof(compileTime));

     //Serial.println(compileTime.unixtime());

     epoch+=tenHours;
     compileTime = DateTime(epoch);
    
     Serial.print(F("ep unix adjusted "));
     sprintf_P(pktbuf, TIME_FORMAT, compileTime.year(), compileTime.month(), compileTime.day(), compileTime.hour(),compileTime.minute(),compileTime.second());
     Serial.println(pktbuf);
    int year = compileTime.year();

  if(year < 2017 || year > 2050) {
    ntpStatus=3;
    return;
  }

  Serial.print(F("updating time"));
  rtc.adjust(compileTime);
  ntpStatus=0;
}

// searches for the string sfind in the string str
// returns 1 if string found
// returns 0 if string not found
int strContains(char *str, char *sfind) {
    int found = 0;
    int index = 0;
    int len;

    len = strlen(str);
    
    if (strlen(sfind) > len) {
        return 0;
    }
    while (index < len) {
        if (str[index] == sfind[found]) {
            found++;
            if (strlen(sfind) == found) {
                return 1;
            }
        }
        else {
            found = 0;
        }
        index++;
    }

    return 0;
}
