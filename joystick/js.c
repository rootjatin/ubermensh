/*
  ESP32 + PS5 DualSense over Bluetooth
  Reads left/right stick, triggers, and a few buttons.

  Library needed:
    Bluepad32 by Ricardo Quesada

  Board:
    ESP32
*/

#include <Bluepad32.h>

ControllerPtr myControllers[BP32_MAX_GAMEPADS];

void onConnectedController(ControllerPtr ctl) {
  bool stored = false;

  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] == nullptr) {
      myControllers[i] = ctl;
      stored = true;
      Serial.printf("Controller connected at index %d\n", i);
      break;
    }
  }

  if (!stored) {
    Serial.println("Controller connected, but no free slot available.");
  }
}

void onDisconnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] == ctl) {
      myControllers[i] = nullptr;
      Serial.printf("Controller disconnected from index %d\n", i);
      break;
    }
  }
}

void processGamepad(ControllerPtr ctl) {
  // Sticks usually range roughly from -512 to +512
  int lx = ctl->axisX();   // Left stick X
  int ly = ctl->axisY();   // Left stick Y
  int rx = ctl->axisRX();  // Right stick X
  int ry = ctl->axisRY();  // Right stick Y

  // Triggers
  int l2 = ctl->brake();
  int r2 = ctl->throttle();

  // Buttons
  bool cross    = ctl->a();
  bool circle   = ctl->b();
  bool square   = ctl->x();
  bool triangle = ctl->y();

  bool l1 = ctl->l1();
  bool r1 = ctl->r1();

  Serial.printf(
    "LX:%4d  LY:%4d  RX:%4d  RY:%4d  L2:%4d  R2:%4d  X:%d O:%d Sq:%d Tr:%d L1:%d R1:%d\n",
    lx, ly, rx, ry, l2, r2,
    cross, circle, square, triangle, l1, r1
  );

  // Example: control onboard LED from Cross button
  digitalWrite(2, cross ? HIGH : LOW);
}

void processControllers() {
  for (auto ctl : myControllers) {
    if (ctl && ctl->isConnected() && ctl->hasData()) {
      processGamepad(ctl);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);

  BP32.setup(&onConnectedController, &onDisconnectedController);

  // Optional: clears old Bluetooth keys/pairings on some setups
  // BP32.forgetBluetoothKeys();

  Serial.println("Bluepad32 ready.");
  Serial.println("Put the PS5 controller in pairing mode, then connect.");
}

void loop() {
  BP32.update();
  processControllers();
  delay(20);
}
