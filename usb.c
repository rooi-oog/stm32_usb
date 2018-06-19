#include <stdlib.h>
#include <errno.h>

#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/cdc.h>

#include "usb.h"
#include "usb_desc.h"
#include "usb_rw.h"

/* ---------------- */
#include "pid_def.h"
/* ---------------- */

/* Buffer used for control requests. */
uint8_t usbd_control_buffer [128];

/* USB Device struct */
usbd_device *usbd_dev;

/* Instance of libusbrw structure */
usbrw_t *_stdout;

#ifdef USB_COMM
int _write(int file, char *ptr, int len)
{
	if (file == 1) {
		usbrw_write (_stdout, ptr, len);	
		return len;
	}

	errno = EIO;
	return -1;
}

int _read (int file, char *ptr, int len)
{
	if (file == 0) {
		if (usbrw_read_nonblock (_stdout))		
			return usbrw_read (_stdout, ptr, len);
		else
			return 0;
	}

	errno = EIO;
	return -1;
}
#endif

void stdout_data_rx_cb (usbd_device *usbd_dev, uint8_t ep)
{
	(void) usbd_dev;
	(void) ep;
	
	_stdout->rx_callback (_stdout);
}

void stdout_data_tx_cb (usbd_device *usbd_dev, uint8_t ep)
{
	(void) usbd_dev;
	(void) ep;
	
	_stdout->tx_callback (_stdout);
}

static enum usbd_request_return_codes cdcacm_control_request(usbd_device *usbd_dev, struct usb_setup_data *req, uint8_t **buf,
		uint16_t *len, void (**complete)(usbd_device *usbd_dev, struct usb_setup_data *req))
{
	(void)complete;
	(void)buf;
	(void)usbd_dev;

	switch (req->bRequest) {
	case USB_CDC_REQ_SET_CONTROL_LINE_STATE: {
		/*
		 * This Linux cdc_acm driver requires this to be implemented
		 * even though it's optional in the CDC spec, and we don't
		 * advertise it in the ACM functional descriptor.
		 */
		char local_buf[10];
		struct usb_cdc_notification *notif = (void*)local_buf;

		/* We echo signals back to host as notification. */
		notif->bmRequestType = 0xA1;
		notif->bNotification = USB_CDC_NOTIFY_SERIAL_STATE;
		notif->wValue = 0;
		notif->wIndex = 0;
		notif->wLength = 2;
		local_buf[8] = req->wValue & 3;
		local_buf[9] = 0;
		// usbd_ep_write_packet(0x83, buf, 10);
		return USBD_REQ_HANDLED;
		}
	case USB_CDC_REQ_SET_LINE_CODING:
		if (*len < sizeof(struct usb_cdc_line_coding))
			return USBD_REQ_NOTSUPP;

		return USBD_REQ_HANDLED;
	}
	return USBD_REQ_NOTSUPP;
}

static void comp_set_config(usbd_device *dev, uint16_t wValue)
{
	(void) wValue;

	usbd_ep_setup (dev, 0x01, USB_ENDPOINT_ATTR_BULK, 64, stdout_data_rx_cb);
	usbd_ep_setup (dev, 0x81, USB_ENDPOINT_ATTR_BULK, 64, stdout_data_tx_cb);
	usbd_ep_setup (dev, 0x82, USB_ENDPOINT_ATTR_INTERRUPT, 16, NULL);

	usbd_register_control_callback (
				dev,
				USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
				USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
				cdcacm_control_request);
}

void usb_cdcacm_init (void)
{
	_stdout = usbrw_new ();
	
	usbd_dev = usbd_init (&st_usbfs_v1_usb_driver, &dev_descr, &config, 
							usb_strings, 3, usbd_control_buffer, sizeof (usbd_control_buffer));
							
	usbrw_init (&_stdout, usbd_dev, 0x01, 0x81);
	
	usbd_register_set_config_callback(usbd_dev, comp_set_config);
		
	
	// TODO: (2 << 4) why?! and why this do a deal?!
	// FIXME: (2 << 4) should be either named constant or define 
	nvic_set_priority (NVIC_USB_LP_CAN_RX0_IRQ, (2 << 4));
	nvic_enable_irq (NVIC_USB_LP_CAN_RX0_IRQ);
	nvic_enable_irq (NVIC_USB_WAKEUP_IRQ);
}

void usb_lp_can_rx0_isr (void) 
{
    usbd_poll (usbd_dev);
}

void usb_wakeup_isr (void) 
{
	usbd_poll (usbd_dev);
}
