#ifndef _BITMAPS_H_
#define _BITMAPS_H_

// images generated by https://lvgl.io/tools/imageconverter, prepended with width, height. "CF_INDEXED_1_BIT"-mode, little-endian
#include <stdint.h>

#include "screen.h"

#ifdef ISDEBUGBUILD
static const uint8_t __code debugbuild[] = {
32,11, 
  0x7f, 0xff, 0xff, 0xe0, 
  0x80, 0x00, 0x00, 0x10, 
  0xb9, 0xee, 0x49, 0x90, 
  0xa5, 0x09, 0x4a, 0x50, 
  0xa5, 0x09, 0x4a, 0x10, 
  0xa5, 0xce, 0x4a, 0xd0, 
  0xa5, 0x09, 0x4a, 0x50, 
  0xa5, 0x09, 0x4a, 0x50, 
  0xb9, 0xee, 0x31, 0x90, 
  0x80, 0x00, 0x00, 0x10, 
  0x7f, 0xff, 0xff, 0xe0, 
};
#endif


#if (SCREEN_WIDTH != 128) 
static const uint8_t __code ant[] = {
  16, 16,
  0x00, 0x40, 
  0x02, 0x20, 
  0x01, 0x20, 
  0x11, 0x20, 
  0x11, 0x20, 
  0x12, 0x20, 
  0x28, 0x40, 
  0x28, 0x00, 
  0x28, 0x00, 
  0x44, 0x00, 
  0x44, 0x00, 
  0x44, 0x00, 
  0x44, 0x00, 
  0x82, 0x00, 
  0x82, 0x00, 
  0xfe, 0x00, 
};
#else
static const uint8_t __code ant[] = {
  // rotated 90 degrees
  16,16,
  0x00, 0x00, 
  0x00, 0x00, 
  0x00, 0x00, 
  0x00, 0x00, 
  0x00, 0x00, 
  0x7c, 0x00, 
  0x82, 0x00, 
  0x00, 0x00, 
  0x38, 0x00, 
  0x44, 0x07, 
  0x00, 0x79, 
  0x03, 0x81, 
  0x1c, 0x01, 
  0x03, 0x81, 
  0x00, 0x79, 
  0x00, 0x07, 
};
#endif
static const uint8_t __code cross[] = {
  8,8,
  0x00, 
  0x63, 
  0x77, 
  0x3e, 
  0x1c, 
  0x3e, 
  0x77, 
  0x63
};

#if (SCREEN_WIDTH != 128) 
static const uint8_t __code battery[] = {
  16,10,
  0x00, 0x00, 
  0x7f, 0xfc, 
  0x40, 0x04, 
  0x58, 0x06, 
  0x58, 0x06, 
  0x58, 0x06, 
  0x58, 0x06, 
  0x40, 0x04, 
  0x7f, 0xfc, 
  0x00, 0x00, 
};
#else
// this battery symbol is rotated 90'
static const uint8_t __code battery[] = {
16,16,
  0x00, 0x00, 
  0x03, 0xc0, 
  0x0f, 0xf0, 
  0x08, 0x10, 
  0x08, 0x10, 
  0x08, 0x10, 
  0x08, 0x10, 
  0x08, 0x10, 
  0x08, 0x10, 
  0x08, 0x10, 
  0x08, 0x10, 
  0x0b, 0xd0, 
  0x0b, 0xd0, 
  0x08, 0x10, 
  0x0f, 0xf0, 
  0x00, 0x00, 
};

#endif

#endif