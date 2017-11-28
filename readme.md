# Notes
With version 1.1 we are diverging from the 3DR compatibility mode, by adding SBUS and auxiliary stream support, to keep the compatiblity
a board.h has a 
```
#define RACRADIO
```
line. To compile a version which can work together with SiK radios, comment out this line.



# TODO: 
* rename packet_injection into control channel injection
* ~~Add parameter setting to a different port or through a command - Via USB~~
* ~~Change RX ad TX serial buffer sizes to match with the ones in the original SiK 2.0 code~~
* Add some sort of easy encryption to the packet handling code
* Add SBUS protocol handling... 
* ~~Add CTS/RTS pins~~


# FIXME:


