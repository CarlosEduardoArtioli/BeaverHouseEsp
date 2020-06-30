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
#endif

// Pin D2 mapped to pin GPIO2/ADC12 of ESP32, or GPIO2/TXD1 of NodeMCU control on-board LED
#define PIN_LED       LED_BUILTIN

#include <ESP_WiFiManager.h>              //https://github.com/khoih-prog/ESP_WiFiManager

// Now support ArduinoJson 6.0.0+ ( tested with v6.14.1 )
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

#include <FirebaseESP32.h>

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
char* dayOfWeekNames[] = {"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sáb"};

//Servo
Servo myservo;
int pos = 0;

//Botão
#define botao 14

#define led 12

FirebaseData firebaseData;

long previousMillisLoop = 0;

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback(void)
{
  SPIFFS.begin(true);
  Serial.println("Deve salvar a configuração");
  shouldSaveConfig = true;
}

bool loadFileFSConfigFile(void)
{
  SPIFFS.begin(true);
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
  SPIFFS.begin(true);
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
  SPIFFS.begin(true);
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
  AP_SSID = "BeaverHouse" + chipID + "_" + device_name;
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

  Serial.println(Firebase.getString(firebaseData, userpath + "/name"));

  //IF para verificar se o nome existe ou não
  if ((Firebase.getString(firebaseData, userpath + "/name")) == 0) {
    //Caso não exista, "gera" a estrutura os dados no banco passados pelo usuário na configuração
    Firebase.setString(firebaseData, userpath + "/icon", "Lâmpada");
    Firebase.setString(firebaseData, userpath + "/mac", mac);
    Firebase.setString(firebaseData, userpath + "/name", device_name);
    Firebase.setString(firebaseData, userpath + "/room", "Nenhum");
    Firebase.setString(firebaseData, userpath + "/status", "desligado");
    Firebase.setString(firebaseData, userpath + "/timer", "");
  }

  else {
    //Caso exista, apenas troca o status do dispositivo para "desligado"
    Firebase.setString(firebaseData, userpath + "/status", "desligado");
    Firebase.setString(firebaseData, userpath + "/name", device_name);
  }

  if (!Firebase.beginStream(firebaseData, userpath))
  {
    Serial.println(firebaseData.errorReason());
  }

  verificaDados();

  setupNTP();

}


void loop()
{

  verificaDados();

  unsigned long currentMillisLoop = millis();

  if (currentMillisLoop - previousMillisLoop > 1000) {
    previousMillisLoop = currentMillisLoop;

    dataNTP();
  }

  check_status();

  deviceStatus();

  estadoBotao();
}

void verificaDados() {

  if (!Firebase.readStream(firebaseData))
  {
    Serial.println(firebaseData.errorReason());
  }

  if (firebaseData.streamTimeout())
  {
    Serial.println("Stream timeout, resume streaming...");
    Serial.println();
  }

  if (firebaseData.streamAvailable())
  {

    if (firebaseData.dataType() == "int")
      Serial.println(firebaseData.intData());
    else if (firebaseData.dataType() == "float")
      Serial.println(firebaseData.floatData(), 5);
    else if (firebaseData.dataType() == "double")
      printf("%.9lf\n", firebaseData.doubleData());
    else if (firebaseData.dataType() == "boolean")
      Serial.println(firebaseData.boolData() == 1 ? "true" : "false");
    else if (firebaseData.dataType() == "string") {
      Serial.println(firebaseData.stringData());
      Serial.println(firebaseData.dataPath());
      if (firebaseData.dataPath() == "/status") {
        devicestatus = firebaseData.stringData();
      }
      else if (firebaseData.dataPath() == "/timer") {
        devicetimer = firebaseData.stringData();
      }
    }
    else if (firebaseData.dataType() == "json") {
      FirebaseJson &json = firebaseData.jsonObject();
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

        if (key == "status") {
          devicestatus = value;
          Serial.println("status do dispositivo: " + devicestatus);
        }
        if (key == "timer") {
          devicetimer = value;
          Serial.println("timer do dispositivo: " + devicetimer);
        }
      }
      json.iteratorEnd();
    }
  }
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

  char datainteira[50];
  sprintf(datainteira, "%02d:%02d:%02d",
          date.hours,
          date.minutes,
          date.seconds);

  Serial.printf(datainteira);
  Serial.println ();


  if (devicetimer == datainteira) {
    if (devicestatus == "ligado") {
      Firebase.setString(firebaseData, userpath + "/status", "desligado");
    }
    else if (devicestatus == "desligado") {
      Firebase.setString(firebaseData, userpath + "/status", "ligado");
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
    Firebase.setString(firebaseData, userpath + "/status", "ligado");
    digitalWrite(led, HIGH);
  }
  else if ( estado_botao == HIGH && devicestatus == "ligado" )
  {
    Firebase.setString(firebaseData, userpath + "/status", "desligado");
    digitalWrite(led, LOW);
  }
}
