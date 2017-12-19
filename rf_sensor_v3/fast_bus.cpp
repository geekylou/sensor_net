/*
    FastBus Driver V1.0
*/

#include "fast_bus.h"
#include <string.h>
#include "hal.h"

USBFastBus::USBFastBus()
{
	this->usbp = NULL;
	enabled = false;
}

void USBFastBus::Init(USBDriver *usbp, usbep_t bulk_in, usbep_t bulk_out)
{   
	this->usbp = usbp;
    this->bulk_in  = bulk_in;
    this->bulk_out = bulk_out;
	
    ibqObjectInit(&(this->ibqueue), false, (this->ib),
                    FAST_BUS_BUFFERS_SIZE, FAST_BUS_BUFFERS_NUMBER,
                    ibnotify, this);
    obqObjectInit(&(this->obqueue), false, this->ob,
                    FAST_BUS_BUFFERS_SIZE, FAST_BUS_BUFFERS_NUMBER,
                    obnotify, this);
}

size_t USBFastBus::Write(const uint8_t *bp,
                         size_t 	    n,
                         systime_t 	    timeout)
{
    size_t retval = obqWriteTimeout(&obqueue, bp, n, timeout);
    
    if (retval > 0)
    {
        obqFlush(&obqueue);
    }
    return retval;
}

size_t USBFastBus::Read(uint8_t *bp,
                         size_t 	    n,
                         systime_t 	    timeout)
{
    //size_t retval = ibqReadTimeout(&ibqueue, bp, n, timeout);
	
	size_t retval = ReadBufferTimeout(&ibqueue, bp, n, timeout);
    return retval;
}

/**
 * @brief   Input queue read with timeout.
 * @details The function reads data from an input queue into a buffer.
 *          The operation completes when the specified amount of data has been
 *          transferred from a single buffer, after the specified timeout or if the queue has
 *          been reset.
 *          This is similur to ibqReadTimeout except it will only return data from a single transfer.
 *
 * @param[in] ibqp      pointer to the @p input_buffers_queue_t object
 * @param[out] bp       pointer to the data buffer
 * @param[in] n         the maximum amount of data to be transferred, the
 *                      value 0 is reserved
 * @param[in] timeout   the number of ticks before the operation timeouts,
 *                      the following special values are allowed:
 *                      - @a TIME_IMMEDIATE immediate timeout.
 *                      - @a TIME_INFINITE no timeout.
 *                      .
 * @return              The number of bytes effectively transferred.
 * @retval 0            if a timeout occurred.
 *
 * @api
 */
size_t USBFastBus::ReadBufferTimeout(input_buffers_queue_t *ibqp, uint8_t *bp,
                      size_t n, systime_t timeout)
{
    size_t r = 0;
    systime_t deadline;

    osalDbgCheck(n > 0U);

    osalSysLock();

    /* Time window for the whole operation.*/
    deadline = osalOsGetSystemTimeX() + timeout;

    volatile msg_t msg;

    /* TIME_INFINITE and TIME_IMMEDIATE are handled differently, no
     deadline.*/
    if ((timeout == TIME_INFINITE) || (timeout == TIME_IMMEDIATE)) 
    {
        msg = ibqGetFullBufferTimeoutS(ibqp, timeout);
    }
    else 
    {
        systime_t next_timeout = deadline - osalOsGetSystemTimeX();

        /* Handling the case where the system time went past the deadline,
           in this case next becomes a very high number because the system
           time is an unsigned type.*/
        if (next_timeout > timeout) 
        {
          osalSysUnlock();
          return r;
        }
        msg = ibqGetFullBufferTimeoutS(ibqp, next_timeout);
    }

    /* Anything except MSG_OK interrupts the operation.*/
    if (msg != MSG_OK) 
    {
		if (msg == MSG_RESET)
		{
			palClearPad(GPIOA, 7);
		}
        osalSysUnlock();
        return r;
    }

  
  while (true) {
    size_t size;

    /* This condition indicates that a new buffer must be acquired.*/
    if (ibqp->ptr == NULL) 
    {
        return r;
    }

    /* Size of the data chunk present in the current buffer.*/
    size = (size_t)ibqp->top - (size_t)ibqp->ptr;
    if (size > (n - r)) {
      size = n - r;
    }

    /* Smaller chunks in order to not make the critical zone too long,
       this impacts throughput however.*/
    if (size > 64U) {
      /* Giving the compiler a chance to optimize for a fixed size move.*/
      memcpy(bp, ibqp->ptr, 64U);
      bp        += 64U;
      ibqp->ptr += 64U;
      r         += 64U;
    }
    else {
      memcpy(bp, ibqp->ptr, size);
      bp        += size;
      ibqp->ptr += size;
      r         += size;
    }

    /* Has the current data buffer been finished? if so then release it.*/
    if (ibqp->ptr >= ibqp->top) {
      ibqReleaseEmptyBufferS(ibqp);
    }

    /* Giving a preemption chance.*/
    osalSysUnlock();
    if (r >= n) {
      return r;
    }
    osalSysLock();
  }
}

