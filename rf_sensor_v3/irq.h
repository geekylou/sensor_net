/*
    EXT IRQ wrapper for RFM69 library.
*/
#include "hal.h"
#include "osal.h"

class IRQWrapper
{
	ioportid_t   	 _port;
	uint8_t          _pad;
	
	public:
		expchannel_t _channel;	
	IRQWrapper(ioportid_t port, uint8_t pad,expchannel_t channel)
	{
		_port = port;
		_pad  = pad;
		_channel = channel;
	}
	

		bool inline Get()
		{
			return palReadPad(_port,_pad);
		}
};