#include <avr/io.h>
#include <avr/interrupt.h>
#include <math.h>

const byte OUT_PIN = 9;               // pin 9 = OC1A
const float OUTPUT_FREQ_HZ = 100.0;   // start low for a smoother filtered sine
const float PWM_RATE_HZ = 62500.0;    // Timer1 8-bit Fast PWM @ 16 MHz / 256

volatile uint32_t phaseAcc = 0;
volatile uint32_t phaseStep = 0;
uint8_t sineTable[256];

void buildSineTable() {
  for (uint16_t i = 0; i < 256; i++) {
    float a = 2.0f * PI * i / 256.0f;
    sineTable[i] = (uint8_t)(127.5f + 127.5f * sin(a));
  }
}

ISR(TIMER1_OVF_vect) {
  phaseAcc += phaseStep;
  OCR1A = sineTable[(uint8_t)(phaseAcc >> 24)];
}

void setup() {
  buildSineTable();

  // DDS tuning word
  phaseStep = (uint32_t)((OUTPUT_FREQ_HZ * 4294967296.0f) / PWM_RATE_HZ);

  pinMode(OUT_PIN, OUTPUT);

  cli();

  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;

  // Timer1: Fast PWM 8-bit, non-inverting on OC1A (pin 9), prescaler = 1
  TCCR1A = _BV(COM1A1) | _BV(WGM10);
  TCCR1B = _BV(WGM12)  | _BV(CS10);

  OCR1A  = 127;          // start at midscale
  TIMSK1 = _BV(TOIE1);   // interrupt every PWM cycle

  sei();
}

void loop() {
  // nothing here
}
