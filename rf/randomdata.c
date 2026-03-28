#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

RF24 radio(9, 10); // CE, CSN

const byte address[6] = "00001";

struct DataPacket {
  int randomValue;
};

void setup() {
  Serial.begin(9600);

  radio.begin();
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_1MBPS);
  radio.stopListening();

  randomSeed(analogRead(A0)); // Seed random generator
}

void loop() {
  DataPacket data;
  data.randomValue = random(0, 1000); // Random number from 0 to 999

  bool success = radio.write(&data, sizeof(data));

  if (success) {
    Serial.print("Sent random value: ");
    Serial.println(data.randomValue);
  } else {
    Serial.println("Send failed");
  }

  delay(1000);
}
