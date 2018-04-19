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

const char FILENAME[] PROGMEM = "%04d%02d%02d.JS";
const char XML_DATE[] PROGMEM = "%04d-%02d-%02d";
const char XML_TIME[] PROGMEM = "%02d:%02d:%02d";

const char HTTP_HEADER_GET[] PROGMEM = "GET ";
const char URL_FILE_INDEX_1[] PROGMEM = "index.htm";
const char URL_FILE_INDEX_2[] PROGMEM = "index.html";
const char URL_FILE_LIST[] PROGMEM = "list.html";

const char INDEX_LINE[] PROGMEM = "<a href=\"%s\">Download %s</a><br>\n";


#define JSON_START F("[\r\n")
#define JSON_END F("\r\n]")
#define JSON_NULL F("null")

#define LOGINTERVAL 60
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
long tmout;   //keeps track of logging interval
int temp,hum,light;   //variables to log
int status=0;     //to display status info on webpage

//for DHT11 interface
//#define DHT11PIN 8
#define DHT11PIN 2
byte DHTtemp,DHThum;
int DHTstatus=0;

// UltraSonic module
#define USONIC_1_TRIG 7
#define USONIC_1_ECHO 6
#define USONIC_2_TRIG 9
#define USONIC_2_ECHO 8
#define USMAX 3000  //??
#define USTIMEOUT_2M   11600  // Timeout for 2m
#define US_TIMEOUT_2_5M 14500  // Timeout for 2.5m
#define US_TIMEOUT_4M   23200  // Timeout for 2m

#define WIFI Serial
#define AT_CMD_SEND F("AT+CIPSEND=")
#define AT_CMD_CLOSE F("AT+CIPCLOSE=")

#define HTTPINDEX_START F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<!DOCTYPE html>\r\n<html><body>\r\n<h2>WIFI DATALOGGER</h2><br>Click <a href=\"list.html\">list</a> to see the files that are availble to download.<br><br>\r\n")
#define HTTPLIST F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<!DOCTYPE html>\r\n<html><body>\r\n<h2>WIFI DATALOGGER</h2><br>Choose a file:<br>\r\n")
#define HTTP404 F("HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<!DOCTYPE html>\r\n<html><body>\r\n<h3>File not found</h3><br><a href=\"index.htm\">Back to index...</a><br></body></html>")
#define HTTPJSON F("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n")
#define STATUSTITLE F("<br>Status (zero is no error): ")
const char STATUS_TEXT[] PROGMEM = "<br>Status (zero is no error): %d<br>Current time: %04d-%02d-%02d %02d:%02d:%02d";
#define HTMLEND F("</body></html>")

const char LOG_MSG_TEMP_VALUE[] PROGMEM = "Analog temp%d = %d, ";

// sketch needs ~450 bytes local space to not crash
//#define PKTSIZE 129
//#define PKTSIZE 257
#define PKTSIZE 161
#define SBUFSIZE 20
char getreq[SBUFSIZE]="";   //for GET request
char fname[SBUFSIZE]="";    //filename
char currentFilename[SBUFSIZE]="";    //filename
int cxn=-1;                 //connection number
char pktbuf[PKTSIZE]="";    //to consolidate data for sending
const char ok[]= "OK\r\n";         //OK response is very common
const char SEND_OK[] = "SEND OK";         //OK response is very common

const char SEND_DATA[] PROGMEM = "AT+CIPSEND=";

const char COMMA[] = ",";


void setup() {
  WIFI.begin(115200);   //start serial port for WIFI

  usonicSetup(USONIC_1_TRIG, USONIC_1_ECHO); //set up ultrasonic sensor

  wifiinit();           //send starting settings- find AP, connect and set to station mode only, start server
  DHTsetup();           //start temp/humidity sensor

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
//  if (! rtc.initialized ()) {
    DateTime now = rtc.now();
    DateTime compileTime = DateTime(F(__DATE__), F(__TIME__));
//      rtc.adjust(compileTime);

    // The 70*60 is a bit of a half arsed attempt to not set the date during summer DST
    if( compileTime.secondstime() > (now.secondstime() + (70*60))) {
      WIFI.println(F("--- 2"));
      // following line sets the RTC to the date & time this sketch was compiled
      rtc.adjust(compileTime);
    }
//  }

  if (!rtc.isrunning()) {       //rtc not running- use ds1307 example to load current time
    errorflash(3);              //flash error code for RTC not running
  }
  
  File fhandle = openDataFile();
  if( ! fhandle) {
    errorflash(4);              //flash error code for RTC not running
  }
  fhandle.close(); //then close the file
//Serial.print(F("freeMemory()=")); Serial.println(freeMemory()); 
  //dirList(F("endSetup"));
}

