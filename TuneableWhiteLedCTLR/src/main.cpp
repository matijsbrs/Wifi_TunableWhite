#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <TelnetStream.h>
#include <PubSubClient.h>


#include "Credentials.h"
#include "Ota.h"
#include "Controls.h"

// function declarations
// void mqtt_callback(char* topic, byte* message, unsigned int length);
void transmit_mqtt(const char * extTopic,const char * Field, const char * payload);

char ssid[20];
char password[20];


ESP8266WebServer server(80);

// MQTT stuff
WiFiClient espClient;
PubSubClient client(espClient);

const char* applicationUUID = "123456789";
const char* default_mqtt_server = "192.168.2.201";
const char* TOPIC = "Light/Keuken";
const char* Version = "V0.0.2";

// MQTT stuff end

void handlePortal();
enum system_state
{
  unknown,
  booting,
  wifi_reset,
  wifi_connecting,
  wifi_ready,
  wifi_ap_mode
};

struct system_configuration
{
  enum system_state state = unknown;
} configuration;

struct settings
{
  char ssid[32];         // the SSID of the netwerk
  char password[32];     // the WifiPassword
  char energy_topic[32]; // the path to the topic on which the current wattage is found.
  char mqtt_server[32];  // Addres of the MQTT server
  unsigned long period;


} user_wifi = {};

unsigned long startMillis; // some global variables available anywhere in the program
unsigned long currentMillis;

// Rotary Encoder State
volatile int encoderPos = 0;
volatile int encoderUpdate = 1;
volatile int encoderMin = 0;
volatile int encoderMax = 255;
volatile int encoderLowRangeStep = 1;
volatile int encoderHighRangeStep = 5;
volatile int encoderRangeChange = 32;

// Button State
volatile bool buttonState = true;

// PWM State
int pwmValue = 0;

const byte iotResetPin = D2;
const byte Led_pin   = D4; // using the built in LED

// PWM Pin Definition
#define PWM_PIN_COOL D0
#define PWM_PIN_WARM D1


// Rotary Encoder Pin Definitions
#define ENC_A_PIN D5
#define ENC_B_PIN D6
#define BUTTON_PIN D2


// Interrupt Service Routine for Rotary Encoder A Pin and Button Pin
void IRAM_ATTR onEncoderAPinChange()
{
  int step = encoderLowRangeStep;
  if ( encoderPos >= encoderRangeChange ) step = encoderHighRangeStep;
  if (digitalRead(ENC_A_PIN) == HIGH) {
    if (digitalRead(ENC_B_PIN) == LOW) {
      encoderPos += step;
      buttonState = true;
    } else {
      encoderPos -= step;
    }
  }
  //  else {
  //   if (digitalRead(ENC_B_PIN) == LOW) {
  //     encoderPos--;
  //   } else {
  //     encoderPos++;
  //   }
  // }

  if (digitalRead(BUTTON_PIN) == LOW) {
    if (buttonState == true) 
      buttonState = false;
    else
      buttonState = true;
  }


  if ( encoderPos < encoderMin ) encoderPos = encoderMin;
  if ( encoderPos > encoderMax ) encoderPos = encoderMax;
  encoderUpdate++;
}

int WarmStart = 0;
int WarmStop = 192;
int CoolStart = 128;
int CoolStop = 255;
int pwmCool = 0;
int pwmWarm = 0;

// Function to Set PWM Signal Based on Encoder Position and Button State
void setPWM() {
  pwmCool = 0;
  pwmWarm = 0;
  pwmValue = 0;
  if (buttonState) {
    // pwmValue = map(encoderPos, 0, 100, 0, 255);
    if (encoderPos >= WarmStart) {
      pwmWarm = map(encoderPos, WarmStart, WarmStop, encoderMin, encoderMax);
      if ( pwmWarm > 255 ) pwmWarm = 255;
      if ( pwmWarm < 0 ) pwmWarm = 0;
    }
    if (encoderPos >= CoolStart) {
      pwmCool = map(encoderPos, CoolStart, CoolStop, encoderMin, encoderMax);
      if ( pwmCool > 255 ) pwmCool = 255;
      if ( pwmCool < 0 ) pwmCool = 0;
    } 
  } 

    // analogWrite(PWM_PIN_COOL, pwmCool);
    // analogWrite(PWM_PIN_WARM, pwmWarm);
}

