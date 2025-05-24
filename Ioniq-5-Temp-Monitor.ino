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
#define USE_DS18B20 1
// If defined, use the TPL timer to shut off the arduino as needed
#define USE_TPL 1 
// If defined, use the FSM to handle restarting the wifi router as needed
#undef USE_FSM 0
// If defined, assume a serial port has been configured and we should log
#define USE_SERIAL 1

// Pins
#define DS18B20_PIN 6
#ifdef USE_TPL
#define TPL_DONE_PIN 5
#endif

#ifdef USE_SERIAL
#define P Serial.print
#define PL Serial.println
#else
#define P (void)0
#define PL (void)0
#endif

#define USE_WINC1500
#include "AdafruitIO_WiFi.h"
// Configure pins for Adafruit ATWINC1500 Feather

AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS, 8,7,4,2);

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
char serverAddress[] = "host.wokwi.internal";
int port = 8080;
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

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);

#ifdef USE_SERIAL  // start the serial connection
  Serial.begin(9600);

  // wait for serial monitor to open
  while (!Serial)
    ;
#endif
#ifdef USE_TPL
  pinMode(TPL_DONE_PIN, OUTPUT);
  digitalWrite(TPL_DONE_PIN, LOW);  // Keep low until we're done
#endif

  PL("Adafruit IO - ADT7410 + ADX343");

  /* Initialise the ADXL343 */
  if(!accel.begin())
  {
    /* There was a problem detecting the ADXL343 ... check your connections */
    PL("Ooops, no ADXL343 detected ... Check your wiring!");
    while(1);
  }

  /* Set the range to whatever is appropriate for your project */
  accel.setRange(ADXL343_RANGE_2_G);

  /* Initialise the ADT7410 */
  if (!tempsensor.begin())
  {
    PL("Couldn't find ADT7410!");
    while (1)
      ;
  }

#ifdef USE_DS18B20
  ds18b20.begin();
#endif
  // sensors take 250 ms to get first readings
  delay(250);
 
  setup_WIFI();

#ifdef USE_FSM
  setupStateMachine();
#endif
}

void loop()
{
  digitalWrite(LED_BUILTIN, HIGH); // Show we're awake

  setup_WIFI();
#ifdef USE_FSM
  HttpClient fsmClient = HttpClient(wifi, serverAddress, port);
  stateMachine.setClient(&client);
  fsm.transitionTo(sleepState);
  fsm.run();
#endif

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
  
  // Read and print out the temperatures
  tempC = tempsensor.readTempC();
  P("Temperature: "); P(tempC); PL("C");

#ifdef USE_DS18B20
  ds18b20.requestTemperatures();
  carTempC = ds18b20.getTempCByIndex(0);
  P("Car emperature: "); P(carTempC); PL("C");
#endif

  PL("Sending to Adafruit IO...");
  device_temperature->save(tempC, 0, 0, 0, 2);
#ifdef USE_DS18B20
  car_temperature->save(carTempC, 0, 0, 0, 2);
#endif
  accel_x->save(accelX);
  accel_y->save(accelY);
  accel_z->save(accelZ);

  PL("Data sent!");

#ifdef USE_TPL
  // === Signal Done to TPL5110 ===
  PL("Signaling TPL5110");
  digitalWrite(TPL_DONE_PIN, HIGH);  // tell timer we’re done
  delay(100);                        // let it shut down
#endif
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
            delay(1000);
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
 */
bool reset_Hotspot() {
#ifdef USE_FSM
  // Use the fsm from RestartStateMachine to kick off the reset process...
  stateMachine.restart();
  fsm.transitionTo(indexState);
  return true;
#else
  return true;
#endif
}

