/*
  Simple 2-Mic Voice Changer for Arduino Uno/Nano
  ------------------------------------------------
  - Mic 1 on A0
  - Mic 2 on A1
  - PWM audio output on D9

  Effect:
  - Mixes both mics
  - Applies a robotic/ring-mod style amplitude modulation

  Notes:
  - Mic modules should have DC bias around mid-supply
  - Output on D9 is PWM audio, use RC filter + amplifier
*/

const int mic1Pin = A0;
const int mic2Pin = A1;
const int audioOutPin = 9;

// Effect parameters
volatile uint8_t lfoPhase = 0;
volatile uint8_t lfoStep = 6;   // increase for stronger/faster robot effect

void setup() {
  pinMode(audioOutPin, OUTPUT);

  // Set up PWM on pin 9 at higher frequency
  // Fast PWM, 8-bit, non-inverting on OC1A (pin 9)
  TCCR1A = _BV(COM1A1) | _BV(WGM10);
  TCCR1B = _BV(WGM12) | _BV(CS10); // no prescaler
  OCR1A = 127; // mid output

  // ADC prescaler for faster reads
  ADCSRA = (ADCSRA & 0xF8) | 0x04; // prescaler 16
}

void loop() {
  // Read two microphones
  int mic1 = analogRead(mic1Pin);
  int mic2 = analogRead(mic2Pin);

  // Mix both microphones
  int mixed = (mic1 + mic2) / 2;

  // Remove DC center (assuming mic modules biased around 512)
  int centered = mixed - 512;

  // Simple robot/ring modulation effect
  // Create square-wave LFO
  lfoPhase += lfoStep;
  int mod;
  if (lfoPhase < 128) {
    mod = 180;   // stronger section
  } else {
    mod = 40;    // weaker section
  }

  // Apply modulation
  long effected = (long)centered * mod / 128;

  // Optional clipping
  if (effected > 511) effected = 511;
  if (effected < -511) effected = -511;

  // Convert back to 8-bit PWM range
  int pwmOut = (effected >> 2) + 127;

  if (pwmOut < 0) pwmOut = 0;
  if (pwmOut > 255) pwmOut = 255;

  OCR1A = pwmOut;
}
