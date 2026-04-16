#include <SPI.h>
#include <RF24.h>

RF24 radio(9, 10); // CE, CSN
const byte address[6] = "AUD01";

struct AudioPacket {
  uint8_t seq;
  uint8_t samples[31];
};

AudioPacket pkt;

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
  radio.openReadingPipe(0, address);
  radio.startListening();

  Serial.println("Audio receiver started");
}

void loop() {
  if (radio.available()) {
    radio.read(&pkt, sizeof(pkt));

    Serial.print("SEQ:");
    Serial.print(pkt.seq);
    Serial.print(" ");

    for (int i = 0; i < 31; i++) {
      int16_t s = (int16_t)pkt.samples[i] - 128;
      Serial.print(s);
      if (i < 30) Serial.print(',');
    }
    Serial.println();
  }
}