#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>


bool ota_debug = true;

void Ota_on_start();


void setupOTA()
{
    ArduinoOTA.onStart(Ota_on_start);
    ArduinoOTA.onEnd([]()
                    { 
                        if ( ota_debug ) {
                            Serial.println("\nEnd"); 
                        }
                    });

    ArduinoOTA.onProgress(
        [](unsigned int progress, unsigned int total)
            { 
                if ( ota_debug )
                    Serial.printf("Progress: %u%%\r", (progress / (total / 100))); 
        });
    ArduinoOTA.onError([](ota_error_t error)
                       {
        if ( ota_debug ) Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) {
          if ( ota_debug ) Serial.println("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
          if ( ota_debug ) Serial.println("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
          if ( ota_debug ) Serial.println("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
          if ( ota_debug ) Serial.println("Receive Failed");
        } else if (error == OTA_END_ERROR) {
          if ( ota_debug ) Serial.println("End Failed");
        } });
    ArduinoOTA.begin();
}

void Ota_on_start() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else {  // U_FS
        type = "filesystem";
      }

      // NOTE: if updating FS this would be the place to unmount FS using FS.end()
      if (ota_debug) Serial.println("Start updating " + type);
}