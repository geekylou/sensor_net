#include <framing.h>
#include <string.h>
#include "board_specific.h"

static uint8_t sequence_no=0; 
static uint8_t src_adddr= 0xf0;

Node nodes[NODES_LENGTH];

void setSourceAddress(uint8_t source_addr)
{
      src_adddr=source_addr;
}

void clear_nodes()
{
    memset(nodes,sizeof(nodes),0);
}

uint8_t check_node(int *uuid)
{
    int count = 0;
    for(;count < NODES_LENGTH; count++)
    {
        if (uuid[0] == nodes[count].UUID[0] && 
            uuid[1] == nodes[count].UUID[1] && 
            uuid[2] == nodes[count].UUID[2] && 
            uuid[3] == nodes[count].UUID[3])
        {
            return count+10; // Start id's from 10
        }
    }
    return 0;
}

uint8_t assign_node(int *uuid)
{
    int count = 0;
    for(;count < NODES_LENGTH; count++)
    {
        if (uuid[0] == nodes[count].UUID[0] && 
            uuid[1] == nodes[count].UUID[1] && 
            uuid[2] == nodes[count].UUID[2] && 
            uuid[3] == nodes[count].UUID[3])
        {
            return count+10; // Start id's from 10
        }
        if (nodes[count].flags == 0)
        {
            nodes[count].flags = NODE_FLAG_ASSIGNED;
            nodes[count].UUID[0] = uuid[0];
            nodes[count].UUID[1] = uuid[1];
            nodes[count].UUID[2] = uuid[2];
            nodes[count].UUID[3] = uuid[3];
            return count+10;
        }
    }
    return 0;
}

uint8_t get_flags(uint8_t node_id,bool clear)
{
    uint8_t flags = 0;
    node_id = node_id - 10;

    if (node_id < NODES_LENGTH)
    {
        flags =  nodes[node_id].flags;
        if (clear) nodes[node_id].flags &= NODE_FLAG_ASSIGNED;
    }
    return flags;
}

void set_flag(uint8_t node_id,uint8_t flags)
{
   node_id = node_id - 10;
   
   if (node_id < NODES_LENGTH)
     nodes[node_id].flags |= (flags & ~NODE_FLAG_ASSIGNED); // It is not allowable to set the assigned flag in the host.
}

/* Request a node id on the network. */
int uuid_node_number_request(uint8_t flags,char *message_payload)
{
	if ((src_adddr == 0xf0) || (flags & NODE_FLAG_ASSIGNED) == 0)
	{
		return create_payload_int_array(message_payload, GATEWAY_ID, PAYLOAD_UUID, 4, serial_no);
	}
	return 0;
}

int create_payload_int(char *message_payload,uint8_t dest,uint8_t type, int payload)
{
  message_payload[0] = src_adddr;
  message_payload[1] = dest;
  message_payload[2] = type;
  message_payload[3] = sequence_no++;
  int *payload_ptr = (int *) (&message_payload[4]);

  *payload_ptr = payload;
  
  return 8;
}

int create_payload_int_array(char *message_payload,uint8_t dest,uint8_t type, int length, int *payload)
{
  int count = 0;
  message_payload[0] = src_adddr;
  message_payload[1] = dest;
  message_payload[2] = type;
  message_payload[3] = sequence_no++;
  int *payload_ptr = (int *) (&message_payload[4]);

  for (count=0;count < length; count++)
  {
	payload_ptr[count] = payload[count];
  }
  return 4 + (length * 4);
}

int create_payload_description(char *message_payload,uint8_t dest,uint8_t type, uint8_t description_type,char *payload)
{
  message_payload[0] = src_adddr;
  message_payload[1] = dest;
  message_payload[2] = type;
  message_payload[3] = description_type;
  switch(description_type & 0xf)
  {
    case PAYLOAD_TYPE_ASCII:
      strcpy((char *)&message_payload[4],payload);
      return strlen(payload) + 4;
    case PAYLOAD_TYPE_HEX_1WIRE:
      memcpy(&message_payload[4],payload,8);
      return 4+8;
  }
  return 4;
}