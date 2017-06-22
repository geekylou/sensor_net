#include <EEPROM.h>
#include <flash_stm32.h>

#include <OneWireSTM.h>

#include <SPI.h>

#include <nRF24L01.h>
#include <RF24.h>
#include <RF24_config.h>

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
long past_millis,received_millis;
byte addr[MAX_SENSORS][8];
byte sensors;

byte sequence_no=0;
byte source_addr=0xf0; // Unconfigured source address.
OneWire  ds(17);  // on pin 10 (a 4.7K resistor is necessary)
bool mppl_pesent;

#define SETUP_ADDRESS 0xf0

#define SOURCE_ADDRESS source_addr
#define DEST_ADDRESS 0xfe          // 0xfe is a special address which defines any internet connected device. 

#define FASTMODE_TIMEOUT 100

#define PAYLOAD_TEMP_BASE       0x12
#define PAYLOAD_BUTTON          0x10
#define PAYLOAD_TEMP_INTERNAL   0x2
#define PAYLOAD_PRESSURE        0x4

#define PAYLOAD_PING            0x80
#define PAYLOAD_SETUP           0x81


#define PAYLOAD_TYPE_HEX_1WIRE  0x1
#define PAYLOAD_TYPE_ASCII      0x2

#define PAYLOAD_LENGTH          16
bool SendRadioMessageRouted(char *buffer, int buffer_length)
{
  char count = 0;
  bool retval = false;
  byte addr = 0xff;
  byte score = 0;
  Serial.println("SendRadioMessageRouted");
  for(count = 0; count < MAX_NEIGHBOURS; count++)
  {
    if (neighbours[count].score > score && neighbours[count].addr != 0xff)
    {
      addr  = neighbours[count].addr;
      score = neighbours[count].score;
    }
  }
  if (addr != 0xff)
  {
    byte old_addr = addr;

    retval |= SendRadioMessage(addr, buffer, buffer_length);
    
    addr = 0xff;
    score = 0;  

    for(count = 0; count < MAX_NEIGHBOURS; count++)
    {
      if (neighbours[count].score > score && neighbours[count].addr != 0xff && neighbours[count].addr != old_addr)
      {
        addr = neighbours[count].addr;
        score = neighbours[count].score;
      }
    }

    if (addr != 0xff)
    {
       retval |= SendRadioMessage(addr, buffer, buffer_length);
    }
  }
  return retval;
}

void setup() {
  char count;
  pinMode(18,OUTPUT);
  pinMode(19,OUTPUT);
  pinMode(20,OUTPUT);
  pinMode(33,OUTPUT);

  digitalWrite(18, 0);
  digitalWrite(19, 0);
  digitalWrite(20, 0);
  
  delay(5000);
  mppl_pesent = mppl3115a2_setup();
  initSensor();

  for(count = 0; count < MAX_NEIGHBOURS; count++)
  {
    neighbours[count].addr = 0xff;
    neighbours[count].score = 0;
  }
  
  for(count = 0; count < MAX_SENSORS; count++)
  {
    if (searchSensor(addr[count]) == 2)
    {
      Serial.println("break");
      
      break;
    }
  }
  Serial.println((int)count);
  sensors = count;

  for(count = 0; count < sensors; count++)
  {
    startSensorConversion(addr[0]);
  }
  

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
    Serial.println("EEPROM");
    int Status = EEPROM.init();
    Serial.println(Status);
    Status = EEPROM.read(0x0, (uint16 *)&source_addr);
    Serial.println(Status);
    Serial.print("Address:");Serial.println(source_addr);
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
    char count;
    for(count = 0; count < sensors; count++)
    {
      if (readTempreture(&temperature,addr[count]))
      {
        len = create_payload(DEST_ADDRESS,PAYLOAD_TEMP_BASE+(count *2),temperature);
        SendRadioMessageRouted((char *)&message_payload[2], len);
        sendFastMode((uint8_t *)message_payload, len);
      }
      message_payload[2] = SOURCE_ADDRESS;
      message_payload[3] = DEST_ADDRESS;
      message_payload[4] = PAYLOAD_TEMP_BASE+(count *2)+1;
      message_payload[5] = PAYLOAD_TYPE_HEX_1WIRE;
      memcpy(&message_payload[6],addr[count],8);
      SendRadioMessageRouted((char *)&message_payload[2], PAYLOAD_LENGTH);
      sendFastMode((uint8_t *)message_payload, PAYLOAD_LENGTH);
      
      startSensorConversion(addr[count]);
      delay(50);
    }

    if (mppl_pesent) {handle_mppl();}
    
    message_payload[2] = SOURCE_ADDRESS;
    message_payload[3] = DEST_ADDRESS;
    message_payload[4] = PAYLOAD_BUTTON+1;
    message_payload[5] = PAYLOAD_TYPE_ASCII;
    strcpy((char *)&message_payload[6],"Button");
    SendRadioMessageRouted((char *)&message_payload[2], PAYLOAD_LENGTH);
    sendFastMode((uint8_t *)message_payload, PAYLOAD_LENGTH);
    
    {
      Serial.print("Neighbor:");
      bool overflow = false;
      for(count = 0; count < MAX_NEIGHBOURS; count++)
      {
        Serial.print(neighbours[count].addr);
        Serial.print(",");
        Serial.print(neighbours[count].score);
        Serial.print(" ");
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
    past_millis = millis_a;
  }

  {
    int button;
  
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
          Serial.print("Button:");Serial.println(button);
          buttons[button].pressed = 1;
          create_payload(DEST_ADDRESS,PAYLOAD_BUTTON,button);
          SendRadioMessageRouted((char *)&message_payload[2], PAYLOAD_LENGTH);
          sendFastMode((uint8_t *)message_payload,sizeof(message_payload));   
        }
      }
    }
  }
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
          //Serial.print("Set LED");Serial.print((int)button_no);Serial.print(",");Serial.println(message_payload[6]);
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
    }
  }
}

