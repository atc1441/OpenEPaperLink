#include "board.h"
#include "cpu.h"
#include "u1.h"


void powerPortsDownForSleep(void)
{
#if 0
   P0 = 0b01000000;
   P1 = 0b01000000;
   P2 = 0b00000001;
   P0DIR = 0b11111111;
   P1DIR = 0b01101110;
   P2DIR = 0b00000001;
   P0SEL = 0;
   P1SEL = 0;
   P2SEL = 0;
#endif
}

