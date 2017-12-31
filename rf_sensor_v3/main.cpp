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

void handle_dht22_ident(uint8_t *flags);
void handle_mppl3115a2_ident(uint8_t *flags);
bool mppl3115a2_present;
void send_frame(uint8_t *flags, char *message_payload, uint8_t length);

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
  chprintf(chp,"UUID: %x %x %x %x\r\n",serial_no[0],serial_no[1],serial_no[2],serial_no[3]);
  chprintf(chp,"%d\r\n",irq.Get());
  chprintf(chp, "\r\n\nback to shell! %d\r\n",radio.readTemperature(0));
}

char msg[64] = "hello world";
  
/* Can be measured using dd if=/dev/xxxx of=/dev/null bs=512 count=10000.*/
static void cmd_stations(BaseSequentialStream *chp, int argc, char *argv[]) 
{ 

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
  
#ifdef DHT_PORT
  DHT dht = DHT(DHT_PORT, DHT_PAD,DHT22);
#endif  
  extChannelEnable(&EXTD1, 0);
  //chprintf((BaseSequentialStream *)&SD2,"blinker\r\n");
  bool sleep = false;
  uint8_t flags = 0;
  int round=0;
  while (true) 
  {
	uint8_t theNodeID;
	uint8_t retVal;
	int buf_len;
    
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
		chprintf((BaseSequentialStream *)&SD2,"%d:Received from TNODE: %d %d %d ",i++,radio.SENDERID,radio.TARGETID,radio.RSSI);
		//pc.printf((char*)radio.DATA);
		for (index=0; index<radio.DATALEN; index++)
		{
			chprintf((BaseSequentialStream *)&SD2,"%x,",radio.DATA[index]);
		}
		chprintf((BaseSequentialStream *)&SD2,"\r\n");
		FastBus1.Write((uint8_t *)radio.DATA,radio.DATALEN,1);
		if (radio.ACKRequested())
		{
            uint8_t node_flags = get_flags(theNodeID,false);
            buf_len = 1;
			theNodeID = radio.SENDERID;
            
            msgBuf[0] = radio.RSSI;
#ifdef GATEWAY_DEVICE
            if (radio.DATA[2] == PAYLOAD_UUID)
            {
                msgBuf[1] = 0x88; // Assign id.
                msgBuf[2] = assign_node((int *)(&radio.DATA[4]));

                if (msgBuf[2] > 0)
                    buf_len = 4;
              
            }
            else if (!(node_flags & NODE_FLAG_ASSIGNED) ||  (node_flags & ~NODE_FLAG_ASSIGNED))
            {
               msgBuf[1] = 0x8a; // Send flags.
               msgBuf[2] = get_flags(theNodeID,true);
               buf_len = 3;
            }
#endif 
            radio.sendACK((void *)msgBuf,buf_len);
            chprintf((BaseSequentialStream *)&SD2," - ACK sent. Receive RSSI: %d args: %d %d\r\n",radio.RSSI,msgBuf[1],msgBuf[2]);
		} else chprintf((BaseSequentialStream *)&SD2,"Receive RSSI: %d\r\n",radio.RSSI);
		palSetPad(LED1_PORT, LED1_PAD);
	}

#ifdef DHT_PORT	
	if (dht.readData() >= 0)
	{
		int arr[2];
		chprintf((BaseSequentialStream *)&SD2,"readData\r\n");
		arr[0] = dht.ReadTemperature();
		arr[1] = dht.ReadHumidity();
		buf_len = create_payload_int_array(msgBuf,GATEWAY_ID,PAYLOAD_DHT11_TEMP, 2, arr);

		send_frame(&flags,msgBuf,buf_len);
	}
#endif
	if(mppl3115a2_present)
    {
        int arr[2];

        arr[1] = readPressure();

        readTemp(&arr[0]);
        
        buf_len = create_payload_int_array(msgBuf,GATEWAY_ID,PAYLOAD_MPPL_TEMP, 2, arr);
        send_frame(&flags,msgBuf,buf_len);
    }
    
#ifndef GATEWAY_DEVICE
    chprintf((BaseSequentialStream *)&SD2,"Flags: %d\r\n",flags);
	if (buf_len = uuid_node_number_request(flags,msgBuf))
	{
        chprintf((BaseSequentialStream *)&SD2,"Request ID \r\n");
		if(radio.sendWithRetry((uint8_t)GATEWAY_ID, msgBuf,buf_len,true))
		{
            if (radio.DATALEN >= 2 && radio.DATA[1] == 0x88)
            {
                radio.setAddress(radio.DATA[2]);
                setSourceAddress(radio.DATA[2]);
                flags |= NODE_FLAG_ASSIGNED;
            }
            else if(radio.DATALEN >= 2 && radio.DATA[1] == 0x8A)
            {
                flags |= radio.DATA[2];
            
                if ((radio.DATA[2] & NODE_FLAG_ASSIGNED) == 0) {flags = flags & ~NODE_FLAG_ASSIGNED;}
            }
			chprintf((BaseSequentialStream *)&SD2,"1:ACK received\r\n");
		}
		else chprintf((BaseSequentialStream *)&SD2,"1:no Ack!\r\n");
	}
    else
#endif
    {
        chprintf((BaseSequentialStream *)&SD2,"Round %d\r\n",round);
        if ((round > 6) || (flags & NODE_FLAG_REQUEST_PAYLOAD_DESCRIPTORS))
        {
#ifdef DHT_PORT
            handle_dht22_ident(&flags);
#endif 
            if(mppl3115a2_present) handle_mppl3115a2_ident(&flags);
            round = 0;
        }
        else round++;
        
        flags = flags & ~NODE_FLAG_REQUEST_PAYLOAD_DESCRIPTORS; // Clear request payload.
    }  
    retVal = FastBus1.Read((uint8_t *)msgBuf,sizeof(msgBuf),0);
	     
	if(retVal != 0)
	{
	    sleep = false;
		chprintf((BaseSequentialStream *)&SD2,"Read(%d) %x %s\r\n",retVal,((uint16_t *)msgBuf)[0],&msgBuf[2]);
		if (*((uint16_t *)msgBuf) == 0x1000)
		{
			if(radio.sendWithRetry((uint8_t)msgBuf[3], &msgBuf[2],retVal,true))
			{
				chprintf((BaseSequentialStream *)&SD2,"3:ACK received %d\r\n",radio.DATALEN);
			}
			else chprintf((BaseSequentialStream *)&SD2,"2:no Ack!\r\n");
		}
        else if (*((uint16_t *)msgBuf) == 0x2000) // Request UUID for node no.
		{
            uint8_t count = msgBuf[2] - 10;
            
            if (count < NODES_LENGTH)
            {
                msgBuf[0] = msgBuf[2];
                msgBuf[1] = 0x1;
                msgBuf[2] = 0x88;
                memcpy(&msgBuf[3], nodes[count].UUID, 16);
                retVal = FastBus1.Write((uint8_t *)msgBuf,19,0);
            }
		}
        else if (*((uint16_t *)msgBuf) == 0x2001) // Request sensor IDs for node no.
	    {
            set_flag(msgBuf[2],msgBuf[3]);
	    }
	}

	chprintf((BaseSequentialStream *)&SD2,"go back to sleep.\r\n");
	if(sleep)
	{
		int timecount;
		msg_t msg = -1;
		chSysLock();		
		for (timecount = 0; (timecount < 5 && msg == -1); timecount++)
		{
			msg = chThdSuspendTimeoutS(&trp,S2ST(1));
		}		
		chSysUnlock();
        chprintf((BaseSequentialStream *)&SD2,"Wakeup %x\r\n",msg);
	}
  }
}

