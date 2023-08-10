
#include "Arduino.h"
#include "sensors.h"

// =================================================================
// == 
// == Functions for reading data from sensors.
// == 
// =================================================================

double readThermister(int pin) {
  int rawADC;
  double temp;
  rawADC = analogRead(pin);
  temp = log(((10240000/rawADC) - 10000));
  temp = 1 / (0.001129148 + (0.000234125 + (0.0000000876741 * temp * temp ))* temp );
  temp = temp - 273.15; // Convert Kelvin to Celcius
  return temp;
}

void setupDHT11( int pin){                    //set pin to output, set high for idle state
  pinMode( pin, OUTPUT);
  digitalWrite( pin, HIGH);
}

byte readDHT11Byte(int pin) {
  byte data;
  for (int i = 0; i < 8; i ++) {
    if (digitalRead( pin) == LOW) {
      while (digitalRead( pin) == LOW); // wait for 50us
      delayMicroseconds(30); // determine the duration of the high level to determine the data is '0 'or '1'
      if (digitalRead( pin) == HIGH)
        data |= (1 << (7-i)); // high front and low in the post
      while (digitalRead( pin) == HIGH); // data '1 ', wait for the next one receiver
     }
  }
  return data;
}

dht11Response_t readDHT11new( int pin){        //returns status 1 on ok, 0 on fail (eg checksum, data not received)
  byte p=0;                     //pointer to current bit
  unsigned long t;              //time
  byte old=0;
  byte newd;
  dht11Response_t response;
  response.status = 0;
  response.temperature = -1;
  response.humidity = -1;

  digitalWrite(pin, LOW); // bus down, send start signal
  delay (30); // delay greater than 18ms, so DHT11 start signal can be detected
 
  digitalWrite (pin, HIGH);
  delayMicroseconds (40); // Wait for DHT11 response
 
  pinMode (pin, INPUT);
  while (digitalRead (pin) == HIGH);
  delayMicroseconds (80); // DHT11 response, pulled the bus 80us
  if (digitalRead (pin) == LOW);
  delayMicroseconds (80); // DHT11 80us after the bus pulled to start sending data
 
  byte data[5]={0,0,0,0,0};

  for (int i = 0; i < 4; i ++) // receive temperature and humidity data, the parity bit is not considered
    data[i] = readDHT11Byte( pin);

  //DHTtemp=data[2];
  //DHThum=data[0];
  response.status = 1;
  response.temperature = data[2];        //temperature
  response.humidity = data[0];           //humidity
  return response;                       //data valid
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
  //char lengthStr[10];

  for(int i=0;i<83;i++){n[i]=0;}
  digitalWrite(pin,LOW);                //start signal
  //delay(30);
  delay(25);
  digitalWrite(pin,HIGH);
  delayMicroseconds(20);
  //delayMicroseconds(80);
  pinMode(pin,INPUT);
  delayMicroseconds(40);
  t=micros()+10000L;
  while((micros()<t)&&(p<83)){          //read bits
    newd=digitalRead(pin);
    if(newd!=old){
      n[p]=micros();
      p++;
      old=newd;
    }
  }
  pinMode(pin,OUTPUT);                  //reset for next cycle
  digitalWrite(pin,HIGH);

  //sprintf(lengthStr, "DHT p=%x", p);
  //debugWiFi(lengthStr);

  if(p!=83){return response;}           //not a valid datastream
  byte data[5]={0,0,0,0,0};
  for(int i=0;i<40;i++){                //store data in array
    if(n[i*2+3]-n[i*2+2]>50){data[i>>3]=data[i>>3]|(128>>(i&7));}
  }
  byte k=0;                              //checksum
  for(int i=0;i<4;i++){k=k+data[i];}
  if((k^data[4])){return response;}      //checksum error
  //DHTtemp=data[2];
  //DHThum=data[0];
  response.status = 1;
  response.temperature = data[2];        //temperature
  response.humidity = data[0];           //humidity
  return response;                       //data valid
}

void usonicSetup(int trigPin, int echoPin){
  pinMode(echoPin, INPUT_PULLUP);
  pinMode(trigPin, OUTPUT);
  digitalWrite(trigPin, LOW);
}

int usonicRead(int trigPin, int echoPin, long utimeout){ //utimeout is maximum time to wait for return in us
  if(digitalRead(echoPin)==HIGH){return -1;} //if echo line is still low from last result, return 0;

  digitalWrite( trigPin, LOW); // Set the trigger pin to low for 2uS 
  delayMicroseconds(2); 
    
  digitalWrite( trigPin, HIGH); // Send a 10uS high to trigger ranging 
  delayMicroseconds(20); 

  digitalWrite( trigPin, LOW); // Send pin low again 
  int distance = pulseIn( echoPin, HIGH,26000); // Read in times pulse 
    
  distance = distance/58;
    
  return distance;
}
