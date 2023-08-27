//outputs CSV data that can be opened by Excel, time is stored as serial number accurate to ~1second
//WIFI webserver to dynamically download data

#include "utils.h"
#include "ioMux.h"
#include "sensors.h"
#include <avr/pgmspace.h>
#include <MemoryFree.h>

//Needed for SD card
//Uses hardware SPI on D10,D11,D12,D13
//LOGINTERVAL is in seconds- takes about 1 second to do a log
#include <SPI.h>

#define LOG_INTERVAL (1L * 1000L)
// Every 10 minutes.
//#define LOG_INTERVAL (10L * 60L * 1000L)


#define LIGHTPIN A0

#define TEMP_A_PIN A0
#define TEMP_B_PIN A1
#define TEMP_C_PIN A2
#define TEMP_D_PIN A3


//LED pins- LED1 is diagnostic, LED2 is card in use (flickers during write)
#define LED1 4
#define LED2 3
long sampleTime;      //keeps track of logging interval
int status=0;         //to display status info on webpage

unsigned long lastSampleTimeUnix = 0;


// UltraSonic module
#define USONIC_1_TRIG D_X
#define USONIC_1_ECHO D_Y
#define USONIC_2_TRIG D_X
#define USONIC_2_ECHO D_Y
#define USMAX 3000  //??
#define USTIMEOUT_2M   11600  // Timeout for 2m
#define US_TIMEOUT_2_5M 14500  // Timeout for 2.5m




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
  Serial.begin(115200);   //start serial port for WIFI

  pinMode(D_A, OUTPUT);
  pinMode(D_B, OUTPUT);
  pinMode(D_DISABLE, OUTPUT);

  digitalWrite(D_A, LOW);
  digitalWrite(D_B, LOW);
  digitalWrite(D_DISABLE, HIGH);

  pinMode(D_X, OUTPUT);
  pinMode(D_Y, OUTPUT);
  digitalWrite(D_X, LOW);
  digitalWrite(D_Y, LOW);

  sampleTime = millis() + 10000;   //Wait for 10 seconds before the first sample.

//   selectDigitalDevice(2);
//   setupDHT11(D_Y);           //start temp/humidity sensor
//   setupDHT11(8);           //start temp/humidity sensor


//   usonicSetup(USONIC_1_TRIG, USONIC_1_ECHO); //set up ultrasonic sensor
   usonicSetup(9, 8); //set up ultrasonic sensor

}

void loop() {

   if(millis()>sampleTime){   //if it's been more than logging interval
     Serial.print(F("Millies = ")); Serial.println(millis());


//     dht11Response_t dht11Response = readDHT11(D_Y);     //fetch data to log
/*
     dht11Response_t dht11Response = readDHT11(8);     //fetch data to log

     Serial.print(F("DHT11 status=")); Serial.print(dht11Response.status);

     Serial.print(F(", temp=")); Serial.print(dht11Response.temperature);
     Serial.print(F(", humidity=")); Serial.println(dht11Response.humidity);
     Serial.println("");
*/

    int distanceB=usonicRead(9, 8, US_TIMEOUT_2_5M); //distance in cm, time out at 11600us or 2m maximum range
    Serial.print(F("DistanceB=")); Serial.println(distanceB);
     
/*
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
    */
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


//    sampleTime+=(LOGINTERVAL*10L);        //add interval, so interval is precise
    sampleTime+=LOG_INTERVAL;        //add interval, so interval is precise
  }
  
  delay( 100);
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

}

void getOutsideClimate(){                      //Returns JSON with the current tempreture, humidity etc.

   setupDHT11(D_Y);           //start temp/humidity sensor
   selectDigitalDevice(2);
   delay(500);
   dht11Response_t dht11Response = readDHT11(D_Y);     //fetch data to log
  
  //dht11Response_t dht11Response = readDHT11(DHT11PIN); //returns status 1 on ok, 0 on fail (eg checksum, data not received)
  int light = -1;
  //int light=analogRead(LIGHTPIN);                //get light sensor data

//  sprintf_P(pktbuf, API_OUTSIDE_CLIMATE_DATA, dht11Response.temperature, dht11Response.humidity, light);

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
