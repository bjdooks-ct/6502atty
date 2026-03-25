# 6502 Atty

A 6502 SBC with an ATMega as a contoller and monitor for the 6502.

This is an initial commit as PCBs are off for manufacture and not been tested. This includes the prototype firmware and the first cut of the firmware code.

![3D PCB view](docs/6502atty_3d2.png)

The ATmega is probably the easiest MCU to get and program which is 5V tolerant. The 1284P is the current choice as the smaller memory versions are not much cheaper.

There is now a [photo album](https://photos.app.goo.gl/qwDDgCh3WiyMoqd4A) of the build on my google photos.

The clock synchronisation logic is provided by U2A and U2B. U2B takes the PWM output from the ATMega and is used to both halve ti and make it a square wave. The /S input is used when the clock is high to stop any transition low (which would complete the 6502's cycle) which comes from the U2A flip-flop. U2A is triggered by an access to the ATMega and sets /CLOCK_STOP active to stop the 6502 clock until the /AT_ACK signal is active, resetting U2A and making /CLOCK_STOP inactive which then allows U2B to continue generating a clock for the 6502.

## Build notes for v1 rev1

- The 100UF capacitors are 16mm long so trying 47uF, layout change?

The LED resistors will probably need to be reduced to 680R or low as 506R depending on the LEDs being fitted.

The pull-up/pull-down resistors can be anywhere from 1K to 4K7, experiments with previous boards show 10K may not be suitable with certain variants of the 6052. Anything below 1K is probably going to have issues with current dissapation as that will be around 5mA per resistor. 

During test it was found that there seems to be a difference in operation dependong on how the PHi2 jumper is selected. If the PHI2O is used, we get proper operation but if CPU_CLK is seleted then the download code works but the code that is downloaded does not work.

## PCB updates (v1 rev2)

The updates for the second PCB revision are mostly bug-fixes and to make the board smaller to try and get to the 10x10cm point that JLCPCB and others use for cheaper services. There may be in future a version with a few bits of surface mount, like moving to one of the USB capable Atmel devices and adding a direct USB port.

- Added J12 to route ~WE signal to the PGM/~WE to allow SRAM
  - Also requires a R/!W signal gated with PHI2 high
  - Added R~{W}ph signal for ROM
  - This new signal could also drive RAM ~WE and connect ~CS to A15
- Connected ROM's VPP signal to GND, if using SRAM, ties A14 to GND
- U9C's pins 9 and 10 swapped for routing
- U11's E1/E2 pins swapped for easier routing
- Transcription error from prototype, ROM_ACC should be from AND not NAND gate
  - Swao U8C for one of U10 spare gates
- RDY line missing pull-up resistor
- 74LS573 replaced with 74LS574
  - 573 latches when Load is high, and our enable is low
  - 574 will load on rising edge of load pin, so should work
- Made PCB smaller, moved a few LEDs and things around to make this work
- Dropped 1K series drive resistor for the crystal
  - ATMega will not start if this resistor is there
- Serial converter is rotated 180deg to the actual unit
- Recomendation for AVRisp is to use 10K on ATMega's nRESET, so changed for now
  - See crystal series resistor notes.
- Added silkscreen with major part info to help with production

Currently unknown issues:
- Firmware only works with PHI2 jumper on PHI2out
  - can't see any scoped difference between CPUclk and PHI2out



# Firmware

The firmware is currently in development. To build, you will need at-least avr-gcc and the relevant libraries and is currently only being tested on Linux (Debian). Once built, download to the ATMega using something like avrdude.

## Revision information

The original prototype version was built on a perfboard, and not released.

