
void patternLayerBlink() {
  for (byte k = 0; k < 3; k++) {
    clearCube();
    for (byte y = 0; y < 5; y++) {
      for (byte x = 0; x < 5; x++) {
        setVoxel(x, y, 0, true);   // bottom layer
      }
    }
    refreshCube(350);

    clearCube();
    for (byte y = 0; y < 5; y++) {
      for (byte x = 0; x < 5; x++) {
        setVoxel(x, y, 1, true);   // top layer
      }
    }
    refreshCube(350);

    clearCube();
    for (byte z = 0; z < 2; z++) {
      for (byte y = 0; y < 5; y++) {
        for (byte x = 0; x < 5; x++) {
          setVoxel(x, y, z, true); // both layers
        }
      }
    }
    refreshCube(350);

    clearCube();
    refreshCube(180);
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

    refreshCube(320);
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
      refreshCube(160);
    }
  }

  // both layers together
  for (byte i = 0; i < 16; i++) {
    clearCube();
    setVoxel(path[i][0], path[i][1], 0, true);
    setVoxel(path[i][0], path[i][1], 1, true);
    refreshCube(160);
  }
}

void patternCenterPulse() {
  // center dot
  clearCube();
  setVoxel(2, 2, 0, true);
  setVoxel(2, 2, 1, true);
  refreshCube(250);

  // small plus
  clearCube();
  for (byte z = 0; z < 2; z++) {
    setVoxel(2, 2, z, true);
    setVoxel(2, 1, z, true);
    setVoxel(2, 3, z, true);
    setVoxel(1, 2, z, true);
    setVoxel(3, 2, z, true);
  }
  refreshCube(250);

  // larger plus
  clearCube();
  for (byte z = 0; z < 2; z++) {
    for (byte i = 0; i < 5; i++) {
      setVoxel(2, i, z, true);
      setVoxel(i, 2, z, true);
    }
  }
  refreshCube(300);

  // full cube
  clearCube();
  for (byte z = 0; z < 2; z++) {
    for (byte y = 0; y < 5; y++) {
      for (byte x = 0; x < 5; x++) {
        setVoxel(x, y, z, true);
      }
    }
  }
  refreshCube(300);

  // back inward
  clearCube();
  for (byte z = 0; z < 2; z++) {
    for (byte i = 0; i < 5; i++) {
      setVoxel(2, i, z, true);
      setVoxel(i, 2, z, true);
    }
  }
  refreshCube(250);

  clearCube();
  for (byte z = 0; z < 2; z++) {
    setVoxel(2, 2, z, true);
    setVoxel(2, 1, z, true);
    setVoxel(2, 3, z, true);
    setVoxel(1, 2, z, true);
    setVoxel(3, 2, z, true);
  }
  refreshCube(250);

  clearCube();
  setVoxel(2, 2, 0, true);
  setVoxel(2, 2, 1, true);
  refreshCube(220);
}

void patternWallSweep() {
  // sweep x walls
  for (byte x = 0; x < 5; x++) {
    clearCube();
    for (byte z = 0; z < 2; z++) {
      for (byte y = 0; y < 5; y++) {
        setVoxel(x, y, z, true);
      }
    }
    refreshCube(220);
  }

  for (int x = 4; x >= 0; x--) {
    clearCube();
    for (byte z = 0; z < 2; z++) {
      for (byte y = 0; y < 5; y++) {
        setVoxel(x, y, z, true);
      }
    }
    refreshCube(220);
  }

  // sweep y walls
  for (byte y = 0; y < 5; y++) {
    clearCube();
    for (byte z = 0; z < 2; z++) {
      for (byte x = 0; x < 5; x++) {
        setVoxel(x, y, z, true);
      }
    }
    refreshCube(220);
  }

  for (int y = 4; y >= 0; y--) {
    clearCube();
    for (byte z = 0; z < 2; z++) {
      for (byte x = 0; x < 5; x++) {
        setVoxel(x, y, z, true);
      }
    }
    refreshCube(220);
  }
}

void patternRandomFill() {
  clearCube();

  // fill randomly
  for (byte n = 0; n < 25; n++) {
    byte x = random(0, 5);
    byte y = random(0, 5);
    byte z = random(0, 2);
    setVoxel(x, y, z, true);
    refreshCube(120);
  }

  // fill a bit more
  for (byte n = 0; n < 20; n++) {
    byte x = random(0, 5);
    byte y = random(0, 5);
    byte z = random(0, 2);
    setVoxel(x, y, z, true);
    refreshCube(100);
  }

  // clear randomly
  for (byte n = 0; n < 35; n++) {
    byte x = random(0, 5);
    byte y = random(0, 5);
    byte z = random(0, 2);
    setVoxel(x, y, z, false);
    refreshCube(100);
  }

  clearCube();
  refreshCube(150);
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
  patternWallSweep();
  patternRandomFill();
}
