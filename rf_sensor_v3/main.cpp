/*
    ChibiOS - Copyright (C) 2006..2016 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/
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

#define SHELL_WA_SIZE   THD_WORKING_AREA_SIZE(2048)
#define USB_PA12_DISCONNECT 1

#define GATEWAY_ID    1
#define NODE_ID       9    // node ID
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
static thread_reference_t trp = NULL;

/* Can be measured using dd if=/dev/xxxx of=/dev/null bs=512 count=10000.*/
static void cmd_date(BaseSequentialStream *chp, int argc, char *argv[]) {
  struct tm tim;
  static RTCDateTime timespec;
  
  if (argc > 0)
  {
      time_t time = atoi(argv[0]);
      tim = *gmtime(&time);
      chprintf(chp,"%d\n",atoi(argv[0]));
      
      rtcConvertStructTmToDateTime(&tim, 0, &timespec);
      rtcSetTime(&RTCD1, &timespec);
	
  }
  else
  {
    rtcGetTime(&RTCD1, &timespec);
    rtcConvertDateTimeToStructTm(&timespec, &tim, NULL);
  }
  chprintf(chp,"\nDate: %2d:%02d:%02d\n",tim.tm_hour,tim.tm_min,tim.tm_sec); 
  
  chprintf((BaseSequentialStream *)&SDU1,"%d\r\n",irq.Get());
  chprintf(chp, "\r\n\nback to shell! %d\r\n",radio.readTemperature(0));
}

/* Can be measured using dd if=/dev/xxxx of=/dev/null bs=512 count=10000.*/
static void cmd_write(BaseSequentialStream *chp, int argc, char *argv[]) 
{ 
  chprintf(chp, "\r\n\nAttempting to write to fast bus\r\n");
  char msg[64] = "hello world";
  
  FastBus1.Write((uint8_t *)msg, strlen(msg), 1000);
  
  chprintf(chp, "\r\n\nback to shell!\r\n");
}

/* Can be measured using dd if=/dev/xxxx of=/dev/null bs=512 count=10000.*/
static void cmd_read(BaseSequentialStream *chp, int argc, char *argv[]) 
{ 
  chprintf(chp, "\r\n\nAttempting to read from fast bus\r\n");
  char msg[65];
  
  msg[0] = 0;
  
  size_t retval;
  
  retval = FastBus1.Read((uint8_t *)msg, sizeof(msg), 1000);
  
  chprintf(chp, "\r\n\nback to shell! (%d) %s\r\n",retval,msg);
}

bool mppl3115a2_setup(BaseSequentialStream *chp);
int readTemp(int *temperature);
int readPressure();

/* Can be measured using dd if=/dev/xxxx of=/dev/null bs=512 count=10000.*/
static void cmd_i2c(BaseSequentialStream *chp, int argc, char *argv[]) 
{ 
  msg_t status = MSG_OK;
  uint8_t i2c_rx_data[8];
  uint8_t i2c_tx_data[8];
  chprintf(chp, "\r\n\nAttempting to read from I2C\r\n");
   
  if (mppl3115a2_setup(chp))
  {     
      int temp;
      
      chprintf(chp, "Pressure %d\r\n", readPressure());
      
      readTemp(&temp);
      
      chprintf(chp, "\r\n\nback to shell! %d (%d)\r\n", temp ,status);
  }
}

/* Can be measured using dd if=/dev/xxxx of=/dev/null bs=512 count=10000.*/
static void cmd_spi(BaseSequentialStream *chp, int argc, char *argv[]) 
{ 
  msg_t status = MSG_OK;
  uint8_t i2c_rx_data[8];
  uint8_t i2c_tx_data[8];
  chprintf(chp, "\r\n\nAttempting to read from I2C\r\n");
   
  if (mppl3115a2_setup(chp))
  {     
      int temp;
      
      chprintf(chp, "Pressure %d\r\n", readPressure());
      
      readTemp(&temp);
      
      chprintf(chp, "\r\n\nback to shell! %d (%d)\r\n", temp ,status);
  }
}

