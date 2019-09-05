// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <sys/param.h>
#include "spi_lcd.h"
#include "driver/gpio.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/xtensa_api.h"
#include "freertos/task.h"
#define SPIFIFOSIZE 16
#define TFT_CMD_DELAY     0x80

#define ST_CMD_DELAY      0x80    // special signifier for command lists

#define ST77XX_NOP        0x00
#define ST77XX_SWRESET    0x01
#define ST77XX_RDDID      0x04
#define ST77XX_RDDST      0x09

#define ST77XX_SLPIN      0x10
#define ST77XX_SLPOUT     0x11
#define ST77XX_PTLON      0x12
#define ST77XX_NORON      0x13

#define ST77XX_INVOFF     0x20
#define ST77XX_INVON      0x21
#define ST77XX_DISPOFF    0x28
#define ST77XX_DISPON     0x29
#define ST77XX_CASET      0x2A
#define ST77XX_RASET      0x2B
#define ST77XX_RAMWR      0x2C
#define ST77XX_RAMRD      0x2E

#define ST77XX_PTLAR      0x30
#define ST77XX_COLMOD     0x3A
#define ST77XX_MADCTL     0x36

#define ST77XX_MADCTL_MY  0x80
#define ST77XX_MADCTL_MX  0x40
#define ST77XX_MADCTL_MV  0x20
#define ST77XX_MADCTL_ML  0x10
#define ST77XX_MADCTL_RGB 0x00

#define ST77XX_RDID1      0xDA
#define ST77XX_RDID2      0xDB
#define ST77XX_RDID3      0xDC
#define ST77XX_RDID4      0xDD

// Some ready-made 16-bit ('565') color settings:
#define	ST77XX_BLACK      0x0000
#define ST77XX_WHITE      0xFFFF
#define	ST77XX_RED        0xF800
#define	ST77XX_GREEN      0x07E0
#define	ST77XX_BLUE       0x0800//0x001F
#define ST77XX_CYAN       0x07FF
#define ST77XX_MAGENTA    0xF81F
#define ST77XX_YELLOW     0xFFE0
#define	ST77XX_ORANGE     0xFC00

// some flags for initR() :(
#define INITR_GREENTAB    0x00
#define INITR_REDTAB      0x01
#define INITR_BLACKTAB    0x02
#define INITR_18GREENTAB  INITR_GREENTAB
#define INITR_18REDTAB    INITR_REDTAB
#define INITR_18BLACKTAB  INITR_BLACKTAB
#define INITR_144GREENTAB 0x01
#define INITR_MINI160x80  0x04
#define INITR_HALLOWING   0x05

// Some register settings
#define ST7735_MADCTL_BGR 0x08
#define ST7735_MADCTL_MH  0x04

#define ST7735_FRMCTR1    0xB1
#define ST7735_FRMCTR2    0xB2
#define ST7735_FRMCTR3    0xB3
#define ST7735_INVCTR     0xB4
#define ST7735_DISSET5    0xB6

#define ST7735_PWCTR1     0xC0
#define ST7735_PWCTR2     0xC1
#define ST7735_PWCTR3     0xC2
#define ST7735_PWCTR4     0xC3
#define ST7735_PWCTR5     0xC4
#define ST7735_VMCTR1     0xC5

#define ST7735_PWCTR6     0xFC

#define ST7735_GMCTRP1    0xE0
#define ST7735_GMCTRN1    0xE1

// Some ready-made 16-bit ('565') color settings:
#define ST7735_BLACK      ST77XX_BLACK
#define ST7735_WHITE      ST77XX_WHITE
#define ST7735_RED        ST77XX_RED
#define ST7735_GREEN      ST77XX_GREEN
#define ST7735_BLUE       ST77XX_BLUE
#define ST7735_CYAN       ST77XX_CYAN
#define ST7735_MAGENTA    ST77XX_MAGENTA
#define ST7735_YELLOW     ST77XX_YELLOW
#define ST7735_ORANGE     ST77XX_ORANGE




#ifndef pgm_read_byte
 #define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#endif
#ifndef pgm_read_word
 #define pgm_read_word(addr) (*(const unsigned short *)(addr))
#endif
#ifndef pgm_read_dword
 #define pgm_read_dword(addr) (*(const unsigned long *)(addr))
#endif

/*
 This struct stores a bunch of command values to be initialized for ILI9341
*/
typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes; //No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} lcd_init_cmd_t;


