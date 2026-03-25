#include <VirtualWire.h>

void setup() {
  Serial.begin(9600);

  vw_set_rx_pin(11);     // RF receiver data pin connected to Arduino pin 11
  vw_setup(2000);        // Same speed as transmitter
  vw_rx_start();         // Start the receiver
}

void loop() {
  uint8_t buf[VW_MAX_MESSAGE_LEN];
  uint8_t buflen = VW_MAX_MESSAGE_LEN;

  if (vw_get_message(buf, &buflen)) {   // Check if a valid message is received
    Serial.print("Received: ");

    for (int i = 0; i < buflen; i++) {
      Serial.print((char)buf[i]);
    }

    Serial.println();
  }
}
