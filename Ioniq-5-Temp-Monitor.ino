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

/************************** Configuration ***********************************/
// How many seconds to wait before attempting to restart the router.
#define CONNECT_TIMEOUT 15
// The restart attempt uses backoff
#define BACKOFF_FACTOR 1.5
// Try at most 3 times before giving up
#define RESTART_ATTEMPTS 3

// If true, more logging!
#define DEBUG true
// If defined, use the external DS18B20 temperature probe
#define USE_DS18B20 true
// If defined, use the TPL timer to shut off the arduino as needed
#define USE_TPL true
// If defined, use the FSM to handle restarting the wifi router as needed
#undef USE_FSM

// Pins
#define DS18B20_PIN 6
#ifdef USE_TPL
#define TPL_DONE_PIN 5
#endif

#define P Serial.print
#define PL Serial.println

#define USE_WINC1500
#include "AdafruitIO_WiFi.h"

// Configure pins for Adafruit ATWINC1500 Feather
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS, 8, 7, 4, 2);

/************************** The good stuff ***********************************/

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_ADT7410.h"
#include <Adafruit_ADXL343.h>
#ifdef USE_DS18B20
#include <OneWire.h>
#include <DallasTemperature.h>
#endif
#ifdef USE_FSM
#include <ArduinoHttpClient.h>
#include "Restarter.h"
#include "RestartStateMachine.h"
#endif

float tempC, carTempC, accelX, accelY, accelZ;
#ifdef USE_DS18B20
// DS18B20 setup
OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);
#endif
#ifdef USE_FSM
char serverAddress[] = "192.168.0.1";
int port = 80;
#endif

// Create the ADT7410 temperature sensor object
Adafruit_ADT7410 tempsensor = Adafruit_ADT7410();

// Create the ADXL343 accelerometer sensor object
Adafruit_ADXL343 accel = Adafruit_ADXL343(12345);

// set up the 'temperature' feeds
AdafruitIO_Feed *device_temperature = io.feed("ioniq-5.ioniq-5-temp");
#ifdef USE_DS18B20
AdafruitIO_Feed *car_temperature = io.feed("ioniq-5.ioniq-5-car-temp");
#endif

// set up the 'accelX' feed
AdafruitIO_Feed *accel_x = io.feed("ioniq-5.ioniq-5-accel-x");

// set up the 'accelY' feed
AdafruitIO_Feed *accel_y = io.feed("ioniq-5.ioniq-5-accel-y");

// set up the 'accelZ' feed
AdafruitIO_Feed *accel_z = io.feed("ioniq-5.ioniq-5-accel-z");

/************************** Custom functions ***********************************/

/**
 * Blink the built-in led to indicate status. 
 */
void debugLed(int count, int duration = 200) {
  for (int i = 0; i < count; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(duration);
    digitalWrite(LED_BUILTIN, LOW);
    delay(duration);
  }
}

/**
 * Setup the various sensors for measuring temperature and acceleration 
 */
void setupSensors() {
  if (!accel.begin()) {
    PL("ADXL343 not found!");
    debugLed(6, 100);
    shutdown();
  }
  accel.setRange(ADXL343_RANGE_2_G);

  if (!tempsensor.begin()) {
    PL("ADT7410 not found!");
    debugLed(7, 100);
    shutdown();
  }
#ifdef USE_DS18B20
  ds18b20.begin();
#endif
  delay(250);
}

/**
 * Connect to Adafruit IO 
 */
bool connectToAdafruitIO() {
  io.connect();
  int count = 0, resetAttempts = 0;
  int maxCount = CONNECT_TIMEOUT;

  while (io.status() < AIO_CONNECTED) {
    if (DEBUG) {
      P("\nio.status: ");
      P(io.status());
      P(" ");
      P(io.statusText());
      P("\n");
      P("WiFi.status: ");
      P(WiFi.status());
      P("\n");
    }
    delay(500);
    count++;

    // If we've wated "too long" try and reset the hotspot
    if (count > maxCount) {
      count = 0;
      if (!resetHotspot()) {
        maxCount *= BACKOFF_FACTOR;
        resetAttempts++;

        if (resetAttempts > RESTART_ATTEMPTS) {
          PL("Well, we've lost and must now sleep forever");
          // TODO: Need to consider if the FSM is in use... then go from here? The logic calling this code probably needs to check
          //       if the FSM is being used and if so do something different to let it run.
          return false;
        }
      }
    }
  }

  // we are connected
  PL();
  PL(io.statusText());
  PL("Connected to Adafruit IO");
  return true;
}

