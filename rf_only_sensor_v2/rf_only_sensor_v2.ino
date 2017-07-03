#include <EEPROM.h>
#include <flash_stm32.h>

#include <OneWireSTM.h>

#include <SPI.h>

#include <nRF24L01.h>
#include <RF24.h>
#include <RF24_config.h>

#define DEBUG_OUT Serial

#include "usb_serial.h"
#include "libmaple/usb_cdcacm.h"

#define STATUS_LED     18

#define BUTTONS        3
#define MAX_SENSORS    8
#define MAX_NEIGHBOURS 8
struct button
{
  long time;
  byte input;
  byte output;
  byte pressed;
};

struct neighbour
{
  byte addr;
  byte score;
};
neighbour neighbours[MAX_NEIGHBOURS];

button buttons[BUTTONS];

byte message_payload[30];
long past_millis,past_millis_ident,received_millis;

byte sensor_addr[MAX_SENSORS][8];
byte sensors;

int sensor_count;

byte sequence_no=0;
byte source_addr=0xf0; // Unconfigured source address.
OneWire  ds(17);  // on pin 10 (a 4.7K resistor is necessary)
bool mppl_pesent;

uint16 config_settings;

#define SETUP_ADDRESS 0xf0

#define SOURCE_ADDRESS source_addr
#define DEST_ADDRESS 0xfe          // 0xfe is a special address which defines any internet connected device. 

#define FASTMODE_TIMEOUT 100

#define FAST_BUS_SEND_RADIO_MESSAGE 0x4000

#define PAYLOAD_TEMP_BASE       0x12
#define PAYLOAD_BUTTON          0x10
#define PAYLOAD_TEMP_INTERNAL   0x2
#define PAYLOAD_PRESSURE        0x4

#define PAYLOAD_PING            0x80
#define PAYLOAD_SETUP           0x81
#define PAYLOAD_CONFIG          0x82

#define PAYLOAD_TYPE_HEX_1WIRE  0x1
#define PAYLOAD_TYPE_ASCII      0x2

#define PAYLOAD_LENGTH          16
bool SendRadioMessageRouted(char *buffer, int buffer_length)
{
  char count = 0,count_saved=0;
  bool retval = false;
  byte addr = 0xff, old_addr = 0xff;
  byte score = 0;
  char retries = 5;
  
  DEBUG_OUT.println("SendRadioMessageRouted");

  do {
    for(count = 0; count < MAX_NEIGHBOURS; count++)
    {
      if (neighbours[count].score > score && neighbours[count].addr != 0xff && neighbours[count].addr != old_addr)
      {
        addr  = neighbours[count].addr;
        score = neighbours[count].score;
        count_saved = count;
      }
    }
    if (addr != 0xff)
    {
      byte old_addr = addr;

      retval |= SendRadioMessage(addr, &buffer[2], buffer_length - 2);
      if (!retval) { DEBUG_OUT.print("--score:");DEBUG_OUT.println((int) count_saved);neighbours[ count_saved].score--; }     
    }
    old_addr = addr;
    addr = 0xff;
    score = 0;
    retries--;
    // Add code here for now to send the message to at least the two
    // best base stations.
  } while( retries > 0 && !retval );
  return retval;
}

void setup() {
  char count;
  Serial.begin(115200);
  DEBUG_OUT.begin(115200);
  pinMode(18,OUTPUT);
  pinMode(19,OUTPUT);
  pinMode(20,OUTPUT);
  pinMode(33,OUTPUT);

  digitalWrite(18, 0);
  digitalWrite(19, 0);
  digitalWrite(20, 0);
  
  delay(3000);
  mppl_pesent = mppl3115a2_setup();
  initSensor();

  for(count = 0; count < MAX_NEIGHBOURS; count++)
  {
    neighbours[count].addr = 0xff;
    neighbours[count].score = 0;
  }
  
  for(count = 0; count < MAX_SENSORS; count++)
  {
    if (searchSensor(sensor_addr[count]) == 2)
    {
      DEBUG_OUT.println("break");
      
      break;
    }
  }
  DEBUG_OUT.println((int)count);
  sensors = count;
  sensor_count = 0;
  startSensorConversion(sensor_addr[0]);

  pinMode(32, INPUT_PULLUP);
  buttons[0].input = 32;
  buttons[0].output = 33;
  buttons[0].pressed = 1;
  pinMode(0, INPUT_PULLUP);
  buttons[1].input = 0;
  buttons[1].output = 19;
  buttons[1].pressed = 1;
  pinMode(1, INPUT_PULLUP);
  buttons[2].input = 1;
  buttons[2].output = 20;
  buttons[2].pressed = 1;

  if(digitalRead(buttons[0].input) != HIGH)
  {
    DEBUG_OUT.println("EEPROM");
    int Status = EEPROM.init();
    DEBUG_OUT.println(Status);
    Status = EEPROM.read(0x0, (uint16 *)&source_addr);
    DEBUG_OUT.println(Status);
    DEBUG_OUT.print("Address:");DEBUG_OUT.println(source_addr);
  }

  nrf_init(source_addr);
}

