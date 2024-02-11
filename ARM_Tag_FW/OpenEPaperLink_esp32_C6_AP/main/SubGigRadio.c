#include "sdkconfig.h"
#ifdef CONFIG_OEPL_SUBGIG_SUPPORT
#include <stddef.h>
#include <string.h>
#include <ctype.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <driver/spi_master.h>
#include <driver/gpio.h>
#include "esp_log.h"

#include "radio.h"
#include "cc1101_radio.h"
#include "SubGigRadio.h"

void DumpHex(void *AdrIn,int Len);
#define LOG(format, ... ) printf("%s: " format,__FUNCTION__,## __VA_ARGS__)
#define LOG_RAW(format, ... ) printf(format,## __VA_ARGS__)
#define LOG_HEX(x,y) DumpHex(x,y)


// SPI Stuff
#if CONFIG_SPI2_HOST
   #define HOST_ID SPI2_HOST
#elif CONFIG_SPI3_HOST
   #define HOST_ID SPI3_HOST
#endif

// Address Config = No address check 
// Base Frequency = xxx.xxx
// CRC Enable = false 
// Carrier Frequency = 915.000000 
// Channel Number = 0 
// Channel Spacing = 199.951172 
// Data Rate = 1.19948 
// Deviation = 5.157471 
// Device Address = 0 
// Manchester Enable = false 
// Modulated = false
// Modulation Format = ASK/OOK 
// PA Ramping = false 
// Packet Length = 255 
// Packet Length Mode = Reserved 
// Preamble Count = 4 
// RX Filter BW = 58.035714 
// Sync Word Qualifier Mode = No preamble/sync 
// TX Power = 10 
// Whitening = false 
// Rf settings for CC1110
const RfSetting gCW[] = {
   {CC1101_PKTCTRL0,0x22},  // PKTCTRL0: Packet Automation Control 
   {CC1101_FSCTRL1,0x06},  // FSCTRL1: Frequency Synthesizer Control 
   {CC1101_MDMCFG4,0xF5},  // MDMCFG4: Modem configuration 
   {CC1101_MDMCFG3,0x83},  // MDMCFG3: Modem Configuration 
   {CC1101_MDMCFG2,0xb0},  // MDMCFG2: Modem Configuration 
   {CC1101_DEVIATN,0x15},  // DEVIATN: Modem Deviation Setting 
   {CC1101_MCSM0,0x18},  // MCSM0: Main Radio Control State Machine Configuration 
   {CC1101_FOCCFG,0x17},  // FOCCFG: Frequency Offset Compensation Configuration 
   {CC1101_FSCAL3,0xE9},  // FSCAL3: Frequency Synthesizer Calibration 
   {CC1101_FSCAL2,0x2A},  // FSCAL2: Frequency Synthesizer Calibration 
   {CC1101_FSCAL1,0x00},  // FSCAL1: Frequency Synthesizer Calibration 
   {CC1101_FSCAL0,0x1F},  // FSCAL0: Frequency Synthesizer Calibration 
   {CC1101_TEST1,0x31},  // TEST1: Various Test Settings 
   {CC1101_TEST0,0x09},  // TEST0: Various Test Settings 
   {0xff,0}   // end of table
};


// Set Base Frequency to 865.999634 
const RfSetting g866Mhz[] = {
   {CC1101_FREQ2,0x21},  // FREQ2: Frequency Control Word, High Byte 
   {CC1101_FREQ1,0x4e},  // FREQ1: Frequency Control Word, Middle Byte 
   {CC1101_FREQ0,0xc4},  // FREQ0: Frequency Control Word, Low Byte 
   {0xff,0}   // end of table
};

// Seet Base Frequency to 915.000000 
const RfSetting g915Mhz[] = {
   {CC1101_FREQ2,0x23},  // FREQ2: Frequency Control Word, High Byte 
   {CC1101_FREQ1,0x31},  // FREQ1: Frequency Control Word, Middle Byte 
   {CC1101_FREQ0,0x3B},  // FREQ0: Frequency Control Word, Low Byte 
   {0xff,0}   // end of table
};

