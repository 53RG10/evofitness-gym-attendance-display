// OLED version of EVO Fitness display
// - XIAO ESP32C3
// - 0.96" SSD1306 OLED (U8g2lib)
//
// Fetches data from EVO Fitness Basel clubs (Gundeli & Steinen)
// and current apparent temperature for Basel, and shows them on the OLED.
// Screen is simply redrawn when values change (no e-paper style refresh logic).

#include <Arduino.h>
#include <U8g2lib.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// -----------------------
// Display
// -----------------------
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(
  U8G2_R0,
  /* clock=*/ SCL,
  /* data=*/ SDA,
  /* reset=*/ U8X8_PIN_NONE
);    // Software I2C

// -----------------------
// WiFi credentials
// -----------------------
const char *ssid     = "WiFi";
const char *password = "Password";

// -----------------------
// API URLs
// -----------------------
const char *apiUrlGundeli = "https://visits.evofitness.ch/api/v1/locations/cc3013fa-9f58-4056-8731-40ed4058663a/current";
const char *apiUrlSteinen = "https://visits.evofitness.ch/api/v1/locations/91aa317c-5c31-4083-a05e-a5b0a7583d48/current";
const char *apiUrlWeather = "https://api.open-meteo.com/v1/forecast?latitude=47.5584&longitude=7.5733&current=apparent_temperature&forecast_days=1";

// -----------------------
// State tracking
// -----------------------
int previousValueGundeli = 0;
int previousValueSteinen = 0;
double previousTemp      = 0.0;

// -----------------------
// Helper: draw current values
// -----------------------
void drawStatus(const String &valueGundeli, const String &valueSteinen, const String &tempStr, bool wifiOk) {
  u8g2.clearBuffer();

  if (wifiOk) {
    u8g2.setFont(u8g2_font_shylock_nbp_tf);
    const int displayWidth = 128;
    const int rightMargin = 2;
    int tempWidth = u8g2.getUTF8Width(tempStr.c_str());
    int tempX = displayWidth - tempWidth - rightMargin;
    if (tempX < 0) tempX = 0;
    u8g2.drawUTF8(tempX, 15, tempStr.c_str());
    
    u8g2.setFont(u8g2_font_Pixellari_tf);
    u8g2.drawStr(0, 35, "Gundeli:");
    int valueWidthG = u8g2.getStrWidth(valueGundeli.c_str());
    int valueXG = 75 - valueWidthG;
    if (valueXG < 0) valueXG = 0;
    u8g2.drawStr(valueXG, 35, valueGundeli.c_str());

    u8g2.drawStr(0, 55, "Steinen:");
    int valueWidthS = u8g2.getStrWidth(valueSteinen.c_str());
    int valueXS = 75 - valueWidthS;
    if (valueXS < 0) valueXS = 0;
    u8g2.drawStr(valueXS, 55, valueSteinen.c_str());

  } else {
    u8g2.drawStr(0, 15, "WiFi error");
    u8g2.drawStr(0, 35, "Reconnecting...");
  }

  u8g2.sendBuffer();
}

void setup(void) {
  Serial.begin(115200);

  u8g2.begin();
  u8g2.setContrast(255);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_shylock_nbp_tf);
  u8g2.drawStr(0, 14, "Connecting WiFi...");
  u8g2.sendBuffer();

  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.println("\nWiFi connected!");

  // Initial placeholder display
  drawStatus("--", "--", "--°C", true);
}
 
void loop(void) {
  int currentValueGundeli = previousValueGundeli;
  int currentValueSteinen = previousValueSteinen;
  double currentTemp      = previousTemp;

  if (WiFi.status() == WL_CONNECTED) {
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
    if (currentValueGundeli != previousValueGundeli ||
        currentValueSteinen != previousValueSteinen ||
        currentTemp != previousTemp) {

      String valueStrG = String(currentValueGundeli);
      String valueStrS = String(currentValueSteinen);
      String tempStr   = String(currentTemp, 1) + "°C";

      drawStatus(valueStrG, valueStrS, tempStr, true);

      Serial.println("Temp: " + tempStr + 
                     "\nGundeli: " + valueStrG +
                     "\nSteinen: " + valueStrS);

      previousValueGundeli = currentValueGundeli;
      previousValueSteinen = currentValueSteinen;
      previousTemp         = currentTemp;
    } else {
      Serial.println("No change detected, skipping OLED update.");
    }

  } else {
    Serial.println("WiFi disconnected!");
    drawStatus("WiFi", "WiFi", "--", false);
    WiFi.reconnect();
  }

  // Match the e-paper refresh interval: once per minute
  delay(60000);
}