void loop() 
{
  checkFastCallback();

  while (ReceivedRadioMessageAvailable())
  {
     int buffer_length;
     memset(message_payload,0,sizeof(message_payload));
     ReceiveRadioMessage((char *)&message_payload[2],&buffer_length);
     sendFastMode((uint8_t *)message_payload, buffer_length+2);
     received_millis = millis();

     // Check it's for us
     if (message_payload[4] == 0x80)
     {  
        char count;
        bool found = false;
        for(count = 0; count < MAX_NEIGHBOURS; count++)
        {
          if (neighbours[count].addr == message_payload[2])
          {
            neighbours[count].score++;
            found = true;
          }
        }
        if (!found)
        {
          for(count = 0; count < MAX_NEIGHBOURS; count++)
          {
            if (neighbours[count].addr == 0xff)
            {
              neighbours[count].addr = message_payload[2];
              neighbours[count].score++;
              break;
            }
          }
        }
     }
     handle_request(message_payload);
  }
  long millis_a = millis();
  
  if (source_addr != SETUP_ADDRESS)
  {
    digitalWrite(STATUS_LED, millis_a - received_millis < 1200 ? (millis_a / 1000) & 0x1 : 0);
  }
  else
  {
    digitalWrite(STATUS_LED, (millis_a / 250) & 0x1);
  }
  if (millis_a - past_millis >= 1200)
  {
    int temperature;
    byte len;
    int present;
    
    if (sensors > 0)
    {
      present = readTempreture(&temperature,sensor_addr[sensor_count]);
      DEBUG_OUT.print("Sensor:");DEBUG_OUT.print(present);DEBUG_OUT.println(sensor_count);
      if (present >0)
      {
        len = create_payload_int(DEST_ADDRESS,PAYLOAD_TEMP_BASE+(sensor_count *2),temperature);
        SendRadioMessageRouted((char *)message_payload, len);
        sendFastMode((uint8_t *)message_payload, len);
      }
   
      sensor_count++;
      if (sensor_count >= sensors)
      {
        sensor_count = 0;
      }
      startSensorConversion(sensor_addr[sensor_count]);
    }
    past_millis = millis();
    
    if (mppl_pesent) {handle_mppl();}
    
    {
      char count;
      DEBUG_OUT.print("Neighbor:");
      bool overflow = false;
      for(count = 0; count < MAX_NEIGHBOURS; count++)
      {
        DEBUG_OUT.print(neighbours[count].addr);
        DEBUG_OUT.print(",");
        DEBUG_OUT.print(neighbours[count].score);
        DEBUG_OUT.print(" ");
        if (neighbours[count].score > 40)
        {
          overflow = true;
          break;
        }
      }
      if (overflow) for(count = 0; count < MAX_NEIGHBOURS; count++)
      {
        if (neighbours[count].score < 40)
        {
          neighbours[count].addr = 0xff;
          neighbours[count].score = 0;
        }
        else
        {
          neighbours[count].score -= 40;
        }
      }
    }
    // past_millis = millis(); is set when conversion is initiated!
  }
  if (millis_a - past_millis_ident >= 5000)
  {
    int len,index;

    if (mppl_pesent) {handle_mppl_ident();}
    
    len = create_payload_description(DEST_ADDRESS,PAYLOAD_BUTTON+1, PAYLOAD_TYPE_ASCII,"Button");
    sendFastMode((uint8_t *)message_payload, len);
    SendRadioMessageRouted((char *)message_payload, len);

    len = create_payload_description(DEST_ADDRESS,PAYLOAD_CONFIG+1, PAYLOAD_TYPE_ASCII,"Config");
    sendFastMode((uint8_t *)message_payload, len);
    SendRadioMessageRouted((char *)message_payload, len);

    for (index=0;index<sensors;index++)
    {
      len = create_payload_description(DEST_ADDRESS,PAYLOAD_TEMP_BASE+(index *2)+1, PAYLOAD_TYPE_HEX_1WIRE,(char *)sensor_addr[index]);
      sendFastMode((uint8_t *)message_payload, len);
      SendRadioMessageRouted((char *)message_payload, len);
    }
    
    past_millis_ident = millis();
  }
  {
    int button,len;
  
    for (button = 0; button < BUTTONS; button++)
    {
      if (digitalRead(buttons[button].input) == HIGH)
      {
        buttons[button].pressed = 0;
        buttons[button].time = millis();
      }
      else
      {
        if ((millis_a - (buttons[button].time) > 50) && (buttons[button].pressed) == 0)
        {
          DEBUG_OUT.print("Button:");DEBUG_OUT.println(button);
          buttons[button].pressed = 1;
          len = create_payload_int(DEST_ADDRESS,PAYLOAD_BUTTON,button);
          SendRadioMessageRouted((char *)message_payload,len);
          sendFastMode((uint8_t *)message_payload,len);   
        }
      }
    }
  }
  asm("wfi"); /* Wait for interrupt.  This halts the processor until we get a interrupt.*/
}

