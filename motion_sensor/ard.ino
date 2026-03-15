#include <Wire.h>
#include <math.h>

uint8_t IMU_ADDR = 0x68;

// Registers
const uint8_t REG_SMPLRT_DIV = 0x19;
const uint8_t REG_CONFIG = 0x1A;
const uint8_t REG_GYRO_CONFIG = 0x1B;
const uint8_t REG_ACCEL_CONFIG = 0x1C;
const uint8_t REG_ACCEL_XOUT_H = 0x3B;
const uint8_t REG_PWR_MGMT_1 = 0x6B;
const uint8_t REG_WHO_AM_I = 0x75;

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

// IMU type
uint8_t detectedWhoAmI = 0x00;

// State
bool imuReady = false;
bool filterInitialized = false;

// Tunables
const float ACCEL_SCALE = 16384.0f;     // +/-2g
const float GYRO_SCALE = 131.0f;        // +/-250 dps
const float COMPLEMENTARY_ALPHA = 0.98f;
const float OUTPUT_SMOOTH_ALPHA = 0.18f;
const float DEAD_ZONE_DEG = 0.5f;
const unsigned long PRINT_INTERVAL_MS = 20;

// If controls feel reversed, change one or both signs below
const float PITCH_SIGN = 1.0f;
const float ROLL_SIGN = 1.0f;

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

  axRaw = (int16_t)((buf[0] << 8) | buf[1]);
  ayRaw = (int16_t)((buf[2] << 8) | buf[3]);
  azRaw = (int16_t)((buf[4] << 8) | buf[5]);
  tempRaw = (int16_t)((buf[6] << 8) | buf[7]);
  gxRaw = (int16_t)((buf[8] << 8) | buf[9]);
  gyRaw = (int16_t)((buf[10] << 8) | buf[11]);
  gzRaw = (int16_t)((buf[12] << 8) | buf[13]);
  return true;
}

bool setupIMU() {
  detectedWhoAmI = readRegister(REG_WHO_AM_I);

  Serial.print("WHO_AM_I = 0x");
  Serial.println(detectedWhoAmI, HEX);

  if (detectedWhoAmI == 0x68) {
    Serial.println("Detected MPU6050-like IMU");
  } else if (detectedWhoAmI == 0x70 || detectedWhoAmI == 0x71 || detectedWhoAmI == 0x73) {
    Serial.println("Detected MPU6500/MPU9250-like IMU");
  } else {
    Serial.println("Unknown IMU response, trying anyway...");
  }

  // Wake up device
  if (!writeRegister(REG_PWR_MGMT_1, 0x80)) return false;  // reset
  delay(100);
  if (!writeRegister(REG_PWR_MGMT_1, 0x01)) return false;  // auto select clock
  delay(50);

  // Low-pass filter and sample settings
  if (!writeRegister(REG_CONFIG, 0x03)) return false;       // DLPF
  if (!writeRegister(REG_SMPLRT_DIV, 0x04)) return false;   // sample divider
  if (!writeRegister(REG_GYRO_CONFIG, 0x00)) return false;  // +/-250 dps
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

  Serial.println();
  Serial.println("STEP 1: Put sensor FLAT and STILL.");
  Serial.println("Calibration starts in 3 seconds...");
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
    Serial.println("Calibration failed: no samples read.");
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

  Serial.println("Flat calibration done.");
  Serial.print("axOffset = "); Serial.println(axOffset, 3);
  Serial.print("ayOffset = "); Serial.println(ayOffset, 3);
  Serial.print("azOffset = "); Serial.println(azOffset, 3);
  Serial.print("gxOffset = "); Serial.println(gxOffset, 3);
  Serial.print("gyOffset = "); Serial.println(gyOffset, 3);
  Serial.print("gzOffset = "); Serial.println(gzOffset, 3);
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
  // gz is available but not used in pitch/roll filter directly

  float accelRoll = atan2f(ay, az) * 180.0f / PI;
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
    fusedRoll = COMPLEMENTARY_ALPHA * (fusedRoll + gx * dt) + (1.0f - COMPLEMENTARY_ALPHA) * accelRoll;
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

  Serial.println();
  Serial.println("STEP 2: Hold sensor in your NORMAL hand position.");
  Serial.println("Capturing neutral pose in 3 seconds...");
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
    Serial.println("Neutral capture failed.");
    return;
  }

  neutralRoll = rollSum / good;
  neutralPitch = pitchSum / good;

  smoothRoll = 0.0f;
  smoothPitch = 0.0f;

  Serial.println("Neutral pose captured.");
  Serial.print("neutralRoll = "); Serial.println(neutralRoll, 2);
  Serial.print("neutralPitch = "); Serial.println(neutralPitch, 2);
  Serial.println("Send 'c' to recenter.");
  Serial.println("Send 'r' to recalibrate.");
}