void handle_mppl()
{
  int len;
  int temperature;
  int pressure = readPressure() * 10;
  Serial.print(" Pressure(Pa):");
  Serial.println(pressure);
  
  memset(message_payload,0,sizeof(message_payload));
  len = create_payload(DEST_ADDRESS,PAYLOAD_PRESSURE,pressure);
  SendRadioMessageRouted((char *)&message_payload[2], len);
  sendFastMode((uint8_t *)message_payload,sizeof(message_payload));
  
  readTemp(&temperature);
  Serial.print(" Temp(c):");
  Serial.println(temperature);
  len = create_payload(DEST_ADDRESS,PAYLOAD_TEMP_INTERNAL,temperature);
  SendRadioMessageRouted((char *)&message_payload[2], len);
  sendFastMode((uint8_t *)message_payload,sizeof(message_payload));
    
  message_payload[2] = SOURCE_ADDRESS;
  message_payload[3] = DEST_ADDRESS;
  message_payload[4] = PAYLOAD_TEMP_INTERNAL+1;
  message_payload[5] = PAYLOAD_TYPE_ASCII;
  strcpy((char *)&message_payload[6],"Temp");
  sendFastMode((uint8_t *)message_payload, PAYLOAD_LENGTH);
  SendRadioMessageRouted((char *)&message_payload[2], PAYLOAD_LENGTH);
  
  message_payload[2] = SOURCE_ADDRESS;
  message_payload[3] = DEST_ADDRESS;
  message_payload[4] = PAYLOAD_PRESSURE+1;
  message_payload[5] = PAYLOAD_TYPE_ASCII;
  strcpy((char *)&message_payload[6],"Pressure");
  sendFastMode((uint8_t *)message_payload, PAYLOAD_LENGTH);
  SendRadioMessageRouted((char *)&message_payload[2], PAYLOAD_LENGTH);
}

int sendFastMode(uint8_t *buffer,uint8_t size)
{
  int return_value=0;
  int timeout_time = millis();

  if (!usb_is_connected(USBLIB) || !usb_is_configured(USBLIB))
  {
    return 0;
  }

  while(!(return_value=sendFastTxCallback(buffer,size)))
  {
    if (millis() - timeout_time > FASTMODE_TIMEOUT)
    {
      resetFastTxCallback();
      Serial.println("resetFastTxCallback");
      break;
    }
    delay(1);
  }
  return return_value;
}

int create_payload(uint8 dest,uint8 type, int payload)
{
  message_payload[2] = SOURCE_ADDRESS;
  message_payload[3] = dest;
  message_payload[4] = type;
  message_payload[9] = sequence_no++;
  int *payload_ptr = (int *) (&message_payload[5]);

  *payload_ptr = payload;
  
  return 10;
}

#define FAST_BUS_SEND_RADIO_MESSAGE 0x4000

void fastDataRxCb(uint32 ep_rx_size, uint8 *ep_rx_data)
{
  int x;
  uint32_t count;
  uint16_t *data_buffer = (uint16_t *)ep_rx_data; 
  Serial.println("fastDataRxCb");
  x = data_buffer[0];

  if (x == 0x4000)
  {
    byte *buf = (byte *)(&data_buffer[1]);

    handle_request((byte *)data_buffer);
    
    SendRadioMessage(buf[1],(char *)buf, ep_rx_size-2);
    return;
  }
}
