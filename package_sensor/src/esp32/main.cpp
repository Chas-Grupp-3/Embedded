#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>      // behövs för Azure
#include <time.h>
#include "secrets.h"               // WIFI_SSID, WIFI_PASSWORD, CLOUD_URL = "https://<app>.azurewebsites.net/packages/1" nu

// Kort förklaring för någon som hoppar in:
// - Detta program simulerar temperatursändningar och PUT:ar JSON till en cloud-URL.
// - WiFi-uppgifter och CLOUD_URL ligger i `secrets.h` (se README i repo för mer).
// - Tidsstämplar skapas med nowIsoStockholmMsSimple() och använder lokal tid enligt configTzTime.


#define SEND_PERIOD_MS 30000       // skicka var 30s ändra senare till exakt period (ms)

const float TEMP_LOW = 10.0f;    // lägsta temperatur för simulering (används för mockvärden)
const float TEMP_HIGH = 25.0f;   // högsta temperatur för simulering (används för mockvärden)


std::vector<String> packageIds;   // sparade packageIds från backend
// packageIds fylls av fetchPackages() och används för att skicka data till respektive paket


//HTTPS/HTTP

// Wrapper för HTTP-början som hanterar både HTTP och HTTPS (enkelt test-setup)
// OBS: client.setInsecure() används för test och ska bytas ut mot riktig cert-hantering i produktion.
static bool httpBegin(HTTPClient& http, const char* url) {       
  if (String(url).startsWith("https://")) {
    static WiFiClientSecure client;
    client.setInsecure();              //BARA FÖR TEST, BYTTA UT MOT RIKTIG CERT-HANTERING SEN
    return http.begin(client, url);
  }
  return http.begin(url);              // HTTP 
}




// Skapar ISO-8601-liknande tidsstämpel med millisekunder och lokal tidszons-offset.
// Exempel output: "2025-10-20T14:03:12.123+02:00"
// Funktionen använder lokal tid (localtime_r) vilket styrs av configTzTime() i setup().
static String nowIsoStockholmMsSimple() {
  struct timeval tv; gettimeofday(&tv, nullptr);
  time_t sec = tv.tv_sec;
  struct tm lt; localtime_r(&sec, &lt); // lokal tid

  char dateTime[32];  // t.ex. 2025-10-20T14:03:12
  strftime(dateTime, sizeof(dateTime), "%Y-%m-%dT%H:%M:%S", &lt);

  char offRaw[8];     // t.ex. +0200 eller +0100 (utan kolon)
  strftime(offRaw, sizeof(offRaw), "%z", &lt);

  // +0200 -> +02:00
  char tz[7] = "+00:00"; // formatterad offset med kolon
  if (strlen(offRaw) == 5) {
    snprintf(tz, sizeof(tz), "%c%c%c:%c%c", offRaw[0], offRaw[1], offRaw[2], offRaw[3], offRaw[4]);
  }

  char out[64]; //Fullt: 2025-10-20T14:03:12.123+02:00
  snprintf(out, sizeof(out), "%s.%03ld%s", dateTime, tv.tv_usec/1000, tz);
  return String(out);
}


