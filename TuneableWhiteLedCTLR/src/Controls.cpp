#include "Controls.h"


void TurnOn(byte pin) {
  digitalWrite(pin, 0);
}

void TurnOff(byte pin) {
  digitalWrite(pin, 1);
}