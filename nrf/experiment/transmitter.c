#include <SPI.h>
#include <RF24.h>

RF24 radio(9, 10);          // CE, CSN
const byte address[6] = "AUD01";

struct AudioPacket {
  uint8_t seq;
  uint8_t samples[31];
};

AudioPacket pkt;
uint8_t sequenceNumber = 0;

const int micPin = A0;

// Sampling delay in microseconds
// Increase for lower sample rate, decrease for higher sample rate
// Around 120-180 us gives rough low-quality audio
const unsigned int sampleDelayUs = 150;

void setup() {
  Serial.begin(115200);

  if (!radio.begin()) {
    Serial.println("nRF24 not found");
    while (1);
  }

  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_2MBPS);
  radio.setChannel(100);
  radio.setAutoAck(false);
  radio.stopListening();
  radio.openWritingPipe(address);

  Serial.println("Audio transmitter started");
}

void loop() {
  pkt.seq = sequenceNumber++;

  for (int i = 0; i < 31; i++) {
    int raw = analogRead(micPin);   // 0 to 1023

    // Convert 10-bit ADC to 8-bit
    uint8_t sample8 = raw >> 2;     // 0 to 255
    pkt.samples[i] = sample8;

    delayMicroseconds(sampleDelayUs);
  }

  bool ok = radio.write(&pkt, sizeof(pkt));

  if (ok) {
    Serial.print("Sent packet SEQ: ");
    Serial.println(pkt.seq);
  } else {
    Serial.println("Send failed");
  }
}