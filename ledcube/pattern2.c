/*
  5x5x2 LED Cuboid for Arduino Mega
  25 columns + 2 layers

  Column pins: 22..46
  Layer pins : 47, 48

  If your cube lights in reverse, flip:
  COL_ON / COL_OFF
  LAYER_ON / LAYER_OFF
*/

const byte colPins[25] = {
  22, 23, 24, 25, 26,
  27, 28, 29, 30, 31,
  32, 33, 34, 35, 36,
  37, 38, 39, 40, 41,
  42, 43, 44, 45, 46
};

const byte layerPins[2] = {47, 48};

// ---- Adjust polarity if needed ----
const byte COL_ON    = HIGH;
const byte COL_OFF   = LOW;
const byte LAYER_ON  = HIGH;
const byte LAYER_OFF = LOW;
// -----------------------------------

bool cube[2][5][5];

// slower timing values
const unsigned long T_DOT   = 180;
const unsigned long T_SHORT = 250;
const unsigned long T_MED   = 350;
const unsigned long T_LONG  = 500;
const unsigned long T_XLONG = 700;

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

void fillCube() {
  for (byte z = 0; z < 2; z++) {
    for (byte y = 0; y < 5; y++) {
      for (byte x = 0; x < 5; x++) {
        cube[z][y][x] = true;
      }
    }
  }
}

void fillLayer(byte z) {
  if (z > 1) return;
  for (byte y = 0; y < 5; y++) {
    for (byte x = 0; x < 5; x++) {
      cube[z][y][x] = true;
    }
  }
}

void clearLayer(byte z) {
  if (z > 1) return;
  for (byte y = 0; y < 5; y++) {
    for (byte x = 0; x < 5; x++) {
      cube[z][y][x] = false;
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
      delayMicroseconds(1600);
      setLayerPin(z, false);
    }
  }

  allLayersOff();
  allColumnsOff();
}

void hold(unsigned long t) {
  refreshCube(t);
}

void showDot(byte x, byte y, byte z, unsigned long t) {
  clearCube();
  setVoxel(x, y, z, true);
  hold(t);
}

void showRow(byte y, byte z, unsigned long t) {
  clearCube();
  for (byte x = 0; x < 5; x++) {
    setVoxel(x, y, z, true);
  }
  hold(t);
}

void showColumn(byte x, byte z, unsigned long t) {
  clearCube();
  for (byte y = 0; y < 5; y++) {
    setVoxel(x, y, z, true);
  }
  hold(t);
}

void showMainDiagonal(byte z, unsigned long t) {
  clearCube();
  for (byte i = 0; i < 5; i++) {
    setVoxel(i, i, z, true);
  }
  hold(t);
}

void showAntiDiagonal(byte z, unsigned long t) {
  clearCube();
  for (byte i = 0; i < 5; i++) {
    setVoxel(4 - i, i, z, true);
  }
  hold(t);
}

void showPlus(byte z, unsigned long t) {
  clearCube();
  for (byte i = 0; i < 5; i++) {
    setVoxel(2, i, z, true);
    setVoxel(i, 2, z, true);
  }
  hold(t);
}

void showBorder(byte z, unsigned long t) {
  clearCube();
  for (byte i = 0; i < 5; i++) {
    setVoxel(i, 0, z, true);
    setVoxel(i, 4, z, true);
    setVoxel(0, i, z, true);
    setVoxel(4, i, z, true);
  }
  hold(t);
}

void showCorners(byte z, unsigned long t) {
  clearCube();
  setVoxel(0, 0, z, true);
  setVoxel(4, 0, z, true);
  setVoxel(0, 4, z, true);
  setVoxel(4, 4, z, true);
  hold(t);
}

// ---------------- PATTERNS ----------------

void patternDotScan() {
  for (byte z = 0; z < 2; z++) {
    for (byte y = 0; y < 5; y++) {
      for (byte x = 0; x < 5; x++) {
        showDot(x, y, z, T_DOT);
      }
    }
  }
}

void patternRows() {
  for (byte z = 0; z < 2; z++) {
    for (byte y = 0; y < 5; y++) {
      showRow(y, z, T_MED);
    }
  }
}

void patternColumns() {
  for (byte z = 0; z < 2; z++) {
    for (byte x = 0; x < 5; x++) {
      showColumn(x, z, T_MED);
    }
  }
}

void patternDiagonals() {
  for (byte z = 0; z < 2; z++) {
    showMainDiagonal(z, T_LONG);
    showAntiDiagonal(z, T_LONG);
  }
}

void patternRain() {
  for (byte n = 0; n < 18; n++) {
    byte x = random(0, 5);
    byte y = random(0, 5);

    clearCube();
    setVoxel(x, y, 1, true);
    hold(T_SHORT);

    clearCube();
    setVoxel(x, y, 0, true);
    hold(T_SHORT);
  }
}

