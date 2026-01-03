/*
 * Pixel Update Receiver for Lilka v2 (ST7789 280x240)
 * Receives per-pixel updates (x, y, RGB565) over TCP and displays them in real-time.
 * 
 * Designed to be loaded from KeiraOS:
 * - Reads WiFi credentials from Keira's NVS storage (namespace "kwifi")
 * - Uses the same SSID hashing scheme as Keira for password retrieval
 * - No interactive WiFi configuration - credentials must be set in Keira first
 * 
 * Protocol v2 (PXUP - Pixel Update Protocol):
 *   Adapted from ESP32 T-Display (135x240) with uint16 coordinates for 280x240 display
 *   Header: 'P' 'X' 'U' 'P' (4 bytes) + version (1 byte, 0x02) + frame_id (uint32 LE) + count (uint16 LE)
 *   Body:   count entries of: x (uint16 LE), y (uint16 LE), color (uint16 LE)
 *   Entry size: 6 bytes (increased from 4 bytes in original uint8 version)
 *
 * Run-length encoding protocol v1 (PXUR):
 *   For efficient bandwidth usage with consecutive same-color pixels
 *   Header: 'P' 'X' 'U' 'R' (4 bytes) + version (1 byte, 0x01) + frame_id (uint32 LE) + count (uint16 LE)
 *   Body:   count entries of: y (uint16 LE), x0 (uint16 LE), length (uint16 LE), color (uint16 LE)
 *   Entry size: 8 bytes per run
 *
 * Performance optimizations:
 * - Display managed by Lilka SDK (automatic SPI configuration)
 * - PSRAM buffer allocation with fallback to regular RAM
 * - Batch pixel updates received per frame before display write
 * - Run-length encoding support for reduced network bandwidth
 */

#include <Arduino.h>
#include <lilka.h>
#include <WiFi.h>
#include <WiFiServer.h>
#include <esp_heap_caps.h>  // for PSRAM allocations
#include "wifi_config.h"

// Network settings
WiFiServer server(8090);  // dedicated port for pixel updates
WiFiClient client;

// Protocol constants (v2 adds frame_id to the header)
const uint8_t MAGIC[4] = {'P', 'X', 'U', 'P'};
const uint8_t PROTO_VERSION = 0x02;
const size_t HEADER_SIZE = 11;  // MAGIC (4) + version (1) + frame_id (4) + count (2)
const uint8_t MAGIC_RUN[4] = {'P', 'X', 'U', 'R'};
const uint8_t RUN_VERSION = 0x01;
const size_t RUN_HEADER_SIZE = 11;  // MAGIC_RUN (4) + version (1) + frame_id (4) + count (2)

// Stats
unsigned long frameCount = 0;
unsigned long lastStats = 0;
unsigned long updatesApplied = 0;
uint32_t lastFrameId = 0;

struct PixelUpdate {
  uint16_t x;
  uint16_t y;
  uint16_t len;    // for run packets
  uint16_t color;
};

PixelUpdate* updateBuffer = nullptr;
uint32_t bufferCapacity = 0;

// Allocate or resize update buffer (tries PSRAM first, falls back to regular RAM)
bool ensureUpdateBuffer(uint32_t needed) {
  if (needed <= bufferCapacity && updateBuffer != nullptr) {
    return true;
  }
  PixelUpdate* tmp = (PixelUpdate*)ps_malloc(needed * sizeof(PixelUpdate));
  if (!tmp) {
    tmp = (PixelUpdate*)malloc(needed * sizeof(PixelUpdate));
  }
  if (!tmp) {
    Serial.println("Failed to allocate update buffer");
    return false;
  }
  if (updateBuffer) {
    free(updateBuffer);
  }
  updateBuffer = tmp;
  bufferCapacity = needed;
  return true;
}

bool readExactly(WiFiClient& c, uint8_t* dst, size_t len) {
  size_t got = 0;
  while (got < len && c.connected()) {
    int chunk = c.read(dst + got, len - got);
    if (chunk > 0) {
      got += chunk;
    } else {
      delay(1);  // allow other tasks
    }
  }
  return got == len;
}

