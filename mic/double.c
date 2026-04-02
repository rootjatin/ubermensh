/*
  Better 2-Microphone Voice Changer for Arduino Uno/Nano
  ------------------------------------------------------
  - Mic 1 -> A0
  - Mic 2 -> A1
  - Audio PWM output -> D9

  Features:
  - Fixed sample rate using Timer2 interrupt
  - Faster ADC settings
  - Mixes two microphones
  - Applies tremolo / robot-like effect
  - Much more stable than using analogRead() in loop()

  IMPORTANT:
  - Works best with analog microphone modules biased around 2.5V
  - Output on D9 must go through RC low-pass filter, then amplifier
  - Arduino Uno/Nano is limited; this is a simple effect, not studio pitch shifting
*/

#include <Arduino.h>

const uint8_t MIC1_PIN = A0;
const uint8_t MIC2_PIN = A1;
const uint8_t AUDIO_OUT_PIN = 9;

// Audio settings
volatile uint8_t pwmSample = 127;

// Effect control
volatile uint8_t lfoPhase = 0;
volatile uint8_t lfoSpeed = 4;   // Increase for faster robot effect

// Read ADC quickly
int readADC(uint8_t channel) {
  ADMUX = (1 << REFS0) | (channel & 0x07);  // AVcc reference, select ADC channel
  ADCSRA |= (1 << ADSC);                    // Start conversion
  while (ADCSRA & (1 << ADSC));             // Wait
  return ADC;
}

// Timer2 interrupt at about 8 kHz sample rate
ISR(TIMER2_COMPA_vect) {
  // Read both microphones
  int mic1 = readADC(0);   // A0
  int mic2 = readADC(1);   // A1

  // Mix them
  int mixed = (mic1 + mic2) >> 1;

  // Remove DC offset
  int sample = mixed - 512;

  // Noise gate to reduce hiss when quiet
  if (sample > -8 && sample < 8) {
    sample = 0;
  }

  // Robot/tremolo effect
  lfoPhase += lfoSpeed;

  // Square-wave modulation
  int gain;
  if (lfoPhase < 128) {
    gain = 200;   // louder part
  } else {
    gain = 30;    // quieter part
  }

  long processed = (long)sample * gain / 128;

  // Soft clipping
  if (processed > 511) processed = 511;
  if (processed < -511) processed = -511;

  // Convert to 8-bit PWM
  int out = (processed >> 2) + 127;

  if (out < 0) out = 0;
  if (out > 255) out = 255;

  OCR1A = out;   // Output on D9
}

void setupPWM() {
  pinMode(AUDIO_OUT_PIN, OUTPUT);

  // Timer1 Fast PWM 8-bit on D9
  TCCR1A = _BV(COM1A1) | _BV(WGM10);
  TCCR1B = _BV(WGM12) | _BV(CS10);   // no prescaler
  OCR1A = 127;
}

void setupADC() {
  // AVcc reference
  ADMUX = (1 << REFS0);

  // Enable ADC, prescaler 16 for faster conversion
  ADCSRA = (1 << ADEN) | (1 << ADPS2);
}

void setupSampleTimer() {
  cli();

  // Timer2 CTC mode
  TCCR2A = (1 << WGM21);
  TCCR2B = 0;

  // Prescaler 8
  TCCR2B |= (1 << CS21);

  // 16 MHz / 8 / (249+1) = 8 kHz
  OCR2A = 249;

  // Enable compare match interrupt
  TIMSK2 |= (1 << OCIE2A);

  sei();
}

void setup() {
  setupPWM();
  setupADC();
  setupSampleTimer();
}

void loop() {
  // Nothing here; audio runs in interrupt
}