void patternSnake() {
  for (byte y = 0; y < 5; y++) {
    if (y % 2 == 0) {
      for (byte x = 0; x < 5; x++) {
        showDot(x, y, 0, T_DOT);
      }
    } else {
      for (int x = 4; x >= 0; x--) {
        showDot((byte)x, y, 0, T_DOT);
      }
    }
  }

  for (byte y = 0; y < 5; y++) {
    if (y % 2 == 0) {
      for (byte x = 0; x < 5; x++) {
        showDot(x, y, 1, T_DOT);
      }
    } else {
      for (int x = 4; x >= 0; x--) {
        showDot((byte)x, y, 1, T_DOT);
      }
    }
  }
}

void patternLayerBlink() {
  for (byte k = 0; k < 3; k++) {
    clearCube();
    fillLayer(0);
    hold(T_MED);

    clearCube();
    fillLayer(1);
    hold(T_MED);

    fillCube();
    hold(T_MED);

    clearCube();
    hold(T_SHORT);
  }
}

void patternCheckerSwap() {
  for (byte step = 0; step < 4; step++) {
    clearCube();

    for (byte z = 0; z < 2; z++) {
      for (byte y = 0; y < 5; y++) {
        for (byte x = 0; x < 5; x++) {
          bool on = ((x + y + z + step) % 2 == 0);
          setVoxel(x, y, z, on);
        }
      }
    }

    hold(T_MED);
  }
}

void patternEdgeSpin() {
  const byte path[16][2] = {
    {0,0}, {1,0}, {2,0}, {3,0}, {4,0},
    {4,1}, {4,2}, {4,3}, {4,4},
    {3,4}, {2,4}, {1,4}, {0,4},
    {0,3}, {0,2}, {0,1}
  };

  for (byte z = 0; z < 2; z++) {
    for (byte i = 0; i < 16; i++) {
      clearCube();
      setVoxel(path[i][0], path[i][1], z, true);
      hold(T_DOT);
    }
  }

  for (byte i = 0; i < 16; i++) {
    clearCube();
    setVoxel(path[i][0], path[i][1], 0, true);
    setVoxel(path[i][0], path[i][1], 1, true);
    hold(T_DOT);
  }
}

void patternCenterPulse() {
  clearCube();
  setVoxel(2, 2, 0, true);
  setVoxel(2, 2, 1, true);
  hold(T_MED);

  clearCube();
  for (byte z = 0; z < 2; z++) {
    setVoxel(2, 2, z, true);
    setVoxel(2, 1, z, true);
    setVoxel(2, 3, z, true);
    setVoxel(1, 2, z, true);
    setVoxel(3, 2, z, true);
  }
  hold(T_MED);

  clearCube();
  for (byte z = 0; z < 2; z++) {
    for (byte i = 0; i < 5; i++) {
      setVoxel(2, i, z, true);
      setVoxel(i, 2, z, true);
    }
  }
  hold(T_LONG);

  fillCube();
  hold(T_LONG);

  clearCube();
  for (byte z = 0; z < 2; z++) {
    for (byte i = 0; i < 5; i++) {
      setVoxel(2, i, z, true);
      setVoxel(i, 2, z, true);
    }
  }
  hold(T_MED);

  clearCube();
  setVoxel(2, 2, 0, true);
  setVoxel(2, 2, 1, true);
  hold(T_MED);
}

void patternWallSweepX() {
  for (byte x = 0; x < 5; x++) {
    clearCube();
    for (byte z = 0; z < 2; z++) {
      for (byte y = 0; y < 5; y++) {
        setVoxel(x, y, z, true);
      }
    }
    hold(T_MED);
  }

  for (int x = 4; x >= 0; x--) {
    clearCube();
    for (byte z = 0; z < 2; z++) {
      for (byte y = 0; y < 5; y++) {
        setVoxel((byte)x, y, z, true);
      }
    }
    hold(T_MED);
  }
}

void patternWallSweepY() {
  for (byte y = 0; y < 5; y++) {
    clearCube();
    for (byte z = 0; z < 2; z++) {
      for (byte x = 0; x < 5; x++) {
        setVoxel(x, y, z, true);
      }
    }
    hold(T_MED);
  }

  for (int y = 4; y >= 0; y--) {
    clearCube();
    for (byte z = 0; z < 2; z++) {
      for (byte x = 0; x < 5; x++) {
        setVoxel(x, (byte)y, z, true);
      }
    }
    hold(T_MED);
  }
}

void patternRandomFill() {
  clearCube();

  for (byte n = 0; n < 35; n++) {
    byte x = random(0, 5);
    byte y = random(0, 5);
    byte z = random(0, 2);
    setVoxel(x, y, z, true);
    hold(120);
  }

  for (byte n = 0; n < 35; n++) {
    byte x = random(0, 5);
    byte y = random(0, 5);
    byte z = random(0, 2);
    setVoxel(x, y, z, false);
    hold(120);
  }

  clearCube();
  hold(T_SHORT);
}

