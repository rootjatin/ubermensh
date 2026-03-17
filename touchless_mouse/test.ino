#include <Mouse.h>

const int trigPin = 9;
const int echoPin = 10;

const int thresholdCm = 20;   // move mouse when object is closer than 20 cm
const int moveStep = 1;       // slow movement
const int moveDelay = 80;     // bigger = slower

float readDistanceCm() {
  // Send trigger pulse
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Read echo pulse (timeout avoids hanging)
  long duration = pulseIn(echoPin, HIGH, 30000);

  if (duration == 0) {
    return -1; // no reading
  }

  // Convert time to distance in cm
  float distance = duration * 0.0343 / 2.0;
  return distance;
}

void setup() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  Serial.begin(9600);

  // Safety delay so the cursor doesn't start moving immediately after upload/reset
  delay(5000);

  Mouse.begin();
}

void loop() {
  float distance = readDistanceCm();

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  if (distance > 0 && distance < thresholdCm) {
    // Move mouse slowly to the right
    Mouse.move(moveStep, 0, 0);
  }

  delay(moveDelay);
}
