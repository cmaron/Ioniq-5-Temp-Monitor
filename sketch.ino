#include <WiFi.h>
#include <memory>

#include <ArduinoHttpClient.h>

#include "ArduinoJson.h"
#include "Hashtable.h"
#include "LinkedList.h"

#include "Restarter.h"
#include "RestartStateMachine.h"

char serverAddress[] = "host.wokwi.internal";
int port = 8080;

WiFiClient wifi;
// HttpClient client = HttpClient(wifi, serverAddress, port);
unsigned long started = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.print("Connecting to WiFi");
  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println(" Connected!");
  setupStateMachine();
  started = millis();
}

void loop() {
  HttpClient client = HttpClient(wifi, serverAddress, port);
  stateMachine.setClient(&client);
  fsm.run();

  Serial.println("Wait five seconds");
  delay(5000);
  unsigned long elapsed = millis() - started;
  Serial.println(elapsed);
  if (elapsed > 20000 && elapsed < 25000) {
    stateMachine.restart();
  }

  if (elapsed > 40000) {
    stateMachine.complete();
  }
}