DRAM_ATTR static const lcd_init_cmd_t ili_init_cmds[]={
    {0xCF, {0x00, 0x83, 0x30}, 3},
    {0xED, {0x64, 0x03, 0x12, 0x81}, 4},
    {0xE8, {0x85, 0x01, 0x79}, 3},
    {0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},
    {0xF7, {0x20}, 1},
    {0xEA, {0x00, 0x00}, 2},
    {0xC0, {0x26}, 1},
    {0xC1, {0x11}, 1},
    {0xC5, {0x35, 0x3E}, 2},
    {0xC7, {0xBE}, 1},
    {0x36, {0x28}, 1},
    {0x3A, {0x55}, 1},
    {0xB1, {0x00, 0x1B}, 2},
    {0xF2, {0x08}, 1},
    {0x26, {0x01}, 1},
    {0xE0, {0x1F, 0x1A, 0x18, 0x0A, 0x0F, 0x06, 0x45, 0X87, 0x32, 0x0A, 0x07, 0x02, 0x07, 0x05, 0x00}, 15},
    {0XE1, {0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3A, 0x78, 0x4D, 0x05, 0x18, 0x0D, 0x38, 0x3A, 0x1F}, 15},
    {0x2A, {0x00, 0x00, 0x00, 0xEF}, 4},
    {0x2B, {0x00, 0x00, 0x01, 0x3f}, 4}, 
    {0x2C, {0}, 0},
    {0xB7, {0x07}, 1},
    {0xB6, {0x0A, 0x82, 0x27, 0x00}, 4},
    {0x11, {0}, 0x80},
    {0x29, {0}, 0x80},
    {0, {0}, 0xff},
};

DRAM_ATTR static const lcd_init_cmd_t ili9488_init_cmds[]={
    {0xF7, {0xA9, 0x51, 0x2C, 0x82}, 4},
    {0xC0, {0x11, 0x09}, 2},
    {0xC1, {0x41}, 1},
    {0xC5, {0x00, 0x0A, 0X80}, 3}, 
    {0xB1, {0xB0, 0x11}, 2},
    {0xB4, {0x02}, 1},
    {0xB6, {0x02, 0x22}, 2},
    {0xB7, {0xC6}, 1},
    {0xBE, {0x00, 0x04}, 2},
    {0xE9, {0x00}, 1},
    {0x36, {0x08}, 1},
    {0x3A, {0x66}, 1},
    {0xE0, {0x00, 0x07, 0x10, 0x09, 0x17, 0x0B, 0x41, 0X89, 0x4B, 0x0A, 0x0C, 0x0E, 0x18, 0x1B, 0x0F}, 15},
    {0XE1, {0x00, 0x17, 0x1A, 0x04, 0x0E, 0x06, 0x2F, 0x45, 0x43, 0x02, 0x0A, 0x09, 0x32, 0x36, 0x0F}, 15},
    {0x11, {0}, 0x80},
    {0x29, {0}, 0x80},
    {0, {0}, 0xff},
};

