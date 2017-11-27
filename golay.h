///
/// @file	golay23.h
///
/// golay 23/12 error correction encoding and decoding
///

/// encode n bytes of data into 2n coded bytes. n must be a multiple 3
extern void golay_encode(uint8_t n, uint8_t * in, uint8_t * out);


/// decode n bytes of coded data into n/2 bytes of original data
/// n must be a multiple of 6
extern uint8_t golay_decode(uint8_t n, uint8_t * in, uint8_t * out);
