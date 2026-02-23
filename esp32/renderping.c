#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

// ================== CONFIG (CHANGE THESE) ==================
const char* WIFI_SSID = "JatinSharma4G";
const char* WIFI_PASS = "123456654321";

const char* HOST = "vertex-vault.onrender.com"; // hostname only (no https://, no port)
const uint16_t PORT = 443;

// If your backend has /health, set it to "/health"
const char* PATH = "/";

// Bluetooth device name
const char* BT_NAME = "ESP32_MONITOR";

// Prints requested by you:
const char* OK_PRINT  = "damodardas mulchand ";
const char* BAD_PRINT = "Frederick christ";
// ===========================================================

const int LED_PIN = 2;
const unsigned long INTERVAL_MS = 60UL * 1000UL; // 1 minute

unsigned long lastRun = 0;
bool lastBtConnected = false;
String lastResult = "No check yet.";

static void blinkOnce() {
  digitalWrite(LED_PIN, HIGH); delay(120);
  digitalWrite(LED_PIN, LOW);  delay(120);
}

static void sendLast() {
  if (SerialBT.connected()) SerialBT.println(lastResult);
}

static bool ensureWiFiConnected(unsigned long timeoutMs = 60000) {
  if (WiFi.status() == WL_CONNECTED) return true;

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);

  Serial.print("WiFi: connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi: connected | IP ");
    Serial.print(WiFi.localIP());
    Serial.print(" | RSSI ");
    Serial.println(WiFi.RSSI());
    return true;
  }

  Serial.println("WiFi: FAILED (timeout)");
  return false;
}

static int parseHttpCode(const String& statusLine) {
  // Example: "HTTP/1.1 200 OK"
  int sp1 = statusLine.indexOf(' ');
  if (sp1 < 0) return -1;
  int sp2 = statusLine.indexOf(' ', sp1 + 1);
  if (sp2 < 0) sp2 = statusLine.length();

  String codeStr = statusLine.substring(sp1 + 1, sp2);
  codeStr.trim();
  if (codeStr.length() < 3) return -1;
  return codeStr.toInt();
}

// Performs one HTTPS GET, returns a compact status line and sets success=true/false
static String checkServiceOnce(bool &success) {
  success = false;

  // 1) WiFi
  if (!ensureWiFiConnected()) {
    return String("WiFi FAIL | not connected | HTTP SKIP | ") + BAD_PRINT;
  }

  String wifiPart = "WiFi OK | IP " + WiFi.localIP().toString() + " | RSSI " + String(WiFi.RSSI());

  // 2) DNS resolve (helps debug “can’t reach host”)
  IPAddress hostIp;
  if (!WiFi.hostByName(HOST, hostIp)) {
    return wifiPart + " | DNS FAIL | " + HOST + " | " + BAD_PRINT;
  }
  wifiPart += " | HOST " + hostIp.toString();

  // 3) HTTPS request
  WiFiClientSecure client;
  client.setInsecure();               // NOTE: disables cert validation (easy, less secure)
  client.setTimeout(60000);           // read timeout (ms)
  client.setHandshakeTimeout(60);     // TLS handshake timeout (seconds)

  unsigned long t0 = millis();

  if (!client.connect(HOST, PORT)) {
    return wifiPart + " | HTTP FAIL | connect " + String(PORT) + " failed | " + BAD_PRINT;
  }

  client.print(String("GET ") + PATH + " HTTP/1.1\r\n");
  client.print(String("Host: ") + HOST + "\r\n");
  client.print("User-Agent: ESP32-Monitor/1.0\r\n");
  client.print("Connection: close\r\n\r\n");

  // Render/cold-start can be slow → wait up to 60s for first byte
  unsigned long waitStart = millis();
  while (!client.available() && client.connected() && (millis() - waitStart) < 60000) {
    delay(10);
  }

  if (!client.available()) {
    client.stop();
    return wifiPart + " | HTTP FAIL | no response (timeout) | " + BAD_PRINT;
  }

  String statusLine = client.readStringUntil('\n');
  statusLine.trim();

  // Optional: read headers to catch redirects
  String location = "";
  while (client.connected()) {
    String h = client.readStringUntil('\n');
    h.trim();
    if (h.length() == 0) break; // end of headers
    if (h.startsWith("Location: ")) location = h.substring(10);
  }

  client.stop();

  unsigned long t1 = millis();
  int code = parseHttpCode(statusLine);

  // Define “connection successful” as: got an HTTP status code 2xx or 3xx
  if (code >= 200 && code < 400) success = true;

  String result = wifiPart;

  if (code > 0) {
    result += " | HTTP " + String(code);
    if (location.length()) result += " | redirect " + location;
    result += " | " + String(t1 - t0) + " ms";
  } else {
    result += " | HTTP FAIL | bad status line | " + String(t1 - t0) + " ms";
  }

  result += " | ";
  result += (success ? OK_PRINT : BAD_PRINT);
  return result;
}

static void runCheckAndReport() {
  bool ok = false;
  lastResult = checkServiceOnce(ok);

  // Print the requested phrases based on success/fail
  if (ok) Serial.println(OK_PRINT);
  else    Serial.println(BAD_PRINT);

  // Also print the full status line
  Serial.println(lastResult);

  // Send over Bluetooth (if connected)
  sendLast();

  blinkOnce();
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);
  delay(300);
  Serial.println("\nBOOT: ESP32 starting...");

  SerialBT.begin(BT_NAME);
  Serial.print("BT: started as ");
  Serial.println(BT_NAME);

  // First check immediately
  runCheckAndReport();
  lastRun = millis();
}

void loop() {
  // Send last result once when phone connects
  bool btNow = SerialBT.connected();
  if (btNow && !lastBtConnected) sendLast();
  lastBtConnected = btNow;

  // Periodic check
  unsigned long now = millis();
  if (now - lastRun >= INTERVAL_MS) {
    lastRun = now;
    runCheckAndReport();
  }

  // Bluetooth commands
  if (SerialBT.available()) {
    String cmd = SerialBT.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();

    if (cmd == "last") {
      sendLast();
    } else if (cmd == "check") {
      runCheckAndReport();
    } else if (cmd == "help") {
      SerialBT.println("Commands: last, check, help");
    } else {
      SerialBT.println("Commands: last, check, help");
    }
  }

  delay(20);
}