DRAM_ATTR static const lcd_init_cmd_t st7789_init_cmds[] = {
    {0xC0, {0x00}, 1},           //LCMCTRL: LCM Control [2C] //sumpremely related to 0x36, MADCTL
    {0xC2, {0x01, 0xFF}, 2},     //VDVVRHEN: VDV and VRH Command Enable [01 FF]
    {0xC3, {0x13}, 1},           //VRHS: VRH Set VAP=???, VAN=-??? [0B]
    {0xC4, {0x20}, 1},           //VDVS: VDV Set [20]
    {0xC6, {0x0F}, 1},           //FRCTRL2: Frame Rate control in normal mode [0F]
    {0xCA, {0x0F}, 1},           //REGSEL2 [0F]
    {0xC8, {0x08}, 1},           //REGSEL1 [08]
    {0x55, {0xB0}, 1},           //WRCACE  [00]
    {0x36, {0x00}, 1},
    {0x3A, {0x55}, 1},             //this says 0x05
    {0xB1, {0x40, 0x02, 0x14}, 3}, //sync setting not reqd
    {0x26, {0x01}, 1}, 
    {0x2A, {0x00, 0x00, 0x00, 0xEF}, 4},
    {0x2B, {0x00, 0x00, 0x01, 0x3F}, 4},
    {0x2C, {0x00}, 1},
    {0xE0, {0xD0, 0x00, 0x05, 0x0E, 0x15, 0x0D, 0x37, 0x43, 0x47, 0x09, 0x15, 0x12, 0x16, 0x19}, 14},    //PVGAMCTRL: Positive Voltage Gamma control        
    {0xE1, {0xD0, 0x00, 0x05, 0x0D, 0x0C, 0x06, 0x2D, 0x44, 0x40, 0x0E, 0x1C, 0x18, 0x16, 0x19}, 14},    //NVGAMCTRL: Negative Voltage Gamma control
    {0x11, {0}, 0x80}, 
    {0x29, {0}, 0x80},
    {0, {0}, 0xff},
};
/*
DRAM_ATTR static const lcd_init_cmd_t st7735R_init_cmds[] = {
	{0x11, {0}, 0x80}, 
    {0xB1, {0x01, 0x2C, 0x2D}, 3},
    {0xB2, {0x01, 0x2C, 0x2D}, 3},
    {0xB3, {0x01, 0x2C, 0x2D, 0x01, 0X2C, 0X2D}, 6},
    {0xB4, {0x07}, 1},

	{0xC0, {0xA2, 0x02, 0x84, 0xC1, 0xC5}, 5}, 
	{0xC2, {0x0A, 0x00}, 2}, 
	{0xC3, {0x8A, 0x2A, 0xC4, 0x8A, 0xEE}, 5}, 
	{0xC5, {0x0E}, 1},

	{0x36, {0xC0}, 1},

	{0xE0, {0x02, 0x1c, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2d, 0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10}, 16}, 
	{0xE1, {0x03, 0x1d, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D, 0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10}, 16}, 
	//{ST7735_GMCTRP1, {0x02, 0x1c, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2d, 0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10}, 16},
	//{ST7735_GMCTRN1, {0x03, 0x1d, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D, 0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10}, 16},

	{0x2A, {0x00, 0x00, 0x00, 0x7F}, 4},
	{0x2B, {0x00, 0x00, 0x00, 0x9F}, 4},

	
	{0xF0, {0x01}, 1},
	{0xF6, {0x00}, 1},

	{0x3A, {0x06}, 1},
	{0x11, {0}, 0x80}, 
	{0x29, {0}, 0x80},
	{0, {0}, 0xff},
};
*/	
DRAM_ATTR static const lcd_init_cmd_t st7796s_init_cmds[] = {
    {0x11, {0}, 0x80}, 
    {0xF0, {0xC3}, 1},           
    {0xF0, {0x96}, 1},     //VDVVRHEN: VDV and VRH Command Enable [01 FF]
    {0x36, {0x48}, 1},           //VRHS: VRH Set VAP=???, VAN=-??? [0B]
    {0x3A, {0x55}, 1},           //VDVS: VDV Set [20]
    {0xB4, {0x01}, 1},           //FRCTRL2: Frame Rate control in normal mode [0F]
    {0xE8, {0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33}, 8},
    {0xC1, {0x06}, 1},           //REGSEL2 [0F]
    {0xC2, {0xA7}, 1},           //REGSEL1 [08]
    {0xC5, {0x18}, 1},           //WRCACE  [00]
    {0xE0, {0xF0, 0x09, 0x0B, 0x06, 0x04, 0x15, 0x2F, 0x54, 0x42, 0x3C, 0x17, 0x14, 0x18, 0x1B}, 14},
    {0xE1, {0xF0, 0x09, 0x0B, 0x06, 0x04, 0x03, 0x2D, 0x43, 0x42, 0x3B, 0x16, 0x14, 0x17, 0x1B}, 14},
    {0xF0, {0x3C}, 1},
    {0xF0, {0x69}, 1},             //this says 0x05
    {0x29, {0}, 0x80},
    {0, {0}, 0xff},
};

DRAM_ATTR static const lcd_init_cmd_t st7735R_init_cmds[] = {
	{ST77XX_SWRESET, {0}, 0x80},
	{ST77XX_SLPOUT, {0}, 0x80},
	
	{ST7735_FRMCTR1, {0x01, 0x2C, 0x2D}, 3},
	{ST7735_FRMCTR2, {0x01, 0x2C, 0x2D}, 3},
	{ST7735_FRMCTR3, {0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D}, 6},
	
	{ST7735_INVCTR, {0x07}, 1},
	{ST7735_PWCTR1, {0xA2, 0x02, 0x84}, 3},
	{ST7735_PWCTR2, {0xC5}, 1},
	{ST7735_PWCTR3, {0x0A, 0x00}, 2},
	{ST7735_PWCTR4, {0x8A, 0x2A}, 2},
	{ST7735_PWCTR5, {0x8A, 0xEE}, 2},
	{ST7735_VMCTR1, {0x0E}, 1},
	//{ST77XX_INVOFF, {0}, 1}, //?
	{ST77XX_MADCTL, {0xC8}, 1},
	{ST77XX_COLMOD, {0x05}, 1},

	//
	{ST77XX_CASET, {0x00, 0x00, 0x00, 0x7F}, 4},
	{ST77XX_RASET, {0x00, 0x00, 0x00, 0x9F}, 4},
	
	//
	{ST7735_GMCTRP1, {0x02, 0x1c, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2d, 0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10}, 16},
	{ST7735_GMCTRN1, {0x03, 0x1d, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D, 0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10}, 16},
	{ST77XX_NORON, {0}, 0x80},
	{ST77XX_DISPON, {0}, 0x80},
	{ST77XX_MADCTL, {0xC0}, 1},
	
	{0x11, {0}, 0x80},
	{0x29, {0}, 0x80},

	{0, {0}, 0xff},
};	

