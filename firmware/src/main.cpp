#include <AnimatedGIF.h>
#include <Arduino.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>

#include "AnimatedGIF_LittleFS.h"
#include "AnimatedGIF_TFT_eSPI.h"

#define PASSWORD "bitbuilt"

WebServer server(80);
TFT_eSPI tft = TFT_eSPI();
AnimatedGIF gif;
bool gifPaused      = false;
bool gifNeedsReload = false;

void handleOta()
{
  // Get the size of the uploaded firmware, if provided
  size_t fsize = UPDATE_SIZE_UNKNOWN;
  if (server.hasArg("size"))
    fsize = server.arg("size").toInt();

  // Handle the firmware upload in chunks
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    // Pause GIF playback while updating firmware
    gifPaused = true;
    gif.close();

    // Start the update process
    Update.begin(fsize);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // Write the uploaded firmware chunk to flash
    Update.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    // Finish the update process
    Update.end(true);
  }
}

void handleOtaEnd()
{
  // Close the connection
  server.sendHeader("Connection", "close");
  if (Update.hasError()) {
    server.send(502, "text/plain", Update.errorString());
    return;
  }

  // Tell the client to refresh the page after a short delay
  server.sendHeader("Refresh", "10");
  server.sendHeader("Location", "/");
  server.send(307);

  // Restart the device to apply the new firmware
  delay(500);
  ESP.restart();
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

void setup()
{
  // Initialize filesystem
  LittleFS.begin();

  // Initialize screen
  tft.init();
  tft.fillScreen(TFT_BLACK);

  // Initialize GIF decoder
  gif.begin(BIG_ENDIAN_PIXELS);

  // Generate a unique SSID based on MAC address
  char ssid[14];
  long unsigned int espmac = ESP.getEfuseMac() >> 24;
  snprintf(ssid, sizeof(ssid), "GCNANO-%06lX", espmac);

  // Initialize WiFi in AP mode
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, PASSWORD);

  // Start mDNS responder for "gcnano.local"
  MDNS.begin("gcnano");

  // Initialize web server
  server.serveStatic("/", LittleFS, "/upload.html");
  server.serveStatic("/ota", LittleFS, "/ota.html");
  server.on("/ota", HTTP_POST, handleOtaEnd, handleOta);
  server.on("/upload", HTTP_POST, handleImageUploadEnd, handleImageUpload);
  server.begin();
}

void playGif()
{
  // Open GIF, falling back to default if uploaded one fails
  if (!gif.open("/uploaded.gif", GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw))
    if (!gif.open("/default.gif", GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw))
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
