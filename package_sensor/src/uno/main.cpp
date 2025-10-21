#include <Arduino.h>
#include <WiFiS3.h>             //  Wi-Fi på UNO R4
#include <ArduinoHttpClient.h>  //  HTTP-klient
#include "DHTSensor.h"          // DHT-funktioner
#include "secrets.h"            // Skapa egen och fyll i WIFI_SSID, WIFI_PASSWORD, DEVICE_ID

// Sätt ESP32:ans IP efter du sett den i ESP32-serialen
const char* ESP32_HOST = "192.168.1.50";  // BYT till IP som ESP32 skriver ut
const int   ESP32_PORT = 80;
const char* INGEST_PATH = "/ingest";


WiFiClient wifi;
HttpClient http(wifi, ESP32_HOST, ESP32_PORT);

threshold limits;
Status currentStatus = Status::NORMAL;

// enkel intervall-hantering
unsigned long lastReading = 0;
const unsigned long normalInterval = 10000; // starta enkelt; byt till 5 min sen
const unsigned long errorInterval  = 2000;
unsigned long currentInterval = normalInterval;

// enkel Wi-Fi reconnect (återanvändbar)
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.print("WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.print("\nUNO IP: "); Serial.println(WiFi.localIP());
}

//  HTTP POST /ingest
static void postJson(float temperature, float humidity, const char* st) {
  unsigned long ts = millis()/1000;  // enkel tidsstämpel (sekunder sen boot)

  char body[200];
#ifdef DEVICE_ID_IS_STRING
  snprintf(body, sizeof(body),
    "{\"id\":\"%s\",\"t\":%lu,\"temp\":%.1f,\"hum\":%.1f,\"status\":\"%s\"}",
    DEVICE_ID, ts, temperature, humidity, st);
#else
  snprintf(body, sizeof(body),
    "{\"id\":%s,\"t\":%lu,\"temp\":%.1f,\"hum\":%.1f,\"status\":\"%s\"}",
    DEVICE_ID, ts, temperature, humidity, st);
#endif

  http.beginRequest();
  http.post(INGEST_PATH);
  http.sendHeader("Content-Type", "application/json");
  http.sendHeader("Connection", "close");
  http.sendHeader("Content-Length", strlen(body));
  http.beginBody();
  http.print(body);
  http.endRequest();

  int status = http.responseStatusCode();
  String resp = http.responseBody();
  Serial.print("POST -> "); Serial.print(status);
  Serial.print(" resp: "); Serial.println(resp);
}

void setup() {
  Serial.begin(115200);
  DHTSensor::initDHTSensor();
  delay(1000);

  connectWiFi();

  lastReading = 0;
  currentInterval = normalInterval;
}

void loop() {

  unsigned long now = millis();

  if (now - lastReading >= currentInterval) {
    lastReading = now;

    //  läs sensor
    DHTSensor::readDHTSensor();
    float temperature = DHTSensor::temperature;
    float humidity    = DHTSensor::humidity;

    // statuslogik
    currentStatus = checkStatus(temperature, limits);
    const char* st = "NORMAL";
    if (currentStatus == Status::LOW_TEMP)  { Serial.println("LOW");  st = "LOW";  currentInterval = errorInterval;  }
    else if (currentStatus == Status::HIGH_TEMP){ Serial.println("HIGH"); st = "HIGH"; currentInterval = errorInterval; }
    else if (currentStatus == Status::ERROR){ Serial.println("ERROR"); st = "ERROR"; currentInterval = errorInterval; }
    else { Serial.println("NORMAL"); currentInterval = normalInterval; }

    // HTTP-post
    connectWiFi();
    postJson(temperature, humidity, st);
  }
}

