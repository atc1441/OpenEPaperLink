#include "asmUtil.h"
#include "printf.h"
#include "radio.h"
#include "board.h"
#include "cpu.h"
#include "comms.h"
#include "printf.h"
#include "settings.h"
#include "timer.h"
#include "adc.h"
#include "logging.h"
#define __packed
#include "../../oepl-definitions.h"
#include "../../oepl-proto.h"

/*
   we use 1-MHz spaced channels 903MHz..927MHz
   250 Kbps, 165KHz deviation, GFSK
   Packets are variable length, CRC16 provided by the radio, as is whitening. 
*/

struct MacHeaderGenericAddr {
   struct MacFcs fcs;
   uint8_t seq;
};

struct MacHeaderShortAddr {
   struct MacFcs fcs;
   uint8_t seq;
   uint16_t pan;
   uint16_t shortDstAddr;
};

struct MacHeaderLongAddr {
   struct MacFcs fcs;
   uint8_t seq;
   uint16_t pan;
   uint8_t longDstAddr[8];
};

// Address Config = No address check 
// CRC Enable = true 
// Carrier Frequency = 902.999756 
// Channel Number = 0 
// Channel Spacing = 335.632324 
// Data Rate = 249.939 
// Deviation = 165.039063 
// Device Address = 22 
// Manchester Enable = false 
// Modulated = true 
// Modulation Format = GFSK 
// PA Ramping = false 
// Packet Length = 255 
// Packet Length Mode = Variable packet length mode. Packet length configured by the first byte after sync word 
// Preamble Count = 24 
// RX Filter BW = 650.000000 
// Sync Word Qualifier Mode = 30/32 sync word bits detected 
// TX Power = 0 
// Whitening = true 
// Rf settings for CC1110
// Table generated by setting Registers in RfStudio export to 
// "    0x@VH@,  // @RN@: @Rd@"
static const __code uint8_t mRadioCfg[] = {
    0xd3,  // SYNC1: Sync Word, High Byte 
    0x91,  // SYNC0: Sync Word, Low Byte 
    RADIO_MAX_PACKET_LEN,  // PKTLEN: Packet Length 
    0x04,  // PKTCTRL1: Packet Automation Control 
    0x45,  // PKTCTRL0: Packet Automation Control 
    0x22,  // ADDR: Device Address 
// Default channel 200 902.999756
    0x00,  // CHANNR: Channel Number 
    0x0f,  // FSCTRL1: Frequency Synthesizer Control 
    0x00,  // FSCTRL0: Frequency Synthesizer Control 
// Default Base Frequency = 902.999756
    0x22,  // FREQ2: Frequency Control Word, High Byte 
    0xbb,  // FREQ1: Frequency Control Word, Middle Byte 
    0x13,  // FREQ0: Frequency Control Word, Low Byte 
    0x1d,  // MDMCFG4: Modem configuration 
    0x3b,  // MDMCFG3: Modem Configuration 
    0x13,  // MDMCFG2: Modem Configuration 
    0x73,  // MDMCFG1: Modem Configuration 
    0xa7,  // MDMCFG0: Modem Configuration 
    0x65,  // DEVIATN: Modem Deviation Setting 
    0x07,  // MCSM2: Main Radio Control State Machine Configuration 
    0x00,  // MCSM1: Main Radio Control State Machine Configuration 
    0x18,  // MCSM0: Main Radio Control State Machine Configuration 
    0x1e,  // FOCCFG: Frequency Offset Compensation Configuration 
    0x1c,  // BSCFG: Bit Synchronization Configuration 
    0xc7,  // AGCCTRL2: AGC Control 
    0x00,  // AGCCTRL1: AGC Control 
    0xb0,  // AGCCTRL0: AGC Control 
    0xb6,  // FREND1: Front End RX Configuration 
    0x10,  // FREND0: Front End TX Configuration 
    0xea,  // FSCAL3: Frequency Synthesizer Calibration 
    0x2a,  // FSCAL2: Frequency Synthesizer Calibration 
    0x00,  // FSCAL1: Frequency Synthesizer Calibration 
    0x1f,  // FSCAL0: Frequency Synthesizer Calibration 
};

#define RX_BUFFER_SIZE        (RADIO_MAX_PACKET_LEN + 1 /* len byte */ + 2 /* RSSI & LQI */)
#define RX_BUFFER_NUM         3

