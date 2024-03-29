#include <FS.h>

#include "DHT.h"
#include <ESP8266WiFi.h>
#include <ThingsBoard.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>

//switch button connected to this pin
#define BUTTON_PIN 12
//DHT22 sensor is connected to this pin
#define DHT_PIN 14
//sensor type is DHT22
#define DHTTYPE DHT22
//memory pool size for JsonDocument
#define JSON_BUFFER_SIZE 1024
//for safety
#define MAX_FILE_SIZE 512
//json field names
#define JSON_SSID "ssid"
#define JSON_PASSWORD "password"
#define JSON_SERVER "server"
#define JSON_TOKEN "token"
//max sizes of paramaters in config
#define SSID_SIZE 32
#define PASSWORD_SIZE 64
#define SERVER_SIZE 64
#define TOKEN_SIZE 64
//filename for config
#define CONFIG_FILE "/config.json"
//default server to send data to
#define TB_SERVER_DEFAULT "demo.thingsboard.io"

struct StoredConfig {
  char ssid[SSID_SIZE];
  char password[PASSWORD_SIZE];
  char server[SERVER_SIZE];
  char token[TOKEN_SIZE];
  boolean success = false;
};

struct SensorData {
  float temperature;
  float humidity;
  boolean success = false;
};

WiFiClient wifiClient;

DHT dht(DHT_PIN, DHTTYPE);

ThingsBoard tb(wifiClient);

int status = WL_IDLE_STATUS;
unsigned long lastSend;

boolean isConfigMode;

StoredConfig storedConfig;

void readMode() {
  // read the state of the pushbutton value:
  int buttonState = digitalRead(BUTTON_PIN);

  // check if the pushbutton is pressed. If it is, the buttonState is HIGH:
  if (buttonState == HIGH) {
    Serial.println("Mode is config");
    isConfigMode = true;
  } else {
    Serial.println("Mode is normal");
    isConfigMode = false;
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println();

  // initialize the pushbutton pin as an input:
  pinMode(BUTTON_PIN, INPUT);

  readMode();

  //clean FS, for testing
  //SPIFFS.format();

  initConfig(&storedConfig);
  
  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists(CONFIG_FILE)) {
      //file exists, reading and loading
      storedConfig = readConfig();
    } else {
      Serial.println("No config file");
    }
  } else {
    Serial.println("failed to mount FS");
  }

  if (isConfigMode) {
    openWifiManager();
    return;
  }

  if (!storedConfig.success) {
    return;
  }

  dht.begin();
  delay(10);
  lastSend = 0;

  connectAndWaitWifi();
}

void loop()
{
  if (isConfigMode) {
    return;
  }
  if (!storedConfig.success) {
    return;
  }

  if ( !tb.connected() ) {
    reconnect();
  }

  if ( millis() - lastSend > 1000 ) { // Update and send only after 1 seconds

    SensorData sd = readSensorData();
    if (sd.success) {
      tb.sendTelemetryFloat("temperature", sd.temperature);
      tb.sendTelemetryFloat("humidity", sd.humidity);
    }

    lastSend = millis();
  }

  tb.loop();
}

void reconnect() {
  Serial.println("Connecting to ThingsBoard node");
  while (!tb.connected()) {
    status = WiFi.status();
    if ( status != WL_CONNECTED) {
      connectAndWaitWifi();
    }
    Serial.print("Connecting to ThingsBoard node ...");
    if ( tb.connect(storedConfig.server, storedConfig.token) ) {
      Serial.println( "[DONE]" );
    } else {
      Serial.print( "[FAILED]" );
      Serial.println( " : retrying in 5 seconds]" );
      // Wait 5 seconds before retrying
      delay( 5000 );
    }
  }
}