// --------------------------------------------------
// Output helpers
// --------------------------------------------------
float applyDeadZone(float value, float deadZone) {
  if (fabs(value) < deadZone) {
    return 0.0f;
  }
  return value;
}

void printJSON(float pitchOut, float rollOut, float gxDegPerSec, float gyDegPerSec, float gzDegPerSec) {
  Serial.print("{\"pitch\":");
  Serial.print(pitchOut, 2);
  Serial.print(",\"roll\":");
  Serial.print(rollOut, 2);
  Serial.print(",\"rawPitch\":");
  Serial.print(rawPitchDeg, 2);
  Serial.print(",\"rawRoll\":");
  Serial.print(rawRollDeg, 2);
  Serial.print(",\"gx\":");
  Serial.print(gxDegPerSec, 2);
  Serial.print(",\"gy\":");
  Serial.print(gyDegPerSec, 2);
  Serial.print(",\"gz\":");
  Serial.print(gzDegPerSec, 2);
  Serial.println("}");
}

// --------------------------------------------------
// Serial commands
// --------------------------------------------------
void handleCommands() {
  while (Serial.available()) {
    char c = (char)Serial.read();
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
}

// --------------------------------------------------
// Setup / loop
// --------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);

  Wire.begin();
  Wire.setClock(400000);

  Serial.println("Starting IMU aircraft controller...");
  Serial.print("Trying 0x");
  Serial.println(IMU_ADDR, HEX);

  if (!setupIMU()) {
    Serial.println("IMU setup failed.");
    imuReady = false;
    return;
  }

  Serial.println("IMU found at address 0x68");
  imuReady = true;

  calibrateFlatStill();
  filterInitialized = false;
  lastMicros = 0;
  letFilterSettle(1500);
  captureNeutralPose();

  lastPrintMs = millis();
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

  float accelRoll = atan2f(ay, az) * 180.0f / PI;
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
    fusedRoll = COMPLEMENTARY_ALPHA * (fusedRoll + gx * dt) + (1.0f - COMPLEMENTARY_ALPHA) * accelRoll;
    fusedPitch = COMPLEMENTARY_ALPHA * (fusedPitch + gy * dt) + (1.0f - COMPLEMENTARY_ALPHA) * accelPitch;
  }

  rawRollDeg = fusedRoll;
  rawPitchDeg = fusedPitch;

  float centeredRoll = (rawRollDeg - neutralRoll) * ROLL_SIGN;
  float centeredPitch = (rawPitchDeg - neutralPitch) * PITCH_SIGN;

  centeredRoll = applyDeadZone(centeredRoll, DEAD_ZONE_DEG);
  centeredPitch = applyDeadZone(centeredPitch, DEAD_ZONE_DEG);

  smoothRoll += (centeredRoll - smoothRoll) * OUTPUT_SMOOTH_ALPHA;
  smoothPitch += (centeredPitch - smoothPitch) * OUTPUT_SMOOTH_ALPHA;

  if (millis() - lastPrintMs >= PRINT_INTERVAL_MS) {
    lastPrintMs = millis();
    printJSON(smoothPitch, smoothRoll, gx, gy, gz);
  }

  delay(2);
}
