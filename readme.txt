This is just a demo application for enc28j60

Only tested on pic18f4550 as the master device, but in theory
pic18f2550 would work rather fine without major pain. Other
controllers might need to require some more work.

The purpose of this project is to share some small and clean
code which implements communication with the chip and which
could be relatively easy compiled with modern xc8 compiler.

This program is based on work done by:
- Guido Socher
- Pascal Stang
- nuelectronics.com -- Ethershield for Arduino
- Sergey Sharybin -- port back to microchip :)
