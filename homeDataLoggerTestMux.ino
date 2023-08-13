//outputs CSV data that can be opened by Excel, time is stored as serial number accurate to ~1second
//WIFI webserver to dynamically download data

#include <avr/pgmspace.h>
#include <MemoryFree.h>


#include <Wire.h>

const char INDEX_LINE[] PROGMEM = "<li><a href=\"%s\">Download %s</a></li>\n";



//#define DHT11PIN 8
#define DHT11PIN 2

struct dht11Response_t {
  int status;
  int temperature;
  int humidity;
} ;


//Needed for RTC
//Uses SCL and SDA aka A4 and A5
#include <Wire.h>


//LED pins- LED1 is diagnostic, LED2 is card in use (flickers during write)
#define LED1 4
#define LED2 3
int status = 0;       //to display status info on webpage
unsigned long lastSampleTimeUnix = 0;


// UltraSonic module
#define USONIC_1_TRIG 7
#define USONIC_1_ECHO 6
#define USONIC_2_TRIG 9
#define USONIC_2_ECHO 8
#define USMAX 3000  //??
#define USTIMEOUT_2M   11600  // Timeout for 2m
#define US_TIMEOUT_2_5M 14500  // Timeout for 2.5m

const char STATUS_TEXT[] PROGMEM = "<br>Status (zero is no error): %d<br>Current time: %04d-%02d-%02d %02d:%02d:%02d<br>";
const char STATUS_TEXT2[] PROGMEM = "Last sample taken at: %04d-%02d-%02d %02d:%02d:%02d<br>Free memory: %d<br>";

// sketch needs ~450 bytes local space to not crash

#define A_IN A0
#define A_A A1
#define A_B A2
#define A_C A3
#define A_DISABLE 2


#define D_X 8
#define D_Y 9
#define D_A 7
#define D_B 6
#define D_DISABLE 5

/*
  #define A_A D15
  #define A_B D16
  #define A_C D17
  #define A_DISABLE D18

  #define A_ONE 255
  #define A_ZERO 0
*/

byte dat [5];
int i = 0;


void setup() {
  Serial.begin(115200);   //start serial port for WIFI

  //  usonicSetup(USONIC_1_TRIG, USONIC_1_ECHO); //set up ultrasonic sensor
  //  usonicSetup(USONIC_2_TRIG, USONIC_2_ECHO); //set up ultrasonic sensor

  //  setupDHT11(DHT11PIN);           //start temp/humidity sensor

//  pinMode(A_IN, INPUT);
//  pinMode(A5, INPUT);
  pinMode(A_IN, OUTPUT);
  pinMode(A5, OUTPUT);


  pinMode(A_A, OUTPUT);
  pinMode(A_B, OUTPUT);
  pinMode(A_C, OUTPUT);
  pinMode(A_DISABLE, OUTPUT);

  digitalWrite(A_A, LOW);
  digitalWrite(A_B, LOW);
  digitalWrite(A_C, LOW);
  digitalWrite(A_DISABLE, HIGH);
  //  digitalWrite(A_A,LOW);
  //  digitalWrite(A_B,LOW);
  //  digitalWrite(A_C,LOW);
  //  digitalWrite(A_DISABLE,HIGH);

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

  //  setupDHT11(DHT11PIN);           //start temp/humidity sensor

  i = 0;
}

void loop() {
//  testAnalog();
  testDigital();
}

void muxWait() {
  delay(2000);
}


void testAnalog() {
  while(1) {

    Serial.println(F("============================================"));

    digitalWrite(A_IN, LOW);
    selectAnalogDevice(i);
    digitalWrite(A_IN, HIGH);
    Serial.print(F("device = ")); Serial.println(i);
    muxWait();

    i = (i >= 7) ? 0 : ++i;
  }
}

