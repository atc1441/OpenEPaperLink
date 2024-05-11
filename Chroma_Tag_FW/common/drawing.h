#ifndef _DRAWING_H_
#define _DRAWING_H_

#include <stdint.h>

#define DRAWING_MIN_BITMAP_SIZE     (128)    //minimum size we'll consider
#define FONT_HEIGHT  16
#define FONT_WIDTH   10

extern __bit gWinColor;
extern __bit gLargeFont;
extern __bit gDirectionY;
extern __xdata int16_t gCharX;
extern __xdata int16_t gCharY;
extern __xdata int8_t gCharWidth;
extern __xdata int8_t gFontHeight;
extern __xdata int16_t gTempX;
extern __xdata int16_t gTempY;
extern __xdata int16_t gWinDrawX;

typedef void (*DrawingFunction)(void) __reentrant;
void DrawScreen(DrawingFunction);
extern DrawingFunction gDrawingFunct;

#pragma callee_saves drawImageAtAddress
void drawImageAtAddress(uint32_t addr);

#pragma callee_saves loadRawBitmap
void loadRawBitmap(uint8_t *bmp,uint16_t x,uint16_t y,bool color);
#pragma callee_saves epdPrintBegin
void epdPrintBegin(uint16_t x,uint16_t y,bool direction,bool fontsize,bool color); 

//expected external funcs
#pragma callee_saves fwVerString
const char __xdata* fwVerString(void);

#ifdef DISABLE_BARCODES
#define printBarcode(x,y,z)
#else
void printBarcode(const char __xdata *string, uint16_t x, uint16_t y);
#endif

#endif
