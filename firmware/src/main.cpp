#include <AnimatedGIF.h>
#include <Arduino.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>

#include "AnimatedGIF_LittleFS.h"
#include "AnimatedGIF_TFT_eSPI.h"

// Static assets
extern const uint8_t index_html_start[] asm("_binary_assets_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_assets_index_html_end");
extern const uint8_t default_gif_start[] asm("_binary_assets_default_gif_start");
extern const uint8_t default_gif_end[] asm("_binary_assets_default_gif_end");

#define PASSWORD "bitbuilt"

WebServer server(80);
TFT_eSPI tft = TFT_eSPI();
AnimatedGIF gif;
bool gifPaused             = false;
bool gifNeedsReload        = false;
static bool settingsLocked = true;

void handleUnlock()
{
  Preferences prefs;
  prefs.begin("wifi", true);
  String settingsPass = prefs.getString("settingspass", "bitbuilt");
  prefs.end();
  if (server.arg("password") == settingsPass) {
    settingsLocked = false;
    server.send(200, "text/plain", "Unlocked");
  } else {
    server.send(401, "text/plain", "Incorrect password");
  }
}

void handleLock()
{
  settingsLocked = true;
  server.send(200, "text/plain", "Locked");
}

void handleGetLocked()
{
  server.send(200, "application/json", settingsLocked ? "true" : "false");
}

void handleOta()
{
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    if (settingsLocked)
      return;
    gifPaused = true;
    gif.close();
    size_t fsize = UPDATE_SIZE_UNKNOWN;
    if (server.hasArg("size"))
      fsize = server.arg("size").toInt();
    Update.begin(fsize);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (!settingsLocked)
      Update.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (!settingsLocked)
      Update.end(true);
  }
}

void handleOtaEnd()
{
  server.sendHeader("Connection", "close");
  if (settingsLocked) {
    server.send(401, "text/plain", "Unauthorized");
    return;
  }
  if (Update.hasError()) {
    gifPaused = false;
    server.send(502, "text/plain", Update.errorString());
    return;
  }
  server.send(200, "text/plain", "Update complete, rebooting...");
  delay(500);
  ESP.restart();
}

void handleImageGet()
{
  if (!LittleFS.exists("/uploaded.gif")) {
    server.send(404, "text/plain", "No image uploaded");
    return;
  }
  File f = LittleFS.open("/uploaded.gif", "r");
  server.streamFile(f, "image/gif");
  f.close();
}

void handleImageDelete()
{
  if (!LittleFS.exists("/uploaded.gif")) {
    server.send(404, "text/plain", "No image uploaded");
    return;
  }
  gif.close();
  LittleFS.remove("/uploaded.gif");
  gifNeedsReload = true;
  server.send(200, "text/plain", "Image deleted");
}

static File uploadFile;

void handleImageUpload()
{
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    gifPaused = true;
    gif.close();
    uploadFile = LittleFS.open("/uploaded.gif", "w");
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
    }
  }
}

void handleImageUploadEnd()
{
  gifNeedsReload = true;
  gifPaused      = false;
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", "Image uploaded successfully");
}

void handleGetSettings()
{
  if (settingsLocked) {
    server.send(401, "text/plain", "Unauthorized");
    return;
  }

  Preferences prefs;
  prefs.begin("wifi", true);
  String savedSsid = prefs.getString("ssid", "");
  bool hasPassword = prefs.getString("pass", "").length() > 0;
  prefs.end();

  bool isSta     = WiFi.getMode() == WIFI_STA;
  bool connected = isSta && WiFi.status() == WL_CONNECTED;
  String ip      = connected ? WiFi.localIP().toString() : WiFi.softAPIP().toString();

  String json = "{";
  json += "\"ssid\":\"" + savedSsid + "\",";
  json += "\"hasPassword\":" + String(hasPassword ? "true" : "false") + ",";
  json += "\"mode\":\"" + String(isSta ? "sta" : "ap") + "\",";
  json += "\"connected\":" + String(connected ? "true" : "false") + ",";
  json += "\"ip\":\"" + ip + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleSaveSettings()
{
  if (settingsLocked) {
    server.send(401, "text/plain", "Unauthorized");
    return;
  }

  String newSsid         = server.arg("ssid");
  String newPass         = server.arg("pass");
  String newSettingsPass = server.arg("settingspass");

  Preferences prefs;
  prefs.begin("wifi", false);
  prefs.putString("ssid", newSsid);
  if (newPass.length() > 0)
    prefs.putString("pass", newPass);
  if (newSettingsPass.length() > 0)
    prefs.putString("settingspass", newSettingsPass);
  prefs.end();

  server.send(200, "text/plain", "Saved. Rebooting...");
  delay(500);
  ESP.restart();
}

void setup()
{
  // Initialize filesystem
  LittleFS.begin(true);

  // Initialize screen
  tft.init();
  tft.fillScreen(TFT_BLACK);

  // Initialize GIF decoder
  gif.begin(BIG_ENDIAN_PIXELS);

  // Generate a unique SSID based on MAC address (used as AP fallback)
  char apSsid[14];
  long unsigned int espmac = ESP.getEfuseMac() >> 24;
  snprintf(apSsid, sizeof(apSsid), "GCNANO-%06lX", espmac);

  // Try to connect to a saved WiFi network, otherwise start AP
  Preferences prefs;
  prefs.begin("wifi", true);
  String savedSsid = prefs.getString("ssid", "");
  String savedPass = prefs.getString("pass", "");
  prefs.end();

  if (savedSsid.length() > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(savedSsid.c_str(), savedPass.c_str());
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
      delay(200);
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSsid, PASSWORD);
  }

  // Start mDNS responder for "gcnano.local"
  MDNS.begin("gcnano");

  // Initialize web server
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", (const char *)index_html_start, index_html_end - index_html_start);
  });
  server.on("/ota", HTTP_POST, handleOtaEnd, handleOta);
  server.on("/image", HTTP_GET, handleImageGet);
  server.on("/image", HTTP_POST, handleImageUploadEnd, handleImageUpload);
  server.on("/image", HTTP_DELETE, handleImageDelete);
  server.on("/settings", HTTP_GET, handleGetSettings);
  server.on("/settings", HTTP_POST, handleSaveSettings);
  server.on("/lock", HTTP_GET, handleGetLocked);
  server.on("/lock", HTTP_PUT, handleLock);
  server.on("/lock", HTTP_DELETE, handleUnlock);
  server.begin();
}

void playGif()
{
  // Open GIF, falling back to default if uploaded one fails
  if (!gif.open("/uploaded.gif", GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw))
    if (!gif.openFLASH((uint8_t *)default_gif_start, default_gif_end - default_gif_start, GIFDraw))
      return;

  gifNeedsReload = false;

  // Start SPI transaction to screen
  tft.startWrite();

  // Play each GIF frame, checking for web server requests between frames
  while (!gifPaused && !gifNeedsReload && gif.playFrame(true, NULL)) {
    server.handleClient();
  }

  // End SPI transaction and close GIF
  tft.endWrite();
  gif.close();
}

void loop()
{
  // Handle web server requests
  server.handleClient();

  // Play GIF if not paused
  if (!gifPaused)
    playGif();
}
