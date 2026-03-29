/*
  5x5x2 LED Cuboid for Arduino Mega
  25 columns + 2 layers

  Column pins: 22..46
  Layer pins : 47, 48

  Change COL_ON/COL_OFF and LAYER_ON/LAYER_OFF if your wiring polarity is opposite.
*/

const byte colPins[25] = {
  22, 23, 24, 25, 26,
  27, 28, 29, 30, 31,
  32, 33, 34, 35, 36,
  37, 38, 39, 40, 41,
  42, 43, 44, 45, 46
};

const byte layerPins[2] = {47, 48};

// ---- Adjust these if needed ----
const byte COL_ON    = HIGH;
const byte COL_OFF   = LOW;
const byte LAYER_ON  = HIGH;
const byte LAYER_OFF = LOW;
// -------------------------------

bool cube[2][5][5];

byte colIndex(byte x, byte y) {
  return y * 5 + x;
}

void setColumnPin(byte x, byte y, bool on) {
  digitalWrite(colPins[colIndex(x, y)], on ? COL_ON : COL_OFF);
}

void setLayerPin(byte z, bool on) {
  digitalWrite(layerPins[z], on ? LAYER_ON : LAYER_OFF);
}

void allLayersOff() {
  for (byte z = 0; z < 2; z++) {
    setLayerPin(z, false);
  }
}

void allColumnsOff() {
  for (byte i = 0; i < 25; i++) {
    digitalWrite(colPins[i], COL_OFF);
  }
}

void clearCube() {
  for (byte z = 0; z < 2; z++) {
    for (byte y = 0; y < 5; y++) {
      for (byte x = 0; x < 5; x++) {
        cube[z][y][x] = false;
      }
    }
  }
}

void setVoxel(byte x, byte y, byte z, bool state) {
  if (x < 5 && y < 5 && z < 2) {
    cube[z][y][x] = state;
  }
}

void refreshCube(unsigned long ms) {
  unsigned long start = millis();

  while (millis() - start < ms) {
    for (byte z = 0; z < 2; z++) {
      allLayersOff();

      for (byte y = 0; y < 5; y++) {
        for (byte x = 0; x < 5; x++) {
          setColumnPin(x, y, cube[z][y][x]);
        }
      }

      setLayerPin(z, true);
      delayMicroseconds(1500);
      setLayerPin(z, false);
    }
  }

  allLayersOff();
  allColumnsOff();
}

void showDot(byte x, byte y, byte z, unsigned long t) {
  clearCube();
  setVoxel(x, y, z, true);
  refreshCube(t);
}

void showRow(byte y, byte z, unsigned long t) {
  clearCube();
  for (byte x = 0; x < 5; x++) {
    setVoxel(x, y, z, true);
  }
  refreshCube(t);
}

void showColumn(byte x, byte z, unsigned long t) {
  clearCube();
  for (byte y = 0; y < 5; y++) {
    setVoxel(x, y, z, true);
  }
  refreshCube(t);
}

void showMainDiagonal(byte z, unsigned long t) {
  clearCube();
  for (byte i = 0; i < 5; i++) {
    setVoxel(i, i, z, true);
  }
  refreshCube(t);
}

void showAntiDiagonal(byte z, unsigned long t) {
  clearCube();
  for (byte i = 0; i < 5; i++) {
    setVoxel(4 - i, i, z, true);
  }
  refreshCube(t);
}

void patternDotScan() {
  for (byte z = 0; z < 2; z++) {
    for (byte y = 0; y < 5; y++) {
      for (byte x = 0; x < 5; x++) {
        showDot(x, y, z, 180);   // was 70
      }
    }
  }
}

void patternRows() {
  for (byte z = 0; z < 2; z++) {
    for (byte y = 0; y < 5; y++) {
      showRow(y, z, 300);        // was 120
    }
  }
}

void patternColumns() {
  for (byte z = 0; z < 2; z++) {
    for (byte x = 0; x < 5; x++) {
      showColumn(x, z, 300);     // was 120
    }
  }
}

void patternDiagonals() {
  for (byte z = 0; z < 2; z++) {
    showMainDiagonal(z, 400);    // was 180
    showAntiDiagonal(z, 400);    // was 180
  }
}

void patternRain() {
  for (byte n = 0; n < 20; n++) {
    byte x = random(0, 5);
    byte y = random(0, 5);

    clearCube();
    setVoxel(x, y, 1, true);   // top layer
    refreshCube(220);          // was 90

    clearCube();
    setVoxel(x, y, 0, true);   // bottom layer
    refreshCube(220);          // was 90
  }
}

void patternSnake() {
  clearCube();

  // bottom layer zig-zag
  for (byte y = 0; y < 5; y++) {
    if (y % 2 == 0) {
      for (byte x = 0; x < 5; x++) {
        showDot(x, y, 0, 180);   // was 60
      }
    } else {
      for (int x = 4; x >= 0; x--) {
        showDot(x, y, 0, 180);   // was 60
      }
    }
  }

  // top layer zig-zag
  for (byte y = 0; y < 5; y++) {
    if (y % 2 == 0) {
      for (byte x = 0; x < 5; x++) {
        showDot(x, y, 1, 180);   // was 60
      }
    } else {
      for (int x = 4; x >= 0; x--) {
        showDot(x, y, 1, 180);   // was 60
      }
    }
  }
}

void setup() {
  for (byte i = 0; i < 25; i++) {
    pinMode(colPins[i], OUTPUT);
    digitalWrite(colPins[i], COL_OFF);
  }

  for (byte z = 0; z < 2; z++) {
    pinMode(layerPins[z], OUTPUT);
    digitalWrite(layerPins[z], LAYER_OFF);
  }

  randomSeed(analogRead(A0));
  clearCube();
}

void loop() {
  patternDotScan();
  patternRows();
  patternColumns();
  patternDiagonals();
  patternRain();
  patternSnake();
}