void selectAnalogDevice(int pin) {

  Serial.print(F("Analog :"));

  digitalWrite(A_DISABLE, HIGH);
  delay(50);

  if (pin & 1) {
    digitalWrite(A_A, HIGH);
    Serial.print(F("AH:"));
  } else {
    digitalWrite(A_A, LOW);
    Serial.print(F("AL:"));
  }

  if (pin & 2) {
    digitalWrite(A_B, HIGH);
    Serial.print(F("BH:"));
  } else {
    digitalWrite(A_B, LOW);
    Serial.print(F("BL:"));
  }

  if (pin & 4) {
    digitalWrite(A_C, HIGH);
    Serial.print(F("CH"));
  } else {
    digitalWrite(A_C, LOW);
    Serial.print(F("CL"));
  }

  Serial.println("");

  delay(50);
  digitalWrite(A_DISABLE, LOW);
  delay(1000);
}

void selectDigitalDevice(int pin) {

  Serial.print(F("Digital :"));

  digitalWrite(D_DISABLE, HIGH);
  delay(50);

  if (pin & 1) {
    digitalWrite(D_A, HIGH);
    Serial.print(F("AH:"));
  } else {
    digitalWrite(D_A, LOW);
    Serial.print(F("AL:"));
  }

  if (pin & 2) {
    digitalWrite(D_B, HIGH);
    Serial.print(F("BH:"));
  } else {
    digitalWrite(D_B, LOW);
    Serial.print(F("BL:"));
  }

  Serial.println("");

  delay(50);
  digitalWrite(D_DISABLE, LOW);
  delay(1000);
}

int readThermister(int pin) {
  int rawADC;
  rawADC = analogRead(pin);
  return rawADC;
}

double readThermister_Orig(int pin) {
  int rawADC;
  double temp;
  rawADC = analogRead(pin);
  temp = log(((10240000 / rawADC) - 10000));
  temp = 1 / (0.001129148 + (0.000234125 + (0.0000000876741 * temp * temp )) * temp );
  temp = temp - 273.15; // Convert Kelvin to Celcius
  return temp;
}

void displayFreeMemory(const __FlashStringHelper *msg) {
  Serial.print(F("freeMemory("));
  Serial.print(msg);
  Serial.print(F(")="));
  Serial.println(freeMemory());
}

byte read_data (int pin) {
  byte data;
  for (int i = 0; i < 8; i ++) {
    if (digitalRead (pin) == LOW) {
      while (digitalRead (pin) == LOW); // wait for 50us
      delayMicroseconds (30); // determine the duration of the high level to determine the data is '0 'or '1'
      if (digitalRead (pin) == HIGH)
        data |= (1 << (7 - i)); // high front and low in the post
      while (digitalRead (pin) == HIGH); // data '1 ', wait for the next one receiver
    }
  }
  return data;
}

void start_test (int pin) {
  memset(dat, 0, sizeof(dat));
  pinMode (pin, OUTPUT);

  digitalWrite (pin, LOW); // bus down, send start signal
  delay (30); // delay greater than 18ms, so DHT11 start signal can be detected

  digitalWrite (pin, HIGH);
  delayMicroseconds (40); // Wait for DHT11 response

  pinMode (pin, INPUT);
  while (digitalRead (pin) == HIGH);
  delayMicroseconds (80); // DHT11 response, pulled the bus 80us
  if (digitalRead (pin) == LOW);
  delayMicroseconds (80); // DHT11 80us after the bus pulled to start sending data

  for (int i = 0; i < 4; i ++) // receive temperature and humidity data, the parity bit is not considered
    dat[i] = read_data (pin);

  pinMode (pin, OUTPUT);
  digitalWrite( pin, HIGH); // send data once after releasing the bus, wait for the host to open the next Start signal
}


void setupDHT11( int pin) {                   //set pin to output, set high for idle state
  pinMode( pin, OUTPUT);
  digitalWrite( pin, HIGH);
}