// Address Config = No address check 
// Base Frequency = 901.934937 (adjusted to compensate for the crappy crystal on the CC1101 board)
// CRC Enable = true 
// Carrier Frequency = 901.934937 
// Channel Number = 0 
// Channel Spacing = 199.951172 
// Data Rate = 38.3835 
// Deviation = 20.629883 
// Device Address = ff 
// Manchester Enable = false 
// Modulated = true 
// Modulation Format = GFSK 
// PA Ramping = false 
// Packet Length = 61 
// Packet Length Mode = Variable packet length mode. Packet length configured by the first byte after sync word 
// Preamble Count = 4 
// RX Filter BW = 101.562500 
// Sync Word Qualifier Mode = 30/32 sync word bits detected 
// TX Power = 10 
// Whitening = false 
// The following was generated by setting the spec for Register to "{CC1101_@RN@,0x@VH@}," 
const RfSetting gIDF_Basic[] = {
    {CC1101_SYNC1,0xC7},
    {CC1101_SYNC0,0x0A},
    {CC1101_PKTLEN,0x3D},
    {CC1101_PKTCTRL0,0x05},
    {CC1101_ADDR,0xFF},
    {CC1101_FSCTRL1,0x08},
    {CC1101_FREQ2,0x22},
    {CC1101_FREQ1,0xB1},
    {CC1101_FREQ0,0x3B},
    {CC1101_MDMCFG4,0xCA},
    {CC1101_MDMCFG3,0x83},
    {CC1101_MDMCFG2,0x93},
    {CC1101_DEVIATN,0x35},
//   {CC1101_MCSM0,0x18},   FS_AUTOCAL = 1, PO_TIMEOUT = 2
    {CC1101_MCSM0,0x18},
    {CC1101_FOCCFG,0x16},
    {CC1101_AGCCTRL2,0x43},
    {CC1101_FSCAL3,0xEF},
    {CC1101_FSCAL2,0x2D},
    {CC1101_FSCAL1,0x25},
    {CC1101_FSCAL0,0x1F},
    {CC1101_TEST2,0x81},
    {CC1101_TEST1,0x35},
    {CC1101_TEST0,0x09},
    {0xff,0}   // end of table
};

// RF configuration from Dimitry's orginal code 
// Address Config = No address check 
// Base Frequency = 902.999756 
// CRC Autoflush = false 
// CRC Enable = true 
// Carrier Frequency = 902.999756 
// Channel Number = 0 
// Channel Spacing = 335.632324 
// Data Format = Normal mode 
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
// Whitening = false 
// Rf settings for CC1101
// The following was generated by setting the spec for Register to "{CC1101_@RN@,0x@VH@}," 
const RfSetting gDmitry915[] = {
   {CC1101_IOCFG0,0x06},
   {CC1101_PKTCTRL0,0x05},
   {CC1101_FREQ2,0x22},
   {CC1101_FREQ1,0xBB},
   {CC1101_FREQ0,0x13},
   {CC1101_MDMCFG4,0x1D},
   {CC1101_MDMCFG3,0x3B},
   {CC1101_MDMCFG2,0x13},
   {CC1101_MDMCFG1,0x73},
   {CC1101_MDMCFG0,0xA7},
   {CC1101_DEVIATN,0x65},
   {CC1101_MCSM0,0x18},
   {CC1101_FOCCFG,0x1E},
   {CC1101_BSCFG,0x1C},
   {CC1101_AGCCTRL2,0xC7},
   {CC1101_AGCCTRL1,0x00},
   {CC1101_AGCCTRL0,0xB0},
   {CC1101_FREND1,0xB6},
   {CC1101_FSCAL3,0xEA},
   {CC1101_FSCAL2,0x2A},
   {CC1101_FSCAL1,0x00},
   {CC1101_FSCAL0,0x1F},
   {CC1101_TEST0,0x09},
   {0xff,0}   // end of table
};

SubGigData gSubGigData;

int CheckSubGigState(void);
void SubGig_CC1101_reset(void);
void SubGig_CC1101_SetConfig(const RfSetting *pConfig);

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
   gSubGigData.RxAvailable = true;
}

SubGigErr SubGig_radio_init(uint8_t ch)
{
   esp_err_t Err;
   const RfSetting *pConfig = NULL;
   spi_device_interface_config_t devcfg = {
      .clock_speed_hz = 5000000, // SPI clock is 5 MHz!
      .queue_size = 7,
      .mode = 0, // SPI mode 0
      .spics_io_num = -1, // we will use manual CS control
      .flags = SPI_DEVICE_NO_DUMMY
   };
   gpio_config_t io_conf = {
      .intr_type = GPIO_INTR_NEGEDGE, // GPIO interrupt type : falling edge
      //bit mask of the pins
      .pin_bit_mask = 1ULL<<CONFIG_GDO0_GPIO,
      //set as input mode
      .mode = GPIO_MODE_INPUT,
      //enable pull-up mode
      .pull_up_en = 1,
      .pull_down_en = 0
   };
   int ErrLine = 0;
   SubGigErr Ret = SUBGIG_ERR_NONE;

   do {
      gpio_reset_pin(CONFIG_CSN_GPIO);
      gpio_set_direction(CONFIG_CSN_GPIO, GPIO_MODE_OUTPUT);
      gpio_set_level(CONFIG_CSN_GPIO, 1);

      spi_bus_config_t buscfg = {
         .sclk_io_num = CONFIG_SCK_GPIO,
         .mosi_io_num = CONFIG_MOSI_GPIO,
         .miso_io_num = CONFIG_MISO_GPIO,
         .quadwp_io_num = -1,
         .quadhd_io_num = -1
      };

      if((Err = spi_bus_initialize(HOST_ID,&buscfg,SPI_DMA_CH_AUTO)) != 0) {
         ErrLine = __LINE__;
         break;
      }

      if((Err = spi_bus_add_device(HOST_ID,&devcfg,&gSpiHndl)) != 0) {
         ErrLine = __LINE__;
         break;
      }

   // interrupt on falling edge
      if((Err = gpio_config(&io_conf)) != 0) {
         ErrLine = __LINE__;
         break;
      }

   // install gpio isr service
      if((Err = gpio_install_isr_service(0)) != 0) {
         ErrLine = __LINE__;
         break;
      }
      //hook isr handler for specific gpio pin
      Err = gpio_isr_handler_add(CONFIG_GDO0_GPIO,gpio_isr_handler,
                                 (void*) CONFIG_GDO0_GPIO);
      if(Err != 0) {
         ErrLine = __LINE__;
         break;
      }
      // Check Chip ID
      if(!CC1101_Present()) {
         Ret = SUBGIG_CC1101_NOT_FOUND;
      }
      gSubGigData.Present = true;
   // Eventually set 866/915 config based on channel argument
      SubGig_CC1101_reset();
      pConfig = gDmitry915;
//    pConfig = gIDF_Basic;

      SubGig_CC1101_SetConfig(pConfig);
      gSubGigData.pConfig = pConfig;
      gSubGigData.Enabled = true;
#if 1
      CC1101_DumpRegs();
#endif
   // good to go!
      Ret = true;
   } while(false);

   if(ErrLine != 0) {
      LOG("%s#%d: failed %d\n",__FUNCTION__,ErrLine,Err);
   }
   return Ret;
}

