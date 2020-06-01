/****************************************************************************************************************************
   AutoConnectWithFSParameters.ino
   For ESP8266 / ESP32 boards

   ESP_WiFiManager is a library for the ESP8266/ESP32 platform (https://github.com/esp8266/Arduino) to enable easy
   configuration and reconfiguration of WiFi credentials using a Captive Portal.

   Forked from Tzapu https://github.com/tzapu/WiFiManager
   and from Ken Taylor https://github.com/kentaylor

   Built by Khoi Hoang https://github.com/khoih-prog/ESP_WiFiManager
   Licensed under MIT license
   Version: 1.0.7

   Version Modified By   Date      Comments
   ------- -----------  ---------- -----------
    1.0.0   K Hoang      07/10/2019 Initial coding
    1.0.1   K Hoang      13/12/2019 Fix bug. Add features. Add support for ESP32
    1.0.2   K Hoang      19/12/2019 Fix bug thatkeeps ConfigPortal in endless loop if Portal/Router SSID or Password is NULL.
    1.0.3   K Hoang      05/01/2020 Option not displaying AvailablePages in Info page. Enhance README.md. Modify examples
    1.0.4   K Hoang      07/01/2020 Add RFC952 setHostname feature.
    1.0.5   K Hoang      15/01/2020 Add configurable DNS feature. Thanks to @Amorphous of https://community.blynk.cc
    1.0.6   K Hoang      03/02/2020 Add support for ArduinoJson version 6.0.0+ ( tested with v6.14.1 )
    1.0.7   K Hoang      13/04/2020 Reduce start time, fix SPIFFS bug in examples, update README.md
 *****************************************************************************************************************************/

//Ported to ESP32
#ifdef ESP32
#include <FS.h>
#include "SPIFFS.h"
#include <esp_wifi.h>
#include <WiFi.h>
#include <WiFiClient.h>

#define ESP_getChipId()   ((uint32_t)ESP.getEfuseMac())

#define LED_BUILTIN       13
#define LED_ON            HIGH
#define LED_OFF           LOW

#else
#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>

#define ESP_getChipId()   (ESP.getChipId())

#define LED_ON      LOW
#define LED_OFF     HIGH
#endif

// Pin D2 mapped to pin GPIO2/ADC12 of ESP32, or GPIO2/TXD1 of NodeMCU control on-board LED
#define PIN_LED   25

#include <ESP_WiFiManager.h>              //https://github.com/khoih-prog/ESP_WiFiManager

// Now support ArduinoJson 6.0.0+ ( tested with v6.14.1 )
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

#include <IOXhop_FirebaseESP32.h>

#include <Servo.h>

char configFileName[] = "/config.json";

// SSID and PW for your Router
String Router_SSID;
String Router_Pass;

// SSID and PW for Config Portal
String AP_SSID;
String AP_PASS;

//define your default values here, if there are different values in configFileName (config.json), they are overwritten.
#define USER_EMAIL_LEN                128
#define DEVICE_NAME_LEN                 128

char user_email  [USER_EMAIL_LEN] ;
char device_name  [DEVICE_NAME_LEN];

//flag for saving data
bool shouldSaveConfig = false;

#define FIREBASE_HOST "beaver-house.firebaseio.com"                     //Your Firebase Project URL goes here without "http:" and "/"
#define FIREBASE_AUTH "qRqsm7LqDNxJlv6eaVasEPezYhxdloWzZYCZE46k"                       //Your Firebase Database Secret goes here                               

String devicestatus = "";                                                     // led status received from firebase
String mac = WiFi.macAddress();
String userpath = "";
String devicename = "";

Servo myservo;

int pos = 0;

//callback notifying us of the need to save config
void saveConfigCallback(void)
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

