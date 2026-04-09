## Circuit Diagram

```text
            LM358 (top view)

           ┌─────────┐
   OUT1  1 │         │ 8  VCC (+5V)
   IN1-  2 │  LM358  │ 7  OUT2
   IN1+  3 │         │ 6  IN2-
   GND   4 │_________│ 5  IN2+
```

```text
HB100 + LM358 + Arduino Uno
===========================

HB100 VCC  -> Arduino 5V
HB100 GND  -> Arduino GND
HB100 IF   -> 104 capacitor -> LM358 pin 3

LM358 pin 8 -> Arduino 5V
LM358 pin 4 -> Arduino GND
LM358 pin 1 -> Arduino A0
LM358 pin 1 -> LM358 pin 2

Bias divider:
Arduino 5V -> 10k -> midpoint -> 10k -> Arduino GND
midpoint -> LM358 pin 3
```

```text
Block view:

         +--------------------+
         |       HB100        |
         |                    |
5V ------| VCC                |
GND -----| GND                |
         | IF -------------------||-------------------+
         +--------------------+   104 capacitor       |
                                                      |
                                                      v
                                           +-------------------+
                                           |    LM358 op-amp   |
                                           |                   |
                                    +5V -->| pin 8        pin1 |-----> Arduino A0
                                    GND -->| pin 4             |
                                           |                   |
                                           | pin 3 <-----------+---- signal input
                                           | pin 2 <-----------+
                                           +-------------------+
                                                         ^
                                                         |
                                                   pin 1 to pin 2

Bias:
5V ----[10k]----+----[10k]---- GND
                |
                +----------------------> LM358 pin 3
```

## Notes
- The `104` capacitor is `0.1uF` (100nF).
- Use the flat antenna side of HB100 facing the target.
- All grounds must be connected together.
- LM358 pin 1 to pin 2 makes the op-amp work as a buffer.
