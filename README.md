# robotic-arm

Firmware for **CARDBIRD**, a cardboard robotic arm driven by an ESP32. The ESP32 joins your WiFi and serves
a control page, so you can move each joint from a phone or laptop browser on the same network
(sliders and step buttons, no page reloads).

## Hardware

- Any ESP32 dev board (PlatformIO board `esp32dev`, e.g. "ESP32 Dev Module" / WROOM-32)
- 4 hobby servos. In the current code one is positional (MG995) and three are continuous-rotation:

| Joint | ESP32 GPIO | Type in code | Notes |
|---|---|---|---|
| Base | 18 | continuous rotation | |
| Shoulder | 14 | positional (MG995) | 500 to 2500 µs pulse, starts at 90° |
| Elbow | 19 | continuous rotation | |
| Gripper | 26 | continuous rotation | |

- A separate **5 V supply for the servos** (5 V / 2 A or more, or 4×AA)

All of this lives in the `USER CONFIG` block at the top of `src/main.cpp`: `servoPin[]`, `servoName[]`,
`type[]` (`CR` or `POS` per joint), angle limits, `stopUs[]` (trim a CR servo that creeps) and `dir[]`
(flip a joint that turns the wrong way).

### Wiring

- Each servo **signal** wire goes to its GPIO from the table.
- Servo **power (+5 V)** goes to the external supply, **not** the ESP32's 3.3 V pin or USB.
- **Ground:** connect the external supply's GND to the ESP32 GND (common ground).

## Run it

Prereqs: Python 3 and [PlatformIO Core](https://platformio.org/install/cli) (or the PlatformIO extension in
VS Code). PlatformIO downloads the ESP32 toolchain and the `ESP32Servo` library (`^3.0.5`) on the first build.

1. Edit `WIFI_SSID` and `WIFI_PASS` near the top of `src/main.cpp` to match your 2.4 GHz network.
2. Build, flash and open the serial monitor:

   ```bash
   pip install platformio
   pio run                    # compile
   pio run -t upload          # flash the ESP32 over USB
   pio device monitor -b 115200
   ```

3. The monitor prints `Ready!  Open  http://<ip>`. Open that address in a browser on the same WiFi.

`pio run` was tested here (PlatformIO 6.2.0, espressif32 7.1.3, ESP32Servo 3.2.1: builds cleanly, about
14% RAM and 60% flash). Uploading and driving real servos was not tested here because it needs the board.

The Arduino IDE also works: install the ESP32 board package and the "ESP32Servo" library, then open
`src/main.cpp` as a sketch.

## Using the control page

- **Positional joints** (tagged HOLDS): drag the slider to set an angle. The joint holds it.
- **Continuous-rotation joints** (tagged SPIN): hold the slider left or right to spin at that speed. It
  springs back to 0 (stop) when released. The ◀ / ▶ buttons nudge the joint for a short burst.
- **Step size** sets the nudge duration (40 to 500 ms).
- **STOP ALL** stops every continuous-rotation joint. Positional joints keep holding.

The page calls these HTTP endpoints, which you can also hit directly:

| Endpoint | Effect |
|---|---|
| `/set?s=<joint>&v=<value>` | positional joint: angle in degrees; CR joint: speed −100 to 100 |
| `/nudge?s=<joint>&d=<1 or -1>&t=<ms>` | spin a CR joint briefly (20 to 800 ms) |
| `/stop` | stop all CR joints |

Joint numbers are 0 to 3 in the order Base, Shoulder, Elbow, Gripper.

## Project structure

```
platformio.ini   board, framework and library config
src/main.cpp     WiFi setup, web server, page HTML/JS, servo control
```

## Notes

- `setup()` waits until WiFi connects. If the serial monitor keeps printing dots, check the SSID and password.
- The web server has no login, so anyone on the same network can move the arm.
