// Import required libraries
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
//#include <WiFiUdp.h>
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include "DHTesp.h"

// WiFi parameters
const char* host = "basement"; 
const char* ssid = "SSID";
const char* pword = "password";
const char* server = "http_server:port";
int delay_minutes = 15;
String delayMinutes;
String serverAddress;
const int PIN = D6;
HTTPClient http;
DHTesp dht; // 11 works fine for ESP8266
long nextReading = millis();

//Variables
float temp; //Stores temperature value

void setup(void)
{

  // Start Serial
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(String(host));

  WiFi.begin(ssid, pword);             // Connect to the network
  Serial.print("Connecting to ");
  Serial.print(ssid); Serial.println(" ...");

  int i = 0;
  while (WiFi.status() != WL_CONNECTED) { // Wait for the Wi-Fi to connect
    delay(1000);
    Serial.print(++i); Serial.print(' ');
  }  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  if (!MDNS.begin(host)) {
    Serial.println("Error setting up MDNS responder!");
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
    nextReading = millis() + (delayMinutes * 60000);
  }
  ArduinoOTA.handle();
}