void loop() {
//  if(millis()>tmout+LOGINTERVAL*1000L){   //if it's been more than logging interval
  if(millis()>tmout+LOGINTERVAL*100L){   //if it's been more than logging interval
    getvalue();                           //fetch data to log
//    tmout=tmout+LOGINTERVAL*1000L;        //add interval, so interval is precise
    int distanceA=usonicRead(USONIC_1_TRIG, USONIC_1_ECHO, US_TIMEOUT_2_5M); //distance in cm, time out at 11600us or 2m maximum range

    //light=analogRead(LIGHTPIN);     //get light sensor data
    float temperatureA = readThermister(TEMP_A_PIN);
    float temperatureB = readThermister(TEMP_B_PIN);
    float temperatureC = readThermister(TEMP_C_PIN);
    float temperatureD = readThermister(TEMP_D_PIN);
//    Serial.print(F("DistanceA=")); Serial.println(distanceA);
//    Serial.print(F("tempA=")); Serial.println(temperatureA);
//    Serial.print(F("tempB=")); Serial.println(temperatureB);
//    Serial.print(F("tempC=")); Serial.println(temperatureC);
//    Serial.print(F("tempD=")); Serial.println(temperatureD);

    logtocard(temperatureA, temperatureB, temperatureC, temperatureD, distanceA, -1);          //log it

//    Serial.print(F("freeMemory()=")); Serial.println(freeMemory()); 
    tmout=tmout+LOGINTERVAL*1000L;        //add interval, so interval is precise
  }
  checkwifi();  
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

void getvalue(){                  //put subroutine for getting data to log here
  DHTtemp=0;                      //zero values to detect errors
  DHThum=0;
  DHTstatus=DHT11();              //DHTstatus=0 if error
  temp=DHTtemp;                   //DHT11() loads temp into DHTtemp      
  hum=DHThum;                     //DHT11() loads humidity into DHThum
}


File openDataFile(){      //put the column headers you would like here, if you don't want headers, comment out the line below
  DateTime now = rtc.now();                   //capture time

  sprintf_P(fname, FILENAME, now.year(), now.month(), now.day());
  //Serial.print(F("o-- 3 "));
  //Serial.println(fname);
  if( strlen(currentFilename) == 0) {
    strcpy(currentFilename,fname);
  }
  if( strcmp(fname,currentFilename) != 0) {
    File currenFile = SD.open(currentFilename, FILE_WRITE);
    currenFile.print(JSON_END);
    currenFile.close();
    strcpy(currentFilename,fname);
  }

//    Serial.print(F("o-- 6:"));
//    Serial.print(filename);
//    Serial.println(F(":"));
  return SD.open(fname, FILE_WRITE);
//    Serial.print(F("o-- 7: "));
//    Serial.println((fhandle));
}

void logtocard(float temperatureA, float temperatureB, float temperatureC, float temperatureD, int distanceA, int distanceB){
  unsigned long timestamp;
  DateTime now = rtc.now();                   //capture time
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
  if(DHTstatus){fhandle.print(temp);}else{fhandle.print(JSON_NULL);}         //put data if valid otherwise blank (will be blank cell in Excel)
  fhandle.print(F(", \"humidity\": "));
  if(DHTstatus){fhandle.print(hum);}else{fhandle.print(JSON_NULL);}          //put data if valid otherwise blank
  fhandle.print(F(", \"lightLevel\": "));
  fhandle.print(light);                       //put data (can't validate analog input)
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

void DHTsetup(){                    //set pin to output, set high for idle state
  pinMode(DHT11PIN,OUTPUT);
  digitalWrite(DHT11PIN,HIGH);
}

int DHT11(){                    //returns 1 on ok, 0 on fail (eg checksum, data not received)
  unsigned int n[83];           //to store bit times
  byte p=0;                     //pointer to current bit
  unsigned long t;              //time
  byte old=0;
  byte newd;

  // Set the values 
  DHTtemp=-1;
  DHThum=-1;
  
  for(int i=0;i<83;i++){n[i]=0;}
  digitalWrite(DHT11PIN,LOW);   //start signal
  delay(25);
  digitalWrite(DHT11PIN,HIGH);
  delayMicroseconds(20);
  pinMode(DHT11PIN,INPUT);
  delayMicroseconds(40);
  t=micros()+10000L;
  while((micros()<t)&&(p<83)){    //read bits
    newd=digitalRead(DHT11PIN);
    if(newd!=old){
      n[p]=micros();
      p++;
      old=newd;
    }
  }
  pinMode(DHT11PIN,OUTPUT);      //reset for next cycle
  digitalWrite(DHT11PIN,HIGH);
  if(p!=83){return 0;}           //not a valid datastream
  byte data[5]={0,0,0,0,0};
  for(int i=0;i<40;i++){         //store data in array
    if(n[i*2+3]-n[i*2+2]>50){data[i>>3]=data[i>>3]|(128>>(i&7));}
  }
  byte k=0;     //checksum
  for(int i=0;i<4;i++){k=k+data[i];}
  if((k^data[4])){return 0;}      //checksum error
  DHTtemp=data[2];                //temperature
  DHThum=data[0];                 //humidity
  return 1;                       //data valid
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
  long t;                              //timeout
  int p=0;                             //pointer to getreq position
  int f=1;                             //flag to tell if first line or not
  int crcount=0;                       //if we get two CR/LF in a row, request is complete
  getreq[0]=0;                         //clear string  
  t=millis()+1000;                     //wait up to a second for data, shouldn't take more than 125ms for 1460 byte MTU
  while((millis()<t)&&(crcount<2)){    //drop out if <CR><LF><CR><LF> seen or timeout
    if(WIFI.available()){
      int d;
      d=WIFI.read();
      if(d>31){                        //if an ASCII character
        crcount=0;                     //clear CR count
        if(f==1){                      //on first line
          getreq[p]=d;                 //add to GET buffer
          p++;
          getreq[p]=0;
        }
      }
      if(d==13){                       //if CR found increase CR count
        crcount++;
        f++;
      }      
    }
  }
  fname[0]=0;                                              //blank
  
  if( strncmp_P(getreq, HTTP_HEADER_GET, strlen_P(HTTP_HEADER_GET)) != 0){crcount=0;}   //no 'GET ' at the start, so change flag to cancel
  if(crcount==2){parseFileName();}                                                       //complete request found, extract name of requested file
  if(fname[0]==0 || strcmp_P(fname,URL_FILE_INDEX_1) == 0 || strcmp_P(fname,URL_FILE_INDEX_2) == 0){
    servepage();
    sendstatus();
    crcount=0;                                      //serve index page, reset crcount on fileserve
  } else if(strcmp_P(fname, URL_FILE_LIST) == 0){
    listpage();
    sendstatus();
    crcount=0;                                      //serve index page, reset crcount on fileserve
  } else {
    crcount = sendcsv(fname);                                 //serve entire data file
  }
  if(crcount){serve404();sendstatus();}                                                    //no valid file served => 404 error
  WIFI.print(AT_CMD_CLOSE);                                                           //close
  WIFI.print(cxn);  
  WIFIcmd(F(""),ok,2000);                                                                     //disconnect
  cxn=-1;                                                                                  //clear for next connection
}


void parseFileName(){
  fname[0]=0;                                              //blank
  int p=5;                                                 //start after 'GET /'
  int t=0;                                                 //use ? to separate fields, ' ' to end
  while((getreq[p]!=' ')&&(getreq[p])&&(getreq[p]!='?')){  //terminate on space or end of string or ?
    fname[strlen(fname)+1]=0;                              //add to fname
    fname[strlen(fname)]=getreq[p];
    p++;
    if(p>SBUFSIZE-2){p=SBUFSIZE-2;}                        //check bounds
  }
}


void sendstatus(){                                   //to show logger status
  WIFI.print(AT_CMD_SEND);                      //send data
  WIFI.print(cxn);                                   //to client
  WIFI.print(F(","));        

  DateTime now = rtc.now();
  sprintf_P(pktbuf, STATUS_TEXT, status, now.year(), now.month(), now.day(), now.hour(),now.minute(),now.second());
  WIFI.println(strlen(pktbuf) + strlen_PF(HTMLEND));            //data has length, needs to be same as string below, plus 1 for status
  delay(50);
  WIFI.print(pktbuf);
  WIFI.print(HTMLEND);
  delay(250);
  WIFIpurge();
}

/*
void sendstatus(){                                   //to show logger status
  WIFI.print(AT_CMD_SEND);                      //send data
  WIFI.print(cxn);                                   //to client
  WIFI.print(F(","));              
  WIFI.println(strlen_PF(STATUSTITLE) + 1 + strlen_PF(HTMLEND));            //data has length, needs to be same as string below, plus 1 for status
  delay(50);
  WIFI.print(STATUSTITLE);
  WIFI.write((status%10)+'0');                       //exactly one digit
  WIFI.print(HTMLEND);
  delay(250);
  WIFIpurge();
}
*/

void servepage(){                                     //for serving a page of data
  WIFI.print(AT_CMD_SEND);                            //send data
  WIFI.print(cxn);                                    //to client
  WIFI.print(F(","));              
  WIFI.println(strlen_PF(HTTPINDEX_START));           //data has length, needs to be same as string below
  delay(50);
  WIFI.print(HTTPINDEX_START);
  delay(250);
  WIFIpurge();
}


void listpage(){                                     //for serving a page of data
  WIFI.print(AT_CMD_SEND);                            //send data
  WIFI.print(cxn);                                    //to client
  WIFI.print(F(","));              
  WIFI.println(strlen_PF(HTTPLIST));                    //data has length, needs to be same as string below
  delay(50);
  WIFI.print(HTTPLIST);
  delay(250);
  WIFIpurge();

  File root = SD.open("/",FILE_READ);
  root.seek(0);
  while (true) {
     File entry =  root.openNextFile();
     if (! entry) {
       // no more files
       break;
     }
     if (! entry.isDirectory()) {
       // files have sizes, directories do not
       //<a href=\"DATA.CSV\">Download DATA.CSV</a><br>
       sprintf_P(pktbuf, INDEX_LINE, entry.name(), entry.name());
       WIFIsenddata(pktbuf,cxn);
     }
     entry.close();
   }
   root.close();
}

void serve404(){                                //for serving a page of data
  WIFI.print(F("AT+CIPSEND="));                 //send data
  WIFI.print(cxn);                              //to client
  WIFI.print(F(","));              
  WIFI.println(strlen_PF(HTTP404));                //data has length, needs to be same as string below
  delay(50);
  WIFI.print(HTTP404);
  delay(250);
  WIFIpurge();
}

int sendcsv(char *filename){             //for providing a csv document to download
  File dataFile = SD.open(filename);     // Open the file and see if it exists.
  if (! dataFile) {
    return 1;
  }
  
  WIFI.print(F("AT+CIPSEND="));          //send data
  WIFI.print(cxn);                       //to client
  WIFI.print(F(","));              
  WIFI.println(strlen_PF(HTTPJSON));        //data has length, needs to be same as string below
  delay(50);
  WIFI.print(HTTPJSON);               //send HTTP header for csv data type, file content to follow
  delay(250);

  bool appendJsonTail = true;
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


void wifiinit(){
  WIFIcmd(F("AT+RST"),"ready\r\n",5000);                                               //reset
  WIFIcmd(F("AT+CWQAP"),ok,5000);                                                      //exit any AP's

  WIFIcmd(WIFI_SSID,"WIFI GOT IP\r\n",10000);  //join AP
  WIFIcmd(F("ATE0"),ok,1000);                                                          //turn echo off
  if( strlen_PF(SET_IP) > 0) {
    WIFIcmd(SET_IP,ok,5000);
  }
  WIFIcmd(F("AT+CWMODE=1"),ok,2000);                                                   //station mode only
  WIFIcmd(F("AT+CIPMUX=1"),ok,2000);                                                   //MUX on (needed for server)
  WIFIcmd(F("AT+CIPSERVER=1,80"),ok,2000);                                             //server on
  WIFIcmd(F("AT+CIPSTO=10"),ok,2000);                                                   //disconnect after x time if no data
}

void checkwifi(){
  while(WIFI.available()){                    //check if any data from WIFI
    int d;
    d=WIFI.read();
    if((d>47)&&(d<58)&&(cxn<0)){cxn=d-48;}    //connection number, could come from IPD or CONNECT
    if(d==':'){digitalWrite(LED2,HIGH);dorequest();digitalWrite(LED2,LOW);}                  //: means end of IPD data, content follows. LED on while busy
  }
}

int WIFIcmd(const __FlashStringHelper *c,char* r,long tmout){   //command c (nocrlf needed), returns true if response r received, otherwise times out
  long t;
  WIFI.println(c);
  t=millis();
  while(millis()-t<tmout){          //until timeout
    while(WIFI.available()){
      if(WIFI.find(r)){return 1;}   //response good
    }    
  }
  return 0;       //response not found
}

void WIFIpurge(){                         //empty serial buffer
  while(WIFI.available()){WIFI.read();}
}

void WIFIsenddata(char* d, int client){   //send data to client
  WIFI.print(F("AT+CIPSEND="));           //send data
  WIFI.print(client);                     //to client
  WIFI.print(F(","));              
  WIFI.println(strlen(d));                //data has length
  delay(50);
  WIFI.find(">");
  WIFI.print(d);                          //data
  delay(100);
  WIFI.find("SEND OK");
  WIFIpurge();                            //clear incoming buffer
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

