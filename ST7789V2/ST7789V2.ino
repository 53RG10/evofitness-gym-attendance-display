/*
 * ST7789V2 LCD version of EVO Fitness display
 * - XIAO ESP32C3
 * - ST7789V2 LCD (240x280 pixels)
 *
 * Fetches data from EVO Fitness Basel clubs (Gundeli & Steinen)
 * and current apparent temperature for Basel, and shows them on the LCD.
 * Optimized screen refresh: clears once, then updates only changed values.
 *
 * Created: 9 January 2026
 * Author: Sergio Guarino
 * Version: 1.1
 * 
*/
#include <Arduino.h>
#include <SPI.h>
#include <st7789v2.h>
#include "secrets.h"
#include "evo.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// -----------------------
// Display
// -----------------------
st7789v2 Display;

// Display dimensions (240x280)
const int displayWidth = 240;
const int displayHeight = 280;

// Custom color definitions (RGB565)
#define ORANGE_COLOR 0xFD20

// -----------------------
// API URLs
// -----------------------
const char *apiUrlGundeli = "https://visits.evofitness.ch/api/v1/locations/cc3013fa-9f58-4056-8731-40ed4058663a/current";
const char *apiUrlSteinen = "https://visits.evofitness.ch/api/v1/locations/91aa317c-5c31-4083-a05e-a5b0a7583d48/current";
const char *apiUrlWeather = "https://api.open-meteo.com/v1/forecast?latitude=47.5584&longitude=7.5733&current=apparent_temperature&forecast_days=1";

// -----------------------
// State tracking
// -----------------------
int previousValueGundeli = -1;  // Initialize to -1 to force first update
int previousValueSteinen = -1;
double previousTemp      = -999.0;
bool screenInitialized   = false;  // Track if screen has been drawn

// String cache for clearing old values
String previousStrGundeli = "";
String previousStrSteinen = "";
String previousStrTemp = "";

// -----------------------
// Helper: get temperature color based on value
// -----------------------
uint16_t getTemperatureColor(double temp) {
  if (temp < 10.0) {
    return CYAN;
  } else if (temp <= 30.0) {
    return WHITE;
  } else {
    return RED;
  }
}

// -----------------------
// Helper: get occupancy color based on value
// -----------------------
uint16_t getOccupancyColor(int value) {
  if (value < 5) {
    return GREEN;
  } else if (value < 10) {
    return YELLOW;
  } else if (value < 15) {
    return ORANGE_COLOR;
  } else {
    return RED;
  }
}

// -----------------------
// Helper: clear a text area by drawing black text
// -----------------------
void clearTextArea(int x, int y, const char* text, sFONT* font) {
  Display.DrawString_EN(x, y, text, font, BLACK, BLACK);
}

// -----------------------
// Helper: initialize screen with static elements
// -----------------------
void initializeScreen() {
  // Clear entire screen
  Display.Clear(BLACK);
  
  // Draw header (logo)
  Display.DrawImage(evo_logo, 50, 20, 180, 54);
  
  // Draw static labels
  Display.DrawString_EN(10, 100, "Gundeli:", &Font24, BLACK, WHITE);
  Display.DrawString_EN(10, 140, "Steinen:", &Font24, BLACK, WHITE);
  
  screenInitialized = true;
}

// -----------------------
// Helper: update only the temperature value
// -----------------------
void updateTemperature(double temp) {
  String tempStr = String(temp, 1) + " C";
  uint16_t tempColor = getTemperatureColor(temp);
  
  // Clear previous temperature (if exists)
  if (previousStrTemp.length() > 0) {
    clearTextArea(displayWidth - 90, 180, previousStrTemp.c_str(), &Font24);
  }
  
  // Draw new temperature
  Display.DrawString_EN(displayWidth - 90, 180, tempStr.c_str(), &Font24, BLACK, tempColor);
  
  // Cache the string for next clear
  previousStrTemp = tempStr;
}

// -----------------------
// Helper: update only the Gundeli value
// -----------------------
void updateGundeli(int value) {
  String valueStr = String(value);
  uint16_t color = getOccupancyColor(value);
  
  // Clear previous value (if exists)
  if (previousStrGundeli.length() > 0) {
    clearTextArea(150, 100, previousStrGundeli.c_str(), &Font24);
  }
  
  // Draw new value
  Display.DrawString_EN(150, 100, valueStr.c_str(), &Font24, BLACK, color);
  
  // Cache the string for next clear
  previousStrGundeli = valueStr;
}

// -----------------------
// Helper: update only the Steinen value
// -----------------------
void updateSteinen(int value) {
  String valueStr = String(value);
  uint16_t color = getOccupancyColor(value);
  
  // Clear previous value (if exists)
  if (previousStrSteinen.length() > 0) {
    clearTextArea(150, 140, previousStrSteinen.c_str(), &Font24);
  }
  
  // Draw new value
  Display.DrawString_EN(150, 140, valueStr.c_str(), &Font24, BLACK, color);
  
  // Cache the string for next clear
  previousStrSteinen = valueStr;
}

// -----------------------
// Helper: draw current values (optimized)
// -----------------------
void drawStatus(int valueGundeli, int valueSteinen, double temp, bool wifiOk) {
  
  if (wifiOk) {
    // Initialize screen with static elements only once
    if (!screenInitialized) {
      initializeScreen();
    }
    
    // Update only changed values
    if (temp != previousTemp) {
      updateTemperature(temp);
    }
    
    if (valueGundeli != previousValueGundeli) {
      updateGundeli(valueGundeli);
    }
    
    if (valueSteinen != previousValueSteinen) {
      updateSteinen(valueSteinen);
    }
    
  } else {
    // WiFi error: clear screen and show error message
    Display.Clear(BLACK);
    Display.DrawString_EN(10, 100, "WiFi error", &Font20, BLACK, RED);
    Display.DrawString_EN(10, 130, "Reconnecting...", &Font16, BLACK, RED);
    screenInitialized = false;  // Force reinit when WiFi returns
  }
}

void setup(void) {
  Serial.begin(115200);

  // Initialize display
  Display.SetRotate(270);  // Portrait mode for 240x280 (adjust as needed: 0, 90, 180, 270)
  Display.Init();
  Display.SetBacklight(100);
  Display.Clear(BLACK);
  
  // Show connecting message
  Display.DrawString_EN(10, 30, "Connecting WiFi...", &Font20, BLACK, WHITE);

  WiFi.begin(SECRET_SSID, SECRET_PASS);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.println("\nWiFi connected!");

  // Initial placeholder display will happen on first loop
  screenInitialized = false;

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

    // --- Update display (optimized to update only changed values) ---
    if (!screenInitialized || 
        currentValueGundeli != previousValueGundeli ||
        currentValueSteinen != previousValueSteinen ||
        currentTemp != previousTemp) {

      drawStatus(currentValueGundeli, currentValueSteinen, currentTemp, true);

      String valueStrG = String(currentValueGundeli);
      String valueStrS = String(currentValueSteinen);
      String tempStr   = String(currentTemp, 1) + " C";

      Serial.println("Temp: " + tempStr + 
                     "\nGundeli: " + valueStrG +
                     "\nSteinen: " + valueStrS);

      previousValueGundeli = currentValueGundeli;
      previousValueSteinen = currentValueSteinen;
      previousTemp         = currentTemp;
    } else {
      Serial.println("No change detected, skipping LCD update.");
    }

  } else {
    Serial.println("WiFi disconnected!");
    drawStatus(0, 0, 0.0, false);
    WiFi.reconnect();
  }

  // Refresh only once every 3 minutes.
  delay(180000);
}
