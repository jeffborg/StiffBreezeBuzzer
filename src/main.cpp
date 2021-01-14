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

// enable this to shink image size for faster uploads
#include <Arduino.h>
// for every n millis function
#include <Wire.h>
#include <LiquidCrystal.h>
// menu builder
#include <LiquidMenu.h>

#include <FastLED.h>
#include <EEPROM.h>
// debouncing library
#include <Bounce2.h>
// timer library
#include <Ticker.h>

#define RELAY_PIN D1
#define BUTTON_PIN D7

#ifndef NO_DASHBOARD
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
#include <DNSServer.h>

#include "secrets.h"

const byte DNS_PORT = 53;
const IPAddress APIP(172, 0, 0, 1); // Gateway

DNSServer dnsServer;
#endif

// uint8_t rs, uint8_t enable,
//                  uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3);
LiquidCrystal lcd(D2, D3, D4, D5, D6, D8);

struct Settings {
  // internal time
  unsigned int intervalSeconds;
  // buzzer internal time
  unsigned int buzzerOnTimeMillis;
};

Settings timerSettings({120, 500});
// unsigned int intervalSeconds = 10;
// unsigned int buzzerOnTimeMillis = 200;

char countdown[10];
char *countdown_ptr = countdown;

char currentInterval[10];
char *currentInterval_ptr = currentInterval;

char runtime[10] = " 0:00";
char *runtime_ptr = runtime;

// menu
// Here the line is set to column 1, row 0 and will print the passed
// string and the passed variable.
LiquidLine welcome_line1(0, 0,        "Running: ", countdown_ptr);
LiquidLine welcome_line1_paused(0, 0, "PAUSED : ", countdown_ptr);
// LiquidLine welcome_line1(0, 0, "Running: ", countdown);
// Here the column is 3, the row is 1 and the string is "Hello Menu".
LiquidLine welcome_line2(0, 1, "Set:", currentInterval_ptr, " Run: ", runtime_ptr);
/*
 * LiquidScreen objects represent a single screen. A screen is made of
 * one or more LiquidLine objects. Up to four LiquidLine objects can
 * be inserted from here, but more can be added later in setup() using
 * welcome_screen.add_line(someLine_object);.
 */
// Here the LiquidLine objects are the two objects from above.
LiquidScreen welcome_screen(welcome_line1, welcome_line2);
LiquidScreen welcome_screen_paused(welcome_line1_paused, welcome_line2);

LiquidMenu menu(lcd, welcome_screen, welcome_screen_paused);


// next scheduled buzzer time
unsigned long nextBuzzerOffTime = 0;

#ifndef NO_DASHBOARD
/* Start Webserver */
AsyncWebServer server(80);

/* Attach ESP-DASH to AsyncWebServer */
ESPDash dashboard(&server);

/* 
  Dashboard Cards 
  Format - (Dashboard Instance, Card Type, Card Name, Card Symbol(optional) )
*/
Card currentInternal(&dashboard, GENERIC_CARD, "Current Interval");
Card interval(&dashboard, SLIDER_CARD, "Interval", "", 10, 300); // 10 seconds to 5 minutes
Card buzzerInternalTime(&dashboard, SLIDER_CARD, "Buzzer", "", 10, 2000); // 10 milliseconds to 1000 milliseconds
Card reset(&dashboard, BUTTON_CARD, "Restart");
Card timerRunningCard(&dashboard, BUTTON_CARD, "Active");
#endif


void triggerBuzzer();

// main timer
Ticker timer(triggerBuzzer, timerSettings.intervalSeconds * 1000);

void secondsToString(char * buffer, unsigned int seconds) {
  uint8_t actualSeconds = seconds % 60;
  sprintf(buffer, "%2d:%.2d", (seconds - actualSeconds) / 60, actualSeconds);
}