/* Triggered when the button is pressed or released. The LED is set to ON.*/
static void extcb1(EXTDriver *extp, expchannel_t channel) {

  (void)extp;
  (void)channel;
  
  chSysLockFromISR();
  chThdResumeI(&trp, (msg_t)0x1337);  /* Resuming the thread with message.*/
  chSysUnlockFromISR();
}

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

/*
 * This is a periodic thread that does absolutely nothing except flashing
 * a LED.
 */
/*static THD_WORKING_AREA(waInterruptThread, 1024);
static THD_FUNCTION(InterruptThread, arg) {

	(void)arg;
	chRegSetThreadName("interrupt");
	while(1)
	{
		//palSetPad(GPIOB, 1);
		//chprintf((BaseSequentialStream *)&SDU1,"IRQ received\r\n");
		while(irq.Get()) radio.interruptHandler();
		chSysLock();
		chThdSuspendS(&trp);
		chSysUnlock();
	}
}*/

/*
 * This is a periodic thread that does absolutely nothing except flashing
 * a LED.
 */
static THD_WORKING_AREA(waThread1, 4096);
static THD_FUNCTION(Thread1, arg) {

  (void)arg;
  int x=0;
  int i=0;
  chRegSetThreadName("blinker");
  extChannelEnable(&EXTD1, 0);
  chprintf((BaseSequentialStream *)&SD2,"blinker\r\n");
  bool sleep = false;
  while (true) 
  {
	uint8_t theNodeID;
	char msgBuf[MSGBUFSIZE];
	/*
	sprintf((char*)msgBuf,"M %d\r\n",x++);
	if(radio.sendWithRetry((uint8_t)GATEWAY_ID, msgBuf,strlen(msgBuf),true))
	{
		chprintf((BaseSequentialStream *)&SDU1,"ACK received\r\n");
	}
	else chprintf((BaseSequentialStream *)&SDU1,"no Ack!\r\n");*/
	
	if(irq.Get()) 
	{
		chprintf((BaseSequentialStream *)&SD2,"4\r\n");
		radio.isr0();
		sleep = false;
	}
	else
	{
		sleep = true;
	}
	//chprintf((BaseSequentialStream *)&SD2,"1\r\n");
	palSetPad(GPIOC, 13);       /* Orange.  */
	if(radio.receiveDone()) 
	{
		int index;
		chprintf((BaseSequentialStream *)&SD2,"3\r\n");
		chprintf((BaseSequentialStream *)&SD2,"%d:Received from TNODE: %d ",i++,radio.SENDERID);
		//pc.printf((char*)radio.DATA);
		for (index=0; index<radio.DATALEN; index++)
		{
			chprintf((BaseSequentialStream *)&SD2,"%x,",radio.DATA[index]);
		}
		chprintf((BaseSequentialStream *)&SD2,"\r\n");
		
		if (radio.ACKRequested()){
			theNodeID = radio.SENDERID;
			radio.sendACK((void *)&radio.RSSI,sizeof(radio.RSSI));
			chprintf((BaseSequentialStream *)&SD2," - ACK sent. Receive RSSI: %d\r\n",radio.RSSI);
		} else chprintf((BaseSequentialStream *)&SD2,"Receive RSSI: %d\r\n",radio.RSSI);
	}
	//chprintf((BaseSequentialStream *)&SD2,"2\r\n");
	
	if(sleep)
	{
		palClearPad(GPIOC, 13);     /* Orange.  */
		chSysLock();
		chprintf((BaseSequentialStream *)&SD2,"%x,",chThdSuspendTimeoutS(&trp,10000));
		chSysUnlock();
	}
	
	
	
    //palSetPad(GPIOC, 13);       /* Orange.  */
    //chThdSleepMilliseconds(10);
    //palClearPad(GPIOC, 13);     /* Orange.  */
    //chThdSleepMilliseconds(500);
  }
}

