#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ch.h"
#include "hal.h"

#include "shell.h"
#include "fast_bus.h"
#include "usbcfg.h"
#include "chprintf.h"
#include "chbsem.h"
#include "RFM69.h"
#include "board_specific.h"

#define SHELL_WA_SIZE   THD_WORKING_AREA_SIZE(2048)
#define USB_PA12_DISCONNECT 1

#define GATEWAY_ID    1
#define NODE_ID       1    // node ID if this isn't what the sending node expects then ACKs won't work!
#define NETWORKID     101    //the same on all nodes that talk to each other
#define MSG_INTERVAL  100

// Uncomment only one of the following three to match radio frequency
//#define FREQUENCY     RF69_433MHZ    
#define FREQUENCY     RF69_868MHZ
//#define FREQUENCY     RF69_915MHZ

#define IS_RFM69HW   //NOTE: uncomment this ONLY for RFM69HW or RFM69HCW
#define ENCRYPT_KEY    "EncryptKey123456"  // use same 16byte encryption key for all devices on net

#define MSGBUFSIZE 128

/*
 * Low speed SPI configuration (281.250kHz, CPHA=0, CPOL=0, MSb first).
 */
static const SPIConfig ls_spicfg = {
  NULL,
  GPIOA,
  4,
  SPI_CR1_BR_2 | SPI_CR1_BR_1,
  0
};

IRQWrapper irq = IRQWrapper(GPIOB, 0, 0); /* GPIOB0 ext channel 0 */
RFM69 radio = RFM69(&SPID1, &ls_spicfg, &irq, true);

static const EXTConfig extcfg = {
  {
    {EXT_CH_MODE_RISING_EDGE | EXT_CH_MODE_AUTOSTART | EXT_MODE_GPIOB, extcb1}, /* PORTB0 */
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL}
  }
};

static const I2CConfig i2cfg1 = {
    OPMODE_I2C,
    400000,
    FAST_DUTY_CYCLE_2,
};

void board_init()
{
	/* This is never done in the testhal code for I2C but appears to be critical to correct operation. */
	palSetPadMode(GPIOB, 10, PAL_MODE_ALTERNATE(4));
	palSetPadMode(GPIOB, 11, PAL_MODE_ALTERNATE(4));

	/*
	* SPI1 I/O pins setup.
	*/
	palSetPadMode(GPIOA, 5, PAL_MODE_ALTERNATE(5));     /* SCK. */
	palSetPadMode(GPIOA, 6, PAL_MODE_ALTERNATE(5));     /* MISO.*/
	palSetPadMode(GPIOA, 7, PAL_MODE_ALTERNATE(5));     /* MOSI.*/
	palSetPadMode(GPIOA, 4, PAL_MODE_OUTPUT_PUSHPULL); /* NSS */
	palSetPadMode(GPIOB, 0, PAL_MODE_INPUT_PULLDOWN); // IRQ
	palClearPad(GPIOA, 4);

	i2cStart(&I2CD2, &i2cfg1);
	/*
	* Initializes a serial-over-USB CDC driver.
	*/
	sduObjectInit(&SDU1);
	sduStart(&SDU1, &serusbcfg);
	extStart(&EXTD1, &extcfg);
	/*
	* Activates the USB driver and then the USB bus pull-up on D+.
	* Note, a delay is inserted in order to not have to disconnect the cable
	* after a reset.
	*/
	usbDisconnectBus(serusbcfg.usbp);
#ifdef USB_PA12_DISCONNECT
	palSetPadMode(GPIOA, 12, PAL_MODE_OUTPUT_PUSHPULL);
	palClearPad(GPIOA, 12);
	chThdSleepMilliseconds(1500);
	palSetPadMode(GPIOA, 12, PAL_MODE_ALTERNATE(10));
#endif
	usbStart(serusbcfg.usbp, &usbcfg);
	usbConnectBus(serusbcfg.usbp);

	/*
	* Activates the serial driver 2 at 115200 baud.
	* PA2(TX) and PA3(RX) are routed to USART2.
	*/
	SerialConfig conf;
	conf.speed = 115200;
	sdStart(&SD2, &conf);
	palSetPadMode(GPIOA, 2, PAL_MODE_ALTERNATE(7));
	palSetPadMode(GPIOA, 3, PAL_MODE_ALTERNATE(7));
	palSetPadMode(LED1_PORT, LED1_PAD, PAL_MODE_OUTPUT_PUSHPULL);
	palSetPadMode(LED1_PORT, 7, PAL_MODE_OUTPUT_PUSHPULL);
}