/** 
 * Fetch and store sensor data
 */
void collectSensorData() {
  PL("Reading sensors...");
  sensors_event_t event;
  accel.getEvent(&event);
  accelX = event.acceleration.x;
  accelY = event.acceleration.y;
  accelZ = event.acceleration.z;

  tempC = tempsensor.readTempC();

  /* Display the results (acceleration is measured in m/s^2) */
  P("X: ");
  P(accelX);
  P("  ");
  P("Y: ");
  P(accelY);
  P("  ");
  P("Z: ");
  P(accelZ);
  P("  ");
  PL("m/s^2 ");
  P("Temperature: ");
  P(tempC);
  PL("C");

#ifdef USE_DS18B20
  ds18b20.requestTemperatures();
  carTempC = ds18b20.getTempCByIndex(0);
  P("Car emperature: ");
  P(carTempC);
  PL("C");
#endif

  PL("Done!");
  debugLed(2);
}

/** 
 * Transmit sensor data
 */
void sendSensorData() {
  PL("Sending to Adafruit IO...");
  device_temperature->save(tempC);
#ifdef USE_DS18B20
  car_temperature->save(carTempC);
#endif
  accel_x->save(accelX);
  accel_y->save(accelY);
  accel_z->save(accelZ);

  for (int i = 0; i < 6; i++) {
    io.run();
    delay(250);
  }
  PL("Data sent!");
  debugLed(2);
}

/**
 * Handle shutdown/cleanup. This will disconnect wifi and potentially signal DONE 
 * to the TPL5110
 */
void shutdown() {
  PL("Disconnecting from Adafruit IO...");

  io.wifi_disconnect();
  delay(100);
  debugLed(2, 100);

#ifdef USE_TPL
  PL("Signaling TPL5110 DONE");
  debugLed(2, 100);
  while (1) {
    digitalWrite(TPL_DONE_PIN, HIGH);
    delay(250);
    digitalWrite(TPL_DONE_PIN, LOW);
    delay(250);
  }
#endif
}


/**
 * Attempt to reset the wifi hotspot. This is a bit of a hefty process as we 
 * have to fake logging into the web ui and do some magic to trigger a reset 
 * of the hotspot. Tracking cookies from raw HTTP requests is old school AF.
 */
bool resetHotspot() {
#ifdef USE_FSM
  // Use the fsm from RestartStateMachine to kick off the reset process...
  stateMachine.restart();
  fsm.transitionTo(indexState);
  return true;
#else
  return true;
#endif
}

/************************** Main loops ***********************************/
void setup() {
#ifdef USE_TPL
  pinMode(TPL_DONE_PIN, OUTPUT);
  digitalWrite(TPL_DONE_PIN, LOW);  // Keep low until we're done
#endif
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(115200);
  if (Serial) {
    // wait for serial monitor to open
    while (!Serial)
      ;
    PL("Serial connected");
  }

  setupSensors();
  // setup_WIFI();
#ifdef USE_FSM
  setupStateMachine();
  fsm.transitionTo(sleepState);
#endif
}

void loop() {
  debugLed(2);

#ifdef USE_FSM
  // If the FSM is restarting the hotspot, continue that loop
  if (fsm.currentState != sleepState->index) {
    debugLed(3, 500);
    HttpClient fsmClient = HttpClient(wifi, serverAddress, port);
    stateMachine.setClient(&client);
    fsm.run();
    delay(1000);
    return;
  }
#endif
// #ifdef USE_TPL
//   PL("DONE pin state: ");
//   PL(digitalRead(TPL_DONE_PIN));
// #endif

  if (!connectToAdafruitIO()) {
    debugLed(3, 500);
#ifndef USE_FSM
    // If we are not relying on the fsm, shutdown and return;
    shutdown();
#endif
    return;
  }

  io.run();

  collectSensorData();
  sendSensorData();
  debugLed(2);
  shutdown();
}

// void setup() {
//   Serial.begin(9600);
//   while (!Serial);

//   Serial.println("lol");
// }

// void loop() {
//   delay(1000);
//   Serial.println("Oh");
// }
