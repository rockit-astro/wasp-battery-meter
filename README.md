# wasp-battery-meter
Firmware for an Arduino board that monitors the voltage of the SuperWASP roof battery.

The unit communicates with a [Voltmeter Click](https://www.mikroe.com/voltmeter-click) board via SPI
and reports the voltage back to the PC every second.  The report format is `%+06.2f\r\n`.

See the figures in the `docs` directory for more information.

### Compilation/Installation

Requires a working `avr-gcc` installation.
* On macOS add the `osx-cross/avr` Homebrew tap then install the `avr-gcc`, `avr-libc`, `avr-binutils`, `avrdude` packages.
* On Ubuntu install the `gcc-avr`, `avr-libc`, `binutils-avr`, `avrdude` packages.
* On Windows WinAVR should work.

Compile using `make`. Press the reset button to put the board into its update mode (the LED should fade in and out) then run `make install` within 8 seconds to install the firmware.