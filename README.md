# PZ1_Pico

A version of the PZ1-system based on 65C02, Raspberry Pi Pico and 512KiB SRAM.

## General info

This is the second public release of SW and HW for the PZ1-project, now with some more bells and whistles.<br>
<br>
The system uses my simple preemptive multitasking scheduler to manage the 512KiB RAM, using 16KiB for itself and system data.<br>
The task switching does not waste many cycles. Every task gets its own stack and a full zero page, minus the 18 bytes used for OS-calls.<br>
EhBASIC is running in a separate task with 64KiB RAM. There are added basic-commands to deal with tasks and the filesystem.<br>
<br>
HW realised in the Pico:<br>
- Serial communication via a virtual USB serial port.<br>
- Serial port, 3.3V RX/TX on pin-header. Working but not used.<br>
- SID-sound is based on TinySID, emulated on the second core of the Pico and mixed at 96000Hz, no SID-filters. 10-bit PWM-output on a single pin.<br>
- IRQ-timer used by the scheduler, 16-bits.<br>
- Frame counter registers, incremented @50Hz, 16-bits.<br>
- HW/SW interfaces to filesystem operations.<br>
- 4 bank registers, 16KiB memory blocks, the scheduler handles ownership of blocks.<br>
<br>
The HW design is not the most efficient, mainly because the Pico lacks enough gpios to connect to the 65C02 and SRAM without buffers. The buffering is done with 74LVC-chips which adds complexity and lowers efficiency. The Pico and buffers are cheaper and more readily available than the faster and less complex Teensy 4.1.<br>
<br>
Running the Pico at the standard 133MHz can run the 65C02 @1MHz. Overclocking the Pico to 250MHz can run the 65C02 @2MHz, just the way I intended. Using the PIOs would probably improve the performance.

## History

I started the PZ1 project because I wanted to code 6502, build proper electronics hardware and investigate multitasking and bank-switched memory on a 6502-system. I also got back to designing proper PCBs, now in KiCad. I've done some iterations experimenting with:<br>
- 6502 emulation or a real 65C02.<br>
- Different MCUs: Teensy 4.0/4.1 / BluePill / Pico.<br>
- External SRAM, RAM/ROM inside MCU, RAM on Teensy quad-SPI.<br>
- Different sound emulation and output (PWM/I2S).<br>
- 74-series glue logic / SPLD.<br>

## Blog

The ongoing work of PZ1 in general can be followed on [my blog at hackaday.io](https://hackaday.io/project/171471-pz1-6502-laptop)

## Hardware

Use KiCad 6 to view/edit the schematics and PCB. There is also a PDF of the schematics.<br>
Gerbers to build the 4-layer PCB are generated and available in a separate folder.<br>
The Pico-socket in 3D-view of the PCB is not correct, too narrow.<br>
<br>
A proper BOM is missing but here are the main components:<br>
- 1x Raspberry Pi Pico<br>
- 1x WDC65C02<br>
- 1x AS6C4008, 512KiB SRAM<br>
- 2x 74LVC244<br>
- 1x 74LVC245<br>
- 2x 74LVC373<br>
- 2x 1x6 pin-header<br>
- 2x 1x4 pin-header<br>
- 1x 1x2 pin-header<br>
- a bunch of decoupling capacitors<br>

## Software

Use Arduino IDE with Earl Philhowers Pico board package, -O2 optimization.<br>
The 6502-sources include pre-assembled h-files in the repository. Assembling on your own requires installation of the very fine assembler [Jasm](http://kollektivet.nu/jasm/).<br>
Run the asm.sh file to create the necessary h-files, along with a bunch of debug- and info-files.<br>

## Contact

Mail: <adam.klotblixt@gmail.com>