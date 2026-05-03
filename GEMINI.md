# Project: Sensor Control Box

## System Overview
- **MCU:** Arduino Uno R4 WiFi
- **Primary Sensor:** CD22 Displacement Sensor (RS485)
- **Interface:** 16x2 I2C LCD, Grayhill 60A Joystick, ITW W48M Switch.

## Architectural Decisions
- **Environment:** VS Code with PlatformIO.
- **Serial Communication:** Hardware Serial (`Serial1`) is dedicated to the RS485 Shield for high-speed sensor data (230400 Baud).
- **Libraries:** All libraries are managed via `platformio.ini` using direct GitHub or Registry links to ensure portability.
- **Stack Monitoring:** `FreeStack.h` is used to monitor memory usage on the RA4M1 chip.

## Library Dependencies
- `https://github.com/vishwam-aggarwal/CD22.git`
- `jrullan/StateMachine @ ^1.0.11`
- `ivanseidel/LinkedList @ 0.0.0-alpha+sha.dac3874d28`
- `greiman/SdFat @ ^2.3.1`
- `marcoschwartz/LiquidCrystal_I2C @ ^1.1.4`

## Maintenance
- **Wiring Guide:** Refer to `resources/wiring_guide.html` for current hardware mapping.
- **Git:** Use the root `.gitignore` to prevent committing build artifacts.
- **Workflow:** Do NOT perform git commits or pushes yourself. Always ask the user to perform these actions through VS Code.
