#ifndef _RADIO_COMMON_H_
#define _RADIO_COMMON_H_

#include <stdbool.h>
#include <stdint.h>



#define RADIO_MAX_PACKET_LEN        (125) //useful payload, not including the crc

#define ADDR_MODE_NONE              (0)
#define ADDR_MODE_SHORT             (2)
#define ADDR_MODE_LONG              (3)

#define FRAME_TYPE_BEACON           (0)
#define FRAME_TYPE_DATA             (1)
#define FRAME_TYPE_ACK              (2)
#define FRAME_TYPE_MAC_CMD          (3)

#define SHORT_MAC_UNUSED            (0x10000000UL) //for radioRxFilterCfg's myShortMac

extern uint8_t __xdata inBuffer[128];

void radioInit(void);
//waits for tx end
bool radioTx(const void __xdata *packet);

#pragma callee_saves radioRxAckReset
void radioRxAckReset(void);
#pragma callee_saves radioRxAckGetLast
int16_t radioRxAckGetLast(void); //get seq of lask ack we got or -1 if none
void radioRxEnable(__bit on, __bit autoAck);

#pragma callee_saves radioSetTxPower
void radioSetTxPower(int8_t dBm);   //-30..+10 dBm

#pragma callee_saves radioSetChannel
void radioSetChannel(uint8_t ch);

void radioRxFlush(void);
int8_t radioRx(void);

#endif