void handle_request(byte *request)
{
  if (request[3] == SOURCE_ADDRESS)
  {
    switch(request[4])
    {
    case PAYLOAD_BUTTON:
      {
        char button_no = request[5];
        if(button_no < BUTTONS && buttons[button_no].output != 0xff)
        {
          //DEBUG_OUT.print("Set LED");DEBUG_OUT.print((int)button_no);DEBUG_OUT.print(",");DEBUG_OUT.println(message_payload[6]);
          digitalWrite(buttons[button_no].output,request[6]);
        }   
      }
      break;
    case PAYLOAD_SETUP:
      if (request[3] == SETUP_ADDRESS)
      {
        EEPROM.write(0x0, (uint16)request[5]);
        setup();
      }
      break;
    case PAYLOAD_CONFIG:
      config_settings = (uint16)request[5];
      EEPROM.write(0x1, (uint16)request[5]);
      break;
    }
  }
}

void handle_mppl()
{
  int len;
  int temperature;
  int pressure = readPressure() * 10;
  DEBUG_OUT.print(" Pressure(Pa):");
  DEBUG_OUT.println(pressure);
  
  memset(message_payload,0,sizeof(message_payload));
  len = create_payload_int(DEST_ADDRESS,PAYLOAD_PRESSURE,pressure);
  SendRadioMessageRouted((char *)message_payload, len);
  sendFastMode((uint8_t *)message_payload,len);
  
  readTemp(&temperature);
  DEBUG_OUT.print(" Temp(c):");
  DEBUG_OUT.println(temperature);
  len = create_payload_int(DEST_ADDRESS,PAYLOAD_TEMP_INTERNAL,temperature);
  SendRadioMessageRouted((char *)message_payload, len);
  sendFastMode((uint8_t *)message_payload,len);
}

void handle_mppl_ident()
{
  int len;
  len = create_payload_description(DEST_ADDRESS,PAYLOAD_TEMP_INTERNAL+1, PAYLOAD_TYPE_ASCII,"Temp");
  sendFastMode((uint8_t *)message_payload, len);
  SendRadioMessageRouted((char *)message_payload, len);
  
  len = create_payload_description(DEST_ADDRESS,PAYLOAD_PRESSURE+1, PAYLOAD_TYPE_ASCII,"Pressure");
  sendFastMode((uint8_t *)message_payload, len);
  SendRadioMessageRouted((char *)message_payload, len);
}

int sendFastMode(uint8_t *buf,uint8_t size)
{
  int return_value=0;
  int timeout_time = millis();

  if (!usb_is_connected(USBLIB) || !usb_is_configured(USBLIB))
  {
    return 0;
  }

  while(!(return_value=sendFastTxCallback(buf,size)))
  {
    if (millis() - timeout_time > FASTMODE_TIMEOUT)
    {
      resetFastTxCallback();
      DEBUG_OUT.println("resetFastTxCallback");
      break;
    }
    delay(1);
  }
  return return_value;
}

int create_payload_int(uint8 dest,uint8 type, int payload)
{
  message_payload[2] = SOURCE_ADDRESS;
  message_payload[3] = dest;
  message_payload[4] = type;
  message_payload[9] = sequence_no++;
  int *payload_ptr = (int *) (&message_payload[5]);

  *payload_ptr = payload;
  
  return 10;
}

int create_payload_description(uint8 dest,uint8 type, char description_type,char *payload)
{
  char length;
  message_payload[2] = SOURCE_ADDRESS;
  message_payload[3] = dest;
  message_payload[4] = type;
  message_payload[5] = description_type;
  switch(description_type & 0xf)
  {
    case PAYLOAD_TYPE_ASCII:
      strcpy((char *)&message_payload[6],payload);
      return strlen(payload) + 6;
    case PAYLOAD_TYPE_HEX_1WIRE:
      memcpy(&message_payload[6],payload,8);
      return 6+8;
  }
  return 5;
}

void fastDataRxCb(uint32 ep_rx_size, uint8 *ep_rx_data)
{
  int x;
  uint32_t count;
  uint16_t *data_buffer = (uint16_t *)ep_rx_data; 
  DEBUG_OUT.println("fastDataRxCb");
  x = data_buffer[0];

  if (x == FAST_BUS_SEND_RADIO_MESSAGE)
  {
    byte *buf = (byte *)(&data_buffer[1]);

    handle_request((byte *)data_buffer);
    
    SendRadioMessage(buf[1],(char *)buf, ep_rx_size-2);
    return;
  }
}
