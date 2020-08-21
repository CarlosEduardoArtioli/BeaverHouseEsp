/****************************************************************************************************************************
   AutoConnectWithFSParameters.ino
   For ESP8266 / ESP32 boards

   ESP_WiFiManager is a library for the ESP8266/ESP32 platform (https://github.com/esp8266/Arduino) to enable easy
   configuration and reconfiguration of WiFi credentials using a Captive Portal.

   Modified from Tzapu https://github.com/tzapu/WiFiManager
   and from Ken Taylor https://github.com/kentaylor

   Built by Khoi Hoang https://github.com/khoih-prog/ESP_WiFiManager
   Licensed under MIT license
   Version: 1.0.8

   Version Modified By   Date      Comments
   ------- -----------  ---------- -----------
    1.0.0   K Hoang      07/10/2019 Initial coding
    1.0.1   K Hoang      13/12/2019 Fix bug. Add features. Add support for ESP32
    1.0.2   K Hoang      19/12/2019 Fix bug thatkeeps ConfigPortal in endless loop if Portal/Router SSID or Password is NULL.
    1.0.3   K Hoang      05/01/2020 Option not displaying AvailablePages in Info page. Enhance README.md. Modify examples
    1.0.4   K Hoang      07/01/2020 Add RFC952 setHostname feature.
    1.0.5   K Hoang      15/01/2020 Add configurable DNS feature. Thanks to @Amorphous of https://community.blynk.cc
    1.0.6   K Hoang      03/02/2020 Add support for ArduinoJson version 6.0.0+ ( tested with v6.14.1 )
    1.0.7   K Hoang      14/04/2020 Use just-in-time scanWiFiNetworks(). Fix bug relating SPIFFS in examples
    1.0.8   K Hoang      10/06/2020 Fix STAstaticIP issue. Restructure code. Add LittleFS support for ESP8266 core 2.7.1+
 *****************************************************************************************************************************/
#if !( defined(ESP8266) ||  defined(ESP32) )
#error This code is intended to run on the ESP8266 or ESP32 platform! Please check your Tools->Board setting.
#endif

#include <FS.h>

//Ported to ESP32
#ifdef ESP32
#include "SPIFFS.h"
#include <esp_wifi.h>
#include <WiFi.h>
#include <WiFiClient.h>

#define ESP_getChipId()   ((uint32_t)ESP.getEfuseMac())

#define LED_BUILTIN       2
#define LED_ON            HIGH
#define LED_OFF           LOW

#define FileFS            SPIFFS

#include <FirebaseESP32.h>
#include <FirebaseESP32HTTPClient.h>
#include <FirebaseJson.h>

#define led 12

#else

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>

#define ESP_getChipId()   (ESP.getChipId())

#define LED_ON            LOW
#define LED_OFF           HIGH

#define USE_LITTLEFS      true

#if USE_LITTLEFS
#define FileFS          LittleFS
#else
#define FileFS          SPIFFS
#endif

#include <LittleFS.h>

#include <FirebaseESP8266.h>
#include <FirebaseESP8266HTTPClient.h>
#include <FirebaseFS.h>
#include <FirebaseJson.h>

#define led 0
#endif

// Pin D2 mapped to pin GPIO2/ADC12 of ESP32, or GPIO2/TXD1 of NodeMCU control on-board LED
#define PIN_LED       LED_BUILTIN

#include <ESP_WiFiManager.h>              //https://github.com/khoih-prog/ESP_WiFiManager

// Now support ArduinoJson 6.0.0+ ( tested with v6.14.1 )
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

#include <Servo.h>

#include <NTPClient.h> //Biblioteca NTPClient modificada
#include <WiFiUdp.h> //Socket 

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

#define FIREBASE_HOST "beaver-house.firebaseio.com"                     //Your Firebase Project URL goes here without "http:" and "/"
#define FIREBASE_AUTH "qRqsm7LqDNxJlv6eaVasEPezYhxdloWzZYCZE46k"                       //Your Firebase Database Secret goes here                               

String devicestatus = "";                                                     // led status received from firebase
String mac = WiFi.macAddress();
String userpath = "";
String devicename = "";
String deviceicon = "";
String deviceroom = "";
String devicetimer = "";
String action;

String timer1action;
String timer1timer;
String timer1week1;
String timer1week2;
String timer1week3;
String timer1week4;
String timer1week5;
String timer1week6;
String timer1week7;

