
#include <WiFi.h>
#include <WiFiClient.h>
#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

// ====== CHANGE THESE ======
const char* WIFI_SSID = "JatinSharma4G";
const char* WIFI_PASS = "123456654321";

// HOST must be ONLY IP/hostname (NO port)
const char* HOST = "192.168.29.220";
const uint16_t PORT = 5000;

const char* PATH = "/health";   // change to "/" if you don't have /health
// ==========================

const int LED_PIN = 2;
const unsigned long INTERVAL_MS = 60UL * 1000UL; // 1 minute

unsigned long lastRun = 0;
bool lastBtConnected = false;
String lastResult = "No check yet.";

void sendLast() {
  if (SerialBT.connected()) SerialBT.println(lastResult);
}

bool ensureWiFiConnected(unsigned long timeoutMs = 15000) {
  if (WiFi.status() == WL_CONNECTED) return true;

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);

  Serial.println("WiFi: connecting...");
  unsigned long start = millis();
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi: connected");
    return true;
  } else {
    Serial.println("WiFi: FAILED (timeout)");
    return false;
  }
}

// HTTP GET and return compact status line
String httpInvokeOnce() {
  // 1) WiFi status first
  bool wifiOk = ensureWiFiConnected();
  String wifiPart;

  if (wifiOk) {
    wifiPart = "WiFi OK | IP " + WiFi.localIP().toString() + " | RSSI " + String(WiFi.RSSI());
  } else {
    wifiPart = "WiFi FAIL | not connected";
    return wifiPart + " | HTTP SKIP";
  }

  // 2) HTTP request
  WiFiClient client;
  client.setTimeout(15000); // server should respond quickly

  unsigned long t0 = millis();

  if (!client.connect(HOST, PORT)) {
    return wifiPart + " | HTTP FAIL | connect " + String(PORT) + " failed";
  }

  // Send HTTP request
  client.print(String("GET ") + PATH + " HTTP/1.1\r\n");
  client.print(String("Host: ") + HOST + "\r\n");
  client.print("User-Agent: ESP32-Monitor/1.0\r\n");
  client.print("Connection: close\r\n\r\n");

  // Wait for response
  unsigned long waitStart = millis();
  while (!client.available() && (millis() - waitStart) < 15000) {
    delay(5);
  }

  if (!client.available()) {
    client.stop();
    return wifiPart + " | HTTP FAIL | no response (timeout)";
  }

  // Read status line: "HTTP/1.1 200 OK"
  String statusLine = client.readStringUntil('\n');
  statusLine.trim();

  // Drain and close
  while (client.available()) client.read();
  client.stop();

  unsigned long t1 = millis();

  if (statusLine.startsWith("HTTP/")) {
    int sp1 = statusLine.indexOf(' ');
    int sp2 = statusLine.indexOf(' ', sp1 + 1);
    String code = (sp1 > 0 && sp2 > sp1) ? statusLine.substring(sp1 + 1, sp2) : "?";
    return wifiPart + " | HTTP " + code + " | " + String(t1 - t0) + " ms";
  }

  return wifiPart + " | HTTP FAIL | bad status line | " + String(t1 - t0) + " ms";
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);
  delay(500);
  Serial.println("\nBOOT: ESP32 starting...");

  SerialBT.begin("ESP32_MONITOR");
  Serial.println("BT: ESP32_MONITOR started");

  // First check immediately
  lastResult = httpInvokeOnce();
  Serial.println(lastResult);
  sendLast();

  lastRun = millis();
}

void loop() {
  // Send last result once when phone connects
  bool btNow = SerialBT.connected();
  if (btNow && !lastBtConnected) sendLast();
  lastBtConnected = btNow;

  // Every 1 minute: update last result
  unsigned long now = millis();
  if (now - lastRun >= INTERVAL_MS) {
    lastRun = now;

    lastResult = httpInvokeOnce();
    Serial.println(lastResult);
    sendLast();

    // Blink to show it ran
    digitalWrite(LED_PIN, HIGH); delay(120);
    digitalWrite(LED_PIN, LOW);  delay(120);
  }

  // Bluetooth commands
  if (SerialBT.available()) {
    String cmd = SerialBT.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();

    if (cmd == "last") {
      sendLast();
    } else if (cmd == "check") {
      lastResult = httpInvokeOnce();
      Serial.println(lastResult);
      sendLast();
    } else {
      SerialBT.println("Commands: last, check");
    }
  }

  delay(20);
}