dht11Response_t readDHT11( int pin) {       //returns status 1 on ok, 0 on fail (eg checksum, data not received)
  unsigned int n[83];           //to store bit times
  byte p = 0;                   //pointer to current bit
  unsigned long t;              //time
  byte old = 0;
  byte newd;
  dht11Response_t response;
  response.status = 0;
  response.temperature = -1;
  response.humidity = -1;
  //char lengthStr[10];

  for (int i = 0; i < 83; i++) {
    n[i] = 0;
  }
  digitalWrite(pin, LOW);               //start signal
  //delay(30);
  delay(25);
  digitalWrite(pin, HIGH);
  delayMicroseconds(20);
  //delayMicroseconds(80);
  pinMode(pin, INPUT);
  delayMicroseconds(40);
  t = micros() + 10000L;
  while ((micros() < t) && (p < 83)) {  //read bits
    newd = digitalRead(pin);
    if (newd != old) {
      n[p] = micros();
      p++;
      old = newd;
    }
  }
  pinMode(pin, OUTPUT);                 //reset for next cycle
  digitalWrite(pin, HIGH);

  //sprintf(lengthStr, "DHT p=%x", p);
  //debugWiFi(lengthStr);

  if (p != 83) {
    return response; //not a valid datastream
  }
  byte data[5] = {0, 0, 0, 0, 0};
  for (int i = 0; i < 40; i++) {        //store data in array
    if (n[i * 2 + 3] - n[i * 2 + 2] > 50) {
      data[i >> 3] = data[i >> 3] | (128 >> (i & 7));
    }
  }
  byte k = 0;                            //checksum
  for (int i = 0; i < 4; i++) {
    k = k + data[i];
  }
  if ((k ^ data[4])) {
    return response; //checksum error
  }
  //DHTtemp=data[2];
  //DHThum=data[0];
  response.status = 1;
  response.temperature = data[2];        //temperature
  response.humidity = data[0];           //humidity
  return response;                       //data valid
}


void testDigital() {

  Serial.println(F(" "));
  Serial.println(F("======================================"));
  Serial.print(F("== Select "));
  Serial.print(i);
  selectDigitalDevice(i);
  digitalWrite(D_X, HIGH);
  digitalWrite(D_Y, LOW);
  Serial.print(F("== Select X"));
  Serial.print(i);
  Serial.println(F(" - High"));
  muxWait();
  digitalWrite(D_X, LOW);
  digitalWrite(D_Y, HIGH);
  Serial.print(F("== Select Y"));
  Serial.print(i);
  Serial.println(F(" - High"));
  muxWait();
  digitalWrite(D_Y, LOW);
  digitalWrite(D_DISABLE, HIGH);

  Serial.println(F(" "));

  i = (i >= 3) ? 0 : ++i;

}



void testDigitalBasic() {

  digitalWrite(D_A, LOW);
  digitalWrite(D_B, LOW);
  digitalWrite(D_DISABLE, LOW);
  Serial.println(F("======================================"));
  Serial.println(F("== Select 0"));
  digitalWrite(D_X, LOW);
  digitalWrite(D_Y, LOW);
  Serial.println(F("== x0 y0 - LOW"));
  muxWait();
  digitalWrite(D_X, HIGH);
  digitalWrite(D_Y, HIGH);
  Serial.println(F("== x0 y0 - HIGH"));
  muxWait();
  digitalWrite(D_DISABLE, HIGH);
  Serial.println(F("== Disable"));
  muxWait();

  digitalWrite(D_A, HIGH);
  digitalWrite(D_B, LOW);
  digitalWrite(D_DISABLE, LOW);
  Serial.println(F("======================================"));
  Serial.println(F("== Select 1"));
  digitalWrite(D_X, LOW);
  digitalWrite(D_Y, LOW);
  Serial.println(F("== x1 y1 - LOW"));
  muxWait();
  digitalWrite(D_X, HIGH);
  digitalWrite(D_Y, HIGH);
  Serial.println(F("== x1 y1 - HIGH"));
  muxWait();
  digitalWrite(D_DISABLE, HIGH);
  Serial.println(F("== Disable"));
  muxWait();

  Serial.println(F(" "));

}