/*
// Initialization commands for 7735B screens
// ------------------------------------
static const uint8_t STP7735_init[] = {
#if PIN_NUM_RST
  16,                       // 17 commands in list
#else
  17,						// 18 commands in list:
  ST7735_SLPOUT,   TFT_CMD_DELAY,	//  2: Out of sleep mode, no args, w/delay
  255,			           			//     255 = 500 ms delay
#endif
  TFT_CMD_PIXFMT, 1+TFT_CMD_DELAY,	//  3: Set color mode, 1 arg + delay:
  0x06, 							//     18-bit color 6-6-6 color format
  10,	          					//     10 ms delay
  ST7735_FRMCTR1, 3+TFT_CMD_DELAY,	//  4: Frame rate control, 3 args + delay:
  0x00,						//     fastest refresh
  0x06,						//     6 lines front porch
  0x03,						//     3 lines back porch
  10,						//     10 ms delay
  TFT_MADCTL , 1      ,		//  5: Memory access ctrl (directions), 1 arg:
  0x08,						//     Row addr/col addr, bottom to top refresh
  ST7735_DISSET5, 2      ,	//  6: Display settings #5, 2 args, no delay:
  0x15,						//     1 clk cycle nonoverlap, 2 cycle gate
  // rise, 3 cycle osc equalize
  0x02,						//     Fix on VTL
  ST7735_INVCTR , 1      ,	//  7: Display inversion control, 1 arg:
  0x0,						//     Line inversion
  ST7735_PWCTR1 , 2+TFT_CMD_DELAY,	//  8: Power control, 2 args + delay:
  0x02,						//     GVDD = 4.7V
  0x70,						//     1.0uA
  10,						//     10 ms delay
  ST7735_PWCTR2 , 1      ,	//  9: Power control, 1 arg, no delay:
  0x05,						//     VGH = 14.7V, VGL = -7.35V
  ST7735_PWCTR3 , 2      ,	// 10: Power control, 2 args, no delay:
  0x01,						//     Opamp current small
  0x02,						//     Boost frequency
  ST7735_VMCTR1 , 2+TFT_CMD_DELAY,	// 11: Power control, 2 args + delay:
  0x3C,						//     VCOMH = 4V
  0x38,						//     VCOML = -1.1V
  10,						//     10 ms delay
  ST7735_PWCTR6 , 2      ,	// 12: Power control, 2 args, no delay:
  0x11, 0x15,
  ST7735_GMCTRP1,16      ,	// 13: Magical unicorn dust, 16 args, no delay:
  0x09, 0x16, 0x09, 0x20,	//     (seriously though, not sure what
  0x21, 0x1B, 0x13, 0x19,	//      these config values represent)
  0x17, 0x15, 0x1E, 0x2B,
  0x04, 0x05, 0x02, 0x0E,
  ST7735_GMCTRN1,16+TFT_CMD_DELAY,	// 14: Sparkles and rainbows, 16 args + delay:
  0x0B, 0x14, 0x08, 0x1E,	//     (ditto)
  0x22, 0x1D, 0x18, 0x1E,
  0x1B, 0x1A, 0x24, 0x2B,
  0x06, 0x06, 0x02, 0x0F,
  10,						//     10 ms delay
  TFT_CASET  , 4      , 	// 15: Column addr set, 4 args, no delay:
  0x00, 0x02,				//     XSTART = 2
  0x00, 0x81,				//     XEND = 129
  TFT_PASET  , 4      , 	// 16: Row addr set, 4 args, no delay:
  0x00, 0x02,				//     XSTART = 1
  0x00, 0x81,				//     XEND = 160
  ST7735_NORON  ,   TFT_CMD_DELAY,	// 17: Normal display on, no args, w/delay
  10,						//     10 ms delay
  TFT_DISPON ,   TFT_CMD_DELAY,  	// 18: Main screen turn on, no args, w/delay
  255						//     255 = 500 ms delay
};

// Init for 7735R, part 1 (red or green tab)
// --------------------------------------
static const uint8_t  STP7735R_init[] = {
//#if PIN_NUM_RST
  //14,                       // 14 commands in list
//#else
  15,						// 15 commands in list:
  ST7735_SWRESET,   TFT_CMD_DELAY,	//  1: Software reset, 0 args, w/delay
  150,						//     150 ms delay
//#endif
  ST7735_SLPOUT ,   TFT_CMD_DELAY,	//  2: Out of sleep mode, 0 args, w/delay
  255,						//     500 ms delay
  ST7735_FRMCTR1, 3      ,	//  3: Frame rate ctrl - normal mode, 3 args:
  0x01, 0x2C, 0x2D,			//     Rate = fosc/(1x2+40) * (LINE+2C+2D)
  ST7735_FRMCTR2, 3      ,	//  4: Frame rate control - idle mode, 3 args:
  0x01, 0x2C, 0x2D,			//     Rate = fosc/(1x2+40) * (LINE+2C+2D)
  ST7735_FRMCTR3, 6      ,	//  5: Frame rate ctrl - partial mode, 6 args:
  0x01, 0x2C, 0x2D,			//     Dot inversion mode
  0x01, 0x2C, 0x2D,			//     Line inversion mode
  ST7735_INVCTR , 1      ,	//  6: Display inversion ctrl, 1 arg, no delay:
  0x07,						//     No inversion
  ST7735_PWCTR1 , 3      ,	//  7: Power control, 3 args, no delay:
  0xA2,
  0x02,						//     -4.6V
  0x84,						//     AUTO mode
  ST7735_PWCTR2 , 1      ,	//  8: Power control, 1 arg, no delay:
  0xC5,						//     VGH25 = 2.4C VGSEL = -10 VGH = 3 * AVDD
  ST7735_PWCTR3 , 2      ,	//  9: Power control, 2 args, no delay:
  0x0A,						//     Opamp current small
  0x00,						//     Boost frequency
  ST7735_PWCTR4 , 2      ,	// 10: Power control, 2 args, no delay:
  0x8A,						//     BCLK/2, Opamp current small & Medium low
  0x2A,
  ST7735_PWCTR5 , 2      ,	// 11: Power control, 2 args, no delay:
  0x8A, 0xEE,
  ST7735_VMCTR1 , 1      ,	// 12: Power control, 1 arg, no delay:
  0x0E,
  TFT_INVOFF , 0      ,		// 13: Don't invert display, no args, no delay
  TFT_MADCTL , 1      ,		// 14: Memory access control (directions), 1 arg:
  0xC0,						//     row addr/col addr, bottom to top refresh, RGB order
  TFT_CMD_PIXFMT , 1+TFT_CMD_DELAY,	//  15: Set color mode, 1 arg + delay:
  0x06,								//      18-bit color 6-6-6 color format
  10						//     10 ms delay
};

// Init for 7735R, part 2 (green tab only)
// ---------------------------------------
static const uint8_t Rcmd2green[] = {
  2,						//  2 commands in list:
  TFT_CASET  , 4      ,		//  1: Column addr set, 4 args, no delay:
  0x00, 0x02,				//     XSTART = 0
  0x00, 0x7F+0x02,			//     XEND = 129
  TFT_PASET  , 4      ,	    //  2: Row addr set, 4 args, no delay:
  0x00, 0x01,				//     XSTART = 0
  0x00, 0x9F+0x01			//     XEND = 160
};

// Init for 7735R, part 2 (red tab only)
// -------------------------------------
static const uint8_t Rcmd2red[] = {
  2,						//  2 commands in list:
  TFT_CASET  , 4      ,	    //  1: Column addr set, 4 args, no delay:
  0x00, 0x00,				//     XSTART = 0
  0x00, 0x7F,				//     XEND = 127
  TFT_PASET  , 4      ,	    //  2: Row addr set, 4 args, no delay:
  0x00, 0x00,				//     XSTART = 0
  0x00, 0x9F				//     XEND = 159
};

// Init for 7735R, part 3 (red or green tab)
// -----------------------------------------
static const uint8_t Rcmd3[] = {
  4,						//  4 commands in list:
  ST7735_GMCTRP1, 16      ,	//  1: Magical unicorn dust, 16 args, no delay:
  0x02, 0x1c, 0x07, 0x12,
  0x37, 0x32, 0x29, 0x2d,
  0x29, 0x25, 0x2B, 0x39,
  0x00, 0x01, 0x03, 0x10,
  ST7735_GMCTRN1, 16      ,	//  2: Sparkles and rainbows, 16 args, no delay:
  0x03, 0x1d, 0x07, 0x06,
  0x2E, 0x2C, 0x29, 0x2D,
  0x2E, 0x2E, 0x37, 0x3F,
  0x00, 0x00, 0x02, 0x10,
  ST7735_NORON  ,    TFT_CMD_DELAY,	//  3: Normal display on, no args, w/delay
  10,						//     10 ms delay
  TFT_DISPON ,    TFT_CMD_DELAY,	//  4: Main screen turn on, no args w/delay
  100						//     100 ms delay
}; 
*/
#define LCD_CMD_LEV   (0)
#define LCD_DATA_LEV  (1)