bool loadSPIFFSConfigFile(void)
{
  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("Mounting FS...");

  if (SPIFFS.begin())
  {
    Serial.println("Mounted file system");

    if (SPIFFS.exists(configFileName))
    {
      //file exists, reading and loading
      Serial.println("Reading config file");
      File configFile = SPIFFS.open(configFileName, "r");

      if (configFile)
      {
        Serial.print("Opened config file, size = ");
        size_t configFileSize = configFile.size();
        Serial.println(configFileSize);

        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[configFileSize + 1]);

        configFile.readBytes(buf.get(), configFileSize);

        Serial.print("\nJSON parseObject() result : ");

#if (ARDUINOJSON_VERSION_MAJOR >= 6)
        DynamicJsonDocument json(1024);
        auto deserializeError = deserializeJson(json, buf.get(), configFileSize);

        if ( deserializeError )
        {
          Serial.println("failed");
          return false;
        }
        else
        {
          Serial.println("OK");

          if (json["user_email"])
            strncpy(user_email, json["user_email"], sizeof(user_email));

          if (json["device_name"])
            strncpy(device_name, json["device_name"], sizeof(device_name));

        }

        //serializeJson(json, Serial);
        serializeJsonPretty(json, Serial);
#else
        DynamicJsonBuffer jsonBuffer;
        // Parse JSON string
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        // Test if parsing succeeds.

        if (json.success())
        {
          Serial.println("OK");

          if (json["user_email"])
            strncpy(user_email, json["user_email"], sizeof(user_email));

          if (json["device_name"])
            strncpy(device_name, json["device_name"], sizeof(device_name));

        }
        else
        {
          Serial.println("failed");
          return false;
        }
        //json.printTo(Serial);
        json.prettyPrintTo(Serial);
#endif

        configFile.close();
      }
    }
  }
  else
  {
    Serial.println("failed to mount FS");
    return false;
  }
  return true;
}

bool saveSPIFFSConfigFile(void)
{
  Serial.println("Saving config");

#if (ARDUINOJSON_VERSION_MAJOR >= 6)
  DynamicJsonDocument json(1024);
#else
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
#endif

  json["user_email"] = user_email;
  json["device_name"]   = device_name;

  File configFile = SPIFFS.open(configFileName, "w");

  if (!configFile)
  {
    Serial.println("Failed to open config file for writing");
  }

#if (ARDUINOJSON_VERSION_MAJOR >= 6)
  //serializeJson(json, Serial);
  serializeJsonPretty(json, Serial);
  // Write data to file and close it
  serializeJson(json, configFile);
#else
  //json.printTo(Serial);
  json.prettyPrintTo(Serial);
  // Write data to file and close it
  json.printTo(configFile);
#endif

  configFile.close();
  //end save
}

void heartBeatPrint(void)
{
  static int num = 1;

  if (WiFi.status() == WL_CONNECTED)
    Serial.print(".");        // H means connected to WiFi
  else
    Serial.print("?");        // F means not connected to WiFi

  if (num == 80)
  {
    Serial.println();
    num = 1;
  }
  else if (num++ % 10 == 0)
  {
    Serial.print(" ");
  }
}

void toggleLED()
{
  //toggle state
  digitalWrite(PIN_LED, !digitalRead(PIN_LED));
}

