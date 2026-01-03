#include "wifi_config.h"
#include <lilka.h>
#include <WiFi.h>
#include <Preferences.h>

// Hash SSID to create storage key (matches Keira's implementation)
String hashSSID(String ssid) {
  uint64_t hash = 0;
  for (int i = 0; i < ssid.length(); i++) {
    hash = (hash << 5) - hash + ssid[i];
  }
  char buffer[9];
  snprintf(buffer, sizeof(buffer), "%08x", (unsigned int)hash);
  return String(buffer);
}

// Load WiFi credentials from Keira's NVS storage
// Returns false if no credentials are configured in Keira
bool loadWiFiCredentials(String& ssid, String& password) {
  Preferences prefs;
  if (!prefs.begin(WIFI_NAMESPACE, true)) {
    // Namespace doesn't exist (no WiFi configured in Keira)
    return false;
  }
  
  if (!prefs.isKey("last_ssid")) {
    prefs.end();
    return false;
  }
  
  ssid = prefs.getString("last_ssid", "");
  if (ssid.isEmpty()) {
    prefs.end();
    return false;
  }
  
  String ssidHash = hashSSID(ssid);
  password = prefs.getString((ssidHash + "_pw").c_str(), "");
  prefs.end();
  
  return true;
}

bool connectToWiFi(String ssid, String password) {
  Serial.printf("Connecting to WiFi: %s\n", ssid.c_str());
  
  lilka::Canvas canvas;
  canvas.begin();
  canvas.fillScreen(lilka::colors::Black);
  canvas.setTextColor(lilka::colors::White);
  canvas.setTextSize(1);
  
  int16_t x1, y1;
  uint16_t w, h;
  
  canvas.getTextBounds("Connecting to WiFi...", 0, 0, &x1, &y1, &w, &h);
  canvas.setCursor((canvas.width() - w) / 2, (canvas.height() / 2) - 10);
  canvas.println("Connecting to WiFi...");
  
  canvas.getTextBounds(ssid.c_str(), 0, 0, &x1, &y1, &w, &h);
  canvas.setCursor((canvas.width() - w) / 2, (canvas.height() / 2) + 10);
  canvas.println(ssid);
  
  lilka::display.drawCanvas(&canvas);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  int attempts = 0;
  while (WiFi.status() == WL_DISCONNECTED && attempts < 30) {
    vTaskDelay(500 / portTICK_PERIOD_MS);
    attempts++;
  }
  
  bool success = (WiFi.status() == WL_CONNECTED);
  
  if (success) {
    canvas.fillScreen(lilka::colors::Black);
    canvas.setTextColor(lilka::colors::Green);
    canvas.setTextSize(1);
    
    canvas.getTextBounds("WiFi connected!", 0, 0, &x1, &y1, &w, &h);
    canvas.setCursor((canvas.width() - w) / 2, (canvas.height() - h) / 2);
    canvas.println("WiFi connected!");
    
    lilka::display.drawCanvas(&canvas);
    
    Serial.println("WiFi connected!");
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    
    delay(1500);
  } else {
    Serial.println("WiFi connection failed!");
  }
  
  return success;
}
