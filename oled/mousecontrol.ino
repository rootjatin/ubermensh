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

USB Usb;
HIDBoot<USB_HID_PROTOCOL_MOUSE> HidMouse(&Usb);

int cursorX = OLED_W / 2;
int cursorY = OLED_H / 2;
bool needRedraw = true;

bool leftPressed = false;
bool middlePressed = false;
bool rightPressed = false;

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
  // Page 0 only
  // Small 4-column blocks
  uint8_t onPattern = 0b00111100;
  uint8_t offPattern = 0b00000000;

  // Left block at x = 2..5
  for (uint8_t x = 2; x <= 5; x++) {
    oledWriteByte(x, 0, leftPressed ? onPattern : offPattern);
  }

  // Middle block at x = 10..13
  for (uint8_t x = 10; x <= 13; x++) {
    oledWriteByte(x, 0, middlePressed ? onPattern : offPattern);
  }

  // Right block at x = 18..21
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

    cursorX += dx;
    cursorY -= dy;

    if (cursorX < 1) cursorX = 1;
    if (cursorX > 126) cursorX = 126;

    // Keep cursor below click indicator row
    if (cursorY < 10) cursorY = 10;
    if (cursorY > 62) cursorY = 62;

    needRedraw = true;

    Serial.print("MOVE dx=");
    Serial.print(dx);
    Serial.print(" dy=");
    Serial.println(dy);
  }

  void OnLeftButtonDown(MOUSEINFO *mi) override {
    leftPressed = true;
    needRedraw = true;
    Serial.println("LEFT DOWN");
  }

  void OnLeftButtonUp(MOUSEINFO *mi) override {
    leftPressed = false;
    needRedraw = true;
    Serial.println("LEFT UP");
  }

  void OnRightButtonDown(MOUSEINFO *mi) override {
    rightPressed = true;
    needRedraw = true;
    Serial.println("RIGHT DOWN");
  }

  void OnRightButtonUp(MOUSEINFO *mi) override {
    rightPressed = false;
    needRedraw = true;
    Serial.println("RIGHT UP");
  }

  void OnMiddleButtonDown(MOUSEINFO *mi) override {
    middlePressed = true;
    needRedraw = true;
    Serial.println("MIDDLE DOWN");
  }

  void OnMiddleButtonUp(MOUSEINFO *mi) override {
    middlePressed = false;
    needRedraw = true;
    Serial.println("MIDDLE UP");
  }
};

MouseParser mouseParser;

void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin();
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

  if (needRedraw) {
    needRedraw = false;
    redrawScreen();
  }
}
