#include <Wire.h>
#include <BleMouse.h>
#include <math.h>

BleMouse bleMouse("ESP32 Air Mouse", "ESP32", 100);

// -------------------- Pins / I2C --------------------
const int SDA_PIN = 21;
const int SCL_PIN = 22;
const uint8_t MPU_ADDR = 0x68;

// -------------------- MPU-6500 registers --------------------
const uint8_t REG_SMPLRT_DIV     = 0x19;
const uint8_t REG_CONFIG         = 0x1A;
const uint8_t REG_GYRO_CONFIG    = 0x1B;
const uint8_t REG_ACCEL_CONFIG   = 0x1C;
const uint8_t REG_ACCEL_CONFIG2  = 0x1D;
const uint8_t REG_USER_CTRL      = 0x6A;
const uint8_t REG_PWR_MGMT_1     = 0x6B;
const uint8_t REG_PWR_MGMT_2     = 0x6C;
const uint8_t REG_ACCEL_XOUT_H   = 0x3B;
const uint8_t REG_WHO_AM_I       = 0x75;

// -------------------- Sensor scales --------------------
const float GYRO_LSB_PER_DPS = 65.5f;    // ±500 dps
const float ACCEL_LSB_PER_G  = 8192.0f;  // ±4 g

// -------------------- Direction tuning --------------------
// If any axis is reversed, flip one of these signs.
const float X_SIGN = -1.0f;
const float Y_SIGN = -1.0f;

// -------------------- Filter / control tuning --------------------
// Lower compAlpha = more accel correction = less drift
const float compAlpha          = 0.965f;

// Bigger deadband = more tilt needed before cursor moves
const float deadbandDeg        = 6.0f;
const float stopBandDeg        = 2.2f;

// Still detection
const float gyroStillThresh    = 1.2f;   // deg/s
const float accelMagStillTol   = 0.08f;  // g tolerance around 1g

// Smoothing
const float accelAngleLPF      = 0.18f;  // low-pass accel angles
const float velocityLPF        = 0.18f;  // low-pass cursor speed

// Slow center auto-trim while device is held still
const float centerAdaptAlpha   = 0.015f;

// Cursor response
const float sensitivity        = 0.55f;
const float responseExponent   = 1.35f;
const float maxStep            = 7.0f;

// -------------------- State --------------------
float gxOffset = 0.0f;
float gyOffset = 0.0f;
float gzOffset = 0.0f;

float rollDeg = 0.0f;
float pitchDeg = 0.0f;

float accRollFilt = 0.0f;
float accPitchFilt = 0.0f;

float centerRollDeg = 0.0f;
float centerPitchDeg = 0.0f;

float velX = 0.0f;
float velY = 0.0f;

float carryX = 0.0f;
float carryY = 0.0f;

unsigned long lastMicros = 0;

// -------------------- Helpers --------------------
float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

float normalizeAngleDeg(float a) {
  while (a > 180.0f) a -= 360.0f;
  while (a < -180.0f) a += 360.0f;
  return a;
}

float angleDiffDeg(float current, float reference) {
  return normalizeAngleDeg(current - reference);
}

float applyDeadband(float v, float db) {
  if (fabs(v) <= db) return 0.0f;
  return (v > 0.0f) ? (v - db) : (v + db);
}

float shapeResponse(float v) {
  if (v == 0.0f) return 0.0f;
  float s = (v > 0.0f) ? 1.0f : -1.0f;
  return s * powf(fabs(v), responseExponent);
}

bool writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return (Wire.endTransmission() == 0);
}

uint8_t readReg(uint8_t reg) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return 0xFF;
  if (Wire.requestFrom((int)MPU_ADDR, 1) != 1) return 0xFF;
  return Wire.read();
}

bool readBytes(uint8_t reg, uint8_t *buf, size_t len) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)MPU_ADDR, (int)len) != (int)len) return false;

  for (size_t i = 0; i < len; i++) {
    buf[i] = Wire.read();
  }
  return true;
}

int16_t makeInt16(uint8_t hi, uint8_t lo) {
  return (int16_t)((hi << 8) | lo);
}

bool initMPU6500() {
  if (!writeReg(REG_PWR_MGMT_1, 0x80)) return false;
  delay(100);

  if (!writeReg(REG_PWR_MGMT_1, 0x01)) return false;
  delay(10);

  if (!writeReg(REG_PWR_MGMT_2, 0x00)) return false;
  if (!writeReg(REG_USER_CTRL, 0x00)) return false;

  // 1 kHz / (1 + 4) = 200 Hz
  if (!writeReg(REG_SMPLRT_DIV, 0x04)) return false;

  // DLPF
  if (!writeReg(REG_CONFIG, 0x04)) return false;

  // Gyro ±500 dps
  if (!writeReg(REG_GYRO_CONFIG, 0x08)) return false;

  // Accel ±4 g
  if (!writeReg(REG_ACCEL_CONFIG, 0x08)) return false;

  // Accel DLPF
  if (!writeReg(REG_ACCEL_CONFIG2, 0x04)) return false;

  delay(20);
  return true;
}