bool fetchPackages() {
  // Ta basen av CLOUD_URL (utan /1 i slutet)
  String url = String(CLOUD_URL);


  // Hämta lista på paket från backend. Förväntas returnera JSON-array med objekt som innehåller fältet "id".
  HTTPClient http;
  http.setTimeout(10000);
  if (!httpBegin(http, url.c_str())) {
    Serial.println("[GET] http.begin failed");
    return false;
  }

  int code = http.GET();
  if (code != 200) {
    Serial.printf("[GET] Failed, code=%d\n", code);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();
  Serial.printf("[GET] OK, len=%d\n", payload.length());

  DynamicJsonDocument doc(8192);
  auto err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("[GET] JSON parse error: %s\n", err.c_str());
    return false;
  }

  // Uppdatera lokal lista med paket-ID:n som backend returnerar
  packageIds.clear();
  if (doc.is<JsonArray>()) {
    for (JsonVariant v : doc.as<JsonArray>()) {
      if (v.is<JsonObject>() && v["id"].is<const char*>()) {
        packageIds.push_back(String(v["id"].as<const char*>()));
      }
    }
  }

  Serial.printf("[GET] %d paket hittade\n", (int)packageIds.size());
  for (auto &id : packageIds) Serial.printf("  - %s\n", id.c_str());
  return !packageIds.empty();
}



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

  
  // Synkronisera tid via NTP. Använder CET (UTC+1) utan sommartid (DST).
  // Byt till DST-regeln "CET-1CEST,M3.5.0/02,M10.5.0/03" om automatisk sommartid önskas.
  configTzTime("CET-1","pool.ntp.org","time.nist.gov");
  // kort vänt för tid (max ~5s)
  for (int i=0; i<20 && time(nullptr) < 1700000000; ++i) { delay(250); }
  
  // Paket-ID:n hämtas från backend. Om det misslyckas används fallback (/1).
  if (!fetchPackages()) {
    Serial.println("[ERR] Kunde inte hämta paket-ID (kommer skicka till /1 som fallback)");
  }
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

  String iso = nowIsoStockholmMsSimple();

  if (packageIds.empty()) {
    float t = TEMP_LOW + ((float)(millis() % 1000) / 1000.0f) * (TEMP_HIGH - TEMP_LOW); // normal temp
    int   h = 42; // fast humidity

    String body = String("{")
      + "\"temperature\":" + String(t, 2) + ","
      + "\"humidity\":"    + String(h)    + ","
      + "\"date\":\""      + iso          + "\""
      + "}";

    HTTPClient http;
    http.setTimeout(8000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    if (!httpBegin(http, CLOUD_URL)) {
      Serial.println("[ERR] http.begin failed (fallback)");
      return;
    }

    http.addHeader("Content-Type", "application/json");
    int code = http.sendRequest("PUT", body);
    String resp = http.getString();
    http.end();

    Serial.printf("[PUT fallback /1] code=%d temp=%.2f hum=%d resp=%s\n", code, t, h, resp.c_str());
    return;
  }

  // Annars: skicka till ALLA paket-ID med mönstret 0,1 normal – 2 hög – 3 låg (loopar)
  for (size_t i = 0; i < packageIds.size(); ++i) {
    float t;
    int   h = 42; // fast humidity (enkel)

    switch (i % 4) {
      case 2: t = TEMP_HIGH + 2.0f; break;   // över 25
      case 3: t = TEMP_LOW  - 2.0f; break;   // under 10
      default:
        t = TEMP_LOW + ((float)(millis() % 1000) / 1000.0f) * (TEMP_HIGH - TEMP_LOW); // normal 10–25
        break;
    }

    String body = String("{")
      + "\"temperature\":" + String(t, 2) + ","
      + "\"humidity\":"    + String(h)    + ","
      + "\"date\":\""      + iso          + "\""
      + "}";

    // Bygg URL för paket: ta bort ev. trailing '/' och lägg till paket-id
    String url = String(CLOUD_URL);
    if (url.endsWith("/")) url.remove(url.length()-1);
    url += "/";
    url += packageIds[i]; // -> .../packages/<id>

    HTTPClient http;
    http.setTimeout(8000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    if (!httpBegin(http, url.c_str())) {
      Serial.println("[ERR] http.begin failed");
      continue; // vi är i en for-loop → giltigt
    }

    http.addHeader("Content-Type", "application/json");
    int code = http.sendRequest("PUT", body);
    String resp = http.getString();
    http.end();

    Serial.printf("[PUT %s] code=%d temp=%.2f hum=%d resp=%s\n",
                  packageIds[i].c_str(), code, t, h, resp.c_str());
  }
}
