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
#include "dht.h"
#include "framing.h"
#include "board_specific.h"

#define SHELL_WA_SIZE   THD_WORKING_AREA_SIZE(2048)

#define MSGBUFSIZE 128

thread_reference_t trp = NULL;
char msgBuf[MSGBUFSIZE];

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
  chprintf(chp,"\nDate: %2d:%02d:%02d\r\n",tim.tm_hour,tim.tm_min,tim.tm_sec); 
  chprintf(chp,"UUID: %x %x %x %x\r\n",serial_no[0],serial_no[1],serial_no[1],serial_no[3]);
  chprintf(chp,"%d\r\n",irq.Get());
  chprintf(chp, "\r\n\nback to shell! %d\r\n",radio.readTemperature(0));
}

char msg[64] = "hello world";
  
/* Can be measured using dd if=/dev/xxxx of=/dev/null bs=512 count=10000.*/
static void cmd_write(BaseSequentialStream *chp, int argc, char *argv[]) 
{ 
  chprintf(chp, "\r\n\nAttempting to write to fast bus\r\n");
  
  FastBus1.Write((uint8_t *)msg, strlen(msg), S2ST(10));
  
  chprintf(chp, "\r\n\nback to shell!\r\n");
}

/* Can be measured using dd if=/dev/xxxx of=/dev/null bs=512 count=10000.*/
static void cmd_read(BaseSequentialStream *chp, int argc, char *argv[]) 
{ 
  chprintf(chp, "\r\n\nAttempting to read from fast bus\r\n");
  char msg[65];
  
  msg[0] = 0;
  
  size_t retval;
  
  retval = FastBus1.Read((uint8_t *)msg, sizeof(msg), S2ST(10));
  
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
  palClearPad(LED1_PORT, LED1_PAD);
  chSysLockFromISR();
  chThdResumeI(&trp, (msg_t)0x1000);  /* Resuming the thread with message.*/
  chSysUnlockFromISR();
}

/*
 * This is woken up by RFM69 IRQs and transmissions from fastbas to handle RX/TX for the RFM69.
 */
static THD_WORKING_AREA(waThread1, 2048);
static THD_FUNCTION(Thread1, arg) {

  (void)arg;
  int i=0;
  chRegSetThreadName("rf_receive");
  DHT dht = DHT(GPIOA, 8,DHT22);
  
  extChannelEnable(&EXTD1, 0);
  //chprintf((BaseSequentialStream *)&SD2,"blinker\r\n");
  bool sleep = false;
  while (true) 
  {
	uint8_t theNodeID;
	uint8_t retVal;
	
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
		palSetPad(LED1_PORT, LED1_PAD);
	}
	retVal = FastBus1.Read((uint8_t *)msgBuf,sizeof(msgBuf),0);
	
	if(retVal != 0)
	{
		chprintf((BaseSequentialStream *)&SD2,"Read(%d) %x %s\r\n",retVal,((uint16_t *)msgBuf)[0],&msgBuf[2]);
		if ( *((uint16_t *)msgBuf) == 0x1000)
		{
			if(radio.sendWithRetry((uint8_t)msgBuf[3], &msgBuf[2],retVal,true))
			{
				chprintf((BaseSequentialStream *)&SD2,"ACK received\r\n");
			}
			else chprintf((BaseSequentialStream *)&SD2,"no Ack!\r\n");
		}
	}
	
	if (dht.readData() >= 0)
	{
		int arr[2];
		int buf_len;
		chprintf((BaseSequentialStream *)&SD2,"readData\r\n");
		arr[0] = dht.ReadTemperature();
		arr[1] = dht.ReadHumidity();
		buf_len = create_payload_int_array(msgBuf,GATEWAY_ID,PAYLOAD_DHT11_TEMP, 2, arr);

#ifdef GATEWAY_ID		
		if(radio.sendWithRetry((uint8_t)GATEWAY_ID, msgBuf,buf_len,true))
		{
			chprintf((BaseSequentialStream *)&SD2,"ACK received\r\n");
		}
		else chprintf((BaseSequentialStream *)&SD2,"no Ack!\r\n");
#endif
		FastBus1.Write((uint8_t *)msgBuf,buf_len,1);
	}
	chprintf((BaseSequentialStream *)&SD2,"go back to sleep.\r\n");
	if(sleep)
	{
		int timecount;
		msg_t msg = -1;
		chSysLock();
		
		for (timecount = 0; (timecount < 10 && msg == -1); timecount++)
		{
			msg = chThdSuspendTimeoutS(&trp,S2ST(1));
		}
		chprintf((BaseSequentialStream *)&SD2,"Wakeup %x\r\n",msg);
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
  /*
   * Shell manager initialization.
   */
  shellInit();
 #if 1 
  setSourceAddress(NODE_ID);
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
	  chprintf((BaseSequentialStream *)&SD2,"couldn't init radio\r\n");
  }
  #endif

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