/*This function is called (in irq context!) just before a transmission starts.
It will set the D/C line to the value indicated in the user field */
void lcd_spi_pre_transfer_callback(spi_transaction_t *t)
{
    lcd_dc_t *dc = (lcd_dc_t *) t->user;
    gpio_set_level((int)dc->dc_io, (int)dc->dc_level);
}

static SemaphoreHandle_t _spi_mux = NULL;
static esp_err_t _lcd_spi_send(spi_device_handle_t spi, spi_transaction_t* t)
{
    xSemaphoreTake(_spi_mux, portMAX_DELAY);
    esp_err_t res = spi_device_transmit(spi, t); //Transmit!
    xSemaphoreGive(_spi_mux);
    return res;
}

void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd, lcd_dc_t *dc)
{
    esp_err_t ret;
    dc->dc_level = LCD_CMD_LEV;
    spi_transaction_t t = {
        .length = 8,                    // Command is 8 bits
        .tx_buffer = &cmd,              // The data is the cmd itself
        .user = (void *) dc,            // D/C needs to be set to 0
    };
    ret = _lcd_spi_send(spi, &t);       // Transmit!
    assert(ret == ESP_OK);              // Should have had no issues.
}

void lcd_data(spi_device_handle_t spi, const uint8_t *data, int len, lcd_dc_t *dc)
{
    esp_err_t ret;
    if (len == 0) {
        return;    //no need to send anything
    }
    dc->dc_level = LCD_DATA_LEV;

    spi_transaction_t t = {
        .length = len * 8,              // Len is in bytes, transaction length is in bits.
        .tx_buffer = data,              // Data
        .user = (void *) dc,            // D/C needs to be set to 1
    };
    ret = _lcd_spi_send(spi, &t);       // Transmit!
    assert(ret == ESP_OK);              // Should have had no issues.
}

