#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <Arduino.h>

// KeiraOS WiFi namespace - shared credentials storage
#define WIFI_NAMESPACE "kwifi"

// WiFi credential functions for Keira integration
String hashSSID(String ssid);
bool loadWiFiCredentials(String& ssid, String& password);
bool connectToWiFi(String ssid, String password);

#endif // WIFI_CONFIG_H
