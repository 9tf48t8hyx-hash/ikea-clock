#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <time.h>
#include <vector>
#include "screen.h"
#include "constants.h"

// Requis par screen.cpp
volatile SYSTEM_STATUS currentStatus = NONE;

// ---------------------------------------------------------------------------
// Luminosité — appui court sur le bouton pour cycler
// ---------------------------------------------------------------------------
const uint8_t BRIGHTNESS_LEVELS[] = {20, 80, 180, 255};
const int     NUM_LEVELS          = 4;
int           brightnessIdx       = 3; // démarre au max

// ---------------------------------------------------------------------------
// Bouton (actif bas, pull-up interne) — broche PIN_BUTTON = 16
// ---------------------------------------------------------------------------
bool          lastBtnRaw    = HIGH;
unsigned long btnPressStart = 0;
bool          btnHandled    = false;
#define BTN_LONG_MS 3000   // maintien 3s = reset WiFi

// ---------------------------------------------------------------------------
// Chenillard — périmètre 16×16 = exactement 60 pixels (1 par seconde)
//
// Sens horaire depuis le coin haut-gauche :
//   Haut   (0,0)→(15,0)  : 16 px
//   Droite (15,1)→(15,15): 15 px
//   Bas    (14,15)→(0,15): 15 px
//   Gauche (0,14)→(0,1)  : 14 px
//   Total  : 60 px
//
// Chaque seconde on allume un pixel de plus (effet remplissage) :
//   - pixels 0..sec-1 : tamisés (secondes écoulées)
//   - pixel sec       : vif (tête du chenillard)
// À la nouvelle minute on repart de 0.
// ---------------------------------------------------------------------------
struct Pt { uint8_t x, y; };
Pt perim[60];

void buildPerimeter() {
    int i = 0;
    for (int x = 0;  x < 16; x++) perim[i++] = {(uint8_t)x, 0};    // haut  G→D
    for (int y = 1;  y < 16; y++) perim[i++] = {15, (uint8_t)y};    // droite H→B
    for (int x = 14; x >= 0; x--) perim[i++] = {(uint8_t)x, 15};   // bas    D→G
    for (int y = 14; y >= 1; y--) perim[i++] = {0, (uint8_t)y};     // gauche B→H
}

void clearPerimeter() {
    for (int i = 0; i < 60; i++)
        Screen.setPixel(perim[i].x, perim[i].y, 0);
}

void drawChenillard(int sec) {
    clearPerimeter();
    for (int i = 0; i < sec; i++)
        Screen.setPixel(perim[i].x, perim[i].y, 1, 130); // écoulées : tamisées
    Screen.setPixel(perim[sec].x, perim[sec].y, 1, 255); // actuelle : vive
}

// ---------------------------------------------------------------------------
// Mise à jour de l'affichage — ne redessine que ce qui a changé
// ---------------------------------------------------------------------------
static int prevSec  = -1;
static int prevMin  = -1;
static int prevHour = -1;
bool       needRedraw = true;

void updateDisplay(struct tm &t) {
    if (needRedraw || t.tm_hour != prevHour) {
        std::vector<int> hh = {t.tm_hour / 10, t.tm_hour % 10};
        Screen.clearRect(3, 2, 9, 6);
        Screen.drawNumbers(3, 2, hh);
        prevHour = t.tm_hour;
    }
    if (needRedraw || t.tm_min != prevMin) {
        std::vector<int> mm = {t.tm_min / 10, t.tm_min % 10};
        Screen.clearRect(3, 8, 9, 6);
        Screen.drawNumbers(3, 8, mm);
        prevMin = t.tm_min;
    }
    if (needRedraw || t.tm_sec != prevSec) {
        drawChenillard(t.tm_sec);
        prevSec = t.tm_sec;
    }
    needRedraw = false;
}

// ---------------------------------------------------------------------------
// Gestion bouton physique
// ---------------------------------------------------------------------------
void handleButton() {
    bool raw = digitalRead(PIN_BUTTON);

    if (raw == LOW && lastBtnRaw == HIGH) {
        btnPressStart = millis();
        btnHandled    = false;
    }
    if (raw == LOW && !btnHandled && (millis() - btnPressStart >= BTN_LONG_MS)) {
        // Appui long : reset WiFi + reboot
        WiFiManager wm;
        wm.resetSettings();
        ESP.restart();
        btnHandled = true;
    }
    if (raw == HIGH && lastBtnRaw == LOW && !btnHandled) {
        // Appui court : cycle luminosité
        brightnessIdx = (brightnessIdx + 1) % NUM_LEVELS;
        Screen.setBrightness(BRIGHTNESS_LEVELS[brightnessIdx]);
        needRedraw = true;
    }
    lastBtnRaw = raw;
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);

    Screen.setup();
    Screen.clear();
    Screen.setBrightness(BRIGHTNESS_LEVELS[brightnessIdx]);
    Screen.setCurrentRotation(0);
    buildPerimeter();
    pinMode(PIN_BUTTON, INPUT_PULLUP);

    // Dots de chargement (identiques au ClockPlugin d'origine)
    Screen.setPixel(4, 7, 1);
    Screen.setPixel(5, 7, 1);
    Screen.setPixel(7, 7, 1);
    Screen.setPixel(8, 7, 1);
    Screen.setPixel(10, 7, 1);
    Screen.setPixel(11, 7, 1);

    // WiFi — portail captif au premier démarrage (SSID : IKEA-Clock)
    WiFiManager wm;
    wm.setConfigPortalTimeout(180);
    if (!wm.autoConnect(WIFI_MANAGER_SSID)) {
        ESP.restart();
    }

    // Synchronisation NTP
    configTzTime(TZ_INFO, NTP_SERVER);
    struct tm t;
    for (int i = 0; i < 30 && !getLocalTime(&t); i++) delay(500);

    // OTA pour mises à jour futures
    ArduinoOTA.setHostname("ikea-clock");
    ArduinoOTA.onStart([]() {
        currentStatus = UPDATE;
        Screen.clear();
    });
    ArduinoOTA.begin();

    Screen.clear();
    needRedraw = true;
}

// ---------------------------------------------------------------------------
// Loop
// ---------------------------------------------------------------------------
void loop() {
    ArduinoOTA.handle();
    handleButton();

    struct tm t;
    if (getLocalTime(&t)) {
        updateDisplay(t);
    }

    delay(50);
}
