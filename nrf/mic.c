#include <Arduino.h>
#include <driver/i2s.h>
#include <SPI.h>
#include <RF24.h>

// ---------- nRF24 ----------
RF24 radio(4, 5); // CE, CSN
const byte address[6] = "AUD01";

// ---------- I2S mic pins ----------
#define I2S_WS   25   // LRCL / WS
#define I2S_SD   33   // DOUT from mic
#define I2S_SCK  26   // BCLK

#define I2S_PORT I2S_NUM_0

// 32-byte payload is ideal for nRF24
struct AudioPacket {
  uint8_t seq;
  uint8_t samples[31];
};

AudioPacket pkt;
uint8_t seqNum = 0;

void setupI2SMic() {
  const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 8000,                      // low-rate voice/audio
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_zero_dma_buffer(I2S_PORT);
}

void setupRadio() {
  if (!radio.begin()) {
    Serial.println("nRF24 not found");
    while (1) delay(100);
  }

  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_2MBPS);
  radio.setChannel(100);
  radio.setAutoAck(false);
  radio.setRetries(0, 0);
  radio.openWritingPipe(address);
  radio.stopListening();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  setupI2SMic();
  setupRadio();

  Serial.println("ESP32 audio transmitter started");
}

void loop() {
  int32_t rawSamples[31];
  size_t bytesRead = 0;

  // Read 31 x 32-bit I2S samples
  esp_err_t result = i2s_read(
    I2S_PORT,
    rawSamples,
    sizeof(rawSamples),
    &bytesRead,
    portMAX_DELAY
  );

  if (result == ESP_OK && bytesRead == sizeof(rawSamples)) {
    pkt.seq = seqNum++;

    for (int i = 0; i < 31; i++) {
      // Many I2S MEMS mics place useful data in upper bits
      int32_t s = rawSamples[i] >> 14;   // reduce dynamic range
      s = constrain(s, -128, 127);

      // Store as unsigned 8-bit for easy packet transfer
      pkt.samples[i] = (uint8_t)(s + 128);
    }

    bool ok = radio.write(&pkt, sizeof(pkt));

    if (!ok) {
      Serial.println("Send failed");
    }
  }
}
