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
#include <EEPROM.h>
// debouncing library
#include <Bounce2.h>
// timer library
#include <Ticker.h>

#define RELAY_PIN D1
#define BUTTON_PIN D7

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

struct Settings {
  // internal time
  unsigned int intervalSeconds;
  // buzzer internal time
  unsigned int buzzerOnTimeMillis;
};

Settings timerSettings({120, 500});
// unsigned int intervalSeconds = 10;
// unsigned int buzzerOnTimeMillis = 200;

// next scheduled buzzer time
unsigned long nextBuzzerOffTime = 0;

/* Start Webserver */
AsyncWebServer server(80);

/* Attach ESP-DASH to AsyncWebServer */
ESPDash dashboard(&server);

/* 
  Dashboard Cards 
  Format - (Dashboard Instance, Card Type, Card Name, Card Symbol(optional) )
*/
Card currentInternal(&dashboard, GENERIC_CARD, "Current Internal");
Card interval(&dashboard, SLIDER_CARD, "Internal", "", 10, 600); // 10 seconds to 5 minutes
Card buzzerInternalTime(&dashboard, SLIDER_CARD, "Buzzer", "", 10, 1000); // 10 milliseconds to 1000 milliseconds
Card reset(&dashboard, BUTTON_CARD, "Restart");
Card timerRunningCard(&dashboard, BUTTON_CARD, "Active");


void triggerBuzzer();

// main timer
Ticker timer(triggerBuzzer, timerSettings.intervalSeconds * 1000);

// update cards on dashboard from values
void updateDashboard() {
  interval.update((int)timerSettings.intervalSeconds, "Seconds");
  buzzerInternalTime.update((int) timerSettings.buzzerOnTimeMillis, "MS");  

  // next time
  if (timer.state() == RUNNING) {
    currentInternal.update((int)(timer.elapsed() / 1000 / 1000) + 1);
  }

  timerRunningCard.update(timer.state() == RUNNING);

  dashboard.sendUpdates();

}

Button resetButton = Button();

// write values to eeprom
void updateEEPROM() {
  EEPROM.put(0, timerSettings);
  EEPROM.commit();
}

void resetTimer(bool value) {
    Serial.println("[Card1] Button Callback Triggered: "+String((value)?"true":"false"));
    timer.start();
    triggerBuzzer();
}

void setup() {
  Serial.begin(74880); // same as esp8266 default

  EEPROM.begin(512);

  // read in settings
  if (EEPROM.read(0) == 0xff) {
    // write default settings to eeprom
    updateEEPROM();
  } else {
    // eeprom is good
    EEPROM.get(0, timerSettings);
  }

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // start with buzzer off

  // SETUP BUTTON A
  resetButton.attach( BUTTON_PIN , INPUT_PULLUP );
  resetButton.interval(5); // interval in ms
  resetButton.setPressedState(LOW); // INDICATE THAT THE LOW STATE CORRESPONDS TO PHYSICALLY PRESSING THE BUTTON
  
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

  // change the buzzer buzzing time
  buzzerInternalTime.attachCallback([&](int value) {
    Serial.println("[buzz internal] Slider Callback Triggered: "+String(value));

    timerSettings.buzzerOnTimeMillis = value;
    updateEEPROM();
    updateDashboard();
  });

  // updating the internal
  interval.attachCallback([&](int value) {
    Serial.println("[Card1] Slider Callback Triggered: "+String(value));
    timerSettings.intervalSeconds = value;
    updateEEPROM();
    // set the new timer interval
    timer.interval(timerSettings.intervalSeconds * 1000);
    updateDashboard();
  });
  timerRunningCard.attachCallback([&](bool value) {
    if (timer.state() == RUNNING) {
      timer.pause();
    } else if (timer.state() == PAUSED) {
      timer.resume();
    }
    updateDashboard();
  });
  reset.attachCallback(resetTimer);

  // start and trigger the buzzer!
  timer.start();
  triggerBuzzer();
}

void loop() {
  resetButton.update();
  timer.update();

  if (resetButton.pressed()) {
    resetTimer(true);
  }
  /* Send Updates to our Dashboard (realtime) */
  EVERY_N_MILLIS (1000) {
    updateDashboard();
  }

  // next internal
  if (nextBuzzerOffTime != 0 && nextBuzzerOffTime <= millis()) {
    nextBuzzerOffTime = 0;
    Serial.println("Buzzer off");
    digitalWrite(RELAY_PIN, LOW);
    updateDashboard();
  }
  
}

void triggerBuzzer() {
    nextBuzzerOffTime = millis() + timerSettings.buzzerOnTimeMillis;
    Serial.println("Buzzer on");
    digitalWrite(RELAY_PIN, HIGH);
    updateDashboard();
}