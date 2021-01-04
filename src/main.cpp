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

#define RELAY_PIN D1

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

// internal time
unsigned int internalSeconds = 10;
// buzzer internal time
unsigned int buzzerOnTimeMillis = 200;

// next scheduled buzzer time
unsigned long lastBuzzerTime = 0;
long nextBuzzerOffTime = -1;

/* Start Webserver */
AsyncWebServer server(80);

/* Attach ESP-DASH to AsyncWebServer */
ESPDash dashboard(&server);

/* 
  Dashboard Cards 
  Format - (Dashboard Instance, Card Type, Card Name, Card Symbol(optional) )
*/
Card totalTime(&dashboard, GENERIC_CARD, "Total Time");
Card currentInternal(&dashboard, GENERIC_CARD, "Current Internal");
Card interval(&dashboard, SLIDER_CARD, "Internal", "", 10, 600); // 10 seconds to 5 minutes
Card buzzerInternalTime(&dashboard, SLIDER_CARD, "Buzzer", "", 10, 1000); // 10 milliseconds to 1000 milliseconds

// update cards on dashboard from values
void updateDashboard() {
  interval.update((int)internalSeconds, "Seconds");
  buzzerInternalTime.update((int) buzzerOnTimeMillis, "MilliSeconds");  

  totalTime.update((int)(millis() / 1000));
  // next time
  currentInternal.update((int)( (lastBuzzerTime + (internalSeconds*1000)) - millis() ) / 1000);

  dashboard.sendUpdates();

}

void setup() {
  Serial.begin(115200);

  pinMode(D1, OUTPUT);
  digitalWrite(D1, LOW); // start with buzzer off
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

  // nextBuzzerTime = millis() + (internalSeconds * 1000);
  updateDashboard();
  // update the dashboard

  buzzerInternalTime.attachCallback([&](int value) {
    Serial.println("[buzz internal] Slider Callback Triggered: "+String(value));

    buzzerOnTimeMillis = value;
    updateDashboard();
  });
  interval.attachCallback([&](int value) {
    Serial.println("[Card1] Slider Callback Triggered: "+String(value));

    internalSeconds = value;
    updateDashboard();
  });
}

void loop() {

  /* Send Updates to our Dashboard (realtime) */
  EVERY_N_MILLIS (1000) {
    updateDashboard();
  }

  // end of internal
  if ((lastBuzzerTime + (internalSeconds*1000)) <= millis()) {
    // buzzer goes off
    lastBuzzerTime = millis();
    // buzzer off time can just be a number
    nextBuzzerOffTime = millis() + buzzerOnTimeMillis;
    Serial.println("Buzzer on");
    digitalWrite(D1, HIGH);
    updateDashboard();
  }
  // next internal
  if (nextBuzzerOffTime != -1 && nextBuzzerOffTime <= millis()) {
    nextBuzzerOffTime = -1;
    Serial.println("Buzzer off");
    digitalWrite(D1, LOW);
    updateDashboard();
  }
  
}