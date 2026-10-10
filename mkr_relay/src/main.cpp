// MKR WiFi 1010 + MKR Relay Proto Shield: control RELAY1 over USB serial.
//
// Protocol (one command per line, '\n' terminated, '\r' ignored, case-insensitive):
//   ON      -> "OK ON"       relay energised, built-in LED (D6) on
//   OFF     -> "OK OFF"      relay released, built-in LED (D6) off
//   STATUS  -> "STATE ON" / "STATE OFF"
//   other   -> "ERR UNKNOWN <cmd>"
//   too long-> "ERR TOO_LONG"
// The relay is always OFF after power-up/reset, then keeps the last commanded state.

#include <Arduino.h>
#include <ctype.h>
#include <string.h>

static const uint8_t RELAY_PIN = 1;  // MKR Relay Proto Shield RELAY1 (RELAY2 is pin 2, unused)
static const size_t MAX_LINE = 32;

static char lineBuf[MAX_LINE + 1];
static size_t lineLen = 0;
static bool lineOverflow = false;
static bool relayOn = false;

static void setRelay(bool on) {
  relayOn = on;
  digitalWrite(RELAY_PIN, on ? HIGH : LOW);
  digitalWrite(LED_BUILTIN, on ? HIGH : LOW);
}

static void handleLine(char *s) {
  // Trim surrounding whitespace and uppercase in place.
  while (isspace((unsigned char)*s)) s++;
  char *end = s + strlen(s);
  while (end > s && isspace((unsigned char)end[-1])) *--end = '\0';
  for (char *p = s; *p; p++) *p = toupper((unsigned char)*p);

  if (*s == '\0') return;  // ignore blank lines

  if (strcmp(s, "ON") == 0) {
    setRelay(true);
    Serial.println("OK ON");
  } else if (strcmp(s, "OFF") == 0) {
    setRelay(false);
    Serial.println("OK OFF");
  } else if (strcmp(s, "STATUS") == 0) {
    Serial.println(relayOn ? "STATE ON" : "STATE OFF");
  } else {
    Serial.print("ERR UNKNOWN ");
    Serial.println(s);
  }
}

void setup() {
  // Drive LOW before switching to output so the relay never glitches on at boot.
  digitalWrite(RELAY_PIN, LOW);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  pinMode(LED_BUILTIN, OUTPUT);
  setRelay(false);

  Serial.begin(115200);  // native USB CDC: baud rate is ignored
  // No while(!Serial): the relay must work without a host attached.
  Serial.println("READY");
}

void loop() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (lineOverflow) {
        Serial.println("ERR TOO_LONG");
      } else {
        lineBuf[lineLen] = '\0';
        handleLine(lineBuf);
      }
      lineLen = 0;
      lineOverflow = false;
    } else if (lineLen < MAX_LINE) {
      lineBuf[lineLen++] = c;
    } else {
      lineOverflow = true;
    }
  }
}