static volatile uint8_t __xdata mLastAckSeq;
static volatile __bit mRxOn, mHaveLastAck;
static volatile uint8_t __xdata mRxBufs[RX_BUFFER_NUM][RX_BUFFER_SIZE];

#ifdef DEBUG_COMMS
static uint32_t __xdata gLastTxTime;
#endif

#define mRxBufNextR     T2PR
#define mRxBufNextW     T4CC0
#define mRxBufNumFree   T4CC1

static volatile struct DmaDescr __xdata mRadioRxDmaCfg = {
   .srcAddrHi = 0xdf,               //RFD is source
   .srcAddrLo = 0xd9,
   .lenHi = 0,
   .vlen = 4,                    //n + 3 bytes transferred (len, and the 2 status bytes)
   .lenLo = RX_BUFFER_SIZE,         //max xfer
   .trig = 19,                   //radio triggers
   .tmode = 0,                   //single mode
   .wordSize = 0,                //transfer bytes
   .priority = 2,                //higher PRIO than CPU
   .m8 = 0,                   //entire 8 bits are valid length (let RADIO limit it)
   .irqmask = 1,                 //interrupt on done
   .dstinc = 1,                  //increment dst
   .srcinc = 0,                  //do not increment src
};
static volatile struct DmaDescr __xdata mRadioTxDmaCfg = {
   .dstAddrHi = 0xdf,               //RFD is destination
   .dstAddrLo = 0xd9,
   .lenHi = 0,
   .vlen = 1,                    //n + 1 bytes transferred (len byte itself)
   .lenLo = RADIO_MAX_PACKET_LEN + 1,  //max xfer
   .trig = 19,                   //radio triggers
   .tmode = 0,                   //single mode
   .wordSize = 0,                //transfer bytes
   .priority = 2,                //higher PRIO than CPU
   .m8 = 0,                   //entire 8 bits are valid length (let RADIO limit it)
   .irqmask = 0,                 //do not interrupt on done
   .dstinc = 0,                  //do not increment dst
   .srcinc = 1,                  //increment src
};

void radioSetChannel(uint8_t ch)
{
   if(ch >= FIRST_866_CHAN && ch < FIRST_866_CHAN + NUM_866_CHANNELS) {
   // Base Frequency = 863.999756   
   // total channels  6 (0 -> 5) (CHANNR 0 -> 15)
   // Channel 100 / CHANNR 0: 863.999756
   // Channel 101 / CHANNR 3: 865.006 Mhz
   // Channel 102 / CHANNR 6: 866.014 Mhz
   // Channel 103 / CHANNR 9: 867.020 Mhz
   // Channel 104 / CHANNR 12: 868.027 Mhz
   // Channel 105 / CHANNR 15: 869.034 Mhz
      COMMS_LOG("866");
      FREQ2 = 0x21;    // Frequency Control Word, High Byte 
      FREQ1 = 0x3b;    // Frequency Control Word, Middle Byte 
      FREQ0 = 0x13;    // Frequency Control Word, Low Byte 
      CHANNR = (ch - FIRST_866_CHAN) * 3;
   }
   else {
   // Base Frequency = 902.999756
   // Dmitry's orginal code used 25 channels in 915 Mhz (0 -> 24, CHANNR 0 -> 72)
   // We don't want to have to scan that many so for OEPL we'll just use 6
   // to match 866.
   // Channel 200 / CHANNR 0: 903.000 Mhz
   // Channel 201 / CHANNR 12: 907.027 Mhz
   // Channel 202 / CHANNR 24: 911.054 Mhz
   // Channel 203 / CHANNR 24: 915.083 Mhz
   // Channel 204 / CHANNR 48: 919.110 Mhz
   // Channel 205 / CHANNR 60: 923.138 Mhz
      COMMS_LOG("915");
      FREQ2 = 0x22;  // Frequency Control Word, High Byte 
      FREQ1 = 0xbb;  // Frequency Control Word, Middle Byte 
      FREQ0 = 0x13;  // Frequency Control Word, Low Byte 

      if(ch >= FIRST_915_CHAN && ch < FIRST_915_CHAN + NUM_915_CHANNELS) {
         CHANNR = (ch - FIRST_915_CHAN) * 12;
      }
      else {
         CHANNR = 0; // default to the first channel on 915
      }
   }
   COMMS_LOG(", %d/CHANNR 0x%x\n",ch,CHANNR);
}

