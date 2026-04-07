#include <Wire.h>
#include <math.h>
#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled in this ESP32 core build
#endif

#if !defined(CONFIG_BT_SPP_ENABLED)
#error Bluetooth Serial (SPP) is not available on this target. Use a classic ESP32 board.
#endif

BluetoothSerial SerialBT;

// --------------------------------------------------
// User config
// --------------------------------------------------
const char* BT_DEVICE_NAME = "ESP32-IMU-Flight";
const bool ENABLE_USB_SERIAL = true;
const bool ENABLE_BT_SERIAL  = true;

// I2C pins for generic ESP32
const int SDA_PIN = 21;
const int SCL_PIN = 22;

// MPU6050 I2C address
uint8_t IMU_ADDR = 0x68;

// Registers
const uint8_t REG_SMPLRT_DIV   = 0x19;
const uint8_t REG_CONFIG       = 0x1A;
const uint8_t REG_GYRO_CONFIG  = 0x1B;
const uint8_t REG_ACCEL_CONFIG = 0x1C;
const uint8_t REG_ACCEL_XOUT_H = 0x3B;
const uint8_t REG_PWR_MGMT_1   = 0x6B;
const uint8_t REG_WHO_AM_I     = 0x75;

// Raw sensor data
int16_t axRaw, ayRaw, azRaw;
int16_t gxRaw, gyRaw, gzRaw;
int16_t tempRaw;

// Offsets
float axOffset = 0.0f;
float ayOffset = 0.0f;
float azOffset = 0.0f;
float gxOffset = 0.0f;
float gyOffset = 0.0f;
float gzOffset = 0.0f;

// Filtered / fused angles
float fusedRoll = 0.0f;
float fusedPitch = 0.0f;

// Output-smoothed centered angles
float smoothRoll = 0.0f;
float smoothPitch = 0.0f;

// Raw fused angles before neutral-centering
float rawRollDeg = 0.0f;
float rawPitchDeg = 0.0f;

// Neutral center pose
float neutralRoll = 0.0f;
float neutralPitch = 0.0f;

// Timing
unsigned long lastMicros = 0;
unsigned long lastPrintMs = 0;
unsigned long lastStatusMs = 0;

// IMU type
uint8_t detectedWhoAmI = 0x00;

// State
bool imuReady = false;
bool filterInitialized = false;

// Tunables
const float ACCEL_SCALE = 16384.0f;      // +/-2g
const float GYRO_SCALE = 131.0f;         // +/-250 dps
const float COMPLEMENTARY_ALPHA = 0.98f;
const float OUTPUT_SMOOTH_ALPHA = 0.18f;
const float DEAD_ZONE_DEG = 0.5f;
const unsigned long PRINT_INTERVAL_MS = 20;
const unsigned long STATUS_INTERVAL_MS = 1500;

// If controls feel reversed, change one or both signs below
const float PITCH_SIGN = 1.0f;
const float ROLL_SIGN  = 1.0f;

// --------------------------------------------------
// Output helpers
// --------------------------------------------------
void outPrint(const String& s) {
  if (ENABLE_USB_SERIAL) Serial.print(s);
  if (ENABLE_BT_SERIAL && SerialBT.hasClient()) SerialBT.print(s);
}

void outPrintln(const String& s) {
  if (ENABLE_USB_SERIAL) Serial.println(s);
  if (ENABLE_BT_SERIAL && SerialBT.hasClient()) SerialBT.println(s);
}

void outPrintf(const char* fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  outPrint(String(buf));
}

void localPrintln(const String& s) {
  if (ENABLE_USB_SERIAL) Serial.println(s);
}

// --------------------------------------------------
// I2C helpers
// --------------------------------------------------
bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return (Wire.endTransmission() == 0);
}

uint8_t readRegister(uint8_t reg) {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return 0xFF;
  }

  uint8_t count = Wire.requestFrom(IMU_ADDR, (uint8_t)1, (uint8_t)true);
  if (count != 1) {
    return 0xFF;
  }
  return Wire.read();
}

