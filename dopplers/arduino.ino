const int sensorPin = A0;
const int ledPin = 13;

// tuning
const int sampleCount = 8;          // averaging samples
const float baselineAlpha = 0.01;   // slow baseline tracking
const int motionThreshold = 8;      // increase if too sensitive, decrease if missing motion
const int clearThreshold = 4;       // hysteresis: lower than motionThreshold
const unsigned long holdTime = 500; // keep motion ON for this many ms

float baseline = 0;
bool motion = false;
unsigned long lastMotionTime = 0;

int readAveragedSignal() {
  long sum = 0;
  for (int i = 0; i < sampleCount; i++) {
    sum += analogRead(sensorPin);
    delay(2);
  }
  return sum / sampleCount;
}

void setup() {
  pinMode(ledPin, OUTPUT);
  Serial.begin(9600);

  long startSum = 0;
  for (int i = 0; i < 100; i++) {
    startSum += readAveragedSignal();
    delay(5);
  }
  baseline = startSum / 100.0;

  Serial.print("Initial baseline: ");
  Serial.println(baseline, 2);
}

void loop() {
  int raw = readAveragedSignal();

  // difference from current baseline
  float diff = raw - baseline;
  float absDiff = diff < 0 ? -diff : diff;

  // only update baseline when no strong motion is happening
  if (absDiff < clearThreshold) {
    baseline = baseline * (1.0 - baselineAlpha) + raw * baselineAlpha;
  }

  // motion detect with hysteresis + hold time
  if (!motion && absDiff >= motionThreshold) {
    motion = true;
    lastMotionTime = millis();
  }

  if (motion) {
    if (absDiff >= clearThreshold) {
      lastMotionTime = millis();  // refresh hold while movement continues
    }

    if (millis() - lastMotionTime > holdTime) {
      motion = false;
    }
  }

  digitalWrite(ledPin, motion ? HIGH : LOW);

  Serial.print("Raw: ");
  Serial.print(raw);
  Serial.print("  Baseline: ");
  Serial.print(baseline, 2);
  Serial.print("  Change: ");
  Serial.print(absDiff, 2);
  Serial.print("  Motion: ");
  Serial.println(motion ? "YES" : "NO");

  delay(20);
}