//LCD Init For 1.44Inch LCD Panel with ST7735R.
void Lcd_Init(spi_device_handle_t spi, lcd_dc_t *dc)
{	
	//LCD_GPIO_Init();
	//Lcd_Reset(); //Reset before LCD Init.

	//LCD Init For 1.44Inch LCD Panel with ST7735R.
	dc->dc_level = LCD_CMD_LEV;
	lcd_cmd(spi, 0x11, dc); 
	vTaskDelay(120 / portTICK_RATE_MS);
	//Lcd_WriteIndex(0x11);//Sleep exit 
	//delay_ms (120);
	
	dc->dc_level = LCD_CMD_LEV;
	lcd_cmd(spi, 0xB1, dc); 
	uint8_t cmd[] = {0x01, 0x2C, 0x2D};
	//lcd_cmd(spi, cmd, 3, dc); 
	//ST7735R Frame Rate
	//Lcd_WriteIndex(0xB1); 
	//Lcd_WriteData(0x01); 
	//Lcd_WriteData(0x2C); 
	//Lcd_WriteData(0x2D); 
/*
	Lcd_WriteIndex(0xB2); 
	Lcd_WriteData(0x01); 
	Lcd_WriteData(0x2C); 
	Lcd_WriteData(0x2D); 

	Lcd_WriteIndex(0xB3); 
	Lcd_WriteData(0x01); 
	Lcd_WriteData(0x2C); 
	Lcd_WriteData(0x2D); 
	Lcd_WriteData(0x01); 
	Lcd_WriteData(0x2C); 
	Lcd_WriteData(0x2D); 
	
	Lcd_WriteIndex(0xB4); //Column inversion 
	Lcd_WriteData(0x07); 
	
	//ST7735R Power Sequence
	Lcd_WriteIndex(0xC0); 
	Lcd_WriteData(0xA2); 
	Lcd_WriteData(0x02); 
	Lcd_WriteData(0x84); 
	Lcd_WriteIndex(0xC1); 
	Lcd_WriteData(0xC5); 

	Lcd_WriteIndex(0xC2); 
	Lcd_WriteData(0x0A); 
	Lcd_WriteData(0x00); 

	Lcd_WriteIndex(0xC3); 
	Lcd_WriteData(0x8A); 
	Lcd_WriteData(0x2A); 
	Lcd_WriteIndex(0xC4); 
	Lcd_WriteData(0x8A); 
	Lcd_WriteData(0xEE); 
	
	Lcd_WriteIndex(0xC5); //VCOM 
	Lcd_WriteData(0x0E); 
	
	Lcd_WriteIndex(0x36); //MX, MY, RGB mode 
	Lcd_WriteData(0xC0); 
	
	//ST7735R Gamma Sequence
	Lcd_WriteIndex(0xe0); 
	Lcd_WriteData(0x0f); 
	Lcd_WriteData(0x1a); 
	Lcd_WriteData(0x0f); 
	Lcd_WriteData(0x18); 
	Lcd_WriteData(0x2f); 
	Lcd_WriteData(0x28); 
	Lcd_WriteData(0x20); 
	Lcd_WriteData(0x22); 
	Lcd_WriteData(0x1f); 
	Lcd_WriteData(0x1b); 
	Lcd_WriteData(0x23); 
	Lcd_WriteData(0x37); 
	Lcd_WriteData(0x00); 	
	Lcd_WriteData(0x07); 
	Lcd_WriteData(0x02); 
	Lcd_WriteData(0x10); 

	Lcd_WriteIndex(0xe1); 
	Lcd_WriteData(0x0f); 
	Lcd_WriteData(0x1b); 
	Lcd_WriteData(0x0f); 
	Lcd_WriteData(0x17); 
	Lcd_WriteData(0x33); 
	Lcd_WriteData(0x2c); 
	Lcd_WriteData(0x29); 
	Lcd_WriteData(0x2e); 
	Lcd_WriteData(0x30); 
	Lcd_WriteData(0x30); 
	Lcd_WriteData(0x39); 
	Lcd_WriteData(0x3f); 
	Lcd_WriteData(0x00); 
	Lcd_WriteData(0x07); 
	Lcd_WriteData(0x03); 
	Lcd_WriteData(0x10);  
	
	Lcd_WriteIndex(0x2a);
	Lcd_WriteData(0x00);
	Lcd_WriteData(0x00);
	Lcd_WriteData(0x00);
	Lcd_WriteData(0x7f);

	Lcd_WriteIndex(0x2b);
	Lcd_WriteData(0x00);
	Lcd_WriteData(0x00);
	Lcd_WriteData(0x00);
	Lcd_WriteData(0x9f);
	
	Lcd_WriteIndex(0xF0); //Enable test command  
	Lcd_WriteData(0x01); 
	Lcd_WriteIndex(0xF6); //Disable ram power save mode 
	Lcd_WriteData(0x00); 
	
	Lcd_WriteIndex(0x3A); //65k mode 
	Lcd_WriteData(0x05); 
	
	
	Lcd_WriteIndex(0x29);//Display on	 
	*/
}