bool readBytes(uint8_t reg, uint8_t count, uint8_t* dest) {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  uint8_t received = Wire.requestFrom(IMU_ADDR, count, (uint8_t)true);
  if (received != count) {
    return false;
  }

  for (uint8_t i = 0; i < count; i++) {
    dest[i] = Wire.read();
  }
  return true;
}

// --------------------------------------------------
// IMU read / setup
// --------------------------------------------------
bool readRawIMU() {
  uint8_t buf[14];
  if (!readBytes(REG_ACCEL_XOUT_H, 14, buf)) {
    return false;
  }

  axRaw   = (int16_t)((buf[0]  << 8) | buf[1]);
  ayRaw   = (int16_t)((buf[2]  << 8) | buf[3]);
  azRaw   = (int16_t)((buf[4]  << 8) | buf[5]);
  tempRaw = (int16_t)((buf[6]  << 8) | buf[7]);
  gxRaw   = (int16_t)((buf[8]  << 8) | buf[9]);
  gyRaw   = (int16_t)((buf[10] << 8) | buf[11]);
  gzRaw   = (int16_t)((buf[12] << 8) | buf[13]);
  return true;
}

bool setupIMU() {
  detectedWhoAmI = readRegister(REG_WHO_AM_I);

  localPrintln("");
  localPrintln("Checking MPU6050...");
  if (ENABLE_USB_SERIAL) {
    Serial.print("WHO_AM_I = 0x");
    Serial.println(detectedWhoAmI, HEX);
  }

  if (detectedWhoAmI != 0x68) {
    localPrintln("Unexpected WHO_AM_I, trying anyway...");
  }

  // Wake/reset
  if (!writeRegister(REG_PWR_MGMT_1, 0x80)) return false;
  delay(100);
  if (!writeRegister(REG_PWR_MGMT_1, 0x01)) return false;
  delay(50);

  // LPF and sample settings
  if (!writeRegister(REG_CONFIG, 0x03))       return false; // DLPF
  if (!writeRegister(REG_SMPLRT_DIV, 0x04))   return false; // sample divider
  if (!writeRegister(REG_GYRO_CONFIG, 0x00))  return false; // +/-250 dps
  if (!writeRegister(REG_ACCEL_CONFIG, 0x00)) return false; // +/-2g

  delay(50);
  return true;
}

// --------------------------------------------------
// Calibration
// --------------------------------------------------
void calibrateFlatStill(int samples = 600) {
  long axSum = 0;
  long aySum = 0;
  long azSum = 0;
  long gxSum = 0;
  long gySum = 0;
  long gzSum = 0;
  int good = 0;

  outPrintln("");
  outPrintln("STEP 1: Put sensor FLAT and STILL.");
  outPrintln("Calibration starts in 3 seconds...");
  delay(3000);

  for (int i = 0; i < samples; i++) {
    if (readRawIMU()) {
      axSum += axRaw;
      aySum += ayRaw;
      azSum += azRaw;
      gxSum += gxRaw;
      gySum += gyRaw;
      gzSum += gzRaw;
      good++;
    }
    delay(3);
  }

  if (good == 0) {
    outPrintln("Calibration failed: no samples read.");
    return;
  }

  float axAvg = (float)axSum / good;
  float ayAvg = (float)aySum / good;
  float azAvg = (float)azSum / good;
  float gxAvg = (float)gxSum / good;
  float gyAvg = (float)gySum / good;
  float gzAvg = (float)gzSum / good;

  float expectedGravity = (azAvg >= 0.0f) ? ACCEL_SCALE : -ACCEL_SCALE;

  axOffset = axAvg;
  ayOffset = ayAvg;
  azOffset = azAvg - expectedGravity;
  gxOffset = gxAvg;
  gyOffset = gyAvg;
  gzOffset = gzAvg;

  outPrintln("Flat calibration done.");
  outPrintf("axOffset = %.3f\n", axOffset);
  outPrintf("ayOffset = %.3f\n", ayOffset);
  outPrintf("azOffset = %.3f\n", azOffset);
  outPrintf("gxOffset = %.3f\n", gxOffset);
  outPrintf("gyOffset = %.3f\n", gyOffset);
  outPrintf("gzOffset = %.3f\n", gzOffset);
}

