// Mic pins
const int mic1Pin = A0;      // Mic 1 analog output
const int mic2Pin = A1;      // Mic 2 analog output

// Control pins to activate/deactivate mics
const int mic1Enable = 7;
const int mic2Enable = 8;

unsigned long previousMillis = 0;
const unsigned long interval = 1000; // 1 second

bool mic1Active = true;

void setup() {
  Serial.begin(9600);

  pinMode(mic1Enable, OUTPUT);
  pinMode(mic2Enable, OUTPUT);

  // Start with Mic 1 active
  activateMic1();
}

void loop() {
  unsigned long currentMillis = millis();

  // Change active mic every 1 second
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    mic1Active = !mic1Active;

    if (mic1Active) {
      activateMic1();
    } else {
      activateMic2();
    }
  }

  // Read only the active mic
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

void activateMic1() {
  digitalWrite(mic1Enable, HIGH);
  digitalWrite(mic2Enable, LOW);
}

void activateMic2() {
  digitalWrite(mic1Enable, LOW);
  digitalWrite(mic2Enable, HIGH);
}