void Load_defaults() {
  user_wifi.period = 1000; 
  strcpy(user_wifi.mqtt_server, default_mqtt_server);
}

void show_settings() {
  char value[32];
  snprintf(value, 32, "%ld", user_wifi.period );
  transmit_mqtt("settings","period",value);

  snprintf(value, 32, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] );
  transmit_mqtt("settings","ip",value);
  transmit_mqtt("settings","version",Version);
}

void Validate_settings() {
    if (user_wifi.period < 100 ) user_wifi.period = 100;
    if (user_wifi.period > 10000 ) user_wifi.period = 10000;
}

void ApMode()
{
  WiFi.mode(WIFI_AP);
  Serial.print("Setting soft-AP configuration ... ");
  //Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");

  Serial.print("Setting soft-AP ... ");
  Serial.println(WiFi.softAP(ssid, NULL) ? "Ready" : "Failed!");
  // WiFi.softAP(ssid);
  // WiFi.softAP(ssid, password, channel, hidden, max_connection)

  Serial.print("Soft-AP IP address = ");
  Serial.println(WiFi.softAPIP());
  
  configuration.state = wifi_ap_mode;
}

void mqtt_callback(char* topic, byte* message, unsigned int length) {
  // Serial.print("Message arrived on topic: ");
  // Serial.print(topic);
  // Serial.print(". Message: ");
  String messageTemp;
  
  // char mytopic[32];
  // snprintf(mytopic, 32, "sensor-%08X/set", ESP.getChipId());

  for (unsigned int i = 0; i < length; i++) {
    // Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  // Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  
    
  if (messageTemp == "settings=read" ) {
    EEPROM.get(0, user_wifi);
    Validate_settings();
  } else if (messageTemp == "settings=write" ) {
    EEPROM.put(0, user_wifi);
    EEPROM.commit();
  } else if (messageTemp == "settings=default" ) {
    Load_defaults();
    EEPROM.put(0, user_wifi);
    EEPROM.commit();
  
  } else if (messageTemp == "settings=show" ) {
    show_settings();
  
  } else if (messageTemp == "system=restart" ) {
    ESP.restart();
  }



}

void setup()
{
  
  pinMode(iotResetPin, INPUT_PULLUP);

  Serial.begin(9600);
  Serial.println("Booting");

   // Initialize Rotary Encoder Pins and Button Pin
  pinMode(ENC_A_PIN, INPUT_PULLUP);
  pinMode(ENC_B_PIN, INPUT_PULLUP);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_A_PIN), onEncoderAPinChange, CHANGE);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onEncoderAPinChange, CHANGE);

  // Initialize PWM Pin
  pinMode(PWM_PIN_COOL, OUTPUT);
  pinMode(PWM_PIN_WARM, OUTPUT);

  configuration.state = booting;
  EEPROM.begin(sizeof(struct settings));
  Load_defaults();

  sprintf(ssid, "sensor-%08X\n", ESP.getChipId());
  sprintf(password, ACCESSPOINT_PASS);

  if (!digitalRead(iotResetPin))
  {
    Serial.println("recover");
    configuration.state = wifi_reset;
    ApMode();
  }
  else
  {
    configuration.state = wifi_connecting;
    // Read network data.
    EEPROM.get(0, user_wifi);

    Serial.printf("Wifi ssid: %s\n", user_wifi.ssid);
    Serial.printf("MQTT host: %s\n", user_wifi.mqtt_server);


    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true); // werkt dit?
    WiFi.begin(user_wifi.ssid, user_wifi.password);
    byte tries = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(1000);
      Serial.println("Retry connecting");
      if (tries++ > 15)
      {
        Serial.println("Failed, start ApMode()");
        ApMode();
        break;
      }
    }
  }

  
  if (WiFi.status() == WL_CONNECTED)
  {
    configuration.state = wifi_ready;
  }

  startMillis = millis(); // initial start time

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  switch (configuration.state)
  {
  case wifi_connecting:
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
    break;

  case wifi_ap_mode:
    TurnOn(Led_pin);
    Serial.printf("started WiFi AP %s\n", ssid);

    server.on("/", handlePortal);
    server.begin();
    break;

  case wifi_ready:
    setupOTA();
    Serial.println("Ready for OTA");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    // set up mqtt stuff
    client.setServer(user_wifi.mqtt_server, 1883);
    client.setCallback(mqtt_callback);
    // on ready push settings to mqtt.
    show_settings();
    
    break;

  default:
    break;
  }
}



