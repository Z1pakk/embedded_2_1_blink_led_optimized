# Blink with Button Modes (ESP32-S3)

Homework 2.1: two LEDs controlled by a non-blocking state machine, with a
button (handled by an interrupt) that switches between LED modes.

## Hardware

| Part       | GPIO | Notes                                              |
|------------|------|----------------------------------------------------|
| Red LED    | 15   | Through a current-limiting resistor to GND         |
| Blue LED   | 16   | Through a current-limiting resistor to GND         |
| Button     | 0    | BOOT button on the DevKitC-1 (active LOW, internal pull-up) |

Board: `esp32-s3-devkitc-1` (PlatformIO, Arduino framework).

## Behaviour

Each button press switches to the next mode:

```
Blink  →  Always on  →  Always off  →  Blink ...
```

- **Blink**: red and blue alternate every 1000 ms.
- **Always on**: both LEDs on.
- **Always off**: both LEDs off.

The program also prints the average duration of one `loop()` iteration
(averaged over 10 000 iterations) to Serial at 115200 baud.

## Design

Everything is in `src/main.cpp`, split into small structs:

- **`Led`**: owns one output pin, with `setState()` and `toggle()`.
- **`Button`**: the ISR only sets a `volatile bool` flag. Debouncing (50 ms)
  happens in `wasPressed()`, which is called from `loop()`.
- **`PeriodicTimer`**: a non-blocking `millis()` timer. `tick()` returns `true`
  once per period.
- **`LedMode`**: a state machine with the states `BlinkRed`, `BlinkBlue`,
  `AlwaysOn` and `AlwaysOff`, and the events `Press` and `Tick`.

```
            Press                 Press                 Press
  BlinkRed ───────► AlwaysOn ───────► AlwaysOff ───────► BlinkRed
   │    ▲
  Tick  Tick          (Press from BlinkBlue also → AlwaysOn)
   ▼    │
  BlinkBlue
```

`loop()` never blocks: it reads the button, feeds events to the state machine
and measures its own timing.

## Build & upload

```sh
pio run                 # build
pio run -t upload       # flash
pio device monitor -b 115200
```