static const ShellCommand commands[] = {
  {"date",  cmd_date},
  {"write", cmd_write},
  {"read", cmd_read},
  {"iic",   cmd_i2c},
  {NULL, NULL}
};

static const ShellConfig shell_cfg1 = {
  (BaseSequentialStream *)&SDU1,
  commands
};

static const I2CConfig i2cfg1 = {
    OPMODE_I2C,
    400000,
    FAST_DUTY_CYCLE_2,
};

/*
 * Application entry point.
 */
int main(void) {

  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  /* This is never done in the testhal code for I2C but appears to be critical to correct operation. */
  palSetPadMode(GPIOB, 6, PAL_MODE_STM32_ALTERNATE_OPENDRAIN);
  palSetPadMode(GPIOB, 7, PAL_MODE_STM32_ALTERNATE_OPENDRAIN);

  /*
   * SPI1 I/O pins setup.
   */
  palSetPadMode(GPIOA, 5, PAL_MODE_STM32_ALTERNATE_PUSHPULL);     /* SCK. */
  palSetPadMode(GPIOA, 6, PAL_MODE_STM32_ALTERNATE_PUSHPULL);     /* MISO.*/
  palSetPadMode(GPIOA, 7, PAL_MODE_STM32_ALTERNATE_PUSHPULL);     /* MOSI.*/
  palSetPadMode(GPIOA, 4, PAL_MODE_OUTPUT_PUSHPULL); /* NSS */
  palSetPadMode(GPIOB, 0, PAL_MODE_INPUT_PULLDOWN); // IRQ
  palClearPad(GPIOA, 4);
  
  i2cStart(&I2CD1, &i2cfg1);
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
  palSetPadMode(GPIOA, 12, PAL_MODE_STM32_ALTERNATE_PUSHPULL /*PAL_MODE_ALTERNATE(10)*/);
#endif
  usbStart(serusbcfg.usbp, &usbcfg);
  usbConnectBus(serusbcfg.usbp);
  
  /*
   * Activates the serial driver 2 using the driver default configuration.
   * PA2(TX) and PA3(RX) are routed to USART2.
   */
  sdStart(&SD2, NULL);
  palSetPadMode(GPIOA, 2, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
  palSetPadMode(GPIOA, 3, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
  palSetPadMode(GPIOC, 13, PAL_MODE_OUTPUT_PUSHPULL);
  
  //setSourceAddress(NODE_ID);
  if(radio.initialize(FREQUENCY, NODE_ID, NETWORKID))
  {
	radio.encrypt(0);
	radio.setPowerLevel(10);
	radio.promiscuous(true);
	//#ifdef IS_RFM69HW
	radio.setHighPower(); //uncomment #define ONLY if radio is of type: RFM69HW or RFM69HCW 
	//#endif

	/*
	* Creates the example thread.
	*/
	//chThdCreateStatic(waInterruptThread, sizeof(waInterruptThread), NORMALPRIO, InterruptThread, NULL);
	chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);  
  }
  else
  {
	  chprintf((BaseSequentialStream *)&SD2,"couldn't init radio\r\n");
  }
  /*
   * Shell manager initialization.
   */
  shellInit();

  /*
   * Normal main() thread activity, in this demo it does nothing except
   * sleeping in a loop and check the button state.
   */
  while (true) {
      //test_execute((BaseSequentialStream *)&SD2);
#if 1
      if (SDU1.config->usbp->state == USB_ACTIVE) {
#endif
      thread_t *shelltp = chThdCreateFromHeap(NULL, SHELL_WA_SIZE,
                                              "shell", NORMALPRIO + 1,
                                              shellThread, (void *)&shell_cfg1);
      chThdWait(shelltp);               /* Waiting termination.             */
#if 1
      }
#endif
    chThdSleepMilliseconds(500);
  }
}
