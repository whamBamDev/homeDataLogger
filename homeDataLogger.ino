//outputs CSV data that can be opened by Excel, time is stored as serial number accurate to ~1second
//WIFI webserver to dynamically download data

#include <avr/pgmspace.h>

//Needed for SD card
//Uses hardware SPI on D10,D11,D12,D13
//LOGINTERVAL is in seconds- takes about 1 second to do a log
#include <SPI.h>
#include <SD.h>

const int chipSelect = 10;
File fhandle;
//#define FILENAME "DATA_%02d_%04d.CSV"
#define FILENAME "D%02d_%04d.js"
//#define JSON_START F("[\r\n")
//#define JSON_END F("\r\n]")
//const char JSON_START[] PROGMEM = "[\r\n";
//const char JSON_END[] PROGMEM = "\r\n]";
const char JSON_START[] = "[\r\n";
const char JSON_END[] = "\r\n]";


#define LOGINTERVAL 60
#define LIGHTPIN A0

#define TEMP_A_PIN A0
#define TEMP_B_PIN A1
#define TEMP_C_PIN A2
#define TEMP_D_PIN A3

//Needed for RTC
//Uses SCL and SDA aka A4 and A5
#include <Wire.h>
#include "RTClib.h"

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

//for WIFI, including SSID name and password and most HTML pages
#include "WifiSettings.h"

#define WIFI Serial
#define HTTPINDEX "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<h2>WIFI DATALOGGER</h2><br>Choose a link:<br><a href=\"DATA.CSV\">Download DATA.CSV</a><br><a href=\"SPARSE.CSV\">Sparse Data file</a><br><a href=\"RECENT.CSV\">Most recent data</a><br>"
#define HTTP404 "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<h3>File not found</h3><br><a href=\"index.htm\">Back to index...</a><br>"
#define HTTPCSV "HTTP/1.1 200 OK\r\nContent-Type: text/csv\r\nConnection: close\r\n\r\n"
#define STATUSTITLE "<br>Status (zero is no error):"



#define DEBUG_CONSOLE true
//#define DEBUG_CONSOLE false
#include "debug.h"

const char LOG_MSG_STARTUP[] PROGMEM = "Setup Starting %d";
const char LOG_MSG_TEMP_VALUE[] PROGMEM = "Analog temp%d = %d, ";

Debug logger = Debug("dataLogger.log", DEBUG_CONSOLE);

// sketch needs ~450 bytes local space to not crash
#define PKTSIZE 257
#define SBUFSIZE 30
char getreq[SBUFSIZE]="";   //for GET request
char fname[SBUFSIZE]="";    //filename
char currentFilename[SBUFSIZE]="";    //filename
int cxn=-1;                 //connection number
char pktbuf[PKTSIZE]="";    //to consolidate data for sending
const char ok[] PROGMEM = "OK\r\n";         //OK response is very common

const char SEND_OK[] PROGMEM = "SEND OK";         //OK response is very common

const char SEND_DATA[] PROGMEM = "AT+CIPSEND=";

const char COMMA[] = ",";

void setup() {
  WIFI.begin(115200);   //start serial port for WIFI

  logger.printfln(LOG_MSG_STARTUP,0);

  usonicSetup(USONIC_1_TRIG, USONIC_1_ECHO); //set up ultrasonic sensor

  wifiinit();           //send starting settings- find AP, connect and set to station mode only, start server
  DHTsetup();           //start temp/humidity sensor
  logger.printfln(LOG_MSG_STARTUP,2);
  pinMode(LED2,OUTPUT);
  digitalWrite(LED2,HIGH);      //turn on LED to show card in use
  if(!SD.begin(chipSelect)){    //SD card not responding
    logger.printfln("Setup Starting - no sd");
    errorflash(1);              //flash error code for card not found
  }

  logger.printfln(LOG_MSG_STARTUP,3);
  if (!rtc.begin()) {           // rtc not responding
    errorflash(2);              //flash error code for RTC not found
  }

    // Use the compile date/time, change to use 
//  if (! rtc.initialized()) {
    logger.printfln(F("RTC check time"));
    DateTime now = rtc.now();
    DateTime compileTime = DateTime(F(__DATE__), F(__TIME__));
//      rtc.adjust(compileTime);

    logger.printfln(F("compile time = %i, now time = %i"), compileTime.secondstime(), now.secondstime());
    if( compileTime.secondstime() > now.secondstime()) {
      // following line sets the RTC to the date & time this sketch was compiled
      logger.printfln(F("RTC setting time to %d"),compileTime.secondstime());
      rtc.adjust(compileTime);
    }
//  }

  if (!rtc.isrunning()) {       //rtc not running- use ds1307 example to load current time
    errorflash(3);              //flash error code for RTC not running
  }

  logger.printfln(LOG_MSG_STARTUP,5);
  fhandle = openDataFile();
  if(!fhandle){                 //if file able to be opened
  logger.printfln(LOG_MSG_STARTUP,7);
    errorflash(4);              //flash error code for file not opened
  }

  fhandle.close();              //close file so data is saved

  logger.printfln(LOG_MSG_STARTUP,1000);
}

