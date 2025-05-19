// SPDX-FileCopyrightText: 2019 Brent Rubell for Adafruit Industries
//
// SPDX-License-Identifier: MIT

// Adafruit IO - Analog Devices ADT7410 + ADXL343 Example
//
// Adafruit invests time and resources providing this open source code.
// Please support Adafruit and open source hardware by purchasing
// products from Adafruit!
//
// Written by Brent Rubell for Adafruit Industries
// Copyright (c) 2019 Adafruit Industries
// Licensed under the MIT license.
//
// All text above must be included in any redistribution.

#include "secrets.h"

// comment out the following three lines if you are using fona or ethernet
#define USE_WINC1500
#include "AdafruitIO_WiFi.h"

#define P Serial.print
#define PL Serial.println

// Configure pins for Adafruit ATWINC1500 Feather
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS, 8,7,4,2);
// 	void setPins(int8_t cs, int8_t irq, int8_t rst, int8_t en = -1);

/************************** Configuration ***********************************/
// time between sending data to adafruit io, in seconds.
// 5 works for plus (60 per minute), but 10 is better for regular (24 per minute). 
// In actual use this should probably be... minutes and not seconds. Maybe see if the 
// processor can be put to sleep while it waits?
#define IO_DELAY 10

// How many seconds to wait before attempting to restart the router. 
#define CONNECT_TIMEOUT 60
// The restart attempt uses backoff
#define BACKOFF_FACTOR 1.5
// Try at most 3 times before giving up
#define RESTART_ATTEMPTS 3

// If true, more logging! Though note this will not do much if deep sleep is enabled
// as reconnected tot he serial port is... annoying.
#define DEBUG true

// If true, attempt to use SleepyDog to let the chips sleep more and save power
#define DEEP_SLEEP false

/************************** The good stuff ***********************************/

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_ADT7410.h"
#include <Adafruit_ADXL343.h>
#include <Adafruit_SleepyDog.h>

float tempC, accelX, accelY, accelZ;

// Create the ADT7410 temperature sensor object
Adafruit_ADT7410 tempsensor = Adafruit_ADT7410();

// Create the ADXL343 accelerometer sensor object
Adafruit_ADXL343 accel = Adafruit_ADXL343(12345);

// set up the 'temperature' feed
AdafruitIO_Feed *huzzah_temperature = io.feed("ioniq-5.ioniq-5-temp");

// set up the 'accelX' feed
AdafruitIO_Feed *huzzah_accel_x = io.feed("ioniq-5.ioniq-5-accel-x");

// set up the 'accelY' feed
AdafruitIO_Feed *huzzah_accel_y = io.feed("ioniq-5.ioniq-5-accel-y");

// set up the 'accelZ' feed
AdafruitIO_Feed *huzzah_accel_z= io.feed("ioniq-5.ioniq-5-accel-z");

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);

  // start the serial connection
  Serial.begin(9600);

  // wait for serial monitor to open
  while (!Serial)
    ;

//  Watchdog.enable(4000);

  PL("Adafruit IO - ADT7410 + ADX343");

  /* Initialise the ADXL343 */
  if(!accel.begin())
  {
    /* There was a problem detecting the ADXL343 ... check your connections */
    PL("Ooops, no ADXL343 detected ... Check your wiring!");
    while(1);
  }

  /* Set the range to whatever is appropriate for your project */
  accel.setRange(ADXL343_RANGE_16_G);

  /* Initialise the ADT7410 */
  if (!tempsensor.begin())
  {
    PL("Couldn't find ADT7410!");
    while (1)
      ;
  }

  // sensor takes 250 ms to get first readings
  delay(250);
 
  if (DEEP_SLEEP) {
    m2m_wifi_set_sleep_mode(M2M_PS_MANUAL, 1);
  } else {
    WiFi.maxLowPowerMode();
  }

  setup_WIFI();
}

void loop()
{
  digitalWrite(LED_BUILTIN, HIGH); // Show we're awake

  setup_WIFI();

  // io.run(); is required for all sketches.
  // it should always be present at the top of your loop
  // function. it keeps the client connected to
  // io.adafruit.com, and processes any incoming data.
  io.run();

   /* Get a new accel. sensor event */
  sensors_event_t event;
  accel.getEvent(&event);

  accelX = event.acceleration.x;
  accelY = event.acceleration.y;
  accelZ = event.acceleration.z;

  /* Display the results (acceleration is measured in m/s^2) */
  P("X: "); P(accelX); P("  ");
  P("Y: "); P(accelY); P("  ");
  P("Z: "); P(accelZ); P("  ");PL("m/s^2 ");
  
  // Read and print out the temperature
  tempC = tempsensor.readTempC();
  P("Temperature: "); P(tempC); PL("C");

  PL("Sending to Adafruit IO...");
  huzzah_temperature->save(tempC, 0, 0, 0, 2);
  huzzah_accel_x->save(accelX);
  huzzah_accel_y->save(accelY);
  huzzah_accel_z->save(accelZ);
  PL("Data sent!");

  P("Waiting ");P(IO_DELAY);PL(" seconds...");

  sleepyTime();
}

/**
 * Take a nap until the next time we want to update
 */
void sleepyTime() {
  digitalWrite(LED_BUILTIN, LOW); // Show we're asleep

  if (DEEP_SLEEP) {
    USBDevice.standby();
    Serial.flush();
    Serial.end();
  }

  // wait IO_DELAY seconds between sends
  for (int i = 0; i < IO_DELAY; i++) {
    if (DEEP_SLEEP) {
      m2m_wifi_request_sleep(1000);
      int sleepMS = Watchdog.sleep(1000);
    } else {
      delay(1000);
      PL("Slept 1000 ms...");
    }
  }
}

/**
 * Ensure we have a proper wifi connection. This will attempt to restart the router if it cannot establish connection
 * after CONNECT_TIMEOUT seconds (with backoff)
 */
void setup_WIFI() {
    // connect to io.adafruit.com
  P("Connecting to Adafruit IO");
  io.connect();

  int count = 0;
  int resetAttempts = 0;
  int maxCount = CONNECT_TIMEOUT;
  // wait for a connection
  while (io.status() < AIO_CONNECTED)
  {
    if (DEBUG) {
      P("\nio.status: "); 
      P(io.status()); P(": "); P(io.statusText()); P("\n"); 
      P("io.networkStatus: "); P(io.networkStatus()); P("\n"); 
      P("io.mqttStatus: "); P(io.mqttStatus()); P("\n"); 
      P("WiFi.status: "); P(WiFi.status()); P("\n");       
    } else {
      P(".");
    }

    delay(1000);
    count++;

    // If we've wated "too long" try and reset the hotspot
    if (count > maxCount) {
      count = 0;
      if (!reset_Hotspot()) {
        maxCount *= BACKOFF_FACTOR;
        resetAttempts++;

        if (resetAttempts > RESTART_ATTEMPTS) {
          PL("Well, we've lost and must now sleep forever");
          WiFi.disconnect();
          while(true) {
            digitalWrite(LED_BUILTIN, HIGH);
            Watchdog.sleep(1000);
            digitalWrite(LED_BUILTIN, LOW);
          }
        }
      }
    }
  }

  // we are connected
  PL();
  PL(io.statusText());
}

/**
 * Attempt to reset the wifi hotspot. This is a bit of a hefty process as we 
 * have to fake logging into the web ui and do some magic to trigger a reset 
 * of the hotspot. Tracking cookies from raw HTTP requests is old school AF.
 * 
 */
bool reset_Hotspot() {
  return true;
}

