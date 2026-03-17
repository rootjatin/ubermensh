#include <Mouse.h>

const int trigLeft  = 2;
const int echoLeft  = 3;
const int trigRight = 4;
const int echoRight = 5;
const int trigUp    = 6;
const int echoUp    = 7;
const int trigDown  = 8;
const int echoDown  = 9;

const int minDist = 5;
const int maxDist = 20;
const int maxSpeed = 12;

long readDistanceCM(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 25000);
  if (duration == 0) return -1;

  long distance = duration * 0.034 / 2;
  return distance;
}

int getSpeedFromDistance(long d, bool invertDir = false) {
  if (d < minDist || d > maxDist) return 0;

  int spd = map(d, minDist, maxDist, maxSpeed, 2);
  if (invertDir) spd = -spd;
  return spd;
}

void setup() {
  pinMode(trigLeft, OUTPUT);   pinMode(echoLeft, INPUT);
  pinMode(trigRight, OUTPUT);  pinMode(echoRight, INPUT);
  pinMode(trigUp, OUTPUT);     pinMode(echoUp, INPUT);
  pinMode(trigDown, OUTPUT);   pinMode(echoDown, INPUT);

  Serial.begin(9600);
  Mouse.begin();
}

void loop() {
  long dL = readDistanceCM(trigLeft, echoLeft);   delay(10);
  long dR = readDistanceCM(trigRight, echoRight); delay(10);
  long dU = readDistanceCM(trigUp, echoUp);       delay(10);
  long dD = readDistanceCM(trigDown, echoDown);   delay(10);

  int moveX = 0;
  int moveY = 0;

  if (dL > 0 && dL <= maxDist && !(dR > 0 && dR <= maxDist)) {
    moveX = getSpeedFromDistance(dL, true);   // left
  } else if (dR > 0 && dR <= maxDist && !(dL > 0 && dL <= maxDist)) {
    moveX = getSpeedFromDistance(dR, false);  // right
  }

  if (dU > 0 && dU <= maxDist && !(dD > 0 && dD <= maxDist)) {
    moveY = getSpeedFromDistance(dU, true);   // up
  } else if (dD > 0 && dD <= maxDist && !(dU > 0 && dU <= maxDist)) {
    moveY = getSpeedFromDistance(dD, false);  // down
  }

  Serial.print("L="); Serial.print(dL);
  Serial.print(" R="); Serial.print(dR);
  Serial.print(" U="); Serial.print(dU);
  Serial.print(" D="); Serial.println(dD);

  if (moveX != 0 || moveY != 0) {
    Mouse.move(moveX, moveY, 0);
  }

  delay(25);
}
