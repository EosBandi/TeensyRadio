
#include <Arduino.h>
#include "radio.h"




void s1printf(char *format, ...)  {
    int n;
    char buf[128];
    va_list args;
    va_start (args, format);
    vsniprintf(buf, sizeof(buf), format, args); // does not overrun sizeof(buf) including null terminator
    va_end (args);
    // the below assumes that the new data will fit into the I/O buffer. If not, Serial may drop it.
    // if Serial had a get free buffer count, we could delay and retry. Such does exist at the device class level, but not at this level.
    Serial.write(buf); // move chars to I/O buffer, freeing up local buf
  }



void debug(char *format,...) {
#ifdef DEBUG
  int n;
  char buf[128];
  va_list args;
  va_start (args, format);
  vsniprintf(buf, sizeof(buf), format, args); // does not overrun sizeof(buf) including null terminator
  va_end (args);
  // the below assumes that the new data will fit into the I/O buffer. If not, Serial may drop it.
  // if Serial had a get free buffer count, we could delay and retry. Such does exist at the device class level, but not at this level.
  Serial.write(buf); // move chars to I/O buffer, freeing up local buf
#else
return;
#endif
}

void panic(char *format,...) {
    int n;
    char buf[128];
    va_list args;
    va_start (args, format);
    vsniprintf(buf, sizeof(buf), format, args); // does not overrun sizeof(buf) including null terminator
    va_end (args);
    // the below assumes that the new data will fit into the I/O buffer. If not, Serial may drop it.
    // if Serial had a get free buffer count, we could delay and retry. Such does exist at the device class level, but not at this level.
    Serial.write(buf); // move chars to I/O buffer, freeing up local buf
  return;
  }