void USBFastBus::transmitted()
{
    size_t n = 0;
	uint8_t *buf;
	
    osalSysLockFromISR();
    
	if (!enabled)
	{
		osalSysUnlockFromISR();
		return;
	}
	
    obqReleaseEmptyBufferI(&(this->obqueue));
    
    /* Checking if there is a buffer ready for transmission.*/
    buf = obqGetFullBufferI(&(this->obqueue), &n);

    if (buf != NULL) 
    {
        /* The endpoint cannot be busy, we are in the context of the callback,
           so it is safe to transmit without a check.*/
        usbStartTransmitI(this->usbp, this->bulk_in, buf, n);
    }
    
    osalSysUnlockFromISR();
}

void USBFastBus::received()
{
    osalSysLockFromISR();
    /* Posting the filled buffer in the queue.*/
    ibqPostFullBufferI(&(this->ibqueue), usbGetReceiveTransactionSizeX(usbp,this->bulk_out));
    
    (void) start_receive();
    
    osalSysUnlockFromISR();
}


bool USBFastBus::start_receive() 
{
  uint8_t *buf = NULL;
  
  /* If the USB driver is not in the appropriate state then transactions
     must not be started.*/
  if (!enabled || usbp == NULL || usbGetDriverStateI(usbp) != USB_ACTIVE)
  {
    return true;
  }

  /* Checking if there is already a transaction ongoing on the endpoint.*/
  if (usbGetReceiveStatusI(usbp, this->bulk_out)) 
  {
    return true;
  }

  /* Checking if there is a buffer ready for incoming data.*/
  buf = ibqGetEmptyBufferI(&ibqueue);
  if (buf == NULL) 
  {
    return true;
  }

  /* Buffer found, starting a new transaction.*/
  usbStartReceiveI(usbp, this->bulk_out,
                   buf, 0x40);

  return false;
}
void USBFastBus::ibnotify(io_buffers_queue_t *bqp) 
{
  USBFastBus *inst = (USBFastBus *)bqGetLinkX(bqp);
  (void) inst->start_receive();
}

void USBFastBus::obnotify(io_buffers_queue_t *bqp)
{
  size_t n;
  USBFastBus *inst = (USBFastBus *)bqGetLinkX(bqp);
  
  /* If the USB driver is not in the appropriate state then transactions
     must not be started.*/
  if (!inst->enabled || inst->usbp == NULL || usbGetDriverStateI(inst->usbp) != USB_ACTIVE) 
    return;
  
  /* Checking if there is already a transaction ongoing on the endpoint.*/
  if(!usbGetTransmitStatusI(inst->usbp, inst->bulk_in)) 
  {
    /* Trying to get a full buffer.*/
    uint8_t *buf = obqGetFullBufferI(&inst->obqueue, &n);
    
    if (buf != NULL) 
	{
      /* Buffer found, starting a new transaction.*/
      usbStartTransmitI((USBDriver*)(inst->usbp), inst->bulk_in, buf, n);
    }
  }
}