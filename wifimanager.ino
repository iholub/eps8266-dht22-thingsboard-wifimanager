//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void openWifiManager() {
  Serial.println("Open wifimanager");
  Serial.println("Stored config values:");
  Serial.println(storedConfig.ssid);
  Serial.println(storedConfig.password);
  Serial.println(storedConfig.server);
  Serial.println(storedConfig.token);

  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  WiFiManagerParameter server_param("server", "Server", storedConfig.server, SERVER_SIZE);
  wifiManager.addParameter(&server_param);

  WiFiManagerParameter token_param("token", "Token", storedConfig.token, TOKEN_SIZE);
  wifiManager.addParameter(&token_param);

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
  Serial.println("Connected");

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    StoredConfig c;
    strcpy(c.ssid, WiFi.SSID().c_str());
    strcpy(c.password, WiFi.psk().c_str());
    strcpy(c.server, server_param.getValue());
    strcpy(c.token, token_param.getValue());

    saveConfig(c);
  }

  Serial.print("Local ip: ");
  Serial.println(WiFi.localIP());
}