// update cards on dashboard from values
void updateDashboard(bool projectRunning = true) {
  #ifndef NO_DASHBOARD
  interval.update((int)timerSettings.intervalSeconds, "Seconds");
  buzzerInternalTime.update((int) timerSettings.buzzerOnTimeMillis, "MS");  
  #endif

  // lcd.setCursor(0, 0);
  // 01234567890123456789
  // INT: 000  HORN: DDDD
  // lcd.printf("INT: %3d  HORN: %4d", timerSettings.intervalSeconds, timerSettings.buzzerOnTimeMillis);
  bool updateDisplay = menu.get_currentScreen() == &welcome_screen || menu.get_currentScreen() == &welcome_screen_paused;
  // next time
  if (timer.state() == RUNNING) {
    int currentTime = (timer.elapsed() / 1000 / 1000) + 1;
    #ifndef NO_DASHBOARD
    currentInternal.update(currentTime);
    #endif
    // Serial.println(countdown);
    secondsToString(countdown, timerSettings.intervalSeconds - currentTime);
    if (updateDisplay) {
      menu.change_screen(&welcome_screen);
    }
  } else {
    if (updateDisplay) {
       menu.change_screen(&welcome_screen_paused);
    }
  }
  if (projectRunning && updateDisplay) {
    menu.update();
  }

  #ifndef NO_DASHBOARD
  timerRunningCard.update(timer.state() == RUNNING);

  dashboard.sendUpdates();
  #endif
}

Button resetButton = Button();

// write values to eeprom
void updateEEPROM() {
  EEPROM.put(0, timerSettings);
  EEPROM.commit();
  secondsToString(currentInterval, timerSettings.intervalSeconds);
}

void resetTimer(bool value) {
    Serial.println("[Card1] Button Callback Triggered: "+String((value)?"true":"false"));
    timer.start();
    triggerBuzzer();
}

void setup() {
  Serial.begin(74880); // same as esp8266 default
  lcd.begin(20,2);               // initialize the lcd 

  EEPROM.begin(512);

  // read in settings
  if (EEPROM.read(0) == 0xff) {
    // write default settings to eeprom
    updateEEPROM();
  } else {
    // eeprom is good
    EEPROM.get(0, timerSettings);
  }
  secondsToString(currentInterval, timerSettings.intervalSeconds);

  // reset the main timer again as we have loaded new time from eeprom
  timer.interval(timerSettings.intervalSeconds * 1000);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // start with buzzer off

  // SETUP BUTTON A
  resetButton.attach( BUTTON_PIN , INPUT_PULLUP );
  resetButton.interval(5); // interval in ms
  resetButton.setPressedState(LOW); // INDICATE THAT THE LOW STATE CORRESPONDS TO PHYSICALLY PRESSING THE BUTTON
  
  #ifndef NO_DASHBOARD
  /* Connect WiFi */
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(APIP, APIP, IPAddress(255,255,255,0));
  WiFi.softAP(SSID_NAME, SSID_PASSWORD);

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  /* Start AsyncWebServer */
  server.begin();

  /* Start DNS */
  dnsServer.start(DNS_PORT, "*", APIP); // DNS spoofing (Only for HTTP)
  #endif

  // nextBuzzerTime = millis() + (internalSeconds * 1000);
  updateDashboard(false);
  // update the dashboard

  #ifndef NO_DASHBOARD
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
  #endif
  // start and trigger the buzzer!
  timer.start();

  welcome_line1.attach_function(0, triggerBuzzer);
  // setup the menu
  // menu.add_screen(welcome_screen);
  menu.update();

  triggerBuzzer();
}

void loop() {
  resetButton.update();
  timer.update();
  
  #ifndef NO_DASHBOARD
  dnsServer.processNextRequest();
  #endif

  if (resetButton.pressed()) {
    resetTimer(true);
    menu.switch_focus();
  }

  /* Send Updates to our Dashboard (realtime) */
  EVERY_N_MILLIS (1000) {
    secondsToString(runtime, millis() / 1000);
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