void updateFilterOnce() {
  if (!readRawIMU()) {
    return;
  }

  float ax = ((float)axRaw - axOffset) / ACCEL_SCALE;
  float ay = ((float)ayRaw - ayOffset) / ACCEL_SCALE;
  float az = ((float)azRaw - azOffset) / ACCEL_SCALE;
  float gx = ((float)gxRaw - gxOffset) / GYRO_SCALE;
  float gy = ((float)gyRaw - gyOffset) / GYRO_SCALE;

  float accelRoll  = atan2f(ay, az) * 180.0f / PI;
  float accelPitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / PI;

  unsigned long nowMicros = micros();
  float dt = 0.0f;
  if (lastMicros == 0) {
    dt = 0.01f;
  } else {
    dt = (nowMicros - lastMicros) / 1000000.0f;
  }
  lastMicros = nowMicros;

  if (!filterInitialized) {
    fusedRoll = accelRoll;
    fusedPitch = accelPitch;
    filterInitialized = true;
  } else {
    fusedRoll  = COMPLEMENTARY_ALPHA * (fusedRoll + gx * dt)  + (1.0f - COMPLEMENTARY_ALPHA) * accelRoll;
    fusedPitch = COMPLEMENTARY_ALPHA * (fusedPitch + gy * dt) + (1.0f - COMPLEMENTARY_ALPHA) * accelPitch;
  }

  rawRollDeg = fusedRoll;
  rawPitchDeg = fusedPitch;
}

void letFilterSettle(unsigned long durationMs = 1500) {
  unsigned long start = millis();
  while (millis() - start < durationMs) {
    updateFilterOnce();
    delay(5);
  }
}

void captureNeutralPose(int samples = 250) {
  float rollSum = 0.0f;
  float pitchSum = 0.0f;
  int good = 0;

  outPrintln("");
  outPrintln("STEP 2: Hold sensor in your NORMAL hand position.");
  outPrintln("Capturing neutral pose in 3 seconds...");
  delay(3000);

  letFilterSettle(1000);

  for (int i = 0; i < samples; i++) {
    updateFilterOnce();
    rollSum += rawRollDeg;
    pitchSum += rawPitchDeg;
    good++;
    delay(6);
  }

  if (good == 0) {
    outPrintln("Neutral capture failed.");
    return;
  }

  neutralRoll = rollSum / good;
  neutralPitch = pitchSum / good;

  smoothRoll = 0.0f;
  smoothPitch = 0.0f;

  outPrintln("Neutral pose captured.");
  outPrintf("neutralRoll = %.2f\n", neutralRoll);
  outPrintf("neutralPitch = %.2f\n", neutralPitch);
  outPrintln("Send 'c' to recenter.");
  outPrintln("Send 'r' to recalibrate.");
}

// --------------------------------------------------
// Output JSON
// --------------------------------------------------
float applyDeadZone(float value, float deadZone) {
  if (fabs(value) < deadZone) {
    return 0.0f;
  }
  return value;
}

void printJSON(float pitchOut, float rollOut, float gxDegPerSec, float gyDegPerSec, float gzDegPerSec) {
  char line[220];
  snprintf(
    line,
    sizeof(line),
    "{\"pitch\":%.2f,\"roll\":%.2f,\"rawPitch\":%.2f,\"rawRoll\":%.2f,\"gx\":%.2f,\"gy\":%.2f,\"gz\":%.2f}",
    pitchOut,
    rollOut,
    rawPitchDeg,
    rawRollDeg,
    gxDegPerSec,
    gyDegPerSec,
    gzDegPerSec
  );

  if (ENABLE_USB_SERIAL) Serial.println(line);
  if (ENABLE_BT_SERIAL && SerialBT.hasClient()) SerialBT.println(line);
}

// --------------------------------------------------
// Commands from USB serial and Bluetooth serial
// --------------------------------------------------
void processCommandChar(char c) {
  if (c == 'c' || c == 'C') {
    captureNeutralPose();
  } else if (c == 'r' || c == 'R') {
    calibrateFlatStill();
    filterInitialized = false;
    lastMicros = 0;
    letFilterSettle(1200);
    captureNeutralPose();
  }
}

