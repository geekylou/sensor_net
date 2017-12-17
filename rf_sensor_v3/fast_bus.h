/*
    FastBus Driver V0.1
*/
#include <string.h>

#include "hal.h"
#include "osal.h"

#define FAST_BUS_BUFFERS_NUMBER  2
#define FAST_BUS_BUFFERS_SIZE    0x42

class USBFastBus
{
    private:
        /**
        * @brief   USB driver to use.
        */
        USBDriver                 *usbp;
        /**
        * @brief   Bulk IN endpoint used for outgoing data transfer.
        */
        usbep_t                   bulk_in;
        /**
        * @brief   Bulk OUT endpoint used for incoming data transfer.
        */
        usbep_t                   bulk_out;

        /* Input buffers queue.*/                                                 
        input_buffers_queue_t     ibqueue;                                        
        /* Output queue.*/                                                        
        output_buffers_queue_t    obqueue;                                        
        /* Input buffer.*/                                                        
        uint8_t                   ib[BQ_BUFFER_SIZE(FAST_BUS_BUFFERS_NUMBER,    
                                                  FAST_BUS_BUFFERS_SIZE)];    
        /* Output buffer.*/                                                       
        uint8_t                   ob[BQ_BUFFER_SIZE(FAST_BUS_BUFFERS_NUMBER,    
                                                  FAST_BUS_BUFFERS_SIZE)];
   public:                                                 
    static void obnotify(io_buffers_queue_t *bqp);
    static void ibnotify(io_buffers_queue_t *bqp);
    size_t ReadBufferTimeout(input_buffers_queue_t *ibqp, uint8_t *bp,
                      size_t n, systime_t timeout);

    volatile bool enabled;
    bool start_receive();
    void received();
    void transmitted();
    USBFastBus();
	void Init(USBDriver *usbp, usbep_t bulk_in, usbep_t bulk_out);
    size_t Write(const uint8_t *bp, size_t n, systime_t timeout);
    size_t Read(uint8_t *bp, size_t n, systime_t timeout);

};
