#include "asmUtil.h"
#include "screen.h"
#include "eeprom.h"
#include "printf.h"
#include "board.h"
#include "cpu.h"
#include <stdlib.h>
#include "settings.h"

#ifdef DEBUGEEPROM
   #define LOG(format, ... ) pr(format,## __VA_ARGS__)
#else
   #define LOG(format, ... )
#endif



#ifndef SFDP_DISABLED
static uint32_t __xdata mEepromSize;
static uint8_t __xdata mOpcodeErz4K = 0, mOpcodeErz32K = 0, mOpcodeErz64K = 0;
#else
#define mEepromSize     EEPROM_SIZE
#define mOpcodeErz4K    EEPROM_4K_ERASE_OPCODE
#define mOpcodeErz32K   EEPROM_32K_ERASE_OPCODE
#define mOpcodeErz64K   EEPROM_64K_ERASE_OPCODE
#endif

#ifndef SFDP_DISABLED
uint32_t eepromGetSize(void) 
{
   return mEepromSize;
}
#endif

void eepromReadStart(uint32_t addr) __reentrant 
{
   eepromPrvSelect();
   eepromByte(0x03);
   eepromByte(addr >> 16);
   eepromByte(addr >> 8);
   eepromByte(addr & 0xff);
}

void eepromRead(uint32_t addr, void __xdata *dstP, uint16_t len) __reentrant 
{
   uint8_t __xdata *dst = (uint8_t __xdata *)dstP;

   eepromPrvSelect();
   eepromByte(0x03);
   eepromByte(addr >> 16);
   eepromByte(addr >> 8);
   eepromByte(addr & 0xff);

   while(len--) {
      *dst++ = eepromByte(0);
   }
   eepromPrvDeselect();
}

static void eepromPrvSimpleCmd(uint8_t cmd) 
{
   eepromPrvSelect();
   eepromByte(cmd);
   eepromPrvDeselect();
}

static bool eepromPrvBusyWait(void) 
{
   uint8_t val;

   eepromPrvSelect();
   eepromByte(0x05);
   while((val = eepromByte(0x00)) & 1) {
      ;
   }
   eepromPrvDeselect();

   return true;
}

static bool eepromWriteLL(uint32_t addr, const void __xdata *srcP, uint16_t len) {
   const uint8_t __xdata *src = (const uint8_t __xdata *)srcP;

   eepromPrvSimpleCmd(0x06);

   eepromPrvSelect();
   eepromByte(0x02);
   eepromByte(addr >> 16);
   eepromByte(addr >> 8);
   eepromByte(addr & 0xff);

   while(len--)
      eepromByte(*src++);
   eepromPrvDeselect();

   return eepromPrvBusyWait();
}

void eepromDeepPowerDown(void) 
{
   eepromPrvSimpleCmd(0xb9);
}

void eepromWakeFromPowerdown(void) 
{
   eepromPrvSimpleCmd(0xab);
}

#pragma callee_saves eepromPrvSfdpRead
static void eepromPrvSfdpRead(uint16_t ofst, uint8_t __xdata *dst, uint8_t len) {
   eepromPrvSelect();
   eepromByte(0x5a);  // cmd
   eepromByte(0);     // addr
   eepromByte(ofst >> 8);
   eepromByte(ofst);
   eepromByte(0x00);  // dummy
   while(len--)
      *dst++ = eepromByte(0);
   eepromPrvDeselect();
}