#pragma callee_saves radioPrvGoIdle
static void radioPrvGoIdle(void)
{
   RFST = 4;               //radio, go idle!
   while (MARCSTATE != 1);
}

void radioInit(void)
{
   volatile uint8_t __xdata *radioRegs = (volatile uint8_t __xdata *)0xdf00;
   uint8_t i;
   
   radioPrvGoIdle();
   
   for (i = 0; i < sizeof(mRadioCfg); i++) {
      radioRegs[i] = mRadioCfg[i];
   }

   TEST2 = 0x88;
   TEST1 = 0x31;
   TEST0 = 0x09;
   PA_TABLE0 = 0x8E;
   RFIM = 0x10;   //only irq on done
   
   mRxBufNextW = 0;
   mRxBufNextR = 0;
   mRxBufNumFree = RX_BUFFER_NUM;
}

//-30..+10 dBm
void radioSetTxPower(int8_t dBm)
{
// for 915 mhz, as per https://www.ti.com/lit/an/swra151a/swra151a.pdf
// this applies to cc1110 as per: https://e2e.ti.com/support/wireless-connectivity/other-wireless/f/667/t/15808
   static const __code uint8_t powTab[] = {
      /* -30 dBm */0x03, /* -28 dBm */0x05, /* -26 dBm */0x06, /* -24 dBm */0x08,
      /* -22 dBm */0x0b, /* -20 dBm */0x0e, /* -18 dBm */0x19, /* -16 dBm */0x1c,
      /* -14 dBm */0x24, /* -12 dBm */0x25, /* -10 dBm */0x27, /*  -8 dBm */0x29,
      /*  -6 dBm */0x38, /*  -4 dBm */0x55, /*  -2 dBm */0x40, /*   0 dBm */0x8d,
      /*   2 dBm */0x8a, /*   4 dBm */0x84, /*   6 dBm */0xca, /*   8 dBm */0xc4,
      /*  10 dBm */0xc0,
   };
   
   if (dBm < -30) {
      dBm = -30;
   }
   else if (dBm > 10) {
      dBm = 10;
   }
   
// sdcc calles integer division here is we use " / 2", le sigh...
   PA_TABLE0 = powTab[((uint8_t)(dBm + 30)) >> 1];
   COMMS_LOG("TxPwr %ddBm/0x%x\n",dBm,PA_TABLE0);
}

#pragma callee_saves radioPrvSetDmaCfgAndArm
static void radioPrvSetDmaCfgAndArm(uint16_t cfgAddr)
{
   DMA0CFG = cfgAddr;
   DMAARM = 1;
   __asm__ (" nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n");  //as per spec
}

#pragma callee_saves radioPrvDmaAbort
static void radioPrvDmaAbort(void)
{
   DMAARM = 0x81;
   __asm__ (" nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n");  //as per spec
}

#pragma callee_saves radioPrvSetupRxDma
static void radioPrvSetupRxDma(uint8_t __xdata* buf)
{
   uint16_t addr = (uint16_t)buf;
   
   radioPrvDmaAbort();
   mRadioRxDmaCfg.dstAddrHi = addr >> 8;
   mRadioRxDmaCfg.dstAddrLo = addr & 0xff;
   radioPrvSetDmaCfgAndArm((uint16_t)(volatile void __xdata*)mRadioRxDmaCfg);
}

#pragma callee_saves radioPrvSetupTxDma
static void radioPrvSetupTxDma(const uint8_t __xdata* buf)
{
   uint16_t addr = (uint16_t)buf;
   
   radioPrvDmaAbort();
   mRadioTxDmaCfg.srcAddrHi = addr >> 8;
   mRadioTxDmaCfg.srcAddrLo = addr & 0xff;
#ifdef DEBUG_COMMS
   gLastTxTime = timerGet();
#endif
   radioPrvSetDmaCfgAndArm((uint16_t)(volatile void __xdata*)mRadioTxDmaCfg);
}

static void radioPrvRxStartListenIfNeeded(void)
{
   if (!mRxOn || !mRxBufNumFree)
      return;
   
   radioPrvGoIdle();
   
   IRCON &= (uint8_t)~(1 << 0);  //clear dma irq flag
   IEN1 |= 1 << 0;               //dma irq on
   
   radioPrvSetupRxDma(mRxBufs[mRxBufNextW]);
   
   RFST = 2;   //rx
}