static void commandList(spi_device_handle_t spi, const uint8_t *addr, lcd_dc_t *dc) {
  uint8_t  numCommands, numArgs, cmd;
  uint16_t ms;

  numCommands = *addr++;				// Number of commands to follow
  while(numCommands--) {				// For each command...
    cmd = *addr++;						// save command
    numArgs  = *addr++;					// Number of args to follow
    ms       = numArgs & TFT_CMD_DELAY;	// If high bit set, delay follows args
    numArgs &= ~TFT_CMD_DELAY;			// Mask out delay bit

	//disp_spi_transfer_cmd_data(cmd, (uint8_t *)addr, numArgs);
	lcd_cmd(spi, cmd, dc);
	//lcd_data(*spi_wr_dev, lcd_init_cmds[cmd].data, lcd_init_cmds[cmd].databytes&0x1F, dc);
	//lcd_data(spi, cmd, dc);

	addr += numArgs;

    if(ms) {
      ms = *addr++;              // Read post-command delay time (ms)
      if(ms == 255) ms = 500;    // If 255, delay for 500 ms
	  vTaskDelay(ms / portTICK_RATE_MS);
    }
  }
}

uint32_t lcd_init(lcd_conf_t* lcd_conf, spi_device_handle_t *spi_wr_dev, lcd_dc_t *dc, int dma_chan)
{

    if (_spi_mux == NULL) {
        _spi_mux = xSemaphoreCreateMutex();
    }
    //Initialize non-SPI GPIOs
    gpio_pad_select_gpio(lcd_conf->pin_num_dc);
    gpio_set_direction(lcd_conf->pin_num_dc, GPIO_MODE_OUTPUT);

    //Reset the display
    if (lcd_conf->pin_num_rst < GPIO_NUM_MAX) {
        gpio_pad_select_gpio(lcd_conf->pin_num_rst);
        gpio_set_direction(lcd_conf->pin_num_rst, GPIO_MODE_OUTPUT);
        gpio_set_level(lcd_conf->pin_num_rst, (lcd_conf->rst_active_level) & 0x1);
        vTaskDelay(100 / portTICK_RATE_MS);
        gpio_set_level(lcd_conf->pin_num_rst, (~(lcd_conf->rst_active_level)) & 0x1);
        vTaskDelay(100 / portTICK_RATE_MS);
    }

    if (lcd_conf->init_spi_bus) {
        //Initialize SPI Bus for LCD
        spi_bus_config_t buscfg = {
            .miso_io_num = lcd_conf->pin_num_miso,
            .mosi_io_num = lcd_conf->pin_num_mosi,
            .sclk_io_num = lcd_conf->pin_num_clk,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
        };
        spi_bus_initialize(lcd_conf->spi_host, &buscfg, dma_chan);
    }

    spi_device_interface_config_t devcfg = {
        // Use low speed to read ID.
        .clock_speed_hz = 1 * 1000 * 1000,     //Clock out frequency
        .mode = 0,                                //SPI mode 0
        .spics_io_num = lcd_conf->pin_num_cs,     //CS pin
        .queue_size = 7,                          //We want to be able to queue 7 transactions at a time
        .pre_cb = lcd_spi_pre_transfer_callback,  //Specify pre-transfer callback to handle D/C line
    };
    spi_device_handle_t rd_id_handle;
    spi_bus_add_device(lcd_conf->spi_host, &devcfg, &rd_id_handle);
    uint32_t lcd_id = lcd_get_id(rd_id_handle, dc);
    spi_bus_remove_device(rd_id_handle);

    // Use high speed to write LCD
    devcfg.clock_speed_hz = lcd_conf->clk_freq;
    devcfg.flags = SPI_DEVICE_HALFDUPLEX;
    spi_bus_add_device(lcd_conf->spi_host, &devcfg, spi_wr_dev);

    int cmd = 0;
    const lcd_init_cmd_t* lcd_init_cmds = NULL;
	
    if(lcd_conf->lcd_model == LCD_MOD_ST7789) {
        lcd_init_cmds = st7789_init_cmds;
    } else if(lcd_conf->lcd_model == LCD_MOD_ILI9341) {       
        lcd_init_cmds = ili_init_cmds;
    } else if(lcd_conf->lcd_model == LCD_MOD_AUTO_DET) {
        if (((lcd_id >> 8) & 0xff) == 0x42) {
            lcd_init_cmds = st7789_init_cmds;
        } else {
            lcd_init_cmds = ili_init_cmds;
        }
    }
    
    
    //lcd_init_cmds = st7735R_init_cmds;
	//lcd_init_cmds = st7735R_init_cmds;
	//lcd_init_cmds = ili_init_cmds;
    //lcd_init_cmds = ili9488_init_cmds;
	//lcd_init_cmds = st7735R_init_cmds;
    //lcd_init_cmds = st7796s_init_cmds;
    assert(lcd_init_cmds != NULL);
    //Send all the commands
    while (lcd_init_cmds[cmd].databytes!=0xff) {
        lcd_cmd(*spi_wr_dev, lcd_init_cmds[cmd].cmd, dc);
        lcd_data(*spi_wr_dev, lcd_init_cmds[cmd].data, lcd_init_cmds[cmd].databytes&0x1F, dc);
        if (lcd_init_cmds[cmd].databytes&0x80) {
            vTaskDelay(100 / portTICK_RATE_MS);
        }
        cmd++;
    }

	//commandList(*spi_wr_dev, STP7735R_init, dc);
	//commandList(*spi_wr_dev, Rcmd2green, dc);
	//commandList(*spi_wr_dev, Rcmd3, dc);
	//commandList(*spi_wr_dev, STP7735_init, dc);

    //Enable backlight
    if (lcd_conf->pin_num_bckl < GPIO_NUM_MAX) {
        gpio_pad_select_gpio(lcd_conf->pin_num_bckl);
        gpio_set_direction(lcd_conf->pin_num_bckl, GPIO_MODE_OUTPUT);
        gpio_set_level(lcd_conf->pin_num_bckl, (lcd_conf->bckl_active_level) & 0x1);
    }
    return lcd_id;
}

