# PZ1_Pico
A version of the PZ1-system based on 65C02, Pico and 512KiB SRAM.

# General info
This is the first public release of SW and HW for the PZ1-project and tries to be as slim as possible.

The system uses 48KiB RAM with EhBASIC running from 16KiB ROM, communicating via a virtual USB serial port of the Pico.
The HW is not the most efficient, mainly because the Pico has too few gpio to connect to the 65C02 and SRAM without buffers. The buffering is done with 74LVC-chips which adds complexity and lowers efficiency.

I started the PZ1 project because I wanted to code 6502, build proper electronis hardware and investigate multitasking and bank-switched memory on a 6502-system. I've done some different iterations with:
- 6502 emulation or real 65C02
- different MCUs Teensy 4.0/4.1 / BluePill / Pico
- external SRAM, RAM inside MCU, RAM on quad-SPI

# Blogg
The ongoing work of PZ1 in general can be followed at https://hackaday.io/project/171471-pz1-6502-laptop

# Hardware
Use KiCad 6 to open the schematics and PCB. There is also a PDF of the schematics.

Gerbers to build the PCB are generated and available in a separate folder.

The Pico-socket in 3D-view of the PCB is not correct.

A proper BOM is missing but here are the main components:
1x Raspberry Pi Pico
1x WDC65C02
1x AS6C4008, 512KiB SRAM
2x 74LVC244
1x 74LVC245
2x 74LVC373
2x 1x6 pin-header
2x 1x4 pin-header
1x 1x2 pin-header

# Software
Use Arduino IDE with Earl Philhowers Pico board package.
The 6502 source includes pre-assembled h-files in the repository. Assembling on your own requires installation of Jasm from http://kollektivet.nu/jasm/
Run the asm.sh file to create the necessary h-files, along with a bunch of debug- and info-files.

# Contact
Mail: adam.klotblixt@gmail.com
