#include <SPI.h>
#include <Wire.h>
#include <Usb.h>
#include <hidboot.h>

#define OLED_ADDR 0x3C
#define OLED_W 128
#define OLED_H 64

// Change these if movement feels wrong
#define INVERT_X true
#define INVERT_Y false

// Smoothness settings
#define CURSOR_MIN_X 1
#define CURSOR_MAX_X 126
#define CURSOR_MIN_Y 10
#define CURSOR_MAX_Y 62

#define REFRESH_MS 12        // ~83 FPS max redraw
#define SMOOTHING 0.35f      // 0.1 = softer, 1.0 = instant
#define MOUSE_SCALE 1.0f     // increase if cursor feels slow

USB Usb;
HIDBoot<USB_HID_PROTOCOL_MOUSE> HidMouse(&Usb);

// current displayed position
float cursorXF = OLED_W / 2.0f;
float cursorYF = OLED_H / 2.0f;

// target position
float targetXF = OLED_W / 2.0f;
float targetYF = OLED_H / 2.0f;

int cursorX = OLED_W / 2;
int cursorY = OLED_H / 2;

bool leftPressed = false;
bool middlePressed = false;
bool rightPressed = false;

unsigned long lastRedraw = 0;

// ---------------- OLED ----------------

void oledCmd(uint8_t c) {
  Wire.beginTransmission(OLED_ADDR);
  Wire.write(0x00);
  Wire.write(c);
  Wire.endTransmission();
}

void oledData(uint8_t d) {
  Wire.beginTransmission(OLED_ADDR);
  Wire.write(0x40);
  Wire.write(d);
  Wire.endTransmission();
}

void oledSetPos(uint8_t col, uint8_t page) {
  oledCmd(0xB0 | page);
  oledCmd(0x00 | (col & 0x0F));
  oledCmd(0x10 | ((col >> 4) & 0x0F));
}

void oledWriteByte(uint8_t col, uint8_t page, uint8_t value) {
  oledSetPos(col, page);
  oledData(value);
}

void oledInit() {
  oledCmd(0xAE);
  oledCmd(0xD5);
  oledCmd(0x80);
  oledCmd(0xA8);
  oledCmd(0x3F);
  oledCmd(0xD3);
  oledCmd(0x00);
  oledCmd(0x40);
  oledCmd(0x8D);
  oledCmd(0x14);
  oledCmd(0x20);
  oledCmd(0x02); // Page addressing mode
  oledCmd(0xA1);
  oledCmd(0xC8);
  oledCmd(0xDA);
  oledCmd(0x12);
  oledCmd(0x81);
  oledCmd(0xCF);
  oledCmd(0xD9);
  oledCmd(0xF1);
  oledCmd(0xDB);
  oledCmd(0x40);
  oledCmd(0xA4);
  oledCmd(0xA6);
  oledCmd(0xAF);
}

void oledClear() {
  for (uint8_t page = 0; page < 8; page++) {
    oledSetPos(0, page);
    for (uint8_t block = 0; block < 8; block++) {
      Wire.beginTransmission(OLED_ADDR);
      Wire.write(0x40);
      for (uint8_t i = 0; i < 16; i++) {
        Wire.write((uint8_t)0x00);
      }
      Wire.endTransmission();
    }
  }
}

// ---------------- Drawing ----------------

void drawCursor(int x, int y) {
  uint8_t page = y / 8;
  uint8_t bit = 1 << (y & 7);

  oledWriteByte(x, page, bit);
  if (x > 0) oledWriteByte(x - 1, page, bit);
  if (x < 127) oledWriteByte(x + 1, page, bit);

  if (y > 0) {
    uint8_t upPage = (y - 1) / 8;
    uint8_t upBit = 1 << ((y - 1) & 7);
    oledWriteByte(x, upPage, upBit);
  }

  if (y < 63) {
    uint8_t downPage = (y + 1) / 8;
    uint8_t downBit = 1 << ((y + 1) & 7);
    oledWriteByte(x, downPage, downBit);
  }
}