SubGigErr SubGig_radioSetChannel(uint8_t ch)
{
   return SUBGIG_ERR_NONE;
}

SubGigErr SubGig_radioTx(uint8_t *packet)
{
   SubGigErr Ret = SUBGIG_ERR_NONE;
   uint8_t TxLen;

   do {
      if(gSubGigData.FreqTest) {
         break;
      }
      if((Ret = CheckSubGigState()) != SUBGIG_ERR_NONE) {
         break;
      }

      TxLen = packet[0];
      if(CC1101_Tx(&packet[1],TxLen)) {
         Ret = SUBGIG_TX_FAILED;
      }
   } while(false);

   return Ret;
}

// returns packet size in bytes data in data
int8_t SubGig_commsRxUnencrypted(uint8_t *data)
{
   int RxBytes;
   int8_t Ret = 0;

   do {
      if(CheckSubGigState() != SUBGIG_ERR_NONE) {
         break;
      }

      if(gSubGigData.FreqTest) {
         break;
      }
      if(gSubGigData.RxAvailable) {
         gSubGigData.RxAvailable = false;
         RxBytes = CC1101_Rx(data,128,NULL,NULL);

         if(RxBytes > 0) {
//          Ret = (uint8_t) RxBytes;
            LOG("Received %d byte subgig frame:\n",RxBytes);
            LOG_HEX(data,RxBytes);
         }
      }
   } while(false);

   return Ret;
}

int CheckSubGigState()
{
   int Err = SUBGIG_ERR_NONE;

   if(!gSubGigData.Present) {
      Err = SUBGIG_CC1101_NOT_FOUND;
   }
   else if(!gSubGigData.Initialized) {
      Err = SUBGIG_NOT_INITIALIZED;
   }
   else if(!gSubGigData.Enabled) {
      Err = SUBGIG_NOT_ENABLED;
   }

   if(Err != SUBGIG_ERR_NONE) {
      LOG("CheckSubGigState: returing %d\n",Err);
   }

   return Err;
}

SubGigErr SubGig_FreqTest(bool b866Mhz,bool bStart)
{
   SubGigErr Err = SUBGIG_ERR_NONE;

   do {
      if((Err = CheckSubGigState()) != SUBGIG_ERR_NONE) {
         break;
      }
      if(bStart) {
         LOG_RAW("Starting %sMhz Freq test\n",b866Mhz ? "866" : "915");
         SubGig_CC1101_reset();
         SubGig_CC1101_SetConfig(gCW);
         SubGig_CC1101_SetConfig(b866Mhz ? g866Mhz : g915Mhz);
         CC1101_Tx(NULL,0);
         gSubGigData.FreqTest = true;
      }
      else {
         LOG_RAW("Ending Freq test\n");
         gSubGigData.FreqTest = false;
         SubGig_CC1101_reset();
         SubGig_CC1101_SetConfig(gSubGigData.pConfig);
      }
   } while(false);

   return Err;
}

void SubGig_CC1101_reset()
{
   gSubGigData.Initialized = false;
   CC1101_reset();
}

void SubGig_CC1101_SetConfig(const RfSetting *pConfig)
{
   CC1101_SetConfig(pConfig);
   gSubGigData.Initialized = true;
}

void DumpHex(void *AdrIn,int Len)
{
   unsigned char *Adr = (unsigned char *) AdrIn;
   int i = 0;
   int j;

   while(i < Len) {
      for(j = 0; j < 16; j++) {
         if((i + j) == Len) {
            break;
         }
         LOG_RAW("%02x ",Adr[i+j]);
      }

      LOG_RAW(" ");
      for(j = 0; j < 16; j++) {
         if((i + j) == Len) {
            break;
         }
         if(isprint(Adr[i+j])) {
            LOG_RAW("%c",Adr[i+j]);
         }
         else {
            LOG_RAW(".");
         }
      }
      i += 16;
      LOG_RAW("\n");
   }
}

#endif // CONFIG_OEPL_SUBGIG_SUPPORT

