#ifndef _CHROMA74_COMMON_H_
#define _CHROMA74_COMMON_H_

#include "u1.h"

//colors for ui messages
#define UI_MSG_MAGNIFY1       3
#define UI_MSG_MAGNIFY2       2
#define UI_MSG_MAGNIFY3       1
#define UI_MSG_BACK_COLOR     6
#define UI_MSG_FORE_COLOR_1      0
#define UI_MSG_FORE_COLOR_2      2
#define UI_MSG_FORE_COLOR_3      3
#define UI_MSG_MAIN_TEXT_V_CENTERED
#define UI_BARCODE_HORIZ

//eeprom spi
#define EEPROM_SIZE              0x00100000L
#define EEPROM_4K_ERASE_OPCODE   0x20
#define EEPROM_32K_ERASE_OPCODE  0
#define EEPROM_64K_ERASE_OPCODE  0xD8
#define eepromByte            u1byte
#define eepromPrvSelect()     do { __asm__("nop"); P2_0 = 0; __asm__("nop"); } while(0)
#define eepromPrvDeselect()      do { __asm__("nop"); P2_0 = 1; __asm__("nop"); } while(0)

//debug uart (enable only when needed, on some boards it inhibits eeprom access)
#define dbgUartOn()           u1setUartMode()
#define dbgUartOff()       u1setEepromMode()
#define dbgUartByte           u1byte

//eeprom map
#define EEPROM_SETTINGS_AREA_START  (0x08000UL)
#define EEPROM_SETTINGS_AREA_LEN    (0x04000UL)
//some free space here
#define EEPROM_UPDATA_AREA_START    (0x10000UL)
#define EEPROM_UPDATE_AREA_LEN      (0x09000UL)
#define EEPROM_IMG_START            (0x19000UL)
#define EEPROM_IMG_EACH             (0x17000UL)
//till end of eeprom really. do not put anything after - it will be erased at pairing time!!!
#define EEPROM_PROGRESS_BYTES       (192)

#define IMAGE_SLOTS                 ((EEPROM_SIZE - EEPROM_IMG_START)/EEPROM_IMG_EACH)

//hw types
#define HW_TYPE_NORMAL              HW_TYPE_74_INCH_DISPDATA_Y
#define HW_TYPE_CYCLING             HW_TYPE_74_INCH_DISPDATA_Y_FRAME_MODE

#endif   // _CHROMA74_COMMON_H_
