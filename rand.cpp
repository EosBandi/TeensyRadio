/**
 * @file rand.cpp
 * 
 * Definitions for radio module and teensy hardware
 */
#include "radio.h"

#define RAND_MAXIMUM 32767

static unsigned long int next = 1;
/**
 * @brief rand() function matching with the sdcc compiler implementation
 * 
 * @return int 
 */
int sdcc_rand(void)
{
    next = next * 1103515245UL + 12345;
    return (unsigned int)(next/65536) % (RAND_MAXIMUM + 1U);
}


/**
 * @brief set seed for the sdcc_rand
 * 
 * @param seed 
 */
void sdcc_srand(unsigned int seed)
{
    next = seed;
}