#if !( defined(ESP8266) ||  defined(ESP32) )
#error This code is intended to run on the ESP8266 or ESP32 platform! Please check your Tools->Board setting.
#endif

// Use from 0 to 4. Higher number, more debugging messages and memory usage.
#define _WIFIMGR_LOGLEVEL_    3

#include <FS.h>

//Ported to ESP32
#ifdef ESP32
#include <esp_wifi.h>
#include <WiFi.h>
#include <WiFiClient.h>

// From v1.1.0
#include <WiFiMulti.h>
WiFiMulti wifiMulti;

#define USE_SPIFFS      true

#if USE_SPIFFS
#include <SPIFFS.h>
FS* filesystem =      &SPIFFS;
#define FileFS        SPIFFS
#define FS_Name       "SPIFFS"
#else
// Use FFat
#include <FFat.h>
FS* filesystem =      &FFat;
#define FileFS        FFat
#define FS_Name       "FFat"
#endif
//////

#define ESP_getChipId()   ((uint32_t)ESP.getEfuseMac())

#define LED_BUILTIN       2
#define LED_ON            HIGH
#define LED_OFF           LOW

#include <FirebaseESP32.h>
#define led 12

#else
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>

// From v1.1.0
#include <ESP8266WiFiMulti.h>
ESP8266WiFiMulti wifiMulti;

#define USE_LITTLEFS      true

#if USE_LITTLEFS
#include <LittleFS.h>
FS* filesystem = &LittleFS;
#define FileFS    LittleFS
#define FS_Name       "LittleFS"
#else
FS* filesystem = &SPIFFS;
#define FileFS    SPIFFS
#define FS_Name       "SPIFFS"
#endif
//////

#define ESP_getChipId()   (ESP.getChipId())

#define LED_ON      LOW
#define LED_OFF     HIGH

#include <FirebaseESP8266.h>
#define led 0

#endif

// Pin D2 mapped to pin GPIO2/ADC12 of ESP32, or GPIO2/TXD1 of NodeMCU control on-board LED
#define PIN_LED       LED_BUILTIN

// Now support ArduinoJson 6.0.0+ ( tested with v6.14.1 )
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <Servo.h>
#include <NTPClient.h> //Biblioteca NTPClient modificada
#include <WiFiUdp.h> //Socket 

#define FIREBASE_HOST "beaver-house.firebaseio.com"                     //Your Firebase Project URL goes here without "http:" and "/"
#define FIREBASE_AUTH "qRqsm7LqDNxJlv6eaVasEPezYhxdloWzZYCZE46k"                       //Your Firebase Database Secret goes here                               

String devicestatus = "";                                                     // led status received from firebase
String mac = WiFi.macAddress();
String userpath = "";

String timers;
String timers_init;

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
FirebaseData firebaseData3;

String parentPath;
String childPath[3] = {"/status", "/timernumber", "/timer"};
size_t childPathSize = 3;

void printResult(FirebaseData &data);

long previousMillisLoop = 0;

char configFileName[] = "/config.json";

// You only need to format the filesystem once
//#define FORMAT_FILESYSTEM       true
#define FORMAT_FILESYSTEM         false

// SSID and PW for your Router
String Router_SSID;
String Router_Pass;

// From v1.1.0
#define MIN_AP_PASSWORD_SIZE    8

#define SSID_MAX_LEN            32
//From v1.0.10, WPA2 passwords can be up to 63 characters long.
#define PASS_MAX_LEN            64

typedef struct
{
  char wifi_ssid[SSID_MAX_LEN];
  char wifi_pw  [PASS_MAX_LEN];
}  WiFi_Credentials;

typedef struct
{
  String wifi_ssid;
  String wifi_pw;
}  WiFi_Credentials_String;

#define NUM_WIFI_CREDENTIALS      2

typedef struct
{
  WiFi_Credentials  WiFi_Creds [NUM_WIFI_CREDENTIALS];
} WM_Config;

WM_Config         WM_config;

#define  CONFIG_FILENAME              F("/wifi_cred.dat")

// Indicates whether ESP has WiFi credentials saved from previous session, or double reset detected
bool initialConfig = false;
//////

// SSID and PW for Config Portal
String AP_SSID;
String AP_PASS;

// Use false if you don't like to display Available Pages in Information Page of Config Portal
// Comment out or use true to display Available Pages in Information Page of Config Portal
// Must be placed before #include <ESP_WiFiManager.h>
#define USE_AVAILABLE_PAGES     false

