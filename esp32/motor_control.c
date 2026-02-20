#include <Arduino.h>
#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

// ===== Motor driver pins (CHANGE THESE to your wiring) =====
// Direction pins:
const int IN1_PIN = 26;
const int IN2_PIN = 27;

// PWM (enable) pin:
const int PWM_PIN = 25;

// Optional: Standby pin (TB6612 has STBY). If not used, set to -1.
const int STBY_PIN = -1;
// ===========================================================

// PWM settings (ESP32 LEDC)
const int PWM_CH = 0;
const int PWM_FREQ = 20000;   // 20 kHz (quiet)
const int PWM_RES  = 8;       // 8-bit -> 0..255

int currentSpeed = 160;       // default speed (0..255)

// ---------- Motor helpers ----------
void motorStopCoast() {
  // Coast: both LOW, PWM 0
  digitalWrite(IN1_PIN, LOW);
  digitalWrite(IN2_PIN, LOW);
  ledcWrite(PWM_CH, 0);
}

void motorBrake() {
  // Brake behavior depends on driver.
  // For many H-bridges: IN1=HIGH, IN2=HIGH with PWM 0 can brake.
  // If your driver behaves differently, adjust this.
  digitalWrite(IN1_PIN, HIGH);
  digitalWrite(IN2_PIN, HIGH);
  ledcWrite(PWM_CH, 0);
}

void motorForward(int pwm) {
  pwm = constrain(pwm, 0, 255);
  digitalWrite(IN1_PIN, HIGH);
  digitalWrite(IN2_PIN, LOW);
  ledcWrite(PWM_CH, pwm);
}

void motorBackward(int pwm) {
  pwm = constrain(pwm, 0, 255);
  digitalWrite(IN1_PIN, LOW);
  digitalWrite(IN2_PIN, HIGH);
  ledcWrite(PWM_CH, pwm);
}

void sendHelp(Stream &out) {
  out.println("ESP32 Motor BT Commands:");
  out.println("  f [0-255]     -> forward at pwm (default uses last speed)");
  out.println("  b [0-255]     -> backward at pwm");
  out.println("  s             -> stop (coast)");
  out.println("  brake         -> brake");
  out.println("  speed [0-255] -> set default speed");
  out.println("  status        -> show speed");
  out.println("  help          -> show this help");
}

int parseValueAfterSpace(const String &cmd, bool &hasVal) {
  int sp = cmd.indexOf(' ');
  if (sp < 0) { hasVal = false; return 0; }
  String tail = cmd.substring(sp + 1);
  tail.trim();
  if (tail.length() == 0) { hasVal = false; return 0; }
  hasVal = true;
  return tail.toInt(); // if invalid, becomes 0
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nBOOT: ESP32 BT Motor Control");

  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);

  if (STBY_PIN >= 0) {
    pinMode(STBY_PIN, OUTPUT);
    digitalWrite(STBY_PIN, HIGH); // enable driver
  }

  // Setup PWM on ESP32
  ledcSetup(PWM_CH, PWM_FREQ, PWM_RES);
  ledcAttachPin(PWM_PIN, PWM_CH);
  ledcWrite(PWM_CH, 0);

  motorStopCoast();

  // Start Bluetooth
  SerialBT.begin("ESP32_MOTOR");
  Serial.println("BT started: ESP32_MOTOR");
  sendHelp(Serial);
}

void loop() {
  // Read commands from Bluetooth (one line per command)
  if (SerialBT.available()) {
    String cmd = SerialBT.readStringUntil('\n');
    cmd.trim();
    String lower = cmd;
    lower.toLowerCase();

    // Commands
    if (lower == "help") {
      sendHelp(SerialBT);

    } else if (lower == "s" || lower == "stop") {
      motorStopCoast();
      SerialBT.println("OK: stop (coast)");

    } else if (lower == "brake") {
      motorBrake();
      SerialBT.println("OK: brake");

    } else if (lower.startsWith("speed")) {
      bool hasVal = false;
      int v = parseValueAfterSpace(lower, hasVal);
      if (!hasVal) {
        SerialBT.println("ERR: speed needs value 0..255");
      } else {
        currentSpeed = constrain(v, 0, 255);
        SerialBT.print("OK: speed set to ");
        SerialBT.println(currentSpeed);
      }

    } else if (lower.startsWith("f")) {
      bool hasVal = false;
      int v = parseValueAfterSpace(lower, hasVal);
      int pwm = hasVal ? v : currentSpeed;
      pwm = constrain(pwm, 0, 255);
      motorForward(pwm);
      SerialBT.print("OK: forward ");
      SerialBT.println(pwm);

    } else if (lower.startsWith("b")) {
      bool hasVal = false;
      int v = parseValueAfterSpace(lower, hasVal);
      int pwm = hasVal ? v : currentSpeed;
      pwm = constrain(pwm, 0, 255);
      motorBackward(pwm);
      SerialBT.print("OK: backward ");
      SerialBT.println(pwm);

    } else if (lower == "status") {
      SerialBT.print("Speed=");
      SerialBT.println(currentSpeed);

    } else {
      SerialBT.println("Unknown cmd. Type 'help'.");
    }

    // Also print on USB serial for debugging
    Serial.print("BT cmd: ");
    Serial.println(cmd);
  }

  delay(10);
}
