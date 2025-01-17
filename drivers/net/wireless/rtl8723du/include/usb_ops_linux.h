/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __USB_OPS_LINUX_H__
#define __USB_OPS_LINUX_H__

#define VENDOR_CMD_MAX_DATA_LEN	254
#define FW_START_ADDRESS	0x1000

#define RTW_USB_CONTROL_MSG_TIMEOUT_TEST	10/* ms */
#define RTW_USB_CONTROL_MSG_TIMEOUT	500/* ms */

#define RECV_BULK_IN_ADDR		0x80/* assign by drv, not real address */
#define RECV_INT_IN_ADDR		0x81/* assign by drv, not real address */

#define INTERRUPT_MSG_FORMAT_LEN 60

/* vendor req retry should be in the situation when each vendor req is atomically submitted from others */
#define MAX_USBCTRL_VENDORREQ_TIMES	10

#define RTW_USB_BULKOUT_TIMEOUT	5000/* ms */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0)) || (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 18))
#define _usbctrl_vendorreq_async_callback(urb, regs)	_usbctrl_vendorreq_async_callback(urb)
#define usb_write_mem_complete(purb, regs)	usb_write_mem_complete(purb)
#define usb_write_port_complete(purb, regs)	usb_write_port_complete(purb)
#define usb_read_port_complete(purb, regs)	usb_read_port_complete(purb)
#define usb_read_interrupt_complete(purb, regs)	usb_read_interrupt_complete(purb)
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 12))
#define rtw_usb_control_msg(dev, pipe, request, requesttype, value, index, data, size, timeout_ms) \
	usb_control_msg((dev), (pipe), (request), (requesttype), (value), (index), (data), (size), (timeout_ms))
#define rtw_usb_bulk_msg(usb_dev, pipe, data, len, actual_length, timeout_ms) \
	usb_bulk_msg((usb_dev), (pipe), (data), (len), (actual_length), (timeout_ms))
#else
#define rtw_usb_control_msg(dev, pipe, request, requesttype, value, index, data, size, timeout_ms) \
	usb_control_msg((dev), (pipe), (request), (requesttype), (value), (index), (data), (size), \
		((timeout_ms) == 0) || ((timeout_ms) * HZ / 1000 > 0) ? ((timeout_ms) * HZ / 1000) : 1)
#define rtw_usb_bulk_msg(usb_dev, pipe, data, len, actual_length, timeout_ms) \
	usb_bulk_msg((usb_dev), (pipe), (data), (len), (actual_length), \
		((timeout_ms) == 0) || ((timeout_ms) * HZ / 1000 > 0) ? ((timeout_ms) * HZ / 1000) : 1)
#endif

unsigned int ffaddr2pipehdl(struct dvobj_priv *pdvobj, u32 addr);

void usb_read_mem(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *rmem);
void usb_write_mem(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *wmem);

void usb_read_port_cancel(struct intf_hdl *pintfhdl);

u32 usb_write_port(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *wmem);
void usb_write_port_cancel(struct intf_hdl *pintfhdl);

int usbctrl_vendorreq(struct intf_hdl *pintfhdl, u8 request, u16 value, u16 index, void *pdata, u16 len, u8 requesttype);
u8 usb_read8(struct intf_hdl *pintfhdl, u32 addr);
u16 usb_read16(struct intf_hdl *pintfhdl, u32 addr);
u32 usb_read32(struct intf_hdl *pintfhdl, u32 addr);
int usb_write8(struct intf_hdl *pintfhdl, u32 addr, u8 val);
int usb_write16(struct intf_hdl *pintfhdl, u32 addr, __le16 val);
int usb_write32(struct intf_hdl *pintfhdl, u32 addr, __le32 val);
int usb_writeN(struct intf_hdl *pintfhdl, u32 addr, u32 length, u8 *pdata);
u32 usb_read_port(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *rmem);
void usb_recv_tasklet(unsigned long priv);

#endif