int connect_mqtt() {
  if (client.connected()) {
    return 1;
  }

  Serial.print("Attempting MQTT connection... ");

  // // Create a random client ID
  // String clientId = "ESP8266Client-";
  // sprintf(ssid, "sensor-%08X\n", ESP.getChipId());
  // clientId += String(random(0xffff), HEX);

  // Attempt to connect
  if (client.connect(ssid)) {
    Serial.println("connected");
    char topic[32];
    snprintf(topic, 32, "sensor-%08X/cmd", ESP.getChipId());
    Serial.println(topic);
    client.subscribe(topic);
    return 1;
  }

  Serial.print("failed, rc=");
  Serial.print(client.state());
  return 0;
}

void transmit_mqtt(const char * extTopic, const char * Field, const char * payload) {
  if (connect_mqtt()) {
    char topic[75];
    snprintf(topic, 75, "sensor-%08X/%s/%s", ESP.getChipId(), extTopic,Field);
    // Serial.println(topic);
    client.publish(topic, payload);
  } else {
    Serial.println("MQTT ISSUE!!");
  }
}

void transmit_mqtt_influx(const char * Field, float value) {
  char payload[75];
  snprintf(payload, 75, "climatic,host=sensor-%08X %s=%f",ESP.getChipId(),Field, value);
  transmit_mqtt(TOPIC, "state", payload);
}

void transmit_mqtt_float(const char * Field, float value) {
  char payload[20];
  snprintf(payload, 20, "%f",value);
  transmit_mqtt(TOPIC, Field, payload);
}



void ShowClients()
{
  unsigned char number_client;
  struct station_info *stat_info;

  struct ip4_addr *IPaddress;
  IPAddress address;
  int cnt = 1;

  number_client = wifi_softap_get_station_num();
  stat_info = wifi_softap_get_station_info();

  Serial.print("Connected clients: ");
  Serial.println(number_client);

  while (stat_info != NULL)
  {
    IPaddress = &stat_info->ip;
    address = IPaddress->addr;

    Serial.print(cnt);
    Serial.print(": IP: ");
    Serial.print((address));
    Serial.print(" MAC: ");

    uint8_t *p = stat_info->bssid;
    Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X", p[0], p[1], p[2], p[3], p[4], p[5]);

    stat_info = STAILQ_NEXT(stat_info, next);
    cnt++;
    Serial.println();
  }
}