String timer2action;
String timer2timer;
String timer2week1;
String timer2week2;
String timer2week3;
String timer2week4;
String timer2week5;
String timer2week6;
String timer2week7;

String timer3action;
String timer3timer;
String timer3week1;
String timer3week2;
String timer3week3;
String timer3week4;
String timer3week5;
String timer3week6;
String timer3week7;

String timer4action;
String timer4timer;
String timer4week1;
String timer4week2;
String timer4week3;
String timer4week4;
String timer4week5;
String timer4week6;
String timer4week7;

//Fuso Horário, no caso horário de verão de Brasília
int timeZone = -3;

//Struct com os dados do dia e hora
struct Date {
  int dayOfWeek;
  int day;
  int month;
  int year;
  int hours;
  int minutes;
  int seconds;
};

//Socket UDP que a lib utiliza para recuperar dados sobre o horário
WiFiUDP udp;

//Objeto responsável por recuperar dados sobre horário
NTPClient ntpClient(
  udp,                    //socket udp
  "0.br.pool.ntp.org",    //URL do servwer NTP
  timeZone * 3600,        //Deslocamento do horário em relacão ao GMT 0
  60000);                 //Intervalo entre verificações online

//Nomes dos dias da semana
char* dayOfWeekNames[] = {"dom", "seg", "ter", "qua", "qui", "sex", "sab"};

//Servo
Servo myservo;
int pos = 0;

//Botão
#define botao 14

FirebaseData firebaseData1;
FirebaseData firebaseData2;

String parentPath = "/users/caduartioli@gmail:com/devices/24:62:AB:D7:C9:BC";
String childPath[5] = {"/timer/timer1", "/timer/timer2", "/timer/timer3", "/timer/timer4", "/status"};
size_t childPathSize = 5;

void printResult(FirebaseData &data);

void streamCallback(MultiPathStreamData stream)
{
  Serial.println();
  Serial.println("Stream Data1 available...");

  size_t numChild = sizeof(childPath) / sizeof(childPath[0]);

  for (size_t i = 0; i < numChild; i++)
  {
    if (stream.get(childPath[i]))
    {
      Serial.println("path: " + stream.dataPath + ", type: " + stream.type + ", value: " + stream.value);
      if (stream.dataPath == "/status") {
        devicestatus = stream.value;
      }
      if (stream.type == "json") {
        FirebaseJson &json = firebaseData1.jsonObject();
        //Print all object data
        Serial.println("Pretty printed JSON data:");
        String jsonStr;
        json.toString(jsonStr, true);
        Serial.println(jsonStr);
        Serial.println();
        Serial.println("Iterate JSON data:");
        Serial.println();
        size_t len = json.iteratorBegin();
        String key, value = "";
        int type = 0;
        for (size_t i = 0; i < len; i++)
        {
          json.iteratorGet(i, type, key, value);
          Serial.print(i);
          Serial.print(", ");
          Serial.print("Type: ");
          Serial.print(type == FirebaseJson::JSON_OBJECT ? "object" : "array");
          if (type == FirebaseJson::JSON_OBJECT)
          {
            Serial.print(", Key: ");
            Serial.print(key);
          }
          Serial.print(", Value: ");
          Serial.println(value);

          if (stream.dataPath == "/timer/timer1") {
            if (key == "action") {
              timer1action = value;
              Serial.println(timer1action);
            }
            if (key == "timer") {
              timer1timer = value;
            }
            if (key == "week1") {
              timer1week1 = value;
            }
            if (key == "week2") {
              timer1week2 = value;
            }
            if (key == "week3") {
              timer1week3 = value;
            }
            if (key == "week4") {
              timer1week4 = value;
            }
            if (key == "week5") {
              timer1week5 = value;
            }
            if (key == "week6") {
              timer1week6 = value;
            }
            if (key == "week7") {
              timer1week7 = value;
            }
          }
          if (stream.dataPath == "/timer/timer2") {
            if (key == "action") {
              timer2action = value;
            }
            if (key == "timer") {
              timer2timer = value;
            }
            if (key == "week1") {
              timer2week1 = value;
            }
            if (key == "week2") {
              timer2week2 = value;
            }
            if (key == "week3") {
              timer2week3 = value;
            }
            if (key == "week4") {
              timer2week4 = value;
            }
            if (key == "week5") {
              timer2week5 = value;
            }
            if (key == "week6") {
              timer2week6 = value;
            }
            if (key == "week7") {
              timer2week7 = value;
            }
          }
          if (stream.dataPath == "/timer/timer3") {
            if (key == "action") {
              timer3action = value;
            }
            if (key == "timer") {
              timer3timer = value;
            }
            if (key == "week1") {
              timer3week1 = value;
            }
            if (key == "week2") {
              timer3week2 = value;
            }
            if (key == "week3") {
              timer3week3 = value;
            }
            if (key == "week4") {
              timer3week4 = value;
            }
            if (key == "week5") {
              timer3week5 = value;
            }
            if (key == "week6") {
              timer3week6 = value;
            }
            if (key == "week7") {
              timer3week7 = value;
            }
          }
          if (stream.dataPath == "/timer/timer4") {
            if (key == "action") {
              timer4action = value;
            }
            if (key == "timer") {
              timer4timer = value;
            }
            if (key == "week1") {
              timer4week1 = value;
            }
            if (key == "week2") {
              timer4week2 = value;
            }
            if (key == "week3") {
              timer4week3 = value;
            }
            if (key == "week4") {
              timer4week4 = value;
            }
            if (key == "week5") {
              timer4week5 = value;
            }
            if (key == "week6") {
              timer4week6 = value;
            }
            if (key == "week7") {
              timer4week7 = value;
            }
          }
        }
        json.iteratorEnd();
      }
    }
    Serial.println();
  }
}
void streamTimeoutCallback(bool timeout)
{
  if (timeout)
  {
    Serial.println();
    Serial.println("Stream timeout, resume streaming...");
    Serial.println();
  }
}

