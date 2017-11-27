/**
 * @file print.cpp
 * 
 * Containt print functions, all of them are printing to Serial0 which is the USB connected serial port
 * 
 */
#include <Arduino.h>
#include "radio.h"




void s1printf(const char *format, ...)  {
    char buf[128];
    va_list args;
    va_start (args, format);
    vsniprintf(buf, sizeof(buf), format, args); // does not overrun sizeof(buf) including null terminator
    va_end (args);
    // the below assumes that the new data will fit into the I/O buffer. If not, Serial may drop it.
    // if Serial had a get free buffer count, we could delay and retry. Such does exist at the device class level, but not at this level.
    Serial.write(buf); // move chars to I/O buffer, freeing up local buf
  }



void debug(const char *format,...) {
#ifdef DEBUG
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

void panic(const char *format,...) {
    char buf[128];
    va_list args;
    va_start (args, format);
    vsniprintf(buf, sizeof(buf), format, args); // does not overrun sizeof(buf) including null terminator
    va_end (args);
    // the below assumes that the new data will fit into the I/O buffer. If not, Serial may drop it.
    // if Serial had a get free buffer count, we could delay and retry. Such does exist at the device class level, but not at this level.
    Serial.write(buf); // move chars to I/O buffer, freeing up local buf
    while(1);
  return;
  }