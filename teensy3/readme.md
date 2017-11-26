These files are the modified libraries for teensyuino. Before compiling copy them to 
C:\Program Files (x86)\Arduino\hardware\teensy\avr\cores\teensy3
overwrite the ones in the target directory
Files from TeensyDuino 1.4, if a newer version come out check compatibility before...


Changes :
serial1.c
...
#define SERIAL1_TX_BUFFER_SIZE     1024 // number of outgoing bytes to buffer
#define SERIAL1_RX_BUFFER_SIZE     1024 // number of incoming bytes to buffer
....
added function serial_peekx

int serial_peekx(uint8_t x)
{
	uint32_t head, tail;

	head = rx_buffer_head;
	tail = rx_buffer_tail;
	
	if (head == tail) return -1;
	for (uint8_t i=0;i<=x;i++) {
	if (++tail >= SERIAL1_RX_BUFFER_SIZE) tail = 0; }
	
	return rx_buffer[tail];
}
......
HardwareSerial.h 
Added method peekx to the class

int serial_peekx(uint8_t x);
...
...
	virtual int peekx( uint8_t x)   { return serial_peekx(x);}
	
	

