#include <Arduino.h>
#include <ArduinoOTA.h>
#include "OTA_PASSWORD.h"

#include <stdint.h>
#include <string.h>

#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h>

#define PIN_TICK    D4

#define HTTP_HOST   "gmc.vinceh121.me"
#define HTTP_PORT   80

#define USER_ID     1234
#define DEVICE_ID   1234

#define LOG_PERIOD  10

static WiFiManager wifiManager;
static WiFiClient wifiClient;
static HTTPClient http;

// the total count value
static volatile unsigned long counts = 0;

static int secondcounts[60];
static unsigned long int secidx_prev = 0;
static unsigned long int count_prev = 0;
static unsigned long int second_prev = 0;

// interrupt routine
ICACHE_RAM_ATTR static void tube_impulse(void)
{
    counts++;
}

void setup(void)
{
    // initialize serial port
    Serial.begin(115200);
    Serial.println("GEIGER\n");

    // setup OTA
    ArduinoOTA.setHostname("esp-geiger");
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.begin();

    // connect to wifi
    Serial.println("Starting WIFI manager ...");
    wifiManager.setConfigPortalTimeout(120);
    wifiManager.autoConnect("ESP-GEIGER");

    // start counting
    memset(secondcounts, 0, sizeof(secondcounts));
    Serial.println("Starting count ...");

    pinMode(PIN_TICK, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_TICK), tube_impulse, FALLING);
}

static bool http_send(int value)
{
    if (http.begin(wifiClient, HTTP_HOST, HTTP_PORT, String("/api/v1/log2?AID=") + USER_ID + "&GID=" + DEVICE_ID + "&CPM=" + value)) {
        Serial.print("Connecting HTTP...");
        int status = http.get();
        if (status == 200) {
            Serial.println("Done!");
            return true;
        } else {
            Serial.print("Failed!");
            Serial.print(" Status: ")
            Serial.println(status);
        }
        String line = http.getString();
        Serial.print("-> ");
        Serial.println(line);
    } else {
        Serial.println("Connect failed!");
    }
    http.end();
    return false;
}

void loop()
{
    // update the circular buffer every second
    unsigned long int second = millis() / 1000;
    unsigned long int secidx = second % 60;
    if (secidx != secidx_prev) {
        // new second, store the counts from the last second
        unsigned long int count = counts;
        secondcounts[secidx_prev] = count - count_prev;
        count_prev = count;
        secidx_prev = secidx;
    }
    // report every LOG_PERIOD
    if ((second - second_prev) >= LOG_PERIOD) {
        second_prev = second;

        // calculate sum
        int cpm = 0;
        for (int i = 0; i < 60; i++) {
            cpm += secondcounts[i];
        }

        // send over HTTP
        if (!http_send(cpm)) {
            Serial.println("Restarting ESP...");
            ESP.restart();
        }
    }

    // allow OTA
    ArduinoOTA.handle();
}

