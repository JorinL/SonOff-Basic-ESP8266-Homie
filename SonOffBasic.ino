#include <Homie.h>
#include <ArduinoOTA.h>

const int PIN_RELAY = 12;
const int PIN_LED = 13;
const int PIN_BUTTON = 0;

#define FW_NAME "sonoff-basic"
#define FW_VERSION "1.0.0"

unsigned long WiFifix = 0;
unsigned long problemDetected = 0;
String problemCause;

unsigned long buttonDownTime = 0;
byte lastButtonState = 1;
byte buttonPressHandled = 0;

HomieNode relayNode("switch", "switch");

bool relayHandler(const HomieRange& range, const String& value) {
  String on = value;
  on.toLowerCase();

  if (on == "true") {
    digitalWrite(PIN_RELAY, HIGH);
  }
  else if (on == "false") {
    digitalWrite(PIN_RELAY, LOW);
  }
  else if (on == "reset") {
    relayNode.setProperty("on/set").send(" ");
    delay(1000);
    Homie.reset();
  }
  else if (on == "reboot") {
    relayNode.setProperty("on/set").send(" ");
    delay(1000);
    Homie.reboot();
  }
  else if (on == "ota-restart") {
    relayNode.setProperty("on/set").send(" ");
    ArduinoOTA.setHostname(Homie.getConfiguration().deviceId);
    ArduinoOTA.begin();
  }
  relayNode.setProperty("on").send(value);
}

void toggleRelay() {
  bool on = digitalRead(PIN_RELAY) == HIGH;
  digitalWrite(PIN_RELAY, on ? LOW : HIGH);
  relayNode.setProperty("on").send(on ? "false" : "true");
  Homie.getLogger() << "Switch is " << (on ? "off" : "on") << endl;
}

void Buttonhandle() {
  byte buttonState = digitalRead(PIN_BUTTON);
  if ( buttonState != lastButtonState ) {
    if (buttonState == LOW) {
      buttonDownTime = millis();
      buttonPressHandled = 0;
    }
    else {
      unsigned long dt = millis() - buttonDownTime;
      if ( dt >= 90 && dt <= 900 && buttonPressHandled == 0 ) {
        toggleRelay();
        buttonPressHandled = 1;
      }
    }
    lastButtonState = buttonState;
  }
}

void fixWiFi() {
  // Posts every 10 seconds the state of WiFi.status(), Homie.getMqttClient().connected() and Homie.isConfigured()
  // Within this interval the connectivity is checked and logged if a problem is detected
  // Then it disconnects Wifi, if Wifi or MQTT is not connected for 1 Minute (but only if Homie is configured)
  if (0 == WiFifix || ((millis() - WiFifix) > 10000)) {
    if (Homie.isConfigured() == 1) {
      Homie.getLogger() << "Wifi-state:" << WiFi.status() << " | MQTT-state:" << Homie.getMqttClient().connected() << " | HomieConfig-state:" << Homie.isConfigured() << endl;
      if (!Homie.getMqttClient().connected() || WiFi.status() != 3) {
        if (0 == problemDetected) {
          if (WiFi.status() != 3) {
            problemCause = "WiFi: Disconnected ";
          }
          if (!Homie.getMqttClient().connected()) {
            problemCause += "MQTT: Disconnected";
          }
          Homie.getLogger() << "Connectivity in problematic state --> " << problemCause << endl;
          problemDetected = millis();
        }
        else if ((millis() - problemDetected) > 60000) {
          Homie.getLogger() << "Connectivity in problematic state --> " << problemCause << "/n This remained for 60 seconds. Disconnecting WiFi to start over." << endl;
          problemDetected = 0;
          problemCause = "";
          WiFi.disconnect();
        }
      }
    }
  }
  WiFifix = millis();
}


void setup() {
  Serial.begin(115200);
  Serial << endl << endl;
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, LOW);
  Homie_setBrand(FW_NAME);
  Homie_setFirmware(FW_NAME, FW_VERSION);
  Homie.setLedPin(PIN_LED, LOW);
  Homie.setResetTrigger(PIN_BUTTON, LOW, 5000);
  relayNode.advertise("on").settable(relayHandler);
  Homie.setup();
  ArduinoOTA.setHostname(Homie.getConfiguration().deviceId);
  ArduinoOTA.begin();
}

void loop() {
  Homie.loop();
  ArduinoOTA.handle();
  Buttonhandle();
  fixWiFi();
}
