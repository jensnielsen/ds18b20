ds18b20
=======

Quite portable C driver for the ds18b20 temperature sensor, including a function that actually calculates the crc (in case you have a system where you can't fit a 256 byte crc table)

Note: There's no parasite power support

Included are example projects for the STM32F4-DISCOVERY Cortex M4 board and the Microchip PICkit 3 PIC18 44-pin demo board. Neither really does anything but they show how it works.

Legal stuff:
* Anything done by me:
   BSD 3-clause license ("New BSD License" or "Modified BSD License")

* Anything done by anybody else:
   As indicated in respective file
