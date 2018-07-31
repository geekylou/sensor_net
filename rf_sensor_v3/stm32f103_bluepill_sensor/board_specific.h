#include "RFM69.h"

#define USB_PA12_DISCONNECT 1

//#define GATEWAY_DEVICE

#define RFM69_ENABLED

#define GATEWAY_ID    1
#define NODE_ID       0xf0    // node ID if this isn't what the sending node expects then ACKs won't work!
#define NETWORKID     101    //the same on all nodes that talk to each other
#define MSG_INTERVAL  100

// Uncomment only one of the following three to match radio frequency
//#define FREQUENCY     RF69_433MHZ    
#define FREQUENCY     RF69_868MHZ
//#define FREQUENCY     RF69_915MHZ

//#define IS_RFM69HW   //NOTE: uncomment this ONLY for RFM69HW or RFM69HCW
#define ENCRYPT_KEY    "EncryptKey123456"  // use same 16byte encryption key for all devices on net

#define SENSOR_I2C I2CD1

#define MPPL311A2

#define DHT_PORT GPIOA
#define DHT_PAD  3

#define LED1_PORT GPIOC
#define LED1_PAD  13

extern RFM69 radio;
extern IRQWrapper irq;

void board_init();
void extcb1(EXTDriver *extp, expchannel_t channel);

extern int *serial_no;