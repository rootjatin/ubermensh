#include <Arduino.h>
#include <ESP_I2S.h>

// -------- Pin setup --------
static const int I2S_BCLK = 26;   // Bit clock
static const int I2S_WS   = 25;   // Word select / LRCLK
static const int I2S_DOUT = 22;   // To MAX98357A DIN
static const int I2S_DIN  = 34;   // From I2S mic SD

// -------- Audio setup --------
static const uint32_t SAMPLE_RATE = 16000;
static const size_t BUFFER_BYTES = 512;   // try 256/512/1024 if needed

I2SClass I2S;

uint8_t audioBuffer[BUFFER_BYTES];

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Starting I2S mic -> speaker passthrough...");

  // Standard I2S mode:
  // bclk, ws, dout, din, mclk(optional)
  I2S.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, I2S_DIN);

  // Duplex standard mode:
  // mode, sample rate, bits, mono/stereo
  if (!I2S.begin(I2S_MODE_STD,
                 SAMPLE_RATE,
                 I2S_DATA_BIT_WIDTH_16BIT,
                 I2S_SLOT_MODE_MONO)) {
    Serial.println("Failed to start I2S!");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("I2S started.");
  Serial.println("Speak into the mic...");
}

void loop() {
  int availableBytes = I2S.available();

  if (availableBytes > 0) {
    size_t toRead = (availableBytes < BUFFER_BYTES) ? availableBytes : BUFFER_BYTES;

    size_t bytesRead = I2S.readBytes((char*)audioBuffer, toRead);

    if (bytesRead > 0) {
      size_t bytesWritten = I2S.write(audioBuffer, bytesRead);

      if (bytesWritten != bytesRead) {
        Serial.printf("Underrun: read %u, wrote %u\n",
                      (unsigned)bytesRead,
                      (unsigned)bytesWritten);
      }
    }
  }
}
