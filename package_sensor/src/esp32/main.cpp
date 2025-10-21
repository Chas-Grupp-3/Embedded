#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>      // behövs för Azure
#include <time.h>
#include "secrets.h"               // WIFI_SSID, WIFI_PASSWORD, CLOUD_URL = "https://<app>.azurewebsites.net/packages/1" nu


#define SEND_PERIOD_MS 30000       // skicka var 30s ändra senare till exakt period


//HTTPS/HTTP

static bool httpBegin(HTTPClient& http, const char* url) {       
  if (String(url).startsWith("https://")) {
    static WiFiClientSecure client;
    client.setInsecure();              //BARA FÖR TEST, BYTTA UT MOT RIKTIG CERT-HANTERING SEN
    return http.begin(client, url);
  }
  return http.begin(url);              // HTTP 
}




// Stockholm tid i formatet - "2025-10-20T12:34:56.123Z"
static String nowIsoStockholmMsSimple() {
  struct timeval tv; gettimeofday(&tv, nullptr);
  time_t sec = tv.tv_sec;
  struct tm lt; localtime_r(&sec, &lt); // lokal tid

  char dateTime[32];  // t.ex. 2025-10-20T14:03:12
  strftime(dateTime, sizeof(dateTime), "%Y-%m-%dT%H:%M:%S", &lt);

  char offRaw[8];     // t.ex. +0200 eller +0100
  strftime(offRaw, sizeof(offRaw), "%z", &lt);

  // +0200 -> +02:00
  char tz[7] = "+00:00";
  if (strlen(offRaw) == 5) {
    snprintf(tz, sizeof(tz), "%c%c%c:%c%c", offRaw[0], offRaw[1], offRaw[2], offRaw[3], offRaw[4]);
  }

  char out[64]; //Fullt: 2025-10-20T14:03:12.123+02:00
  snprintf(out, sizeof(out), "%s.%03ld%s", dateTime, tv.tv_usec/1000, tz);
  return String(out);
}


// Mockvärden

static float temperatureVal() { return 21.0f + (millis()%1000)/1000.0f * 2.0f; } // ~21–23
static int   humidityVal()    { return (int)round(40.0 + (millis()%1000)/1000.0 * 10.0); } // 40–50

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nESP32 Boot OK (minimal PUT-sändare)"); // Status vid start

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); } //Väntar tills WiFi är anslutet
  Serial.printf("\nESP32 IP: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("CLOUD_URL: %s\n", CLOUD_URL); // Visa rätt URL

  // Hämta UTC-tid via NTP
  configTzTime("CET-1CEST,M3.5.0/02,M10.5.0/03","pool.ntp.org","time.nist.gov");
  // kort vänt för tid (max ~5s)
  for (int i=0; i<20 && time(nullptr) < 1700000000; ++i) { delay(250); }
}

void loop() {
  static uint32_t lastSend = 0; // Tidpunkt för senaste sändning
  if (millis() - lastSend < SEND_PERIOD_MS) {
    // enkel WiFi-reconnect utan att blockera
    static uint32_t lastCheck = 0;
    if (millis() - lastCheck > 5000) { // kolla var 5:e sekund
      lastCheck = millis();
      if (WiFi.status() != WL_CONNECTED) { // inte ansluten, försök igen
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      }
    }
    return;
  }
  lastSend = millis(); // uppdatera tidpunkt

  //PUT-body (API tar “temperature”, “humidity”, “date”)
  float t = temperatureVal();    
  int   h = humidityVal();       
  String iso = nowIsoStockholmMsSimple();
  
  // Meddelandet
  String body = String("{")
    + "\"temperature\":" + String(t, 2) + ","
    + "\"humidity\":"    + String(h)    + ","
    + "\"date\":\""      + iso          + "\""
    + "}";


  Serial.print("[PUT] URL=");  Serial.println(CLOUD_URL);
  Serial.print("[PUT] BODY="); Serial.println(body);

  HTTPClient http;
  http.setTimeout(8000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  if (!httpBegin(http, CLOUD_URL)) {
    Serial.println("[ERR] http.begin failed");
    return;
  }

  http.addHeader("Content-Type", "application/json");
  //Placera autentiseringstoken här sen
  
  int code = http.sendRequest("PUT", body);            
  String resp = http.getString(); // Svar från server
  http.end();

  Serial.printf("[PUT] code=%d resp=%s\n", code, resp.c_str()); // Logga svar

}
/* //SANITY TEST: enkel tick-test som POST:ar var 2:a sekund till CLOUD_URL
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "secrets.h"

unsigned long lastTick = 0;
uint32_t tickNo = 0;

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n[BOOT] tick tester");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.print("\nESP32 IP: "); Serial.println(WiFi.localIP());
  Serial.print("CLOUD_URL: "); Serial.println(CLOUD_URL);
}

void loop() {
  if (millis() - lastTick >= 2000) {            // var 2 s
    lastTick = millis();
    tickNo++;

    String body = String("{\"tick\":") + tickNo +
                  ",\"ip\":\"" + WiFi.localIP().toString() + "\"}";

    HTTPClient http;
    http.setTimeout(8000);
    http.begin(CLOUD_URL);                      // ex: http://<DIN-IP>:8000/v1/ingest
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(body);
    String resp = http.getString();
    http.end();

    Serial.printf("[TICK %lu] POST %d resp=%s\n",
                  (unsigned long)tickNo, code, resp.c_str());
  }
} */