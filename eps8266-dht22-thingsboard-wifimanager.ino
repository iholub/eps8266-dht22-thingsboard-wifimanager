#include <FS.h>

#include "DHT.h"
#include <ESP8266WiFi.h>
#include <ThingsBoard.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>

// DHT
#define DHTPIN 14
#define DHTTYPE DHT22

char thingsboardServer[] = "demo.thingsboard.io";

WiFiClient wifiClient;

// Initialize DHT sensor.
DHT dht(DHTPIN, DHTTYPE);

ThingsBoard tb(wifiClient);

int status = WL_IDLE_STATUS;
unsigned long lastSend;

const int buttonPin = 12;
boolean isConfigMode;

char mqtt_server[32];
char mqtt_port[64];
char blynk_token[34];

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

// Prints the content of a file to the Serial
void printFile() {
  // Open file for reading
  Serial.println("---File start---");
  File file = SPIFFS.open("/config.json", "r");
  if (!file) {
    Serial.println(F("Failed to read file"));
    return;
  }

  // Extract each characters by one by one
  while (file.available()) {
    Serial.print((char)file.read());
  }
  // Close the file
  file.close();

  Serial.println("\n---File end---");
}

void readConfig() {
  Serial.println("Reading config file");
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");;
    return;
  }
  Serial.println("Opened config file");;
  size_t size = configFile.size();
  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  configFile.readBytes(buf.get(), size);

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/v6/assistant to compute the capacity.
  StaticJsonDocument<512> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, buf.get());

  if (!error) {
    Serial.println("\nparsed json");

    strcpy(mqtt_server, doc["mqtt_server"]);
    strcpy(mqtt_port, doc["mqtt_port"]);
    strcpy(blynk_token, doc["blynk_token"]);

    Serial.println("Config values:");
    Serial.print("Wifi SSID: ");
    Serial.println(mqtt_server);
    Serial.print("Wifi password: ");
    Serial.println(mqtt_port);
    Serial.print("Token: ");
    Serial.println(blynk_token);

  } else {
    Serial.println("failed to load json config");
  }
  configFile.close();

}

void readMode() {
  // read the state of the pushbutton value:
  int buttonState = digitalRead(buttonPin);

  // check if the pushbutton is pressed. If it is, the buttonState is HIGH:
  if (buttonState == HIGH) {
    Serial.println("Mode is config");
    isConfigMode = true;
  } else {
    Serial.println("Mode is normal");
    isConfigMode = false;
  }
}

void openWifiManager() {
  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 32);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 32);
  WiFiManagerParameter custom_blynk_token("blynk", "blynk token", blynk_token, 32);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_blynk_token);

  //reset settings - for testing
  //wifiManager.resetSettings();

  if (!wifiManager.startConfigPortal()) {
    //TODO timeout??
    Serial.println("failed to connect and hit timeout");
    //delay(3000);
    //reset and try again, or maybe put it to deep sleep
    //ESP.reset();
    //delay(5000);
    return;
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");

    //read updated parameters
    strcpy(blynk_token, custom_blynk_token.getValue());

    StaticJsonDocument<256> doc;

    doc["mqtt_server"] = WiFi.SSID();
    doc["mqtt_port"] = WiFi.psk();
    doc["blynk_token"] = blynk_token;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    } else {
      // Serialize JSON to file

      // Serialize JSON to file
      if (serializeJson(doc, configFile) == 0) {
        Serial.println(F("Failed to write to file"));
      }
      configFile.close();

      printFile();
    }
  }

  //end save

  Serial.println("local ip");
  Serial.println(WiFi.localIP());

}

void setup()
{
  Serial.begin(115200);
  Serial.println();

  // initialize the pushbutton pin as an input:
  pinMode(buttonPin, INPUT);

  readMode();

  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading

      printFile();

      readConfig();
    } else {
      Serial.println("No config file");
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  if (isConfigMode) {

    openWifiManager();

  } else {
    dht.begin();
    delay(10);
    InitWiFi();
    lastSend = 0;

  }
}

void loop()
{
  if (isConfigMode) {
    return;
  }

  if ( !tb.connected() ) {
    reconnect();
  }

  if ( millis() - lastSend > 1000 ) { // Update and send only after 1 seconds
    getAndSendTemperatureAndHumidityData();
    lastSend = millis();
  }

  tb.loop();
}

void getAndSendTemperatureAndHumidityData()
{
  Serial.println("Collecting temperature data.");

  // Reading temperature or humidity takes about 250 milliseconds!
  float humidity = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float temperature = dht.readTemperature();

  // Check if any reads failed and exit early (to try again).
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  Serial.println("Sending data to ThingsBoard:");
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" *C ");

  tb.sendTelemetryFloat("temperature", temperature);
  tb.sendTelemetryFloat("humidity", humidity);
}

void InitWiFi()
{
  Serial.println("Connecting to AP ...");
  // attempt to connect to WiFi network

  WiFi.begin(mqtt_server, mqtt_port);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to AP");
}

void reconnect() {
  // Loop until we're reconnected
  while (!tb.connected()) {
    status = WiFi.status();
    if ( status != WL_CONNECTED) {
      WiFi.begin(mqtt_server, mqtt_port);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
      Serial.println("Connected to AP");
    }
    Serial.print("Connecting to ThingsBoard node ...");
    if ( tb.connect(thingsboardServer, blynk_token) ) {
      Serial.println( "[DONE]" );
    } else {
      Serial.print( "[FAILED]" );
      Serial.println( " : retrying in 5 seconds]" );
      // Wait 5 seconds before retrying
      delay( 5000 );
    }
  }
}