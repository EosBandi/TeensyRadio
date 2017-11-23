#include "radio.h"

#define RAND_MAX 32767

static uint32_t s = 0x80000001;
static unsigned long int next = 1;


int sdcc_rand(void)
{


    //return(rand());
	//register unsigned long t = s;

	//t ^= t >> 10;
	//t ^= t << 9;
	//t ^= t >> 25;

	//s = t;

    //return(t & RAND_MAX);
    

    next = next * 1103515245UL + 12345;
    return (unsigned int)(next/65536) % (RAND_MAX + 1U);

}

void sdcc_srand(unsigned int seed)
{

    //srand(seed);
    //s = seed | 0x80000000; /* s shall not become 0 */
    next = seed;
}