// From v1.0.10 to permit disable/enable StaticIP configuration in Config Portal from sketch. Valid only if DHCP is used.
// You'll loose the feature of dynamically changing from DHCP to static IP, or vice versa
// You have to explicitly specify false to disable the feature.
#define USE_STATIC_IP_CONFIG_IN_CP          false

// Use false to disable NTP config. Advisable when using Cellphone, Tablet to access Config Portal.
// See Issue 23: On Android phone ConfigPortal is unresponsive (https://github.com/khoih-prog/ESP_WiFiManager/issues/23)
#define USE_ESP_WIFIMANAGER_NTP     false

// Use true to enable CloudFlare NTP service. System can hang if you don't have Internet access while accessing CloudFlare
// See Issue #21: CloudFlare link in the default portal (https://github.com/khoih-prog/ESP_WiFiManager/issues/21)
#define USE_CLOUDFLARE_NTP          false

// New in v1.0.11
#define USING_CORS_FEATURE          true
//////

// Use USE_DHCP_IP == true for dynamic DHCP IP, false to use static IP which you have to change accordingly to your network
#if (defined(USE_STATIC_IP_CONFIG_IN_CP) && !USE_STATIC_IP_CONFIG_IN_CP)
// Force DHCP to be true
#if defined(USE_DHCP_IP)
#undef USE_DHCP_IP
#endif
#define USE_DHCP_IP     true
#else
// You can select DHCP or Static IP here
//#define USE_DHCP_IP     true
#define USE_DHCP_IP     false
#endif

#if ( USE_DHCP_IP || ( defined(USE_STATIC_IP_CONFIG_IN_CP) && !USE_STATIC_IP_CONFIG_IN_CP ) )
// Use DHCP
#warning Using DHCP IP
IPAddress stationIP   = IPAddress(0, 0, 0, 0);
IPAddress gatewayIP   = IPAddress(192, 168, 2, 1);
IPAddress netMask     = IPAddress(255, 255, 255, 0);
#else
// Use static IP
#warning Using static IP

#ifdef ESP32
IPAddress stationIP   = IPAddress(192, 168, 2, 232);
#else
IPAddress stationIP   = IPAddress(192, 168, 2, 186);
#endif

IPAddress gatewayIP   = IPAddress(192, 168, 2, 1);
IPAddress netMask     = IPAddress(255, 255, 255, 0);
#endif

#define USE_CONFIGURABLE_DNS      true

IPAddress dns1IP      = gatewayIP;
IPAddress dns2IP      = IPAddress(8, 8, 8, 8);

#include <ESP_WiFiManager.h>              //https://github.com/khoih-prog/ESP_WiFiManager

//define your default values here, if there are different values in configFileName (config.json), they are overwritten.
#define USER_EMAIL_LEN                128

char user_email  [USER_EMAIL_LEN] = "example@email.com" ;

// Function Prototypes
uint8_t connectMultiWiFi(void);

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback(void)
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

