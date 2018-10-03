#include <string.h>
#include <stdlib.h>
#include "usb_rw.h"

#define LIMIT_MAX(l,a)	(a > l ? l : a)

static void _rx_data_cb (struct usbrw *inst)
{
	char tmp [64];
	
	int len = usbd_ep_read_packet (inst->usbd_dev, inst->rx_ep, tmp, 64);
	
	for (int i = 0; i < len; i++) {
		inst->_fifo.rx_buf [inst->_fifo.rx_produce] = tmp [i];
		inst->_fifo.rx_produce = (inst->_fifo.rx_produce + 1) & USB_RINGBUFFER_MASK_RX;
	}
}

static void _tx_data_cb (struct usbrw *inst)
{
	int len;
	
	/* Disable direct call of this function */
	inst->_fifo.tx_cts = 0;
	
	/* Calc message length */
	len = inst->_fifo.tx_produce - inst->_fifo.tx_consume;	

	/* Check for overlap */
	if (len < 0) {
		len = LIMIT_MAX (64, USB_RINGBUFFER_SIZE_TX - inst->_fifo.tx_consume);
		usbd_ep_write_packet (inst->usbd_dev, inst->tx_ep, &inst->_fifo.tx_buf [inst->_fifo.tx_consume], len);
		inst->_fifo.tx_consume = (inst->_fifo.tx_consume + len) & USB_RINGBUFFER_MASK_TX;
		return;
	}
	
	if ((len = LIMIT_MAX (64, len))) {
		/* Send message by usb */
		usbd_ep_write_packet (inst->usbd_dev, inst->tx_ep, &inst->_fifo.tx_buf [inst->_fifo.tx_consume], len);
		inst->_fifo.tx_consume = (inst->_fifo.tx_consume + len) & USB_RINGBUFFER_MASK_TX;		
	} else		
		inst->_fifo.tx_cts = 1;		/* Nothing to transmit, allowing direct call */

}

static void _usbrw_write (usbrw_t *inst, char *buf, uint8_t len)
{	
	for (uint8_t l = 0; l < len; l++) {
		inst->_fifo.tx_buf [inst->_fifo.tx_produce] = buf [l];
		inst->_fifo.tx_produce = (inst->_fifo.tx_produce + 1) & USB_RINGBUFFER_MASK_TX;
	}

	/* If TX queue is empty now we are free to call callback function */
	if (inst->_fifo.tx_cts)
		_tx_data_cb (inst);	
}

usbrw_t *usbrw_new (void)
{
	return (usbrw_t *) malloc (sizeof (struct usbrw));
}

void usbrw_destroy (usbrw_t **inst)
{
	free (*inst);
	*inst = NULL;
}

void usbrw_init (usbrw_t **inst, usbd_device *dev, uint8_t rx_ep, uint8_t tx_ep)
{
	bzero (*inst, sizeof (usbrw_t));
	(*inst)->usbd_dev = dev;
	(*inst)->rx_ep = rx_ep;
	(*inst)->tx_ep = tx_ep;
	(*inst)->rx_callback = _rx_data_cb;
	(*inst)->tx_callback = _tx_data_cb;
	(*inst)->_fifo.tx_cts = 1;
}

int usbrw_read_nonblock (usbrw_t *inst)
{
	return abs (inst->_fifo.rx_produce - inst->_fifo.rx_consume);
}

int usbrw_read (usbrw_t *inst, char *buf, int len) 
{
	int l;
	
	for (l = 0; inst->_fifo.rx_produce != inst->_fifo.rx_consume && l < len; l++)	{
		buf [l] = inst->_fifo.rx_buf [inst->_fifo.rx_consume];
		inst->_fifo.rx_consume = (inst->_fifo.rx_consume + 1) & USB_RINGBUFFER_MASK_RX;
	}
	
	return l;	
}

void usbrw_write (usbrw_t *inst, char *buf, int len)
{
	if (len <= 64) {
		_usbrw_write (inst, buf, len);
	} else {
		int i;	
		for (i = 0; (i + 64) < len; i += 64)	
			_usbrw_write (inst, &buf [i], 64);
	
		_usbrw_write (inst, &buf [i], len - i);
	}	
}