// Display waiting screen with IP address and status message
void showWaitingScreen() {
  lilka::display.fillScreen(lilka::colors::Black);
  int16_t x1, y1;
  uint16_t w, h;
  
  lilka::display.setTextSize(1);
  lilka::display.getTextBounds("IP Address:", 0, 0, &x1, &y1, &w, &h);
  lilka::display.setCursor((lilka::display.width() - w) / 2, 100);
  lilka::display.println("IP Address:");
  
  String ipStr = WiFi.localIP().toString();
  lilka::display.setTextSize(1);
  lilka::display.setTextColor(lilka::colors::Green);
  lilka::display.getTextBounds(ipStr.c_str(), 0, 0, &x1, &y1, &w, &h);
  lilka::display.setCursor((lilka::display.width() - w) / 2, 125);
  lilka::display.println(ipStr);
  
  lilka::display.setTextSize(1);
  lilka::display.setTextColor(lilka::colors::Yellow);
  lilka::display.getTextBounds("Waiting for connection...", 0, 0, &x1, &y1, &w, &h);
  lilka::display.setCursor((lilka::display.width() - w) / 2, 210);
  lilka::display.println("Waiting for connection...");
}

void setup() {
  // Initialize Lilka (display, buttons, SD card, etc.)
  lilka::begin();
  lilka::display.fillScreen(lilka::colors::Black);

  // Load WiFi credentials from Keira's NVS storage
  String ssid, password;
  if (!loadWiFiCredentials(ssid, password)) {
    // No WiFi configured in Keira
    lilka::Alert alert(
      "WiFi Error",
      "No WiFi configured.\n\nPlease configure WiFi in Keira first.\n\nPress A to restart."
    );
    alert.draw(&lilka::display);
    while (!alert.isFinished()) {
      alert.update();
    }
    ESP.restart();
  }
  
  Serial.printf("Found WiFi credentials for: %s\n", ssid.c_str());
  
  // Connect to WiFi
  if (!connectToWiFi(ssid, password)) {
    // Connection failed
    lilka::Alert alert(
      "Connection Failed",
      "Failed to connect to WiFi.\n\nCheck credentials in Keira.\n\nPress A to restart."
    );
    alert.draw(&lilka::display);
    while (!alert.isFinished()) {
      alert.update();
    }
    ESP.restart();
  }

  showWaitingScreen();

  server.begin();
  server.setNoDelay(true);
  Serial.println("Server listening on port 8090");
}

