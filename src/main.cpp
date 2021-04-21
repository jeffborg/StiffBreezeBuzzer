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
  #include <ArduinoOTA.h>
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

struct charge
{
    unsigned int socpct;
    float volts;
};

// taken from https://web.archive.org/web/20100722144045/http://www.windsun.com/Batteries/Battery_FAQ.htm#Battery%20Voltages
const struct charge soctable[] = {
    { 0, 10.5 },
    { 10, 11.31 },
    { 20, 11.58 },
    { 30, 11.75 },
    { 40, 11.9 },
    { 50, 12.06 },
    { 60, 12.20 },
    { 70, 12.32 },
    { 80, 12.42 },
    { 90, 12.5 },
    { 100, 12.7 }
};
const unsigned int soctableitems = sizeof(soctable) / sizeof(struct charge);

// convert a voltage to a rough percentage
float voltToPercent(float volts) {
    for(unsigned int i = 0; i < soctableitems; i++) {
        if (volts < soctable[i].volts) {
            if (i == 0) {
                // first item is basically flat!
                return soctable[i].socpct;
            } else {
                // have pervious item to integrate value
                struct charge prevv = soctable[i-1];
                struct charge thisv = soctable[i];
                return prevv.socpct + ((thisv.socpct - prevv.socpct) * (volts - prevv.volts) / (thisv.volts - prevv.volts));
            }
        }
    }
    // beyond table just return 100
    return 100;
}

float batteryPercentage;


// are we in the menu system
bool bInMenu = false;
// new setting for seconds
unsigned int newIntervalSeconds;

// custom characters

// play
// pause
// battery
// timer
// runtime

namespace glyphs {
  uint8_t play[8] = {
        0B10000,
        0B11000,
        0B11100,
        0B11110,
        0B11100,
        0B11000,
        0B10000,
        0B00000
  };
  uint8_t pause[8] = {
        0B11011,
        0B11011,
        0B11011,
        0B11011,
        0B11011,
        0B11011,
        0B11011,
        0B00000
  };

  uint8_t clock[8] = {
			0b00000,
			0b01110,
			0b10101,
			0b10111,
			0b10001,
			0b01110,
			0b00000,
			0b00000
  };
  byte play_glyphIndex = 0;
  byte pause_glyphIndex = 1;
  byte clock_glyphIndex = 2;
}

// menu
// Here the line is set to column 1, row 0 and will print the passed
// string and the passed variable.
// 01234567890123456789
// X N:NN RUN VLT NN.NN 
// X N:NN --- BAT NN.NN
// X N:NN SET RUN DD:DD
LiquidLine welcome_line1(0, 0,        glyphs::play_glyphIndex,  countdown_ptr, " RUN VLT ", batteryVoltage);
LiquidLine welcome_line1_paused(0, 0, glyphs::pause_glyphIndex, countdown_ptr, " --- PCT ", batteryPercentage);
// LiquidLine welcome_line1(0, 0, "Running: ", countdown);
// Here the column is 3, the row is 1 and the string is "Hello Menu".
LiquidLine welcome_line2(0, 1, glyphs::clock_glyphIndex, currentInterval_ptr, " SET RUN ", runtime_ptr);
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
LiquidLine config_timer_up(0, 0, " +5S: ", newIntervalSeconds);
LiquidLine config_timer_down(0, 1, " -5S:", newInterval_ptr);
LiquidLine config_save(0, 1, " Save");
LiquidLine config_cancel(0, 1, " Cancel");
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

// callback for single button click - will pause timer in run mode or select selection in config mode
void buttonClick();
// callback for entering config in run mode or selecting the next item in config mode
void buttonDoubleClick();
// call back to start the buzzer and begin the timer reset
void buttonLongPressStart();
// call back to stop the buzzer
void buttonLongPressStop();

// this will take the temp settings (timer interval) and commit to EEPROM as well as update the working setting in memory
void saveAndCommitSettings();
// this will simply exit config mode
void cancelSettings();
// this will increase the temp interval by 5 seconds
void updateNewIntervalUp();
// this will decrease the temp interval by 5 seconds
void updateNewIntervalDown();

// main timer
Ticker timer(triggerBuzzer, timerSettings.intervalSeconds * 1000);

// convert a single number of seconds to a string formatted DD:DD for minutes and seconds the string will be placed into a buffer
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
    int currentTime = (timer.elapsed() / 1000 / 1000);
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

// every second + at the state
void updateEverySecond() {
    secondsToString(runtime, millis() / 1000);
    analogValue = analogRead(VOLTAGE_PIN);
    batteryVoltage = (analogValue * VOLATE_MULTIPLIER) + VOLTAGE_OFFSET;
    batteryPercentage = voltToPercent(batteryVoltage);
    updateDashboard();
}

void setup() {
  Serial.begin(74880); // same as esp8266 default
  lcd.begin(20,2);               // initialize the lcd 
  lcd.createChar(glyphs::play_glyphIndex, glyphs::play);
  lcd.createChar(glyphs::pause_glyphIndex, glyphs::pause);
  lcd.createChar(glyphs::clock_glyphIndex, glyphs::clock);

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
  secondsToString(countdown, timerSettings.intervalSeconds);

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

  /* Arduino ota */
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();

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

  // setup glyphs
  welcome_line1.set_asGlyph(1);
  welcome_line1_paused.set_asGlyph(1);
  welcome_line2.set_asGlyph(1);

  // config menu scrolls
  config_timer_up.attach_function(1,  updateNewIntervalUp);
  config_timer_down.attach_function(1,  updateNewIntervalDown);
  config_save.attach_function(1, saveAndCommitSettings);
  config_cancel.attach_function(1, buttonLongPressStart);
  config_screen.set_displayLineCount(2);

  menu.set_focusPosition(Position::LEFT);

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

  // do initial update
  updateEverySecond();
}

void loop() {
  ArduinoOTA.handle();
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
    updateEverySecond();
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
    // bug skip the last line if it's not focusable
    if (!menu.set_focusedLine(menu.get_focusedLine())) {
      menu.switch_focus();
    }
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