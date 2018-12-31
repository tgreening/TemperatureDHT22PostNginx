// Import required libraries
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
//#include <WiFiUdp.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include "DHTesp.h"

// WiFi parameters
const char* host = "basement";
String delayMinutes;
String serverAddress;
const int PIN = D6;
HTTPClient http;
DHTesp dht; // 11 works fine for ESP8266
long nextReading = millis();

//Variables
float temp; //Stores temperature value

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}
void setup(void)
{
  const char* server;
  const char* delay_minutes;

  // Start Serial
  Serial.begin(115200);
  WiFiManager wifiManager;
  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          server = json["serverAddress"];
          delay_minutes = json["delay_minutes"];
        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  WiFiManagerParameter custom_delay_minutes("delayMinutes", "Delay Minutes", delay_minutes, 2);
  WiFiManagerParameter custom_server("serverAddress", "Server Address", server, 40);
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here
  wifiManager.addParameter(&custom_delay_minutes);
  wifiManager.addParameter(&custom_server);
  WiFi.hostname(String(host));

  wifiManager.setConfigPortalTimeout(90);
  if (!wifiManager.startConfigPortal(host)) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }
  WiFi.mode(WIFI_STA);
  //if you ;get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  if (!MDNS.begin(host)) {
    Serial.println("Error setting up MDNS responder!");
  }

  server = custom_server.getValue();
  delay_minutes = custom_delay_minutes.getValue();
  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["serverAddress"] = server;
    json["delay_minutes"] = delay_minutes;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  ArduinoOTA.setHostname(host);
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();


  // Connect to WiFi
  WiFi.hostname(host);
  Serial.println("");
  Serial.println("WiFi connected");
  // pinMode(3, INPUT);
  dht.setup(PIN, DHTesp::DHT22);

  // Print the IP address
  Serial.println(WiFi.localIP());
  serverAddress = String(server);
  delayMinutes = String(delay_minutes);
  Serial.println("Server: " + serverAddress);
  Serial.println("Delay: " + delayMinutes);
}

void loop() {
  if ((long) millis() - nextReading >= 0) {
    String URL = "http://" + serverAddress + "/temperature/" + String(host) + "/reading/";
    Serial.println(URL);
    http.begin(URL);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    delay(dht.getMinimumSamplingPeriod());
    Serial.println(dht.getTemperature());
    float t = dht.toFahrenheit(dht.getTemperature());
    String payload = "degrees=" ;
    payload.concat(t);
    payload += "&units=F";
    Serial.println(payload);
    int httpCode = http.POST(payload);
    String answer = http.getString();                  //Get the response payload

    Serial.println(httpCode);   //Print HTTP return code
    Serial.println(answer);    //Print request response payload

    http.end();  //Close connection
    nextReading = millis() + (delayMinutes.toInt() * 60000);
  }
  ArduinoOTA.handle();
}
