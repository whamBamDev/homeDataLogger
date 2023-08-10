
#include "Arduino.h"

#define A_IN A0
#define A_A A1
#define A_B A2
#define A_C A3
#define A_DISABLE A4

#define D_X 8
#define D_Y 9
#define D_A 7
#define D_B 6
#define D_DISABLE 5


#define disableAnalogMux() setDeviceStatus(A_DISABLE,HIGH)
#define enableAnalogMux() setDeviceStatus(A_DISABLE,LOW)

#define disableDigitalMux() setDeviceStatus(D_DISABLE,HIGH)
#define enableDigitalMux() setDeviceStatus(D_DISABLE,LOW)

void setDeviceStatus(int pin, int value);

void selectAnalogDevice(int pin);
void selectDigitalDevice(int pin);