bool readMPU(
  int16_t &axRaw, int16_t &ayRaw, int16_t &azRaw,
  int16_t &tempRaw,
  int16_t &gxRaw, int16_t &gyRaw, int16_t &gzRaw
) {
  uint8_t data[14];

  if (!readBytes(REG_ACCEL_XOUT_H, data, 14)) return false;

  axRaw   = makeInt16(data[0],  data[1]);
  ayRaw   = makeInt16(data[2],  data[3]);
  azRaw   = makeInt16(data[4],  data[5]);
  tempRaw = makeInt16(data[6],  data[7]);
  gxRaw   = makeInt16(data[8],  data[9]);
  gyRaw   = makeInt16(data[10], data[11]);
  gzRaw   = makeInt16(data[12], data[13]);

  return true;
}

void calibrateGyro() {
  const int samples = 1500;
  float sumX = 0.0f;
  float sumY = 0.0f;
  float sumZ = 0.0f;

  Serial.println("Keep still for gyro calibration...");

  for (int i = 0; i < samples; i++) {
    int16_t axRaw, ayRaw, azRaw, tempRaw, gxRaw, gyRaw, gzRaw;

    if (readMPU(axRaw, ayRaw, azRaw, tempRaw, gxRaw, gyRaw, gzRaw)) {
      sumX += ((float)gxRaw / GYRO_LSB_PER_DPS);
      sumY += ((float)gyRaw / GYRO_LSB_PER_DPS);
      sumZ += ((float)gzRaw / GYRO_LSB_PER_DPS);
    }

    delay(2);
  }

  gxOffset = sumX / samples;
  gyOffset = sumY / samples;
  gzOffset = sumZ / samples;

  Serial.println("Gyro calibration done.");
}

void captureStartupCenter() {
  const int samples = 200;
  float sumRoll = 0.0f;
  float sumPitch = 0.0f;
  int good = 0;

  Serial.println("Hold in neutral position...");

  for (int i = 0; i < samples; i++) {
    int16_t axRaw, ayRaw, azRaw, tempRaw, gxRaw, gyRaw, gzRaw;

    if (readMPU(axRaw, ayRaw, azRaw, tempRaw, gxRaw, gyRaw, gzRaw)) {
      float ax = (float)axRaw / ACCEL_LSB_PER_G;
      float ay = (float)ayRaw / ACCEL_LSB_PER_G;
      float az = (float)azRaw / ACCEL_LSB_PER_G;

      float r = normalizeAngleDeg(atan2(ay, az) * 180.0f / PI);
      float p = normalizeAngleDeg(atan2(-ax, sqrt(ay * ay + az * az)) * 180.0f / PI);

      sumRoll += r;
      sumPitch += p;
      good++;
    }
    delay(4);
  }

  if (good == 0) {
    Serial.println("Failed to capture startup center.");
    return;
  }

  centerRollDeg = sumRoll / good;
  centerPitchDeg = sumPitch / good;

  rollDeg = centerRollDeg;
  pitchDeg = centerPitchDeg;

  accRollFilt = centerRollDeg;
  accPitchFilt = centerPitchDeg;

  Serial.println("Startup center captured.");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Starting ESP32 Air Mouse...");

  Wire.begin(SDA_PIN, SCL_PIN, 100000);
  delay(100);

  uint8_t who = readReg(REG_WHO_AM_I);
  Serial.print("WHO_AM_I = 0x");
  if (who < 16) Serial.print("0");
  Serial.println(who, HEX);

  if (who != 0x70) {
    Serial.println("Unexpected sensor ID. Expected 0x70.");
    while (1) delay(10);
  }

  if (!initMPU6500()) {
    Serial.println("Failed to initialize MPU-6500.");
    while (1) delay(10);
  }

  Serial.println("MPU-6500 initialized.");

  calibrateGyro();
  captureStartupCenter();

  bleMouse.begin();
  lastMicros = micros();

  Serial.println("Bluetooth mouse started.");
  Serial.println("Pair with: ESP32 Air Mouse");
}

