#ifndef _CONTROLS_H
#define _CONTROLS_H TRUE

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>

void TurnOn(byte pin);
void TurnOff(byte pin);



#endif