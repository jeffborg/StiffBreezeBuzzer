/*
  -------------------------
  ESPDASH - Dynamic Example
  -------------------------

  Skill Level: Advanced ( Must have good knowledge about heap, pointers and destructors )

  In this example we will be creating a dynamic dashboard which consists 
  of some cards and then remove a card from dashboard after some time.

  Github: https://github.com/ayushsharma82/ESP-DASH
  WiKi: https://ayushsharma82.github.io/ESP-DASH/

  Works with both ESP8266 & ESP32
*/

#include <Arduino.h>
// for every n millis function
#include <FastLED.h>
#include <DateTime.h>

#if defined(ESP8266)
  /* ESP8266 Dependencies */
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
  #include <ESPAsyncWebServer.h>
#elif defined(ESP32)
  /* ESP32 Dependencies */
  #include <WiFi.h>
  #include <AsyncTCP.h>
  #include <ESPAsyncWebServer.h>
#endif
#include <ESPDash.h>

#include "secrets.h"

/* Start Webserver */
AsyncWebServer server(80);

/* Attach ESP-DASH to AsyncWebServer */
ESPDash dashboard(&server);

/* 
  Dashboard Cards 
  Format - (Dashboard Instance, Card Type, Card Name, Card Symbol(optional) )
*/
Card totalTime(&dashboard, GENERIC_CARD, "Total Time");

void setup() {
  Serial.begin(115200);
  DateTime.sync(0); // start the clock

  /* Connect WiFi */
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.printf("WiFi Failed!\n");
      return;
  }
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  /* Start AsyncWebServer */
  server.begin();
}

void loop() {

  /* Send Updates to our Dashboard (realtime) */
  EVERY_N_MILLIS (1000) {
    totalTime.update((int)(millis() / 1000));
    dashboard.sendUpdates();
  }
  
}