# NES-Controller-To-USB-GamePad
Atmel Attiny85 base circuit to read NES controller shift register and present as a USB Gamepad.
NES-to-USB Gamepad (ATtiny45/85)
This project allows you to interface a classic NES Controller with a modern computer via USB. It uses an ATtiny85 (or ATtiny45) microcontroller and the V-USB software library to emulate a standard USB HID Gamepad. No special drivers are required; it is recognized natively by Windows, macOS, and Linux.
________________________________________
Features
Plug-and-Play: Functions as a standard HID Gamepad.
V-USB Powered: Implements USB 1.1 low-speed protocol in software.
Low Component Count: Minimal external parts required.
Internal RC Oscillator: Configured to run at 16.5 MHz using the internal PLL, eliminating the need for an external crystal.
________________________________________
Hardware Configuration
Pin Assignment
| ATtiny45/85 Pin | Function | NES Controller Wire |
| :--- | :--- | :--- |
| **PB0 (Pin 5)** | USB D- | — |
| **PB1 (Pin 6)** | NES CLOCK | Clock (Pulse) |
| **PB2 (Pin 7)** | USB D+ | — |
| **PB3 (Pin 2)** | NES LATCH | Latch (Strobe) |
| **PB4 (Pin 3)** | NES DATA | Data (Serial Out) |
________________________________________
Controller Mapping
The firmware reads the 8-bit shift register from the NES controller and maps them to USB HID report bits:
D-Pad: Mapped to X/Y axes (Left/Right = X, Up/Down = Y).
Buttons: A, B, Select, and Start are mapped as Buttons 1–4.
________________________________________
Technical Details
Clock & Calibration
The project is configured for a CPU frequency of 16.5 MHz. Because the internal RC oscillator can drift, the firmware includes an automatic calibration routine (calibrateOscillator) that runs upon USB reset to ensure timing stays within USB specifications. The calibration value is stored in and read from EEPROM. 
Fuse Settings
To run at the correct speed and stability, the following fuses must be set: 
Low Fuse (0xe1): HF PLL clock source, no clock division. 
High Fuse (0xdd): Brown-out detection at 2.7V, SPI enabled. 
________________________________________
Installation & Compiling
Prerequisites
avr-gcc compiler
avrdude (for flashing)
A programmer (e.g., USBasp)
Build Instructions
<br>
Compile the code:
```bash
make all
```
Set the Fuses (Crucial for USB timing):
```bash
make fuse
```
Flash the firmware:
```bash
make flash
```
________________________________________
Important Notes
WARNING
Ensure you have 3.6V Zener diodes on the D+ and D- lines if you are powering the ATtiny from 5V, as USB data lines operate at 3.3V.
