#include "radio.h"

//Define my own rand implemantation to match with the one in SDCC compiler.
//This need to make it compatibile with SiK 1.9 radio modems. (As long as SBUS injection is not used)


#define RAND_MAXIMUM 32767

static unsigned long int next = 1;

int sdcc_rand(void)
{
    next = next * 1103515245UL + 12345;
    return (unsigned int)(next/65536) % (RAND_MAXIMUM + 1U);
}

void sdcc_srand(unsigned int seed)
{
    next = seed;
}