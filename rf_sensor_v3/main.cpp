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
void extcb1(EXTDriver *extp, expchannel_t channel)
{

  (void)extp;
  (void)channel;
  palClearPad(GPIOC, 13);
  chSysLockFromISR();
  chThdResumeI(&trp, (msg_t)0x1337);  /* Resuming the thread with message.*/
  chSysUnlockFromISR();
}

/*
 * This is a periodic thread that does absolutely nothing except flashing
 * a LED.
 */
static THD_WORKING_AREA(waInterruptThread, 1024);
static THD_FUNCTION(InterruptThread, arg) {

	(void)arg;
	chRegSetThreadName("fastbus_rx");
	while(1)
	{
	}
}

/*
 * This is a periodic thread that does absolutely nothing except flashing
 * a LED.
 */
static THD_WORKING_AREA(waThread1, 4096);
static THD_FUNCTION(Thread1, arg) {

  (void)arg;
  int x=0;
  int i=0;
  chRegSetThreadName("rf_receive");
  extChannelEnable(&EXTD1, 0);
  //chprintf((BaseSequentialStream *)&SD2,"blinker\r\n");
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
		//chprintf((BaseSequentialStream *)&SD2,"4\r\n");
		radio.isr0();
		sleep = false;
	}
	else
	{
		sleep = true;
	}
	//chprintf((BaseSequentialStream *)&SD2,"1\r\n");
	//palSetPad(GPIOC, 13);       /* Orange.  */
	if(radio.receiveDone()) 
	{
		int index;
		//chprintf((BaseSequentialStream *)&SD2,"3\r\n");
		chprintf((BaseSequentialStream *)&SD2,"%d:Received from TNODE: %d %d ",i++,radio.SENDERID,radio.TARGETID);
		//pc.printf((char*)radio.DATA);
		for (index=0; index<radio.DATALEN; index++)
		{
			chprintf((BaseSequentialStream *)&SD2,"%x,",radio.DATA[index]);
		}
		chprintf((BaseSequentialStream *)&SD2,"\r\n");
		FastBus1.Write((uint8_t *)radio.DATA,radio.DATALEN,1);
		if (radio.ACKRequested())
		{
			theNodeID = radio.SENDERID;
			radio.sendACK((void *)&radio.RSSI,sizeof(radio.RSSI));
			chprintf((BaseSequentialStream *)&SD2," - ACK sent. Receive RSSI: %d\r\n",radio.RSSI);
		} else chprintf((BaseSequentialStream *)&SD2,"Receive RSSI: %d\r\n",radio.RSSI);
		palSetPad(GPIOC, 13);
	}
	//chprintf((BaseSequentialStream *)&SD2,"2\r\n");
	
	if(sleep)
	{
		//palClearPad(GPIOC, 13);     /* Orange.  */
		chSysLock();
		chThdSuspendTimeoutS(&trp,10);
		//chprintf((BaseSequentialStream *)&SD2,"%x,",);
		chSysUnlock();
	}
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

  board_init();
  
  //setSourceAddress(NODE_ID);
  if(radio.initialize(FREQUENCY, NODE_ID, NETWORKID))
  {
	radio.encrypt(0);
	radio.setPowerLevel(0);
	radio.promiscuous(true);
#ifdef IS_RFM69HW
	radio.setHighPower(); //uncomment #define ONLY if radio is of type: RFM69HW or RFM69HCW 
#endif

	/*
	* Creates the example thread.
	*/
	//chThdCreateStatic(waInterruptThread, sizeof(waInterruptThread), NORMALPRIO, InterruptThread, NULL);
	chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);  
  }
  else
  {
	  //chprintf((BaseSequentialStream *)&SD2,"couldn't init radio\r\n");
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