void loop() {
//  if(millis()>tmout+LOGINTERVAL*1000L){   //if it's been more than logging interval
  if(millis()>tmout+LOGINTERVAL*100L){   //if it's been more than logging interval
    logger.printfln(F("Sampling "));
    getvalue();                           //fetch data to log
//    tmout=tmout+LOGINTERVAL*1000L;        //add interval, so interval is precise
    logger.printfln(F("Temp complete, distance check"));
    int distanceA=usonicRead(USONIC_1_TRIG, USONIC_1_ECHO, US_TIMEOUT_2_5M); //distance in cm, time out at 11600us or 2m maximum range
    logger.printfln(F("Distance = %d"), distanceA);

    //light=analogRead(LIGHTPIN);     //get light sensor data
    float temperatureA = readThermister(TEMP_A_PIN);
    float temperatureB = readThermister(TEMP_B_PIN);
    float temperatureC = readThermister(TEMP_C_PIN);
    float temperatureD = readThermister(TEMP_D_PIN);
    logger.printf(LOG_MSG_TEMP_VALUE,1,(100*temperatureA));
    logger.printf(LOG_MSG_TEMP_VALUE,2,temperatureB);
    logger.printf(LOG_MSG_TEMP_VALUE,3,temperatureC);
    logger.printfln(LOG_MSG_TEMP_VALUE,4,temperatureD);

    logtocard(temperatureA, temperatureB, temperatureC, temperatureD, distanceA, -1);          //log it
    logger.printfln(F("Sampled"));

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
  logger.printfln(F("status = %d, temp = %d, humidty = %d"),DHTstatus,temp,hum);
}

void errorflash(int n){           //non-recoverable error, flash code on LED1
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

// TODO: rename to sprintf.
const char * createFilename( const char *format, ...){
  va_list args;
  va_start(args, format);
  vsnprintf(fname, sizeof(fname), format, args);
  va_end(args);
  return fname;  
}

File openDataFile(){      //put the column headers you would like here, if you don't want headers, comment out the line below
  DateTime now = rtc.now();                   //capture time

  const char *filename = createFilename( FILENAME, now.month(), now.year());
  logger.printfln(F("opening data file %s"), filename);
  if( strcmp(filename,currentFilename) == 0) {
    File currenFile = SD.open(currentFilename, FILE_WRITE);
    currenFile.print(JSON_END);
    currenFile.close();
    strcpy(currentFilename,filename);
  }

  fhandle = SD.open(filename, FILE_WRITE);
  return fhandle;
}


void logtocard(float temperatureA, float temperatureB, float temperatureC, float temperatureD, int distanceA, int distanceB){
  unsigned long timestamp;
  DateTime now = rtc.now();                   //capture time
  logger.printfln(F("timestamp = %d"),timestamp);
  digitalWrite(LED2,HIGH);                    //turn on LED to show card in use
  delay(200);                                 //a bit of warning that card is being accessed, can be reduced if faster sampling needed
  
  fhandle = openDataFile();

  unsigned long s;
  s=fhandle.size();             //get file size
   logger.printfln(F("size %d - %d - %d"),s,strlen(JSON_START),strlen(JSON_END));
  if(!s){                       //if file is empty, add column headers
    logger.printfln(F("new file"));
    fhandle.print(JSON_START);
  } else {
    fhandle.println(COMMA);
  }
    
  // Timestamo format ISO 8601: 2017-12-26T07:44:19Z
  fhandle.print(F(" {\r\n  \"sampleTime\": \"")); //write timestamp to card, integer part, converted to Excel time serial number with resolution of ~1 second
  createFilename("%04d-%02d-%02d",now.year(),now.month(),now.day());
  logger.printfln(F("sample date = %s"),fname);
  fhandle.print(fname);
  fhandle.print(F("T"));
  createFilename("%02d:%02d:%02d",now.hour(),now.minute(),now.second());
  logger.printfln(F("sample time = %s"),fname);
  fhandle.print(fname);
  fhandle.println(F("Z\","));

  fhandle.print(F("  \"outside\": { \"temperature\": "));
  if(DHTstatus){fhandle.print(temp);}         //put data if valid otherwise blank (will be blank cell in Excel)
  fhandle.print(F(", \"humidity\": "));
  if(DHTstatus){fhandle.print(hum);}          //put data if valid otherwise blank
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
    errorflash(5);                            //error code
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
  if((getreq[0]!='G')||(getreq[1]!='E')||(getreq[2]!='T')||(getreq[3]!=' ')){crcount=0;}   //no 'GET ' at the start, so change flag to cancel
  if(crcount==2){parsefile();}                                                             //complete request found, extract name of requested file
  if(fname[0]==0){servepage();sendstatus();crcount=0;}                                     //serve index page, reset crcount on fileserve
  if(strmatch("index.htm",fname)){servepage();sendstatus();crcount=0;}                     //serve index page, reset crcount on fileserve
  if(strmatch("DATA.CSV",fname)){sendcsv();crcount=0;}                                     //serve entire data file
  if(strmatch("SPARSE.CSV",fname)){sendcsvsparse(60);crcount=0;}                           //serve file with every nth sample
  if(strmatch("RECENT.CSV",fname)){sendcsvrecent(2000);crcount=0;}                         //serve header, and approximately last n (will typically be slightly more)
  if(crcount){serve404();sendstatus();}                                                    //no valid file served => 404 error
  WIFI.print(F("AT+CIPCLOSE="));                                                           //close
  WIFI.print(cxn);  
  WIFIcmd("",ok,2000);                                                                     //disconnect
  cxn=-1;                                                                                  //clear for next connection
}


void parsefile(){
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
  WIFI.print(F("AT+CIPSEND="));                      //send data
  WIFI.print(cxn);                                   //to client
  WIFI.print(",");              
  WIFI.println(strlen(STATUSTITLE)+1);               //data has length, needs to be same as string below, plus 1 for status
  delay(50);
  WIFI.print(F(STATUSTITLE));
  WIFI.write((status%10)+'0');                       //exactly one digit
  delay(250);
  WIFIpurge();
  
}

void servepage(){                                     //for serving a page of data
  WIFI.print(F("AT+CIPSEND="));                       //send data
  WIFI.print(cxn);                                    //to client
  WIFI.print(",");              
  WIFI.println(strlen(HTTPINDEX));                    //data has length, needs to be same as string below
  delay(50);
  WIFI.print(F(HTTPINDEX));
  delay(250);
  WIFIpurge();
}

void serve404(){                                //for serving a page of data
  WIFI.print(F("AT+CIPSEND="));                 //send data
  WIFI.print(cxn);                              //to client
  WIFI.print(",");              
  WIFI.println(strlen(HTTP404));                //data has length, needs to be same as string below
  delay(50);
  WIFI.print(F(HTTP404));
  delay(250);
  WIFIpurge();
}

void sendcsv(){                         //for providing a csv document to download
  WIFI.print(F("AT+CIPSEND="));         //send data
  WIFI.print(cxn);                      //to client
  WIFI.print(",");              
  WIFI.println(strlen(HTTPCSV));        //data has length, needs to be same as string below
  delay(50);
  WIFI.print(F(HTTPCSV));               //send HTTP header for csv data type, file content to follow
  delay(250);
  WIFIpurge();
  pktbuf[0]=0;                          //empty buffer
  fhandle = SD.open(FILENAME);
  while (fhandle.available()) {         //send it all
    char c=fhandle.read();
    addtobuffer(pktbuf,PKTSIZE,c);      //add to buffer
    if(strlen(pktbuf)>PKTSIZE-2){       //if buffer full
      WIFIsenddata(pktbuf,cxn);         //send data
      pktbuf[0]=0;                      //empty buffer      
      }    
    }
  if(pktbuf[0]){                        //send data if any left in buffer
      WIFIsenddata(pktbuf,cxn);
  }
  fhandle.close();                      //close file
}

void sendcsvsparse(int n){               //only send 1/n samples
  if(n==0){n=1;}                         //to avoid divide by zero error
  WIFI.print(F("AT+CIPSEND="));          //send data
  WIFI.print(cxn);                       //to client
  WIFI.print(",");              
  WIFI.println(strlen(HTTPCSV));         //header  has length, needs to be same as string below
  delay(50);
  WIFI.print(F(HTTPCSV));                //send csv header
  delay(250);
  WIFIpurge();
  pktbuf[0]=0;    //empty buffer
  unsigned int lfcount=0;                //only output when lfcount%n==0
  fhandle = SD.open(FILENAME);
  while (fhandle.available()) {          //scan it all
    char c=fhandle.read();
    if((lfcount%n)==0){                  //only every nth line (but first, with headers will get sent)
      addtobuffer(pktbuf,PKTSIZE,c);
      if(strlen(pktbuf)>PKTSIZE-2){
        WIFIsenddata(pktbuf,cxn);        //send data      
      pktbuf[0]=0;                       //empty buffer      
      }
    }
    if(c==10){lfcount++;}
  }
  if(pktbuf[0]){
      WIFIsenddata(pktbuf,cxn);
  }
  fhandle.close();
}

void sendcsvrecent(int n){                    //only send header line and last n bytes ((approximately)
  long p;
  WIFI.print(F("AT+CIPSEND="));               //send data
  WIFI.print(cxn);                            //to client
  WIFI.print(",");              
  WIFI.println(strlen(HTTPCSV));              //data has length, needs to be same as string below
  delay(50);
  WIFI.print(F(HTTPCSV));                     //send csv header
  delay(250);
  WIFIpurge();
  pktbuf[0]=0;                                //empty buffer
  unsigned int lfcount=0;                     //to make sure first line is sent
  fhandle = SD.open(FILENAME);
  while (fhandle.available()) {               //scan it all, send all except what gets skipped below
    char c=fhandle.read();
    addtobuffer(pktbuf,PKTSIZE,c);            //add to buffer
    if(strlen(pktbuf)>PKTSIZE-2){             //if buffer nearly full
      WIFIsenddata(pktbuf,cxn);               //send data      
      pktbuf[0]=0;                            //empty buffer
    }
  if((c==10)&&(lfcount==0)){                  //after first lf
    lfcount=1;                                //tag that the next one isn't first
    p=fhandle.position();                     //find current file position
    if((fhandle.size()-n)>p){
      fhandle.seek(fhandle.size()-n);
      }                                       //if we're not already near the end, seek there
    fhandle.find("\n");                       //need to find next lf to cleanly start line    
    }
  }
  if(pktbuf[0]){      //if buffer not empty, send it
    WIFIsenddata(pktbuf,cxn);
  }
  fhandle.close();
}


void wifiinit(){
#if DEBUG_CONSOLE == false
  WIFIcmd("AT+RST","ready\r\n",5000);                                               //reset
  WIFIcmd("AT+CWQAP",ok,5000);                                                      //exit any AP's
  WIFIcmd("AT+CWJAP=\""  SSIDNAME  "\",\"" SSIDPWD  "\"","WIFI GOT IP\r\n",10000);  //join AP
  WIFIcmd("ATE0",ok,1000);                                                          //turn echo off
  WIFIcmd("AT+CWMODE=1",ok,2000);                                                   //station mode only
  WIFIcmd("AT+CIPMUX=1",ok,2000);                                                   //MUX on (needed for server)
  WIFIcmd("AT+CIPSERVER=1,80",ok,2000);                                             //server on
  WIFIcmd("AT+CIPSTO=5",ok,2000);                                                   //disconnect after x time if no data
#endif    
}

void checkwifi(){
#if DEBUG_CONSOLE == false
  while(WIFI.available()){                    //check if any data from WIFI
    int d;
    d=WIFI.read();
    if((d>47)&&(d<58)&&(cxn<0)){cxn=d-48;}    //connection number, could come from IPD or CONNECT
    if(d==':'){digitalWrite(LED2,HIGH);dorequest();digitalWrite(LED2,LOW);}                  //: means end of IPD data, content follows. LED on while busy
  }
#endif    
}
  
int WIFIcmd(char* c,char* r,long tmout){   //command c (nocrlf needed), returns true if response r received, otherwise times out
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

void WIFIsenddata(char* d,int client){    //send data to client
//
  WIFI.print(F("AT+CIPSEND="));           //send data
  WIFI.print(client);                     //to client
  WIFI.print(F(","));              
  WIFI.println(strlen(d));                //data has length
  //delay(50);
  WIFI.find(">");
  WIFI.print(d);                          //data
  delay(350);
  WIFI.find("SEND OK");
  WIFIpurge();                            //clear incoming buffer
}

void WIFIpurge(){                         //empty serial buffer
  while(WIFI.available()){WIFI.read();}
}

int strmatch(char str1[], char str2[], int n) {   //test for match in first n characters
  int k = -1;                                     //default return success
  for (int i = 0; i < n; i++) {
    if (str1[i] != str2[i]) {
      k = 0;                                      //non match found
    }
  }
  return k;
}

int strmatch(char str1[], char str2[]) {    //test for absolute match
  int n =strlen(str2);                      //as above, n is length of second string
  if(n!=strlen(str1)){return 0;}            // not the same length, can't be the same
  int k = -1;                               //default return success   
  for (int i = 0; i < n; i++) {
    if (str1[i] != str2[i]) {
      k = 0;                                //non match found
    }
  }
  return k;
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

void usonicSetup(int trigPin, int echoPin){
  pinMode(echoPin, INPUT);
  pinMode(trigPin, OUTPUT);
  digitalWrite(trigPin, LOW);
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