void drawButtonIndicators() {
  uint8_t onPattern = 0b00111100;
  uint8_t offPattern = 0b00000000;

  for (uint8_t x = 2; x <= 5; x++) {
    oledWriteByte(x, 0, leftPressed ? onPattern : offPattern);
  }

  for (uint8_t x = 10; x <= 13; x++) {
    oledWriteByte(x, 0, middlePressed ? onPattern : offPattern);
  }

  for (uint8_t x = 18; x <= 21; x++) {
    oledWriteByte(x, 0, rightPressed ? onPattern : offPattern);
  }
}

void redrawScreen() {
  oledClear();
  drawButtonIndicators();
  drawCursor(cursorX, cursorY);
}

// ---------------- Mouse ----------------

class MouseParser : public MouseReportParser {
protected:
  void OnMouseMove(MOUSEINFO *mi) override {
    int dx = (int8_t)mi->dX;
    int dy = (int8_t)mi->dY;

    if (INVERT_X) dx = -dx;
    if (INVERT_Y) dy = -dy;

    // Apply both axes together to preserve diagonal movement
    targetXF += dx * MOUSE_SCALE;
    targetYF -= dy * MOUSE_SCALE;

    // Clamp target area
    if (targetXF < CURSOR_MIN_X) targetXF = CURSOR_MIN_X;
    if (targetXF > CURSOR_MAX_X) targetXF = CURSOR_MAX_X;
    if (targetYF < CURSOR_MIN_Y) targetYF = CURSOR_MIN_Y;
    if (targetYF > CURSOR_MAX_Y) targetYF = CURSOR_MAX_Y;

    Serial.print("MOVE dx=");
    Serial.print(dx);
    Serial.print(" dy=");
    Serial.println(dy);
  }

  void OnLeftButtonDown(MOUSEINFO *mi) override {
    leftPressed = true;
    Serial.println("LEFT DOWN");
  }

  void OnLeftButtonUp(MOUSEINFO *mi) override {
    leftPressed = false;
    Serial.println("LEFT UP");
  }

  void OnRightButtonDown(MOUSEINFO *mi) override {
    rightPressed = true;
    Serial.println("RIGHT DOWN");
  }

  void OnRightButtonUp(MOUSEINFO *mi) override {
    rightPressed = false;
    Serial.println("RIGHT UP");
  }

  void OnMiddleButtonDown(MOUSEINFO *mi) override {
    middlePressed = true;
    Serial.println("MIDDLE DOWN");
  }

  void OnMiddleButtonUp(MOUSEINFO *mi) override {
    middlePressed = false;
    Serial.println("MIDDLE UP");
  }
};

MouseParser mouseParser;

void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin();
  Wire.setClock(400000);   // faster I2C for smoother OLED update
  delay(100);

  oledInit();
  oledClear();
  redrawScreen();

  Serial.println("Starting USB mouse test...");

  if (Usb.Init() == -1) {
    Serial.println("USB Host Shield init failed");
    while (1);
  }

  HidMouse.SetReportParser(0, &mouseParser);

  Serial.println("USB Host Shield initialized");
  Serial.println("Move mouse...");
}

void loop() {
  Usb.Task();

  unsigned long now = millis();
  if (now - lastRedraw >= REFRESH_MS) {
    lastRedraw = now;

    // Smooth easing towards target
    cursorXF += (targetXF - cursorXF) * SMOOTHING;
    cursorYF += (targetYF - cursorYF) * SMOOTHING;

    int newX = (int)(cursorXF + 0.5f);
    int newY = (int)(cursorYF + 0.5f);

    if (newX < CURSOR_MIN_X) newX = CURSOR_MIN_X;
    if (newX > CURSOR_MAX_X) newX = CURSOR_MAX_X;
    if (newY < CURSOR_MIN_Y) newY = CURSOR_MIN_Y;
    if (newY > CURSOR_MAX_Y) newY = CURSOR_MAX_Y;

    bool changed =
      (newX != cursorX) ||
      (newY != cursorY);

    cursorX = newX;
    cursorY = newY;

    if (changed || leftPressed || middlePressed || rightPressed) {
      redrawScreen();
    }
  }
}
