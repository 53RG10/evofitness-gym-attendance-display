/*
 * 
 * The following code works with:
 *  - XIAO ESP32C3
 *  - XIAO E-paper display 2.13"
 *  
 * Change Wifi ssid and password before use.
 * 
 * 
 * The program fetches data from EVO Fitness clubs in Basel to retrieve number of current users every minute.
 * In addition, current temperature for Basel is displayed.
 * Display is partially refreshed and only if the value has changed.
 * A full refresh of the display is done every 10 partial refreshes.
 * 
 * Created: 7 November 2025
 * Author: Sergio Guarino
 * Version: 4.0
 * 
*/
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeSans12pt7b.h>

// WiFi credentials
const char *ssid = "WiFi";
const char *password = "Password";

// API URLs
// NOTE: The current URL below is for EVO Basel Steinenvorstadt (Steinen)
const char *apiUrlGundeli = "https://visits.evofitness.ch/api/v1/locations/cc3013fa-9f58-4056-8731-40ed4058663a/current"; // replace with Gundeli URL when provided
const char *apiUrlSteinen = "https://visits.evofitness.ch/api/v1/locations/91aa317c-5c31-4083-a05e-a5b0a7583d48/current";
const char *apiUrlWeather = "https://api.open-meteo.com/v1/forecast?latitude=47.5584&longitude=7.5733&current=apparent_temperature&forecast_days=1";

// E-Paper pin definitions for Seeed Studio XIAO ESP32C3 adapter board
const int EINK_BUSY = D5;
const int EINK_RST  = D0;
const int EINK_DC   = D3;
const int EINK_CS   = D1;
const int EINK_SCK  = D8;
const int EINK_MOSI = D10;

// Create display object
GxEPD2_BW<GxEPD2_213_flex, GxEPD2_213_flex::HEIGHT>
  display(GxEPD2_213_flex(EINK_CS, EINK_DC, EINK_RST, EINK_BUSY));

int updateCount = 0;
bool firstRefresh = true;
int previousValueGundeli = 0;  // Gym count tracking - Gundeli
int previousValueSteinen = 0;  // Gym count tracking - Steinen
double previousTemp = 0.0;  // Temperature tracking

void updateDisplay(const String &valueGundeli, const String &valueSteinen, const String &tempStr) {
  int textY1 = 20; // first line Y position (Gundeli)
  int textY1b = 50; // second line Y position (Steinen)
  int textY2 = display.height() - 10; // bottom line (temperature)

  Serial.print("updateCount = ");
  Serial.println(updateCount);

  display.setFullWindow();
  display.setRotation(1);
  display.setTextColor(GxEPD_BLACK);
  display.setTextSize(1);
  display.setFont(&FreeSans12pt7b);

  if (firstRefresh || updateCount >= 10) {
    // Full refresh
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(0, textY1);
    display.print("Gundeli: ");
    display.print(valueGundeli);

    display.setCursor(0, textY1b);
    display.print("Steinen: ");
    display.print(valueSteinen);

    int16_t tbx, tby;
    uint16_t tbw, tbh;
    display.getTextBounds(tempStr, 0, textY2, &tbx, &tby, &tbw, &tbh);
    display.setCursor(display.width() - tbw - 5, textY2);
    display.print(tempStr);

    display.display(true);
    updateCount = 0;
    firstRefresh = false;
  } else {
    // Partial or simulated partial refresh
    display.fillRect(0, 0, display.width(), 75, GxEPD_WHITE);
    display.fillRect(0, textY2 - 20, display.width(), 25, GxEPD_WHITE);

    display.setCursor(0, textY1);
    display.print("Gundeli: ");
    display.print(valueGundeli);

    display.setCursor(0, textY1b);
    display.print("Steinen: ");
    display.print(valueSteinen);

    int16_t tbx, tby;
    uint16_t tbw, tbh;
    display.getTextBounds(tempStr, 0, textY2, &tbx, &tby, &tbw, &tbh);
    display.setCursor(display.width() - tbw - 5, textY2);
    display.print(tempStr);

    display.display(true);
  }

  updateCount++;
}

void setup() {
  Serial.begin(115200);
  display.init(115200);
  display.setFullWindow();
  display.fillScreen(GxEPD_WHITE);
  display.setRotation(1);
  display.setTextColor(GxEPD_BLACK);
  display.setTextSize(1);
  display.setFont(&FreeSans12pt7b);
  display.setCursor(0, 20);
  display.print("Connecting WiFi...");
  display.display(true);

  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  display.setFullWindow();
  display.fillScreen(GxEPD_WHITE);
  display.setCursor(0, 20);
  display.print("Gundeli: --");
  display.setCursor(0, 50);
  display.print("Steinen: --");
  display.display(true);
  delay(1000);
}

void loop() {
  int currentValueGundeli = previousValueGundeli;
  int currentValueSteinen = previousValueSteinen;
  double currentTemp = previousTemp;

  if ((WiFi.status() == WL_CONNECTED)) {
    HTTPClient http;

    // --- Gym occupancy: Gundeli ---
    http.begin(apiUrlGundeli);
    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      StaticJsonDocument<256> doc;
      if (deserializeJson(doc, payload) == DeserializationError::Ok) {
        currentValueGundeli = doc["current"];
      } else {
        Serial.println("Gundeli JSON parse failed");
      }
    } else {
      Serial.println("Gundeli HTTP failed");
    }
    http.end();

    // --- Gym occupancy: Steinen ---
    http.begin(apiUrlSteinen);
    httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      StaticJsonDocument<256> doc;
      if (deserializeJson(doc, payload) == DeserializationError::Ok) {
        currentValueSteinen = doc["current"];
      } else {
        Serial.println("Steinen JSON parse failed");
      }
    } else {
      Serial.println("Steinen HTTP failed");
    }
    http.end();

    // --- Weather ---
    http.begin(apiUrlWeather);
    httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      StaticJsonDocument<512> doc;
      if (deserializeJson(doc, payload) == DeserializationError::Ok) {
        currentTemp = doc["current"]["apparent_temperature"];
      } else {
        Serial.println("Weather JSON parse failed");
      }
    } else {
      Serial.println("Weather HTTP failed");
    }
    http.end();

    // --- Update display only if values changed ---
    if (currentValueGundeli != previousValueGundeli || currentValueSteinen != previousValueSteinen || currentTemp != previousTemp) {
      String valueStrG = String(currentValueGundeli);
      String valueStrS = String(currentValueSteinen);
      String tempStr = String(currentTemp, 1) + " C";
      updateDisplay(valueStrG, valueStrS, tempStr);
      Serial.println("Gundeli: " + valueStrG + "  Steinen: " + valueStrS + "  Temp: " + tempStr);
      previousValueGundeli = currentValueGundeli;
      previousValueSteinen = currentValueSteinen;
      previousTemp = currentTemp;
    } else {
      Serial.println("No change detected, skipping display update.");
    }

  } else {
    Serial.println("WiFi disconnected!");
    updateDisplay("WiFi", "WiFi", "--");
    WiFi.reconnect();
  }

  delay(60000);
}
