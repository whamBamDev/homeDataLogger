
struct dht11Response_t {
  int status;
  int temperature;
  int humidity;
} ;


// =================================================================
// == 
// == Functions for reading data from sensors.
// == 
// =================================================================

double readThermister(int pin);

void setupDHT11( int pin);

byte readDHT11Byte(int pin);

dht11Response_t readDHT11new( int pin);

dht11Response_t readDHT11( int pin);

void usonicSetup(int trigPin, int echoPin);

int usonicRead(int trigPin, int echoPin, long utimeout);