bool handleClient() {
  // Accept new client
  if (!client || !client.connected()) {
    client = server.available();
    if (client) {
      Serial.println("Client connected");
      client.setNoDelay(true);
      client.setTimeout(50);  // short timeout for reads
      frameCount = 0;
      updatesApplied = 0;
      lilka::display.fillScreen(lilka::colors::Black);
    }
  }

  if (!client || !client.connected()) {
    return false;
  }

  // Require header to begin processing (pixel or run)
  if (client.available() < 11) {
    return true;  // keep connection, wait for more data
  }

  // Peek magic to decide packet type
  uint8_t magicBuf[4];
  if (!readExactly(client, magicBuf, 4)) {
    client.stop();
    return false;
  }
  bool isRun = (memcmp(magicBuf, MAGIC_RUN, 4) == 0);
  bool isPixel = (memcmp(magicBuf, MAGIC, 4) == 0);

  if (!isRun && !isPixel) {
    Serial.println("Bad magic; flushing stream");
    client.stop();
    return false;
  }

  if (isPixel) {
    uint8_t rest[HEADER_SIZE - 4];
    if (!readExactly(client, rest, sizeof(rest))) {
      Serial.println("Failed to read pixel header; dropping client");
      client.stop();
      return false;
    }
    if (rest[0] != PROTO_VERSION) {
      Serial.print("Unsupported pixel version: ");
      Serial.println(rest[0], HEX);
      client.stop();
      return false;
    }

    uint32_t frameId = ((uint32_t)rest[1]) | ((uint32_t)rest[2] << 8) | ((uint32_t)rest[3] << 16) | ((uint32_t)rest[4] << 24);
    uint16_t count = rest[5] | (rest[6] << 8);  // little-endian
    if (count == 0) {
      frameCount++;
      lastFrameId = frameId;
      return true;
    }
    if (count > (lilka::display.width() * lilka::display.height())) {
      Serial.print("Update count too large: ");
      Serial.println(count);
      client.stop();
      return false;
    }

    if (!ensureUpdateBuffer(count)) {
      Serial.println("No buffer for updates; dropping client");
      client.stop();
      return false;
    }

    uint8_t entry[6];
    for (uint16_t i = 0; i < count; i++) {
      if (!readExactly(client, entry, 6)) {
        Serial.println("Stream ended mid-frame; dropping client");
        client.stop();
        return false;
      }
      updateBuffer[i].x = entry[0] | (entry[1] << 8);
      updateBuffer[i].y = entry[2] | (entry[3] << 8);
      updateBuffer[i].color = entry[4] | (entry[5] << 8);
    }

    // Apply all updates in one batch after the full frame is received
    for (uint16_t i = 0; i < count; i++) {
      uint16_t x = updateBuffer[i].x;
      uint16_t y = updateBuffer[i].y;
      if (x < lilka::display.width() && y < lilka::display.height()) {
        lilka::display.drawPixel(x, y, updateBuffer[i].color);
        updatesApplied++;
      }
    }

    frameCount++;
    lastFrameId = frameId;
    unsigned long now = millis();
    if (now - lastStats > 2000) {
      Serial.print("Frames: ");
      Serial.print(frameCount);
      Serial.print(" (last frameId ");
      Serial.print(lastFrameId);
      Serial.print(") | Updates applied: ");
      Serial.println(updatesApplied);
      lastStats = now;
    }
    return true;
  }

  // Run packet
  uint8_t rest[RUN_HEADER_SIZE - 4];
  if (!readExactly(client, rest, sizeof(rest))) {
    Serial.println("Failed to read run header; dropping client");
    client.stop();
    return false;
  }
  if (rest[0] != RUN_VERSION) {
    Serial.print("Unsupported run version: ");
    Serial.println(rest[0], HEX);
    client.stop();
    return false;
  }

  uint32_t frameId = ((uint32_t)rest[1]) | ((uint32_t)rest[2] << 8) | ((uint32_t)rest[3] << 16) | ((uint32_t)rest[4] << 24);
  uint16_t count = rest[5] | (rest[6] << 8);  // number of runs
  if (count == 0) {
    frameCount++;
    lastFrameId = frameId;
    return true;
  }
  if (count > (lilka::display.width() * lilka::display.height())) {
    Serial.print("Run count too large: ");
    Serial.println(count);
    client.stop();
    return false;
  }

  if (!ensureUpdateBuffer(count)) {
    Serial.println("No buffer for run updates; dropping client");
    client.stop();
    return false;
  }

  // Each run entry: y (2), x0 (2), length (2), color (2) = 8 bytes
  uint8_t entry[8];
  for (uint16_t i = 0; i < count; i++) {
    if (!readExactly(client, entry, 8)) {
      Serial.println("Stream ended mid-run frame; dropping client");
      client.stop();
      return false;
    }
    updateBuffer[i].y = entry[0] | (entry[1] << 8);
    updateBuffer[i].x = entry[2] | (entry[3] << 8);
    updateBuffer[i].len = entry[4] | (entry[5] << 8);
    updateBuffer[i].color = entry[6] | (entry[7] << 8);
  }

  // Apply runs in one batch
  for (uint16_t i = 0; i < count; i++) {
    uint16_t x0 = updateBuffer[i].x;
    uint16_t y = updateBuffer[i].y;
    uint16_t runLen = updateBuffer[i].len;
    if (x0 < lilka::display.width() && y < lilka::display.height() && runLen > 0 && (x0 + runLen) <= lilka::display.width()) {
      lilka::display.fillRect(x0, y, runLen, 1, updateBuffer[i].color);
      updatesApplied += runLen;
    }
  }

  frameCount++;
  lastFrameId = frameId;
  unsigned long now = millis();
  if (now - lastStats > 2000) {
    Serial.print("Frames: ");
    Serial.print(frameCount);
    Serial.print(" (last frameId ");
    Serial.print(lastFrameId);
    Serial.print(") | Updates applied: ");
    Serial.println(updatesApplied);
    lastStats = now;
  }

  return true;
}

void loop() {
  handleClient();
  if (client && !client.connected()) {
    Serial.println("Client disconnected");
    showWaitingScreen();
  }
  delay(1);
}