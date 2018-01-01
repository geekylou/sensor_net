#include <stdint.h>

#define PAYLOAD_TEMP_BASE       0x12
#define PAYLOAD_BUTTON          0x10
#define PAYLOAD_MPPL_TEMP       0x2
#define PAYLOAD_MPPL_PRESSURE   0x4

#define PAYLOAD_DHT11_TEMP      0x6
#define PAYLOAD_DHT11_HUMIDITY  0x8

#define PAYLOAD_PING            0x80
#define PAYLOAD_SETUP           0x82
#define PAYLOAD_CONFIG          0x84
#define PAYLOAD_UUID            0x86

#define PAYLOAD_TYPE_HEX_1WIRE  0x1
#define PAYLOAD_TYPE_ASCII      0x2

#define PAYLOAD_LENGTH          16

#define AUTO_NODE_OFFSET        10

int create_payload_int(char *message_payload,uint8_t dest,uint8_t type, int payload);
int create_payload_int_array(char *message_payload,uint8_t dest,uint8_t type, int length, int *payload);
int create_payload_description(char *message_payload,uint8_t dest,uint8_t type, uint8_t description_type,char *payload);
void setSourceAddress(uint8_t source_addr);
int uuid_node_number_request(uint8_t flags,char *message_payload);
uint8_t assign_node(int *uuid);
void clear_nodes();
uint8_t get_flags(uint8_t node_id,bool clear);
void set_flag(uint8_t node_id,uint8_t flags);
uint8_t check_node(int *uuid);

#define NODE_FLAG_ASSIGNED                    0x1
#define NODE_FLAG_REQUEST_PAYLOAD_DESCRIPTORS 0x2

struct Node
{
    int UUID[4];
    uint8_t flags;
    uint8_t RSSI;
    uint8_t tx_power;
};

#define NODES_LENGTH 64

extern Node nodes[];