void loop() {
  int16_t axRaw, ayRaw, azRaw, tempRaw, gxRaw, gyRaw, gzRaw;

  if (!readMPU(axRaw, ayRaw, azRaw, tempRaw, gxRaw, gyRaw, gzRaw)) {
    delay(5);
    return;
  }

  unsigned long nowMicros = micros();
  float dt = (nowMicros - lastMicros) / 1000000.0f;
  lastMicros = nowMicros;

  if (dt <= 0.0f || dt > 0.03f) {
    dt = 0.01f;
  }

  // Convert raw data
  float ax = (float)axRaw / ACCEL_LSB_PER_G;
  float ay = (float)ayRaw / ACCEL_LSB_PER_G;
  float az = (float)azRaw / ACCEL_LSB_PER_G;

  float gxDeg = ((float)gxRaw / GYRO_LSB_PER_DPS) - gxOffset;
  float gyDeg = ((float)gyRaw / GYRO_LSB_PER_DPS) - gyOffset;
  float gzDeg = ((float)gzRaw / GYRO_LSB_PER_DPS) - gzOffset;

  // Accelerometer tilt estimate
  float accRollDeg = normalizeAngleDeg(atan2(ay, az) * 180.0f / PI);
  float accPitchDeg = normalizeAngleDeg(
    atan2(-ax, sqrt(ay * ay + az * az)) * 180.0f / PI
  );

  // Low-pass accel tilt to reduce noise
  accRollFilt = normalizeAngleDeg(
    accRollFilt + accelAngleLPF * angleDiffDeg(accRollDeg, accRollFilt)
  );
  accPitchFilt = normalizeAngleDeg(
    accPitchFilt + accelAngleLPF * angleDiffDeg(accPitchDeg, accPitchFilt)
  );

  // Integrate gyro
  rollDeg = normalizeAngleDeg(rollDeg + gxDeg * dt);
  pitchDeg = normalizeAngleDeg(pitchDeg + gyDeg * dt);

  // Complementary correction using shortest-angle difference
  rollDeg = normalizeAngleDeg(
    rollDeg + (1.0f - compAlpha) * angleDiffDeg(accRollFilt, rollDeg)
  );
  pitchDeg = normalizeAngleDeg(
    pitchDeg + (1.0f - compAlpha) * angleDiffDeg(accPitchFilt, pitchDeg)
  );

  // Map axes
  float tiltLeftRight = angleDiffDeg(rollDeg, centerRollDeg);
  float tiltUpDown    = angleDiffDeg(pitchDeg, centerPitchDeg);

  float gyroMag = sqrt(gxDeg * gxDeg + gyDeg * gyDeg + gzDeg * gzDeg);
  float accelMag = sqrt(ax * ax + ay * ay + az * az);

  bool still = (gyroMag < gyroStillThresh) &&
               (fabs(accelMag - 1.0f) < accelMagStillTol);

  // Auto-trim center while still to remove slow upward drift
  if (still) {
    centerRollDeg = normalizeAngleDeg(
      centerRollDeg + centerAdaptAlpha * angleDiffDeg(rollDeg, centerRollDeg)
    );
    centerPitchDeg = normalizeAngleDeg(
      centerPitchDeg + centerAdaptAlpha * angleDiffDeg(pitchDeg, centerPitchDeg)
    );
  }

  float moveX = 0.0f;
  float moveY = 0.0f;

  // Only respond when outside intentional movement threshold
  if (!(still && fabs(tiltLeftRight) < stopBandDeg && fabs(tiltUpDown) < stopBandDeg)) {
    float cmdX = applyDeadband(tiltLeftRight, deadbandDeg);
    float cmdY = applyDeadband(tiltUpDown, deadbandDeg);

    moveX = X_SIGN * sensitivity * shapeResponse(cmdX);
    moveY = Y_SIGN * sensitivity * shapeResponse(cmdY);
  }

  moveX = clampf(moveX, -maxStep, maxStep);
  moveY = clampf(moveY, -maxStep, maxStep);

  // Smooth cursor speed, not just angle
  velX = velocityLPF * moveX + (1.0f - velocityLPF) * velX;
  velY = velocityLPF * moveY + (1.0f - velocityLPF) * velY;

  // Strong damping near center
  if (fabs(moveX) < 0.05f) velX *= 0.75f;
  if (fabs(moveY) < 0.05f) velY *= 0.75f;

  // Fractional carry for smoother low-speed motion
  carryX += velX;
  carryY += velY;

  int mouseX = (int)roundf(carryX);
  int mouseY = (int)roundf(carryY);

  carryX -= mouseX;
  carryY -= mouseY;

  if (bleMouse.isConnected()) {
    if (mouseX != 0 || mouseY != 0) {
      bleMouse.move(mouseX, mouseY);
    }
  }

  delay(5);
}