void patternSparkle() {
  for (byte n = 0; n < 20; n++) {
    clearCube();
    for (byte i = 0; i < 6; i++) {
      setVoxel(random(0, 5), random(0, 5), random(0, 2), true);
    }
    hold(T_SHORT);
  }
}

void patternPlusSweep() {
  showPlus(0, T_LONG);
  showPlus(1, T_LONG);

  clearCube();
  for (byte i = 0; i < 5; i++) {
    setVoxel(2, i, 0, true);
    setVoxel(i, 2, 1, true);
  }
  hold(T_LONG);

  clearCube();
  for (byte i = 0; i < 5; i++) {
    setVoxel(i, 2, 0, true);
    setVoxel(2, i, 1, true);
  }
  hold(T_LONG);
}

void patternCornerHop() {
  const byte corners[4][2] = {
    {0,0}, {4,0}, {4,4}, {0,4}
  };

  for (byte z = 0; z < 2; z++) {
    for (byte i = 0; i < 4; i++) {
      showDot(corners[i][0], corners[i][1], z, T_LONG);
    }
  }

  for (byte i = 0; i < 4; i++) {
    clearCube();
    setVoxel(corners[i][0], corners[i][1], 0, true);
    setVoxel(corners[i][0], corners[i][1], 1, true);
    hold(T_LONG);
  }
}

void patternBorderFlash() {
  for (byte k = 0; k < 3; k++) {
    clearCube();
    for (byte z = 0; z < 2; z++) {
      for (byte i = 0; i < 5; i++) {
        setVoxel(i, 0, z, true);
        setVoxel(i, 4, z, true);
        setVoxel(0, i, z, true);
        setVoxel(4, i, z, true);
      }
    }
    hold(T_MED);

    clearCube();
    for (byte z = 0; z < 2; z++) {
      for (byte i = 0; i < 5; i++) {
        setVoxel(2, i, z, true);
        setVoxel(i, 2, z, true);
      }
    }
    hold(T_MED);
  }
}

void patternLayerAlternate() {
  for (byte i = 0; i < 4; i++) {
    clearCube();
    fillLayer(0);
    hold(T_LONG);

    clearCube();
    fillLayer(1);
    hold(T_LONG);
  }
}

void patternExpandCollapse() {
  clearCube();
  setVoxel(2, 2, 0, true);
  setVoxel(2, 2, 1, true);
  hold(T_MED);

  clearCube();
  for (byte z = 0; z < 2; z++) {
    setVoxel(2, 2, z, true);
    setVoxel(2, 1, z, true);
    setVoxel(2, 3, z, true);
    setVoxel(1, 2, z, true);
    setVoxel(3, 2, z, true);
  }
  hold(T_MED);

  clearCube();
  for (byte z = 0; z < 2; z++) {
    for (byte i = 0; i < 5; i++) {
      setVoxel(i, 0, z, true);
      setVoxel(i, 4, z, true);
      setVoxel(0, i, z, true);
      setVoxel(4, i, z, true);
    }
  }
  hold(T_LONG);

  fillCube();
  hold(T_LONG);

  clearCube();
  for (byte z = 0; z < 2; z++) {
    for (byte i = 0; i < 5; i++) {
      setVoxel(i, 0, z, true);
      setVoxel(i, 4, z, true);
      setVoxel(0, i, z, true);
      setVoxel(4, i, z, true);
    }
  }
  hold(T_LONG);

  clearCube();
  setVoxel(2, 2, 0, true);
  setVoxel(2, 2, 1, true);
  hold(T_MED);
}

void patternCrossMove() {
  for (byte x = 0; x < 5; x++) {
    clearCube();
    for (byte z = 0; z < 2; z++) {
      for (byte y = 0; y < 5; y++) setVoxel(x, y, z, true);
      for (byte i = 0; i < 5; i++) setVoxel(i, 2, z, true);
    }
    hold(T_MED);
  }

  for (byte y = 0; y < 5; y++) {
    clearCube();
    for (byte z = 0; z < 2; z++) {
      for (byte x = 0; x < 5; x++) setVoxel(x, y, z, true);
      for (byte i = 0; i < 5; i++) setVoxel(2, i, z, true);
    }
    hold(T_MED);
  }
}

// ---------------- SETUP / LOOP ----------------

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

  patternLayerBlink();
  patternCheckerSwap();
  patternEdgeSpin();
  patternCenterPulse();
  patternWallSweepX();
  patternWallSweepY();
  patternRandomFill();
  patternSparkle();
  patternPlusSweep();
  patternCornerHop();
  patternBorderFlash();
  patternLayerAlternate();
  patternExpandCollapse();
  patternCrossMove();
}