void handleCommands() {
  while (ENABLE_USB_SERIAL && Serial.available()) {
    char c = (char)Serial.read();
    processCommandChar(c);
  }

  while (ENABLE_BT_SERIAL && SerialBT.available()) {
    char c = (char)SerialBT.read();
    processCommandChar(c);
  }
}

// --------------------------------------------------
// Setup / loop
// --------------------------------------------------
void setup() {
  if (ENABLE_USB_SERIAL) {
    Serial.begin(115200);
    delay(300);
  }

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  if (ENABLE_BT_SERIAL) {
    SerialBT.begin(BT_DEVICE_NAME);
  }

  localPrintln("Starting ESP32 MPU6050 Bluetooth flight controller...");
  localPrintln("USB Serial: 115200");
  if (ENABLE_USB_SERIAL) {
    Serial.printf("Bluetooth name: %s\n", BT_DEVICE_NAME);
  }

  if (!setupIMU()) {
    localPrintln("IMU setup failed.");
    imuReady = false;
    return;
  }

  localPrintln("IMU found at address 0x68");
  imuReady = true;

  calibrateFlatStill();
  filterInitialized = false;
  lastMicros = 0;
  letFilterSettle(1500);
  captureNeutralPose();

  lastPrintMs = millis();
  lastStatusMs = millis();
}

void loop() {
  handleCommands();

  if (!imuReady) {
    delay(100);
    return;
  }

  if (!readRawIMU()) {
    delay(5);
    return;
  }

  float ax = ((float)axRaw - axOffset) / ACCEL_SCALE;
  float ay = ((float)ayRaw - ayOffset) / ACCEL_SCALE;
  float az = ((float)azRaw - azOffset) / ACCEL_SCALE;
  float gx = ((float)gxRaw - gxOffset) / GYRO_SCALE;
  float gy = ((float)gyRaw - gyOffset) / GYRO_SCALE;
  float gz = ((float)gzRaw - gzOffset) / GYRO_SCALE;

  float accelRoll  = atan2f(ay, az) * 180.0f / PI;
  float accelPitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / PI;

  unsigned long nowMicros = micros();
  float dt = 0.01f;
  if (lastMicros != 0) {
    dt = (nowMicros - lastMicros) / 1000000.0f;
    if (dt <= 0.0f || dt > 0.1f) {
      dt = 0.01f;
    }
  }
  lastMicros = nowMicros;

  if (!filterInitialized) {
    fusedRoll = accelRoll;
    fusedPitch = accelPitch;
    filterInitialized = true;
  } else {
    fusedRoll  = COMPLEMENTARY_ALPHA * (fusedRoll + gx * dt)  + (1.0f - COMPLEMENTARY_ALPHA) * accelRoll;
    fusedPitch = COMPLEMENTARY_ALPHA * (fusedPitch + gy * dt) + (1.0f - COMPLEMENTARY_ALPHA) * accelPitch;
  }

  rawRollDeg = fusedRoll;
  rawPitchDeg = fusedPitch;

  float centeredRoll  = (rawRollDeg  - neutralRoll)  * ROLL_SIGN;
  float centeredPitch = (rawPitchDeg - neutralPitch) * PITCH_SIGN;

  centeredRoll  = applyDeadZone(centeredRoll, DEAD_ZONE_DEG);
  centeredPitch = applyDeadZone(centeredPitch, DEAD_ZONE_DEG);

  smoothRoll  += (centeredRoll  - smoothRoll)  * OUTPUT_SMOOTH_ALPHA;
  smoothPitch += (centeredPitch - smoothPitch) * OUTPUT_SMOOTH_ALPHA;

  if (millis() - lastPrintMs >= PRINT_INTERVAL_MS) {
    lastPrintMs = millis();
    printJSON(smoothPitch, smoothRoll, gx, gy, gz);
  }

  if (millis() - lastStatusMs >= STATUS_INTERVAL_MS) {
    lastStatusMs = millis();
    if (ENABLE_USB_SERIAL) {
      Serial.printf("[BT] client=%s\n", SerialBT.hasClient() ? "connected" : "waiting");
    }
  }

  delay(2);
}
