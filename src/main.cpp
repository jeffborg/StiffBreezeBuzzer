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
// One button lib
#include <OneButton.h>
// timer library
#include <Ticker.h>

#define RELAY_PIN D1
#define BUTTON_PIN D7
#define VOLTAGE_PIN A0
#define VOLATE_MULTIPLIER (0.015362776025237)
#define VOLTAGE_OFFSET (0.779)

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
LiquidCrystal lcd(D2, D3, D4, D5, D6, D0);

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

char newInterval[10];
char *newInterval_ptr = newInterval;


char runtime[10] = " 0:00";
char *runtime_ptr = runtime;

int analogValue;
float batteryVoltage;

// are we in the menu system
bool bInMenu = false;
// new setting for seconds
unsigned int newIntervalSeconds;

// Used for attaching something to the lines, to make them focusable.
void blankFunction() {
    return;
}

// menu
// Here the line is set to column 1, row 0 and will print the passed
// string and the passed variable.
LiquidLine welcome_line1(0, 0,        "Run:", countdown_ptr, " Bat: ", batteryVoltage);
LiquidLine welcome_line1_paused(0, 0, "---:", countdown_ptr, " Bat: ", batteryVoltage);
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

// config menus
LiquidLine config_timer_up(0, 0, "Adjust +5: ", newIntervalSeconds);
LiquidLine config_timer_down(0, 1, "Adjust -5:", newInterval_ptr);
LiquidLine config_save(0, 1, "Save");
LiquidLine config_cancel(0, 1, "Cancel");
LiquidScreen config_screen(config_timer_up, config_timer_down, config_save, config_cancel);

LiquidMenu menu(lcd, welcome_screen, welcome_screen_paused, config_screen);


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
Card buzzerInternalTime(&dashboard, SLIDER_CARD, "Buzzer", "", 10, 5000); // 10 milliseconds to 1000 milliseconds
Card reset(&dashboard, BUTTON_CARD, "Restart");
Card timerRunningCard(&dashboard, BUTTON_CARD, "Active");
#endif

void triggerBuzzer();

void buttonClick();
void buttonDoubleClick();
void buttonLongPressStart();
void buttonLongPressStop();

void saveAndCommitSettings();
void cancelSettings();
void updateNewIntervalUp();
void updateNewIntervalDown();

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
  // next time
  if (timer.state() == RUNNING) {
    int currentTime = (timer.elapsed() / 1000 / 1000) + 1;
    #ifndef NO_DASHBOARD
    currentInternal.update(currentTime);
    #endif
    // Serial.println(countdown);
    secondsToString(countdown, timerSettings.intervalSeconds - currentTime);
    if (!bInMenu) {
      menu.change_screen(&welcome_screen);
    }
  } else {
    if (!bInMenu) {
       menu.change_screen(&welcome_screen_paused);
    }
  }
  if (projectRunning && !bInMenu) {
    menu.update();
  }

  #ifndef NO_DASHBOARD
  timerRunningCard.update(timer.state() == RUNNING);

  dashboard.sendUpdates();
  #endif
}

OneButton button(BUTTON_PIN); // active low and pulled up (button is wired to GROUND)

// write values to eeprom
void updateEEPROM() {
  EEPROM.put(0, timerSettings);
  EEPROM.commit();
  secondsToString(currentInterval, timerSettings.intervalSeconds);
}

void updateIntervalSetting(int value) {
    Serial.println("[Card1] Slider Callback Triggered: "+String(value));
    timerSettings.intervalSeconds = value;
    updateEEPROM();
    // set the new timer interval
    timer.interval(timerSettings.intervalSeconds * 1000);
    updateDashboard();
  }


void resetTimer(bool value = true) {
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
  // in case of corruped memory
  if (timerSettings.intervalSeconds >= 3600 || timerSettings.buzzerOnTimeMillis >= 60000) {
    timerSettings.intervalSeconds = 120;
    timerSettings.buzzerOnTimeMillis = 2000;
    updateEEPROM();
  }
  Serial.printf("Interval: %d, BuzzerTime: %d\n", timerSettings.intervalSeconds, timerSettings.buzzerOnTimeMillis);
  secondsToString(currentInterval, timerSettings.intervalSeconds);

  // reset the main timer again as we have loaded new time from eeprom
  timer.interval(timerSettings.intervalSeconds * 1000);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // start with buzzer off
  
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
  interval.attachCallback(updateIntervalSetting);

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

  // config menu scrolls
  config_timer_up.attach_function(1,  updateNewIntervalUp);
  config_timer_down.attach_function(1,  updateNewIntervalDown);
  config_save.attach_function(1, saveAndCommitSettings);
  config_cancel.attach_function(1, buttonLongPressStart);
  config_screen.set_displayLineCount(2);

  menu.set_focusPosition(Position::RIGHT);

  // setup the menu
  // menu.add_screen(welcome_screen);
  menu.update();

  if (digitalRead(BUTTON_PIN) != LOW) {
    triggerBuzzer();
  }

  button.attachClick(buttonClick);
  button.attachDoubleClick(buttonDoubleClick);
  button.attachLongPressStart(buttonLongPressStart);
  button.attachLongPressStop(buttonLongPressStop);
}

void loop() {
  button.tick();
  timer.update();
  
  #ifndef NO_DASHBOARD
  dnsServer.processNextRequest();
  #endif

  // if (resetButton.pressed()) {
  //   resetTimer(true);
  //   menu.switch_focus();
  // }

  /* Send Updates to our Dashboard (realtime) */
  EVERY_N_MILLIS (1000) {
    secondsToString(runtime, millis() / 1000);
    analogValue = analogRead(VOLTAGE_PIN);
    batteryVoltage = (analogValue * VOLATE_MULTIPLIER) + VOLTAGE_OFFSET;
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

void buttonClick() {
  if (bInMenu) {
    menu.call_function(1);
  } else {
    // pause resume
    if (timer.state() == RUNNING) {
      timer.pause();
    } else if (timer.state() == PAUSED) {
      timer.resume();
    }
    updateDashboard();
  }
  Serial.println("CLICK! (QUICK PRESS)");
}

void buttonDoubleClick() {
  if (bInMenu) {
    menu.switch_focus();
  } else {
    // enter menu
    bInMenu = true;
    newIntervalSeconds = timerSettings.intervalSeconds;
    secondsToString(newInterval, newIntervalSeconds);
    menu.change_screen(&config_screen);
    menu.set_focusedLine(0);
    menu.update();
  }
  Serial.println("DOUBLE CLICK!");
}

// this is back or force reset
void buttonLongPressStart() {
  if (bInMenu) {
    bInMenu = false;
    // force screen refresh
    updateDashboard();
  } else {
    resetTimer();
    nextBuzzerOffTime = millis() + 60000; // max 60 seconds
  }
  Serial.println("buttonLongPressStart");
}

// only useful when outside the menu
void buttonLongPressStop() {
  if (!bInMenu && nextBuzzerOffTime != 0) {
    nextBuzzerOffTime = 1; // force buzzer off
  }
  Serial.println("buttonLongPressStop");
}

void saveAndCommitSettings() {
  updateIntervalSetting(newIntervalSeconds);
  buttonLongPressStart();
}

void updateNewIntervalUp() {
  newIntervalSeconds += 5;
  secondsToString(newInterval, newIntervalSeconds);
}

void updateNewIntervalDown() {
  newIntervalSeconds -= 5;
  if (newIntervalSeconds < 10) {
    newIntervalSeconds = 10;
  }
  secondsToString(newInterval, newIntervalSeconds);
}