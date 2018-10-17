#ifndef USB_RW_HANDLER_H
#define USB_RW_HANDLER_H

#include <libopencm3/usb/usbd.h>

#define USB_RINGBUFFER_SIZE_RX	1024
#define USB_RINGBUFFER_MASK_RX (USB_RINGBUFFER_SIZE_RX - 1)

#define USB_RINGBUFFER_SIZE_TX 1024
#define USB_RINGBUFFER_MASK_TX (USB_RINGBUFFER_SIZE_TX - 1)

typedef struct 
{
	char rx_buf [USB_RINGBUFFER_SIZE_RX];
	uint32_t rx_produce;
	uint32_t rx_consume;
	
	char tx_buf [USB_RINGBUFFER_SIZE_TX];
	uint32_t tx_produce;
	uint32_t tx_consume;
	bool tx_cts;
} usbrw_fifo_t;

struct usbrw
{
	uint8_t rx_ep;
	uint8_t tx_ep;
	usbd_device *usbd_dev;
	void (*rx_callback) (struct usbrw *);
	void (*tx_callback) (struct usbrw *);
	usbrw_fifo_t _fifo;
};

typedef struct usbrw usbrw_t;

usbrw_t *usbrw_new (void);
void usbrw_destroy (usbrw_t **);
void usbrw_init (usbrw_t **, usbd_device *, uint8_t, uint8_t);
int usbrw_read_nonblock (usbrw_t *);
int usbrw_read (usbrw_t *, char *, int len);
void usbrw_write (usbrw_t *, char *, int);

#endif /* USB_RW_HANDLER_H */

