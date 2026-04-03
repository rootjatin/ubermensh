const int sensorPin = 2;   // Sensor output connected to Arduino pin 2
const int ledPin = 13;     // Built-in LED

void setup() {
  pinMode(sensorPin, INPUT);   // Use INPUT_PULLUP if needed
  pinMode(ledPin, OUTPUT);
  
  Serial.begin(9600);
  Serial.println("Photoelectric sensor test started");
}

void loop() {
  int sensorState = digitalRead(sensorPin);

  if (sensorState == HIGH) {
    Serial.println("Object detected");
    digitalWrite(ledPin, HIGH);
  } else {
    Serial.println("No object");
    digitalWrite(ledPin, LOW);
  }

  delay(200);
}
