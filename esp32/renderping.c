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

// Prints requested by you (kept for Serial debug)
const char* OK_PRINT  = "damodardas mulchand ";
const char* BAD_PRINT = "Frederick christ";
// ===========================================================

const int LED_PIN = 2;
const unsigned long INTERVAL_MS = 7UL *60UL * 1000UL; // 1 minute

// Limit body to avoid running out of RAM if server returns big HTML
const size_t MAX_BODY_CHARS = 4096;

unsigned long lastRun = 0;
bool lastBtConnected = false;

// What Bluetooth will show (BODY)
String lastResult = "No response yet.";

// Optional: debug info (for Serial / or Bluetooth command "info")
String lastInfo = "No info yet.";

static void blinkOnce() {
  digitalWrite(LED_PIN, HIGH); delay(120);
  digitalWrite(LED_PIN, LOW);  delay(120);
}

static void sendLast() {
  if (SerialBT.connected()) {
    SerialBT.println("----- BODY START -----");
    SerialBT.println(lastResult);
    SerialBT.println("------ BODY END ------");
  }
}

static void sendInfo() {
  if (SerialBT.connected()) {
    SerialBT.println("----- INFO -----");
    SerialBT.println(lastInfo);
  }
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

// Returns BODY (for Bluetooth) and fills `infoOut` (for Serial / optional BT "info")
static String fetchBodyOnce(bool &success, String &infoOut) {
  success = false;
  infoOut = "";

  // 1) WiFi
  if (!ensureWiFiConnected()) {
    infoOut = String("WiFi FAIL | HTTP SKIP | ") + BAD_PRINT;
    return String(BAD_PRINT);
  }

  // 2) DNS
  IPAddress hostIp;
  if (!WiFi.hostByName(HOST, hostIp)) {
    infoOut = "WiFi OK | DNS FAIL | " + String(HOST) + " | " + BAD_PRINT;
    return String(BAD_PRINT);
  }

  String wifiPart = "WiFi OK | IP " + WiFi.localIP().toString() +
                    " | RSSI " + String(WiFi.RSSI()) +
                    " | HOST " + hostIp.toString();

  // 3) HTTPS
  WiFiClientSecure client;
  client.setInsecure();            // easy mode (no cert validation)
  client.setTimeout(60000);        // ms
  client.setHandshakeTimeout(60);  // seconds

  unsigned long t0 = millis();

  if (!client.connect(HOST, PORT)) {
    infoOut = wifiPart + " | CONNECT FAIL " + String(PORT) + " | " + BAD_PRINT;
    return String(BAD_PRINT);
  }

  // Ask server not to gzip (keeps body readable)
  client.print(String("GET ") + PATH + " HTTP/1.1\r\n");
  client.print(String("Host: ") + HOST + "\r\n");
  client.print("User-Agent: ESP32-Monitor/1.0\r\n");
  client.print("Accept-Encoding: identity\r\n");
  client.print("Connection: close\r\n\r\n");

  // Wait for first byte (Render cold start can be slow)
  unsigned long waitStart = millis();
  while (!client.available() && client.connected() && (millis() - waitStart) < 60000) {
    delay(10);
  }

  if (!client.available()) {
    client.stop();
    infoOut = wifiPart + " | HTTP FAIL | no response (timeout) | " + BAD_PRINT;
    return String(BAD_PRINT);
  }

  // Status line
  String statusLine = client.readStringUntil('\n');
  statusLine.trim();
  int code = parseHttpCode(statusLine);

  if (code >= 200 && code < 400) success = true;

  // Read headers (and capture Location for redirects)
  String location = "";
  while (client.connected()) {
    String h = client.readStringUntil('\n');
    if (h == "\r") break; // end headers
    h.trim();
    if (h.startsWith("Location: ")) location = h.substring(10);
  }

  // Read BODY (limited)
  String body = "";
  while (client.connected() || client.available()) {
    while (client.available()) {
      char c = (char)client.read();
      if (body.length() < MAX_BODY_CHARS) body += c;
    }
    delay(1);
  }

  client.stop();
  unsigned long t1 = millis();

  // Info summary (Serial)
  infoOut = wifiPart +
            " | HTTP " + String(code > 0 ? code : -1) +
            (location.length() ? (" | redirect " + location) : "") +
            " | " + String(t1 - t0) + " ms" +
            " | bodyLen " + String(body.length()) +
            " | " + (success ? OK_PRINT : BAD_PRINT);

  // If server returns empty body, at least return OK/BAD string
  if (body.length() == 0) return success ? String(OK_PRINT) : String(BAD_PRINT);

  return body;
}

static void runCheckAndReport() {
  bool ok = false;
  String info;
  String body = fetchBodyOnce(ok, info);

  lastResult = body;   // <-- Bluetooth will show BODY
  lastInfo = info;     // <-- Debug info stored

  // USB Serial: show requested phrases + debug
  Serial.println(ok ? OK_PRINT : BAD_PRINT);
  Serial.println(lastInfo);

  // Bluetooth: show BODY
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

  runCheckAndReport();
  lastRun = millis();
}

void loop() {
  // Send last body once when phone connects
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
      sendLast();            // prints BODY
    } else if (cmd == "check") {
      runCheckAndReport();   // refresh + prints BODY
    } else if (cmd == "info") {
      sendInfo();            // prints debug summary (no body)
    } else {
      SerialBT.println("Commands: last, check, info");
    }
  }

  delay(20);
}