void check_status()
{
  static ulong checkstatus_timeout  = 0;
  static ulong LEDstatus_timeout    = 0;
  static ulong currentMillis;

#define HEARTBEAT_INTERVAL    10000L
#define LED_INTERVAL          2000L

  currentMillis = millis();

  if ((currentMillis > LEDstatus_timeout) || (LEDstatus_timeout == 0))
  {
    // Toggle LED at LED_INTERVAL = 2s
    toggleLED();
    LEDstatus_timeout = currentMillis + LED_INTERVAL;
  }

  // Print hearbeat every HEARTBEAT_INTERVAL (10) seconds.
  if ((currentMillis > checkstatus_timeout) || (checkstatus_timeout == 0))
  {
    heartBeatPrint();
    checkstatus_timeout = currentMillis + HEARTBEAT_INTERVAL;
  }
}

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("\nStarting AutoConnectWithFSParams");
  pinMode(12, OUTPUT);
  myservo.attach(13);

  loadSPIFFSConfigFile();

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  ESP_WMParameter custom_user_email("user_email", "E-mail", user_email, USER_EMAIL_LEN + 1);
  ESP_WMParameter custom_device_name("device_name",   "Nome do Dispositivo",   device_name,   DEVICE_NAME_LEN + 1);

  // Use this to default DHCP hostname to ESP8266-XXXXXX or ESP32-XXXXXX
  //ESP_WiFiManager ESP_wifiManager;
  // Use this to personalize DHCP hostname (RFC952 conformed)
  ESP_WiFiManager ESP_wifiManager("AutoConnect-FSParams");

  //set config save notify callback
  ESP_wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here
  ESP_wifiManager.addParameter(&custom_user_email);
  ESP_wifiManager.addParameter(&custom_device_name);

  //reset settings - for testing
  ESP_wifiManager.resetSettings();

  ESP_wifiManager.setDebugOutput(true);

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //ESP_wifiManager.setMinimumSignalQuality();

  //set custom ip for portal
  ESP_wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 100, 1), IPAddress(192, 168, 100, 1), IPAddress(255, 255, 255, 0));

  ESP_wifiManager.setMinimumSignalQuality(-1);
  // Set static IP, Gateway, Subnetmask, DNS1 and DNS2. New in v1.0.5+
  ESP_wifiManager.setSTAStaticIPConfig(IPAddress(10, 0 , 0, 80), IPAddress(10, 0, 0, 1), IPAddress(255, 255, 255, 0),
                                       IPAddress(10, 0, 0, 1), IPAddress(8, 8, 8, 8));

  // We can't use WiFi.SSID() in ESP32 as it's only valid after connected.
  // SSID and Password stored in ESP32 wifi_ap_record_t and wifi_config_t are also cleared in reboot
  // Have to create a new function to store in EEPROM/SPIFFS for this purpose
  Router_SSID = ESP_wifiManager.WiFi_SSID();
  Router_Pass = ESP_wifiManager.WiFi_Pass();

  //Remove this line if you do not want to see WiFi password printed
  Serial.println("\nStored: SSID = " + Router_SSID + ", Pass = " + Router_Pass);

  if (Router_SSID != "")
  {
    ESP_wifiManager.setConfigPortalTimeout(120); //If no access point name has been previously entered disable timeout.
    Serial.println("Got stored Credentials. Timeout 120s");
  }
  else
  {
    Serial.println("No stored Credentials. No timeout");
  }

  String chipID = String(ESP_getChipId(), HEX);
  chipID.toUpperCase();

  // SSID and PW for Config Portal
  AP_SSID = "BeaverHouse" + chipID;
  AP_PASS = chipID;

  // Get Router SSID and PASS from EEPROM, then open Config portal AP named "ESP_XXXXXX_AutoConnectAP" and PW "MyESP_XXXXXX"
  // 1) If got stored Credentials, Config portal timeout is 60s
  // 2) If no stored Credentials, stay in Config portal until get WiFi Credentials
  if (!ESP_wifiManager.autoConnect(AP_SSID.c_str(), AP_PASS.c_str()))
  {
    Serial.println("failed to connect and hit timeout");

    //reset and try again, or maybe put it to deep sleep
#ifdef ESP8266
    ESP.reset();
#else   //ESP32
    ESP.restart();
#endif
    delay(1000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("WiFi connected");

  //read updated parameters
  strncpy(user_email,   custom_user_email.getValue(),   sizeof(user_email));
  strncpy(device_name, custom_device_name.getValue(), sizeof(device_name));

  //save the custom parameters to FS
  if (shouldSaveConfig)
  {
    saveSPIFFSConfigFile();
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());

  //Inicia a conexão com o Firebase0
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);

  Serial.println(user_email);

  userpath = "/users/";
  userpath += user_email;
  userpath += "/devices/";
  userpath += mac;

  userpath.replace(".", ":");

  Firebase.setString(userpath + "/icon", "Lâmpada");
  Firebase.setString(userpath + "/mac", mac);
  Firebase.setString(userpath + "/name", device_name);
  Firebase.setString(userpath + "/room", "Nenhum");
  Firebase.setString(userpath + "/status", "desligado");

}

void loop()
{
  // put your main code here, to run repeatedly:
  check_status();

  devicestatus = Firebase.getString(userpath + "/status");                     // get led status input from firebase

  if (devicestatus == "ligado") {                         // compare the input of led status received from firebase
    Serial.println("Led Turned ON");
    digitalWrite(12, HIGH);
    myservo.write(180); 
  }

  else if (devicestatus == "desligado") {              // compare the input of led status received from firebase
    Serial.println("Led Turned OFF");
    digitalWrite(12, LOW);
    myservo.write(0); 
  }
  else {
    digitalWrite(12, LOW);
    myservo.write(0); 
  }
  
}