void send_frame(uint8_t *flags,char *message_payload, uint8_t length)
{
#ifndef GATEWAY_DEVICE
    if(radio.sendWithRetry((uint8_t)GATEWAY_ID, msgBuf,length,true))
    {
        chprintf((BaseSequentialStream *)&SD2,"3:ACK received %d\r\n",radio.DATALEN);
        int index;
		//pc.printf((char*)radio.DATA);
		for (index=0; index<radio.DATALEN; index++)
		{
			chprintf((BaseSequentialStream *)&SD2,"%x,",radio.DATA[index]);
		}
		chprintf((BaseSequentialStream *)&SD2,"\r\n");
        
        if (radio.DATA[1] == 0x8A) // 0x8a Flags were returned in ACK.
        {
            *flags |= radio.DATA[2];
            
            if ((radio.DATA[2] & NODE_FLAG_ASSIGNED) == 0) {*flags = *flags & ~NODE_FLAG_ASSIGNED;}
        }
    }
    else chprintf((BaseSequentialStream *)&SD2,"3:no Ack!\r\n");
#endif
    FastBus1.Write((uint8_t *)message_payload,length,0);
}

void handle_dht22_ident(uint8_t *flags)
{
    int len;
    len = create_payload_description(msgBuf,GATEWAY_ID,PAYLOAD_DHT11_TEMP+1,PAYLOAD_TYPE_ASCII,(char *)"Temp");
    send_frame(flags,msgBuf,len);

    len = create_payload_description(msgBuf,GATEWAY_ID,PAYLOAD_DHT11_HUMIDITY+1,PAYLOAD_TYPE_ASCII,(char *)"Humidity");
    send_frame(flags,msgBuf,len);
}

void handle_mppl3115a2_ident(uint8_t *flags)
{
    int len;
    len = create_payload_description(msgBuf,GATEWAY_ID,PAYLOAD_MPPL_TEMP+1,PAYLOAD_TYPE_ASCII,(char *)"Temp");
    send_frame(flags,msgBuf,len);

    len = create_payload_description(msgBuf,GATEWAY_ID,PAYLOAD_MPPL_PRESSURE+1,PAYLOAD_TYPE_ASCII,(char *)"Humidity");
    send_frame(flags,msgBuf,len);
}

static const ShellCommand commands[] = {
  {"date",  cmd_date},
  {"stations", cmd_stations},
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
  clear_nodes();
  setSourceAddress(NODE_ID);
  if(radio.initialize(FREQUENCY, NODE_ID, NETWORKID))
  {
	radio.encrypt(0);
	radio.setPowerLevel(0);
	radio.promiscuous(true);
#ifdef IS_RFM69HW
	radio.setHighPower(); //uncomment #define ONLY if radio is of type: RFM69HW or RFM69HCW 
#endif

    mppl3115a2_present = mppl3115a2_setup((BaseSequentialStream *)&SD2);

	/*
	* Creates the sensor thread.
	*/
	//chThdCreateStatic(waInterruptThread, sizeof(waInterruptThread), NORMALPRIO, InterruptThread, NULL);
	chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);  
  }
  else
  {
	  chprintf((BaseSequentialStream *)&SD2,"couldn't init radio\r\n");
  }
  
  /*
   * Normal main() thread activity, in this demo it does nothing except
   * sleeping in a loop and check the button state.
   */
  while (true) {
      //test_execute((BaseSequentialStream *)&SD2);

      if (SDU1.config->usbp->state == USB_ACTIVE) {

      thread_t *shelltp = chThdCreateFromHeap(NULL, SHELL_WA_SIZE,
                                              "shell", NORMALPRIO + 1,
                                              shellThread, (void *)&shell_cfg1);
      chThdWait(shelltp);               /* Waiting termination.             */

      }

    chThdSleepMilliseconds(500);
  }
}