#ifndef SFDP_DISABLED
__bit eepromInit(void) 
{
   uint8_t __xdata buf[8];
   uint8_t i, nParamHdrs;
   uint8_t __xdata *tempBufferE = malloc(320);
   __bit Ret = false;

// process SFDP
   eepromPrvSfdpRead(0, buf, 8);
   do {
      if(buf[0] != 0x53 || buf[1] != 0x46 || buf[2] != 0x44 || buf[3] != 0x50 
         || buf[7] != 0xff) {
         LOG("SFDP: header not found\n");
         break;
      }

      if(buf[5] != 0x01) {
         LOG("SFDP: version wrong: %u.%d\n", buf[5], buf[4]);
         break;
      }

      nParamHdrs = buf[6];
      if(nParamHdrs == 0xff)  // that case is very unlikely and we just do not care
         nParamHdrs--;

      // now we need to find the JEDEC parameter table header
      for(i = 0; i <= nParamHdrs; i++) {
         eepromPrvSfdpRead(mathPrvMul8x8(i, 8) + 8, buf, 8);
         if(buf[0] == 0x00 && buf[2] == 0x01 && buf[3] >= 9) {
            uint8_t j;

            eepromPrvSfdpRead(*(uint16_t __xdata *)(buf + 4),tempBufferE,9 * 4);
            if((tempBufferE[0] & 3) != 1) {
               LOG("SFDP: no 4K ERZ\n");
               break;
            }
            if(!(tempBufferE[0] & 0x04)) {
               LOG("SFDP: no large write buf\n");
               break;
            }
            if((tempBufferE[2] & 0x06)) {
               LOG("SFDP: addr.len != 3\n");
               break;
            }

            if(!tempBufferE[1] || tempBufferE[1] == 0xff) {
               LOG("SFDP: 4K ERZ opcode invalid\n");
               break;
            }
            mOpcodeErz4K = tempBufferE[1];

            if(tempBufferE[7] & 0x80) {
               LOG("SFDP: device too big\n");
               break;
            }
            uint8_t __xdata t;

            if(t = tempBufferE[7])
               mEepromSize = 0x00200000UL;
            else if(t = tempBufferE[6])
               mEepromSize = 0x00002000UL;
            else if(t = tempBufferE[5])
               mEepromSize = 0x00000020UL;
            else {
               LOG("SFDP: device so small?!\n");
               break;
            }

            while(t) {
               mEepromSize <<= 1;
               t >>= 1;
            }

            // get erase opcodes
            for(j = 0x1c; j < 0x24; j += 2) {
               uint8_t instr = tempBufferE[j + 1];

               if(!instr || instr == 0xff)
                  continue;

               switch(tempBufferE[j]) {
                  case 0x0c:
                     if(mOpcodeErz4K != instr) {
                        LOG("4K ERZ opcode disagreement\n");
                        return false;
                     }
                     break;

                  case 0x0f:  // 32K erase
                     mOpcodeErz32K = instr;
                     break;

                  case 0x10:  // 64K erase
                     mOpcodeErz64K = instr;
                     break;
               }
            }

            LOG("EEPROM accepted\n");
            LOG(" ERZ opcodes: \n");
            if(mOpcodeErz4K) {
               LOG(" 4K:  %02xh\n", mOpcodeErz4K);
            }
            if(mOpcodeErz32K) {
               LOG(" 32K: %02xh\n", mOpcodeErz32K);
            }
            if(mOpcodeErz64K) {
               LOG(" 64K: %02xh\n", mOpcodeErz64K);
            }
            LOG(" Size: 0x%*08lx\n", (uint16_t)&mEepromSize);
            Ret = true;
         }
      }
   } while(false);
   free(tempBufferE);
   if(!Ret) {
      LOG("SFDP: no JEDEC table of expected version found\n");
   }
   return Ret;
}
#endif

bool eepromWrite(uint32_t addr, const void __xdata *srcP, uint16_t len) __reentrant 
{
   const uint8_t __xdata *src = (const uint8_t __xdata *)srcP;

   while(len) {
      uint16_t lenNow = EEPROM_WRITE_PAGE_SZ - (addr & (EEPROM_WRITE_PAGE_SZ - 1));

      if(lenNow > len)
         lenNow = len;

      if(!eepromWriteLL(addr, src, lenNow))
         return false;

      addr += lenNow;
      src += lenNow;
      len -= lenNow;
   }
   return true;
}

bool eepromErase(uint32_t addr, uint16_t nSec) __reentrant 
{
   uint8_t now;

   if(((uint16_t)addr) & 0x0fff) {
      return false;
   }

   for(; nSec; nSec -= now) {
      eepromPrvSimpleCmd(0x06);
      eepromPrvSelect();

      if(nSec >= 16 && !(uint16_t)addr && mOpcodeErz64K) {
      // erase 64K
         eepromByte(mOpcodeErz64K);
         now = 16;
      }
      else if(nSec >= 8 && !(((uint16_t)addr) & 0x7fff) && mOpcodeErz32K) {  
      // erase 32K
         eepromByte(mOpcodeErz32K);
         now = 8;
      }
      else {
      // erase 4K
         eepromByte(mOpcodeErz4K);
         now = 1;
      }

      eepromByte(addr >> 16);
      eepromByte(addr >> 8);
      eepromByte(addr);
      eepromPrvDeselect();

      if(!eepromPrvBusyWait()) {
         return false;
      }

      addr += mathPrvMul16x8(EEPROM_ERZ_SECTOR_SZ, now);
   }

   return true;
}

void eepromOtpModeEnter(void) 
{
   eepromPrvSimpleCmd(0xb1);
}

void eepromOtpModeExit(void) 
{
   eepromPrvSimpleCmd(0xc1);
}