#include <VirtualWire.h>

void setup() {
  vw_set_tx_pin(12);     // RF transmitter data pin connected to Arduino pin 12
  vw_setup(2000);        // Data rate: 2000 bits per second
}

void loop() {
  const char *msg = "Hello from Tx";

  vw_send((uint8_t *)msg, strlen(msg));
  vw_wait_tx();          // Wait until the whole message is sent

  delay(1000);           // Send every 1 second
}
