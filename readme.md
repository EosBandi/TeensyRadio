# Notes
With version 1.1 we are diverging from the 3DR compatibility by adding SBUS and auxiliary stream support, and changing the tdm timebase to exactly 16uS.
A 1.0 version with 3Dr radio compatibility was branched. If you want to use it with 3DR radios please use the SiKCompatibile branch



# TODO: 
* Check that the CTS pin is really working. (In normal use CTS pin is not used for communications), currently it is disabled because asserted cts cause hang in serial.putchar
* rename packet_injection into control channel injection
* ~~Add parameter setting to a different port or through a command - Via USB~~
* ~~Change RX ad TX serial buffer sizes to match with the ones in the original SiK 2.0 code~~
* Add some sort of easy encryption to the packet handling code
* Add SBUS protocol handling, number of transmitted channels is settable. (Could it be part of the trailer.stream byte? No need for more than 4 bits.) 
* Add auxiliary data streams support
* ~~Add CTS/RTS pins~~


# FIXME:
* SBUS serial data, how to make sure that the input buffer only contains one full SBUS packet ? (set the RX buffer for that serial port to 11byte ?)