#pragma callee_saves radioRxStopIfRunning
static void radioRxStopIfRunning(void)
{
   IEN1 &= (uint8_t)~(1 << 0);         //dma irq off
   radioPrvDmaAbort();
   radioPrvGoIdle();
   
   DMAIRQ = 0;                   //clear any lagging DMA irqs
   IRCON &= (uint8_t)~(1 << 0);     //clear global dma irq flag
}

void radioTx(const void __xdata *packet)
{
   uint16_t Lowest = 0xffff;

#ifdef DEBUG_TX_DATA
   TX_DATA_LOG("Sending %d bytes\n",packet[0]);
   DumpHex((void *) packet + 1,Len);
#endif
   radioRxStopIfRunning();
   radioPrvSetupTxDma(packet);
   RFIF = 0;
   
   RFST = 3;   //TX
   
   while(!(RFIF & (1 << 4))) {
      ADCRead(ADC_CHAN_VDD_3);

      if(Lowest > gRawA2DValue) {
         Lowest = gRawA2DValue;
      }
   }
   radioPrvDmaAbort();                 //abort DMA just in case
   radioPrvRxStartListenIfNeeded();
// Update lowest battery voltage seen while transmitting
   gTxBattV = ADCScaleVDD(Lowest);
}

void DMA_ISR(void) __interrupt (8)
{
   if (DMAIRQ & (1 << 0)) {
      #define hdr    ((struct MacHeaderGenericAddr __xdata*)(buf + 1))  //SDCC wastes RAM on these as variables, so make them defines
      #define hdrSA  ((struct MacHeaderShortAddr __xdata*)(buf + 1))
      #define hdrLA  ((struct MacHeaderLongAddr __xdata*)(buf + 1))
      uint8_t __xdata *buf = mRxBufs[mRxBufNextW];
      __bit acceptPacket = false;
      uint8_t len;
      
      DMAIRQ &= (uint8_t)~(1 << 0);
      len = buf[0];
      //verify length was proper, crc is a match 
      if(len <= RADIO_MAX_PACKET_LEN && (buf[(uint8_t)(len + 2)] & 0x80)) {
         if (++mRxBufNextW == RX_BUFFER_NUM) {
            mRxBufNextW = 0;
         }
         mRxBufNumFree--;
      }
      radioPrvRxStartListenIfNeeded();
   }
   else {
      pr("dma irq unexpected\n");
   }
   
   IRCON &= (uint8_t)~(1 << 0);
}

void radioRxEnable(__bit on)
{
   if (!on) {
      radioRxStopIfRunning();
      mRxOn = false;
   }
   else if (!mRxOn) {
      mRxOn = true;
      radioPrvRxStartListenIfNeeded();
   }
}

int8_t radioRx()
{
   uint8_t prevNumFree;
   uint8_t lqi;
   const uint8_t __xdata *buf = mRxBufs[mRxBufNextR];
   uint8_t len = buf[0];
   
   if(mRxBufNumFree == RX_BUFFER_NUM) {
      return COMMS_RX_ERR_NO_PACKETS;
   }

#ifdef DEBUG_COMMS
   if(gLastTxTime != 0) {
      COMMS_LOG("%ld ",
                mathPrvDiv32x16(timerGet() - gLastTxTime,TIMER_TICKS_PER_MS));
      gLastTxTime = 0;
   }
#endif
   
   lqi = 148 - (buf[len + 2] & 0x7f);
   if (lqi >= 127){
      lqi = 255;
   }
   else {
      lqi *= 2;
   }
   mLastLqi = lqi;
   mLastRSSI = ((int8_t)buf[len + 1] >> 1) - 77;
   xMemCopyShort(inBuffer,buf + 1,len);
   
   if(++mRxBufNextR == RX_BUFFER_NUM) {
      mRxBufNextR = 0;
   }
   
   __critical {
      prevNumFree = mRxBufNumFree++;
   }
   
   if(!prevNumFree) {
      radioPrvRxStartListenIfNeeded();
   }
   
#ifdef DEBUG_RX_DATA
   RX_DATA_LOG("Got %d bytes\n",len);
   DumpHex(inBuffer,len);
#endif
      
   return len;
}

void radioRxFlush(void)
{
   uint8_t wasFree;
   
   __critical {
      wasFree = mRxBufNumFree;
      mRxBufNumFree = RX_BUFFER_NUM;
   }
   
   if (!wasFree)
      radioPrvRxStartListenIfNeeded();
}