byte loper = 0x01;
uint32 pwmStartMillis = 0;
int pwmCoolWrk = 0;
int pwmWarmWrk = 0;
void loop()
{
  switch (configuration.state)
  {
  case wifi_ready:
    // wifi connected to network. ready
    ArduinoOTA.handle();
    break;
  case wifi_ap_mode:
    // could not connect, waiting for new configuration.
    server.handleClient();
    break;

  default:
    break;
  }

 
  client.loop(); // mqtt loop
  // Blinking led.
 
  setPWM();

  if ( encoderUpdate > 0 ) {
    Serial.println(encoderPos);
    // Serial.println(startMillis);
    // Serial.println(currentMillis);
    // Serial.println(user_wifi.period);

    encoderUpdate = 0;
  }

  currentMillis = millis();                  // get the current "time" (actually the number of milliseconds since the program started)
  if ( (currentMillis - pwmStartMillis) >= 100 ) {
    pwmStartMillis = currentMillis + 100;
    
    
    if ( pwmCool > pwmCoolWrk ) pwmCoolWrk++;
    if ( pwmWarm > pwmWarmWrk ) pwmWarmWrk++;
    if ( pwmCool < pwmCoolWrk ) pwmCoolWrk--;
    if ( pwmWarm < pwmWarmWrk ) pwmWarmWrk--;

    analogWrite(PWM_PIN_COOL, pwmCoolWrk);
    analogWrite(PWM_PIN_WARM, pwmWarmWrk);
  }
  if (((currentMillis - startMillis) >= user_wifi.period) && (configuration.state == wifi_ready) )// test whether the period has elapsed
  {
    Serial.print("Cool: ");
    Serial.print(pwmCoolWrk);
    Serial.print(" ");
    Serial.println(pwmCool);
    Serial.print("Warm: ");
    Serial.print(pwmWarmWrk);
    Serial.print(" ");
    Serial.println(pwmWarm);
    // Serial.println(encoderPos);
    // Serial.println(buttonState);


    // digitalWrite(Heater_1, !digitalRead(Heater_1));  //if so, change the state of the LED.  Uses a neat trick to change the state
    if (configuration.state == wifi_ap_mode)
      ShowClients();

    startMillis = currentMillis; // IMPORTANT to save the start time of the current LED state.
  }
}

void handlePortal()
{

  if (server.method() == HTTP_POST)
  {

    strncpy(user_wifi.ssid, server.arg("ssid").c_str(), sizeof(user_wifi.ssid));
    strncpy(user_wifi.password, server.arg("password").c_str(), sizeof(user_wifi.password));
    user_wifi.ssid[server.arg("ssid").length()] = user_wifi.password[server.arg("password").length()] = '\0';

    // load operational defaults
    strcpy(user_wifi.mqtt_server, default_mqtt_server);
   
    EEPROM.put(0, user_wifi);
    EEPROM.commit();

    server.send(200, "text/html", "<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Wifi Setup</title><style>*,::after,::before{box-sizing:border-box;}body{margin:0;font-family:'Segoe UI',Roboto,'Helvetica Neue',Arial,'Noto Sans','Liberation Sans';font-size:1rem;font-weight:400;line-height:1.5;color:#212529;background-color:#f5f5f5;}.form-control{display:block;width:100%;height:calc(1.5em + .75rem + 2px);border:1px solid #ced4da;}button{border:1px solid transparent;color:#fff;background-color:#007bff;border-color:#007bff;padding:.5rem 1rem;font-size:1.25rem;line-height:1.5;border-radius:.3rem;width:100%}.form-signin{width:100%;max-width:400px;padding:15px;margin:auto;}h1,p{text-align: center}</style> </head> <body><main class='form-signin'> <h1>Wifi Setup</h1> <br/> <p>Your settings have been saved successfully!<br />Please restart the device.</p></main></body></html>");
  }
  else
  {
    server.send(200, "text/html", "<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Wifi Setup</title> <style>*,::after,::before{box-sizing:border-box;}body{margin:0;font-family:'Segoe UI',Roboto,'Helvetica Neue',Arial,'Noto Sans','Liberation Sans';font-size:1rem;font-weight:400;line-height:1.5;color:#212529;background-color:#f5f5f5;}.form-control{display:block;width:100%;height:calc(1.5em + .75rem + 2px);border:1px solid #ced4da;}button{cursor: pointer;border:1px solid transparent;color:#fff;background-color:#007bff;border-color:#007bff;padding:.5rem 1rem;font-size:1.25rem;line-height:1.5;border-radius:.3rem;width:100%}.form-signin{width:100%;max-width:400px;padding:15px;margin:auto;}h1{text-align: center}</style> </head> <body><main class='form-signin'> <form action='/' method='post'> <h1 class=''>Wifi Setup</h1><br/><div class='form-floating'><label>SSID</label><input type='text' class='form-control' name='ssid'> </div><div class='form-floating'><br/><label>Password</label><input type='password' class='form-control' name='password'></div><br/><br/><button type='submit'>Save</button><p style='text-align: right'><a href='https://www.mrdiy.ca' style='color: #32C5FF'>mrdiy.ca</a></p></form></main> </body></html>");
  }
}