bool loadFileFSConfigFile(void)
{
  //clean FS, for testing
  //FileFS.format();

  //read configuration from FS json
  Serial.println("Mounting FS...");

  if (FileFS.begin())
  {
    Serial.println("Mounted file system");

    if (FileFS.exists(configFileName))
    {
      //file exists, reading and loading
      Serial.println("Reading config file");
      File configFile = FileFS.open(configFileName, "r");

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

bool saveFileFSConfigFile(void)
{
  Serial.println("Saving config");

#if (ARDUINOJSON_VERSION_MAJOR >= 6)
  DynamicJsonDocument json(1024);
#else
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
#endif

  json["user_email"] = user_email;

  File configFile = FileFS.open(configFileName, "w");

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
    Serial.print("H");        // H means connected to WiFi
  else
    Serial.print("F");        // F means not connected to WiFi

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

void check_WiFi(void)
{
  if ( (WiFi.status() != WL_CONNECTED) )
  {
    Serial.println("\nWiFi lost. Call connectMultiWiFi in loop");
    connectMultiWiFi();
  }
}

void check_status()
{
  static ulong checkstatus_timeout  = 0;
  static ulong LEDstatus_timeout    = 0;
  static ulong checkwifi_timeout    = 0;

  static ulong currentMillis;

#define HEARTBEAT_INTERVAL    10000L
#define LED_INTERVAL          2000L
#define WIFICHECK_INTERVAL    1000L

  currentMillis = millis();

  // Check WiFi every WIFICHECK_INTERVAL (1) seconds.
  if ((currentMillis > checkwifi_timeout) || (checkwifi_timeout == 0))
  {
    check_WiFi();
    checkwifi_timeout = currentMillis + WIFICHECK_INTERVAL;
  }

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

void loadConfigData(void)
{
  File file = FileFS.open(CONFIG_FILENAME, "r");
  LOGERROR(F("LoadWiFiCfgFile "));

  if (file)
  {
    file.readBytes((char *) &WM_config, sizeof(WM_config));
    file.close();
    LOGERROR(F("OK"));
  }
  else
  {
    LOGERROR(F("failed"));
  }
}

void saveConfigData(void)
{
  File file = FileFS.open(CONFIG_FILENAME, "w");
  LOGERROR(F("SaveWiFiCfgFile "));

  if (file)
  {
    file.write((uint8_t*) &WM_config, sizeof(WM_config));
    file.close();
    LOGERROR(F("OK"));
  }
  else
  {
    LOGERROR(F("failed"));
  }
}

uint8_t connectMultiWiFi(void)
{
#if ESP32
  // For ESP32, this better be 0 to shorten the connect time
#define WIFI_MULTI_1ST_CONNECT_WAITING_MS       0
#else
  // For ESP8266, this better be 2200 to enable connect the 1st time
#define WIFI_MULTI_1ST_CONNECT_WAITING_MS       2200L
#endif

#define WIFI_MULTI_CONNECT_WAITING_MS           100L

  uint8_t status;

  LOGERROR(F("ConnectMultiWiFi with :"));

  if ( (Router_SSID != "") && (Router_Pass != "") )
  {
    LOGERROR3(F("* Flash-stored Router_SSID = "), Router_SSID, F(", Router_Pass = "), Router_Pass );
  }

  for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++)
  {
    // Don't permit NULL SSID and password len < MIN_AP_PASSWORD_SIZE (8)
    if ( (String(WM_config.WiFi_Creds[i].wifi_ssid) != "") && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE) )
    {
      LOGERROR3(F("* Additional SSID = "), WM_config.WiFi_Creds[i].wifi_ssid, F(", PW = "), WM_config.WiFi_Creds[i].wifi_pw );
    }
  }

  LOGERROR(F("Connecting MultiWifi..."));

  WiFi.mode(WIFI_STA);

#if !USE_DHCP_IP
#if USE_CONFIGURABLE_DNS
  // Set static IP, Gateway, Subnetmask, DNS1 and DNS2. New in v1.0.5
  WiFi.config(stationIP, gatewayIP, netMask, dns1IP, dns2IP);
#else
  // Set static IP, Gateway, Subnetmask, Use auto DNS1 and DNS2.
  WiFi.config(stationIP, gatewayIP, netMask);
#endif
#endif

  int i = 0;
  status = wifiMulti.run();
  delay(WIFI_MULTI_1ST_CONNECT_WAITING_MS);

  while ( ( i++ < 10 ) && ( status != WL_CONNECTED ) )
  {
    status = wifiMulti.run();

    if ( status == WL_CONNECTED )
      break;
    else
      delay(WIFI_MULTI_CONNECT_WAITING_MS);
  }

  if ( status == WL_CONNECTED )
  {
    LOGERROR1(F("WiFi connected after time: "), i);
    LOGERROR3(F("SSID:"), WiFi.SSID(), F(",RSSI="), WiFi.RSSI());
    LOGERROR3(F("Channel:"), WiFi.channel(), F(",IP address:"), WiFi.localIP() );
  }
  else
    LOGERROR(F("WiFi not connected"));

  return status;
}

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(led, OUTPUT);
  myservo.attach(13);
  pinMode(botao, INPUT);
  while (!Serial);

  Serial.print("\nStarting AutoConnectWithFSParams using " + String(FS_Name));
  Serial.println(" on " + String(ARDUINO_BOARD));

  if (FORMAT_FILESYSTEM)
    FileFS.format();

  // Format FileFS if not yet
#ifdef ESP32
  if (!FileFS.begin(true))
#else
  if (!FileFS.begin())
#endif
  {
    Serial.print(FS_Name);
    Serial.println(F(" failed! AutoFormatting."));

#ifdef ESP8266
    FileFS.format();
#endif
  }

  loadFileFSConfigFile();

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  ESP_WMParameter custom_user_email("user_email", "E-mail", user_email, USER_EMAIL_LEN + 1);

  unsigned long startedAt = millis();

  // Use this to default DHCP hostname to ESP8266-XXXXXX or ESP32-XXXXXX
  //ESP_WiFiManager ESP_wifiManager;
  // Use this to personalize DHCP hostname (RFC952 conformed)
  ESP_WiFiManager ESP_wifiManager("BeaverHouse");

  //set config save notify callback
  ESP_wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here
  ESP_wifiManager.addParameter(&custom_user_email);

  //reset settings - for testing
  //ESP_wifiManager.resetSettings();

  ESP_wifiManager.setDebugOutput(true);

  //set custom ip for portal
  ESP_wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 100, 1), IPAddress(192, 168, 100, 1), IPAddress(255, 255, 255, 0));

  //set minimum quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //ESP_wifiManager.setMinimumSignalQuality();
  ESP_wifiManager.setMinimumSignalQuality(-1);

  // From v1.0.10 only
  // Set config portal channel, default = 1. Use 0 => random channel from 1-13
  ESP_wifiManager.setConfigPortalChannel(0);
  //////

#if !USE_DHCP_IP
#if USE_CONFIGURABLE_DNS
  // Set static IP, Gateway, Subnetmask, DNS1 and DNS2. New in v1.0.5
  ESP_wifiManager.setSTAStaticIPConfig(stationIP, gatewayIP, netMask, dns1IP, dns2IP);
#else
  // Set static IP, Gateway, Subnetmask, Use auto DNS1 and DNS2.
  ESP_wifiManager.setSTAStaticIPConfig(stationIP, gatewayIP, netMask);
#endif
#endif

  // New from v1.1.1
#if USING_CORS_FEATURE
  ESP_wifiManager.setCORSHeader("Your Access-Control-Allow-Origin");
#endif

  // We can't use WiFi.SSID() in ESP32 as it's only valid after connected.
  // SSID and Password stored in ESP32 wifi_ap_record_t and wifi_config_t are also cleared in reboot
  // Have to create a new function to store in EEPROM/SPIFFS/LittleFS for this purpose
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

  // From v1.1.0, Don't permit NULL password
  if ( (Router_SSID == "") || (Router_Pass == "") )
  {
    Serial.println("We haven't got any access point credentials, so get them now");

    initialConfig = true;

    // Starts an access point
    //if (!ESP_wifiManager.startConfigPortal((const char *) ssid.c_str(), password))
    if ( !ESP_wifiManager.startConfigPortal(AP_SSID.c_str(), AP_PASS.c_str()) )
      Serial.println("Not connected to WiFi but continuing anyway.");
    else
      Serial.println("WiFi connected...yeey :)");

    // Stored  for later usage, from v1.1.0, but clear first
    memset(&WM_config, 0, sizeof(WM_config));

    for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++)
    {
      String tempSSID = ESP_wifiManager.getSSID(i);
      String tempPW   = ESP_wifiManager.getPW(i);

      if (strlen(tempSSID.c_str()) < sizeof(WM_config.WiFi_Creds[i].wifi_ssid) - 1)
        strcpy(WM_config.WiFi_Creds[i].wifi_ssid, tempSSID.c_str());
      else
        strncpy(WM_config.WiFi_Creds[i].wifi_ssid, tempSSID.c_str(), sizeof(WM_config.WiFi_Creds[i].wifi_ssid) - 1);

      if (strlen(tempPW.c_str()) < sizeof(WM_config.WiFi_Creds[i].wifi_pw) - 1)
        strcpy(WM_config.WiFi_Creds[i].wifi_pw, tempPW.c_str());
      else
        strncpy(WM_config.WiFi_Creds[i].wifi_pw, tempPW.c_str(), sizeof(WM_config.WiFi_Creds[i].wifi_pw) - 1);

      // Don't permit NULL SSID and password len < MIN_AP_PASSWORD_SIZE (8)
      if ( (String(WM_config.WiFi_Creds[i].wifi_ssid) != "") && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE) )
      {
        LOGERROR3(F("* Add SSID = "), WM_config.WiFi_Creds[i].wifi_ssid, F(", PW = "), WM_config.WiFi_Creds[i].wifi_pw );
        wifiMulti.addAP(WM_config.WiFi_Creds[i].wifi_ssid, WM_config.WiFi_Creds[i].wifi_pw);
      }
    }

    saveConfigData();
  }
  else
  {
    wifiMulti.addAP(Router_SSID.c_str(), Router_Pass.c_str());
  }

  startedAt = millis();

  if (!initialConfig)
  {
    // Load stored data, the addAP ready for MultiWiFi reconnection
    loadConfigData();

    for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++)
    {
      // Don't permit NULL SSID and password len < MIN_AP_PASSWORD_SIZE (8)
      if ( (String(WM_config.WiFi_Creds[i].wifi_ssid) != "") && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE) )
      {
        LOGERROR3(F("* Add SSID = "), WM_config.WiFi_Creds[i].wifi_ssid, F(", PW = "), WM_config.WiFi_Creds[i].wifi_pw );
        wifiMulti.addAP(WM_config.WiFi_Creds[i].wifi_ssid, WM_config.WiFi_Creds[i].wifi_pw);
      }
    }

    if ( WiFi.status() != WL_CONNECTED )
    {
      Serial.println("ConnectMultiWiFi in setup");

      connectMultiWiFi();
    }
  }

  Serial.print("After waiting ");
  Serial.print((float) (millis() - startedAt) / 1000L);
  Serial.print(" secs more in setup(), connection result is ");

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("connected. Local IP: ");
    Serial.println(WiFi.localIP());
  }
  else
    Serial.println(ESP_wifiManager.getStatus(WiFi.status()));

  //read updated parameters
  strncpy(user_email, custom_user_email.getValue(), sizeof(user_email));

  //save the custom parameters to FS
  if (shouldSaveConfig)
  {
    saveFileFSConfigFile();
  }

  //"Gerando" caminho do usuário do banco
  userpath = "/users/";
  userpath += "beaverhouseapp@gmail.com";
  userpath += "/devices/";
  userpath += mac;

  //Trocando "." por ":" no caminho do usuário para não ter incopatibilidade
  userpath.replace(".", "");
  userpath.replace(":", "");
  userpath.replace("@", "");

  Serial.println(userpath);
  parentPath = userpath;

  Serial.print("Local IP = ");
  Serial.println(WiFi.localIP());

  //Inicia a conexão com o Firebase
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);

  if (Firebase.getJSON(firebaseData3, userpath + "/timer"))
  {
    timers_init = firebaseData3.jsonString();
    Serial.println(timers_init);
    Firebase.deleteNode(firebaseData3, userpath + "/timer");
  }

  if (!Firebase.beginMultiPathStream(firebaseData1, parentPath, childPath, childPathSize))
  {
    Serial.println("------------------------------------");
    Serial.println("Can't begin stream connection...");
    Serial.println("REASON: " + firebaseData1.errorReason());
    Serial.println("------------------------------------");
    Serial.println();
  }

  //Set the reserved size of stack memory in bytes for internal stream callback processing RTOS task.
  //8192 is the minimum size.
  Firebase.setMultiPathStreamCallback(firebaseData1, streamCallback, streamTimeoutCallback, 8192);

  Serial.println(Firebase.getString(firebaseData2, userpath + "/name"));

  //IF para verificar se o nome existe ou não
  if ((Firebase.getString(firebaseData2, userpath + "/name")) == 0) {
    //Caso não exista, "gera" a estrutura os dados no banco
    Firebase.setString(firebaseData2, userpath + "/ap", "BeaverHouse" + chipID);
    Firebase.setString(firebaseData2, userpath + "/icon/icon", "/assets/svg/hardware.svg");
    Firebase.setString(firebaseData2, userpath + "/icon/iconName", "Hardware");
    Firebase.setString(firebaseData2, userpath + "/mac", mac);
    Firebase.setString(firebaseData2, userpath + "/name", mac);
    Firebase.setString(firebaseData2, userpath + "/room/icon", "/assets/svg/casa.svg");
    Firebase.setString(firebaseData2, userpath + "/room/name", "Nenhum");
    Firebase.setString(firebaseData2, userpath + "/status", "desligado");
    Firebase.setInt(firebaseData2, userpath + "/timernumber", 1);
  }

  else {
    //Caso exista, apenas troca o status do dispositivo para "desligado"
    Firebase.setString(firebaseData2, userpath + "/status", "desligado");
  }

  setupNTP();

  for (size_t i = 0; i < 100; i++)
  {
    Serial.println(timers_init);
    String parsedtimer = getValue(timers_init, '{', i);
    Serial.println(parsedtimer);
    String timer = getValue(parsedtimer, '"', 7);
    Serial.println(timer);
    String week1 = getValue(parsedtimer, '"', 11);
    String week2 = getValue(parsedtimer, '"', 15);
    String week3 = getValue(parsedtimer, '"', 19);
    String week4 = getValue(parsedtimer, '"', 23);
    String week5 = getValue(parsedtimer, '"', 27);
    String week6 = getValue(parsedtimer, '"', 31);
    String week7 = getValue(parsedtimer, '"', 35);
    String action = getValue(parsedtimer, '"', 3);
    if (action != "") {
      Firebase.setString(firebaseData3, userpath + "/timer/timer" + i + "/action", action);
      Firebase.setString(firebaseData3, userpath + "/timer/timer" + i + "/timer", timer);
      Firebase.setString(firebaseData3, userpath + "/timer/timer" + i + "/week1", week1);
      Firebase.setString(firebaseData3, userpath + "/timer/timer" + i + "/week2", week2);
      Firebase.setString(firebaseData3, userpath + "/timer/timer" + i + "/week3", week3);
      Firebase.setString(firebaseData3, userpath + "/timer/timer" + i + "/week4", week4);
      Firebase.setString(firebaseData3, userpath + "/timer/timer" + i + "/week5", week5);
      Firebase.setString(firebaseData3, userpath + "/timer/timer" + i + "/week6", week6);
      Firebase.setString(firebaseData3, userpath + "/timer/timer" + i + "/week7", week7);
    }
  }
  if (Firebase.getJSON(firebaseData3, userpath + "/timer"))
  {
    Serial.println(firebaseData3.jsonString());
    timers = firebaseData3.jsonString();
    Serial.println(timers);
  }
  else
  {
    //Failed to get JSON data at defined database path, print out the error reason
    Serial.println(firebaseData3.errorReason());
  }
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
  for (size_t i = 0; i < 100; i++)
  {
    String parsedtimer = getValue(timers, '{', i);
    String timer = getValue(parsedtimer, '"', 7);
    if (timer == horario || timer == horariosemseg) {
      String week1 = getValue(parsedtimer, '"', 11);
      String week2 = getValue(parsedtimer, '"', 15);
      String week3 = getValue(parsedtimer, '"', 19);
      String week4 = getValue(parsedtimer, '"', 23);
      String week5 = getValue(parsedtimer, '"', 27);
      String week6 = getValue(parsedtimer, '"', 31);
      String week7 = getValue(parsedtimer, '"', 35);
      if (week1 == diadasemana || week2 == diadasemana || week3 == diadasemana || week4 == diadasemana || week5 == diadasemana || week6 == diadasemana || week7 == diadasemana) {
        String action = getValue(parsedtimer, '"', 3);
        if (action == "ligar") {
          devicestatus = "ligado";
          Firebase.setString(firebaseData2, userpath + "/status", "ligado");
        }
        else if (action == "desligar") {
          devicestatus = "desligado";
          Firebase.setString(firebaseData2, userpath + "/status", "desligado");
        }
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
        Serial.println(devicestatus);
      }
      if (stream.dataPath == "/timernumber") {
        if (Firebase.getJSON(firebaseData3, userpath + "/timer"))
        {
          Serial.println(firebaseData3.jsonString());
          timers = firebaseData3.jsonString();
          Serial.println(timers);
        }
        else
        {
          //Failed to get JSON data at defined database path, print out the error reason
          Serial.println(firebaseData3.errorReason());
        }
      }
      if (stream.type == "null" && stream.value == "null") {
        if (Firebase.getJSON(firebaseData3, userpath + "/timer"))
        {
          Serial.println(firebaseData3.jsonString());
          timers = firebaseData3.jsonString();
          Serial.println(timers);
        }
        else
        {
          //Failed to get JSON data at defined database path, print out the error reason
          Serial.println(firebaseData3.errorReason());
        }
      }
      if (stream.type == "json") {
        if (Firebase.getJSON(firebaseData3, userpath + "/timer"))
        {
          Serial.println(firebaseData3.jsonString());
          timers = firebaseData3.jsonString();
          Serial.println(timers);
        }
        else
        {
          //Failed to get JSON data at defined database path, print out the error reason
          Serial.println(firebaseData3.errorReason());
        }
      }
    }
  }
  Serial.println();
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

String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
