const int mic1Pin = A0;
const int mic2Pin = A1;

unsigned long previousMillis = 0;
const unsigned long interval = 1000;

bool mic1Active = true;

void setup() {
  Serial.begin(9600);
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    mic1Active = !mic1Active;
  }

  int micValue;

  if (mic1Active) {
    micValue = analogRead(mic1Pin);
    Serial.print("Mic 1 Active, Value: ");
  } else {
    micValue = analogRead(mic2Pin);
    Serial.print("Mic 2 Active, Value: ");
  }

  Serial.println(micValue);
  delay(50);
}