long previousMillisLoop = 0;

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback(void)
{
  Serial.println("Deve salvar a configuração");
  shouldSaveConfig = true;
}

bool loadFileFSConfigFile(void)
{
  //clean FS, for testing
  //FileFS.format();

  //read configuration from FS json
  Serial.println("Montando FS...");

  if (FileFS.begin())
  {
    Serial.println("File system montado");

    if (FileFS.exists(configFileName))
    {
      //file exists, reading and loading
      Serial.println("Lendo arquivo de configuração");
      File configFile = FileFS.open(configFileName, "r");

      if (configFile)
      {
        Serial.print("Arquivo de configuração aberto, tamanho = ");
        size_t configFileSize = configFile.size();
        Serial.println(configFileSize);

        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[configFileSize + 1]);

        configFile.readBytes(buf.get(), configFileSize);

        Serial.print("\nJSON parseObject() resultado : ");

#if (ARDUINOJSON_VERSION_MAJOR >= 6)
        DynamicJsonDocument json(1024);
        auto deserializeError = deserializeJson(json, buf.get(), configFileSize);

        if ( deserializeError )
        {
          Serial.println("Falha");
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
          Serial.println("Falha");
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
    Serial.println("Falha ao montar FS");
    return false;
  }
  return true;
}

bool saveFileFSConfigFile(void)
{
  Serial.println("Salvando configuração");

#if (ARDUINOJSON_VERSION_MAJOR >= 6)
  DynamicJsonDocument json(1024);
#else
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
#endif

  json["user_email"] = user_email;
  json["device_name"] = device_name;

  File configFile = FileFS.open(configFileName, "w");

  if (!configFile)
  {
    Serial.println("Falha ao abrir o arquivo de configuração para a escrita");
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
    Serial.print(".");        // . means connected to WiFi
  else
    Serial.print("?");        // ? means not connected to WiFi

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
  Serial.println("\nIniciando AutoConnectWithFSParams");
  pinMode(led, OUTPUT);
  myservo.attach(13);
  pinMode(botao, INPUT);

  loadFileFSConfigFile();

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  ESP_WMParameter custom_user_email("user_email", "E-mail", user_email, USER_EMAIL_LEN + 1);
  ESP_WMParameter custom_device_name("device_name",   "Nome do Dispositivo",   device_name,   DEVICE_NAME_LEN + 1);

  // Use this to default DHCP hostname to ESP8266-XXXXXX or ESP32-XXXXXX
  //ESP_WiFiManager ESP_wifiManager;
  // Use this to personalize DHCP hostname (RFC952 conformed)
  ESP_WiFiManager ESP_wifiManager("BeaverHouse");

  //set config save notify callback
  ESP_wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here
  ESP_wifiManager.addParameter(&custom_user_email);
  ESP_wifiManager.addParameter(&custom_device_name);

  //reset settings - for testing
  //ESP_wifiManager.resetSettings();

  ESP_wifiManager.setDebugOutput(true);

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //ESP_wifiManager.setMinimumSignalQuality();

  //set custom ip for portal
  ESP_wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 100, 1), IPAddress(192, 168, 100, 1), IPAddress(255, 255, 255, 0));

  ESP_wifiManager.setMinimumSignalQuality(-1);

  // We can't use WiFi.SSID() in ESP32 as it's only valid after connected.
  // SSID and Password stored in ESP32 wifi_ap_record_t and wifi_config_t are also cleared in reboot
  // Have to create a new function to store in EEPROM/SPIFFS/LittleFS for this purpose
  Router_SSID = ESP_wifiManager.WiFi_SSID();
  Router_Pass = ESP_wifiManager.WiFi_Pass();

  //Remove this line if you do not want to see WiFi password printed
  Serial.println("\nSalvo: SSID = " + Router_SSID + ", Pass = " + Router_Pass);

  if (Router_SSID != "")
  {
    ESP_wifiManager.setConfigPortalTimeout(20); //If no access point name has been previously entered disable timeout.
    Serial.println("Obteve credenciais armazenadas. Tempo limite de 20s");
  }
  else
  {
    Serial.println("Nenhuma credencial armazenada. Sem tempo limite");
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
    Serial.println("Falha ao conectar e atingir o tempo limite");

    //reset and try again, or maybe put it to deep sleep
#ifdef ESP8266
    ESP.reset();
#else   //ESP32
    ESP.restart();
#endif
    delay(1000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("WiFi conectado");

  //read updated parameters
  strncpy(user_email, custom_user_email.getValue(), sizeof(user_email));
  strncpy(device_name, custom_device_name.getValue(), sizeof(device_name));

  //save the custom parameters to FS
  if (shouldSaveConfig)
  {
    saveFileFSConfigFile();
  }

  Serial.println("IP local");
  Serial.println(WiFi.localIP());


  //Inicia a conexão com o Firebase
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);

  Firebase.reconnectWiFi(true);

  //"Gerando" caminho do usuário do banco
  userpath = "/users/";
  userpath += user_email;
  userpath += "/devices/";
  userpath += mac;

  Serial.println(userpath);

  //Trocando "." por ":" no caminho do usuário para não ter incopatibilidade
  userpath.replace(".", ":");

  Serial.println(Firebase.getString(firebaseData2, userpath + "/name"));

  //IF para verificar se o nome existe ou não
  if ((Firebase.getString(firebaseData2, userpath + "/name")) == 0) {
    //Caso não exista, "gera" a estrutura os dados no banco passados pelo usuário na configuração
    Firebase.setString(firebaseData2, userpath + "/icon", "Hardware");
    Firebase.setString(firebaseData2, userpath + "/mac", mac);
    Firebase.setString(firebaseData2, userpath + "/iconRoom", "Casa");
    Firebase.setString(firebaseData2, userpath + "/ap", "BeaverHouse" + chipID);
    Firebase.setString(firebaseData2, userpath + "/name", device_name);
    Firebase.setString(firebaseData2, userpath + "/room", "Nenhum");
    Firebase.setString(firebaseData2, userpath + "/status", "desligado");

    Firebase.setString(firebaseData2, userpath + "/timer/timer1/timer", "");
    Firebase.setBool(firebaseData2, userpath + "/timer/timer1/show", false);
    Firebase.setString(firebaseData2, userpath + "/timer/timer1/action", "");
    Firebase.setString(firebaseData2, userpath + "/timer//timer1/week1", "");
    Firebase.setString(firebaseData2, userpath + "/timer//timer1/week2", "");
    Firebase.setString(firebaseData2, userpath + "/timer//timer1/week3", "");
    Firebase.setString(firebaseData2, userpath + "/timer//timer1/week4", "");
    Firebase.setString(firebaseData2, userpath + "/timer//timer1/week5", "");
    Firebase.setString(firebaseData2, userpath + "/timer//timer1/week6", "");
    Firebase.setString(firebaseData2, userpath + "/timer//timer1//week7", "");

    Firebase.setString(firebaseData2, userpath + "/timer//timer2/timer", "");
    Firebase.setBool(firebaseData2, userpath + "/timer/timer2/show", false);
    Firebase.setString(firebaseData2, userpath + "/timer/timer2/action", "");
    Firebase.setString(firebaseData2, userpath + "/timer//timer2/week1", "");
    Firebase.setString(firebaseData2, userpath + "/timer//timer2/week2", "");
    Firebase.setString(firebaseData2, userpath + "/timer//timer2/week3", "");
    Firebase.setString(firebaseData2, userpath + "/timer//timer2/week4", "");
    Firebase.setString(firebaseData2, userpath + "/timer//timer2/week5", "");
    Firebase.setString(firebaseData2, userpath + "/timer//timer2/week6", "");
    Firebase.setString(firebaseData2, userpath + "/timer//timer2//week7", "");

    Firebase.setString(firebaseData2, userpath + "/timer//timer3/timer", "");
    Firebase.setBool(firebaseData2, userpath + "/timer/timer3/show", false);
    Firebase.setString(firebaseData2, userpath + "/timer/timer3/action", "");
    Firebase.setString(firebaseData2, userpath + "/timer//timer3/week1", "");
    Firebase.setString(firebaseData2, userpath + "/timer//timer3/week2", "");
    Firebase.setString(firebaseData2, userpath + "/timer//timer3/week3", "");
    Firebase.setString(firebaseData2, userpath + "/timer//timer3/week4", "");
    Firebase.setString(firebaseData2, userpath + "/timer//timer3/week5", "");
    Firebase.setString(firebaseData2, userpath + "/timer//timer3/week6", "");
    Firebase.setString(firebaseData2, userpath + "/timer//timer3//week7", "");

    Firebase.setString(firebaseData2, userpath + "/timer//timer4/timer", "");
    Firebase.setBool(firebaseData2, userpath + "/timer/timer4/show", false);
    Firebase.setString(firebaseData2, userpath + "/timer/timer4/action", "");
    Firebase.setString(firebaseData2, userpath + "/timer//timer4/week1", "");
    Firebase.setString(firebaseData2, userpath + "/timer//timer4/week2", "");
    Firebase.setString(firebaseData2, userpath + "/timer//timer4/week3", "");
    Firebase.setString(firebaseData2, userpath + "/timer//timer4/week4", "");
    Firebase.setString(firebaseData2, userpath + "/timer//timer4/week5", "");
    Firebase.setString(firebaseData2, userpath + "/timer//timer4/week6", "");
    Firebase.setString(firebaseData2, userpath + "/timer//timer4//week7", "");
  }

  else {
    //Caso exista, apenas troca o status do dispositivo para "desligado"
    Firebase.setString(firebaseData2, userpath + "/status", "desligado");
    Firebase.setString(firebaseData2, userpath + "/name", device_name);
  }

  if (!Firebase.beginMultiPathStream(firebaseData1, parentPath, childPath, childPathSize))
  {
    Serial.println("------------------------------------");
    Serial.println("Can't begin stream connection...");
    Serial.println("REASON: " + firebaseData1.errorReason());
    Serial.println("------------------------------------");
    Serial.println();
  }

  Firebase.setMultiPathStreamCallback(firebaseData1, streamCallback, streamTimeoutCallback);

  setupNTP();

}


void loop()
{

  unsigned long currentMillisLoop = millis();

  if (currentMillisLoop - previousMillisLoop > 1000) {
    previousMillisLoop = currentMillisLoop;

    dataNTP();

  }

  check_status();

  deviceStatus();

  estadoBotao();
}

void setupNTP() {
  //Inicializa o client NTP
  ntpClient.begin();

  //Espera pelo primeiro update online
  Serial.println("Esperando pelo primeiro update (ntpClient)");
  while (!ntpClient.update())
  {
    Serial.print(".");
    ntpClient.forceUpdate();
    delay(500);
  }

  Serial.println();
  Serial.println("Primeiro update completo (ntpClient)");
}

void dataNTP() {
  //Recupera os dados sobre a data e horário
  Date date = getDate();


  /*char datainteira1[50];
    sprintf(datainteira1, "\n\n %s %02d/%02d/%d %02d:%02d:%02d",
          dayOfWeekNames[date.dayOfWeek],
          date.day,
          date.month,
          date.year,
          date.hours,
          date.minutes,
          date.seconds);*/

  char horario[50];
  sprintf(horario, "%02d:%02d:%02d",
          date.hours,
          date.minutes,
          date.seconds);

  char horariosemseg[50];
  sprintf(horariosemseg, "%02d:%02d",
          date.hours,
          date.minutes);

  char diadasemana[50];
  sprintf(diadasemana, "%s",
          dayOfWeekNames[date.dayOfWeek]);

  Serial.printf(diadasemana);
  Serial.printf(horario);
  Serial.println ();


  if ( timer1timer == horario || timer1timer == horariosemseg) {
    if (timer1week1 == diadasemana || timer1week2 == diadasemana || timer1week3 == diadasemana || timer1week3 == diadasemana || timer1week4 == diadasemana || timer1week5 == diadasemana || timer1week6 == diadasemana || timer1week7 == diadasemana) {
      if (timer1action == "ligar") {
        devicestatus = "ligado";
        Firebase.setString(firebaseData2, userpath + "/status", "ligado");
      }
      else if (timer1action == "desligar") {
        devicestatus = "desligado";
        Firebase.setString(firebaseData2, userpath + "/status", "desligado");
      }
    }
  }

  if ( timer2timer == horario  || timer2timer == horariosemseg) {
    if (timer2week1 == diadasemana || timer2week2 == diadasemana || timer2week3 == diadasemana || timer2week3 == diadasemana || timer2week4 == diadasemana || timer2week5 == diadasemana || timer2week6 == diadasemana || timer2week7 == diadasemana) {
      if (timer2action == "ligar") {
        devicestatus = "ligado";
        Firebase.setString(firebaseData2, userpath + "/status", "ligado");
      }
      else if (timer2action == "desligar") {
        devicestatus = "desligado";
        Firebase.setString(firebaseData2, userpath + "/status", "desligado");
      }
    }
  }

  if ( timer3timer == horario || timer3timer == horariosemseg) {
    if (timer3week1 == diadasemana || timer3week2 == diadasemana || timer3week3 == diadasemana || timer3week3 == diadasemana || timer3week4 == diadasemana || timer3week5 == diadasemana || timer3week6 == diadasemana || timer3week7 == diadasemana) {
      if (timer3action == "ligar") {
        devicestatus = "ligado";
        Firebase.setString(firebaseData2, userpath + "/status", "ligado");
      }
      else if (timer3action == "desligar") {
        devicestatus = "desligado";
        Firebase.setString(firebaseData2, userpath + "/status", "desligado");
      }
    }
  }

  if ( timer4timer == horario || timer4timer == horariosemseg) {
    if (timer4week1 == diadasemana || timer4week2 == diadasemana || timer4week3 == diadasemana || timer4week3 == diadasemana || timer4week4 == diadasemana || timer4week5 == diadasemana || timer4week6 == diadasemana || timer4week7 == diadasemana) {
      if (timer4action == "ligar") {
        devicestatus = "ligado";
        Firebase.setString(firebaseData2, userpath + "/status", "ligado");
      }
      else if (timer4action == "desligar") {
        devicestatus = "desligado";
        Firebase.setString(firebaseData2, userpath + "/status", "desligado");
      }
    }
  }
}

Date getDate() {
  //Recupera os dados de data e horário usando o client NTP
  char* strDate = (char*)ntpClient.getFormattedDate().c_str();

  //Passa os dados da string para a struct
  Date date;
  sscanf(strDate, "%d-%d-%dT%d:%d:%dZ",
         &date.year,
         &date.month,
         &date.day,
         &date.hours,
         &date.minutes,
         &date.seconds);

  //Dia da semana de 0 a 6, sendo 0 o domingo
  date.dayOfWeek = ntpClient.getDay();
  return date;
}

void deviceStatus() {

  if (devicestatus == "ligado") {
    digitalWrite(led, HIGH);
    myservo.write(180);
  }

  else if (devicestatus == "desligado") {
    digitalWrite(led, LOW);
    myservo.write(0);
  }
  else {
    digitalWrite(led, LOW);
    myservo.write(0);
  }
}

void estadoBotao() {

  int estado_botao = digitalRead(botao);

  if ( estado_botao == HIGH && devicestatus == "desligado" )
  {
    digitalWrite(led, HIGH);
    myservo.write(180);
    devicestatus = "ligado";
    Firebase.setString(firebaseData2, userpath + "/status", "ligado");
  }
  else if ( estado_botao == HIGH && devicestatus == "ligado" )
  {
    digitalWrite(led, LOW);
    myservo.write(0);
    devicestatus = "desligado";
    Firebase.setString(firebaseData2, userpath + "/status", "desligado");
  }
}
