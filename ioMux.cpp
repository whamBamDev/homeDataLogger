
#include "ioMux.h"


void selectAnalogDevice(int pin) {

  disableAnalogMux();

  digitalWrite(A_A, (pin & 1) ? HIGH: LOW);
  digitalWrite(A_B, (pin & 2) ? HIGH: LOW);
  digitalWrite(A_C, (pin & 4) ? HIGH: LOW);

  delay(50);
  enableAnalogMux();

}

void selectDigitalDevice(int pin) {

  disableDigitalMux();

  digitalWrite(D_A, (pin & 1) ? HIGH: LOW);
  digitalWrite(D_B, (pin & 2) ? HIGH: LOW);

  delay(50);
  enableDigitalMux();
}

void setDeviceStatus(int pin, int value) {
  digitalWrite(pin, value);
  delay(50);
}