void lcd_send_uint16_r(spi_device_handle_t spi, const uint16_t data, int32_t repeats, lcd_dc_t *dc)
{
    uint32_t i;
    uint32_t word = data << 16 | data;
    uint32_t word_tmp[16];
    spi_transaction_t t;
    dc->dc_level = LCD_DATA_LEV;

    while (repeats > 0) {
        uint16_t bytes_to_transfer = MIN(repeats * sizeof(uint16_t), SPIFIFOSIZE * sizeof(uint32_t));
        for (i = 0; i < (bytes_to_transfer + 3) / 4; i++) {
            word_tmp[i] = word;
        }

        memset(&t, 0, sizeof(t));           //Zero out the transaction
        t.length = bytes_to_transfer * 8;   //Len is in bytes, transaction length is in bits.
        t.tx_buffer = word_tmp;             //Data
        t.user = (void *) dc;               //D/C needs to be set to 1
        _lcd_spi_send(spi, &t);             //Transmit!
        repeats -= bytes_to_transfer / 2;
    }
}

uint32_t lcd_get_id(spi_device_handle_t spi, lcd_dc_t *dc)
{
    //get_id cmd
    lcd_cmd( spi, 0x04, dc);

    spi_transaction_t t;
    dc->dc_level = LCD_DATA_LEV;
    memset(&t, 0, sizeof(t));
    t.length = 8 * 4;
    t.flags = SPI_TRANS_USE_RXDATA;
    t.user = (void *) dc;
    esp_err_t ret = _lcd_spi_send(spi, &t);
    assert( ret == ESP_OK );

    return *(uint32_t*) t.rx_data;
}

