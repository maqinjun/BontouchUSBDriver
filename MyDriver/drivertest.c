//
//  drivertest.c
//  MyDriver
//
//  Created by maqj on 23/12/13.
//  Copyright (c) 2013 maqj. All rights reserved.
//
//##########################################################################
//#                                                                        #
//#        Copyright (C) 2012 by Beijing BonXone Technology Co., Ltd.      #
//#                                                                        #
//#        BonXone USB Driver for single-touch Touchscreen                 #
//#                                                                        #
//##########################################################################
//#include <linux/kernel.h>
//#include <linux/slab.h>
//#include <linux/input.h>
//#include <linux/module.h>
//#include <linux/init.h>
//#include <linux/usb.h>
//#include <linux/usb/input.h>
//#include <linux/hid.h>
//
//#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/fs.h>


/* Version Information */
#define DRIVER_VERSION    "v1.4.0 for android"
#define DRIVER_AUTHOR     "Beijing BonXone Technology Co., Ltd."
#define DRIVER_DESC       "BonXone USB driver for single-touch touchscreen"

#define TOUCH_DRIVER_NAME "bontouch"
#define TOUCH_DEVICE_NAME   "bontouch"

#define MAJOR_DEVICE_NUMBER 47
#define MINOR_DEVICE_NUMBER 191
#define USB_TRANSFER_LENGTH 8

#define CTLCODE_GET_COORDINATE   0xc0
#define CTLCODE_SET_CALIB_PARA_X 0xc1
#define CTLCODE_SET_CALIB_PARA_Y 0xc2
#define CTLCODE_GET_CALIB_PARA_X 0xc3
#define CTLCODE_GET_CALIB_PARA_Y 0xc4
#define CTLCODE_START_CALIB      0xc5
#define CTLCODE_GET_CALIB_STATUS 0xc6
#define CTLCODE_DEVICE_RESET     0xc7
#define CTLCODE_GET_SCR_PARA     0xc8
#define CTLCODE_GET_PRODUCT_SN   0xc9
#define CTLCODE_GET_PRODUCT_ID   0xca
#define CTLCODE_SET_SCAN_MODE    0xcb
#define CTLCODE_IS_TOUCHED       0xcc
#define CTLCODE_SAVE_PARAM       0xcd

#define DEVTYPE_METOUCH 0

#define USB_DEVICE_HID_CLASS(vend, prod) \
.match_flags = USB_DEVICE_ID_MATCH_INT_CLASS \
| USB_DEVICE_ID_MATCH_DEVICE, \
.idVendor = (vend), \
.idProduct = (prod), \
.bInterfaceClass = USB_INTERFACE_CLASS_HID, \
.bInterfaceProtocol = USB_INTERFACE_PROTOCOL_MOUSE

/* Define these values to match your devices */
#define METOUCH_VENDOR_ID	  0x255E
#define METOUCH_PRODUCT_ID	  0x0001
#define METUSB_MINOR_BASE   	  0x0

#pragma pack(1)
struct bontouch_point
{
	unsigned char PointId;
	unsigned char status;
	short x;
	short y;
};

struct points
{
	int x;
	int y;
};

struct calib_param
{
	long  A00;
	long  A01;
	long  A10;
	unsigned char reserve[46];
};

struct  device_config
{
	unsigned char deviceID;
	unsigned char monitorID;
	unsigned char reserved[8];
	unsigned char calibrateStatus;
	unsigned char reserved1[47];
};

#pragma pack()
/* device specifc data/functions */
struct usbtouch_usb;
struct usbtouch_device_info {
	int min_xc, max_xc;
	int min_yc, max_yc;
	int min_press, max_press;
	int rept_size;
    
	/*
	 * Always service the USB devices irq not just when the input device is
	 * open. This is useful when devices have a watchdog which prevents us
	 * from periodically polling the device. Leave this unset unless your
	 * touchscreen device requires it, as it does consume more of the USB
	 * bandwidth.
	 */
	bool irq_always;
    
	void (*process_pkt) (struct usbtouch_usb *usbtouch, unsigned char *pkt, int len);
    
	/*
	 * used to get the packet len. possible return values:
	 * > 0: packet len
	 * = 0: skip one byte
	 * < 0: -return value more bytes needed
	 */
	int  (*get_pkt_len) (unsigned char *pkt, int len);
    
	int  (*read_data)   (struct usbtouch_usb *usbtouch, unsigned char *pkt);
	int  (*alloc)       (struct usbtouch_usb *usbtouch);
	int  (*init)        (struct usbtouch_usb *usbtouch);
	void (*exit)	    (struct usbtouch_usb *usbtouch);
};

/* a usbtouch device */
struct usbtouch_usb {
	unsigned char *data;
	dma_addr_t data_dma;
	unsigned char *buffer;
	int buf_len;
	struct urb *irq;
	struct usb_interface *interface;
	struct input_dev *input;
	struct usbtouch_device_info *type;
	char name[128];
	char phys[64];
	void *priv;
	int x, y;
	int touch, press;
};
struct device_context
{
	bool startCalib;
	bool isCalibX;
	bool isCalibY;
	u16  productid;
	struct points points;
	struct calib_param calibX;
	struct calib_param calibY;
	struct device_config devConfig;
};

struct cdev bontouch_cdev;
struct device_context *bontouchdev_context = NULL;
static struct class *irser_class;
static struct usb_driver usbtouch_driver;
static struct file_operations bontouch_fops;
volatile static int tflag = 0;

static int metouch_read_data(struct usbtouch_usb *dev, unsigned char *pkt);
static void usbtouch_process_pkt(struct usbtouch_usb *usbtouch,unsigned char *pkt, int len);
static void usbtouch_irq(struct urb *urb);
static int usbtouch_open(struct input_dev *input);
static void usbtouch_close(struct input_dev *input);
static int usbtouch_suspend(struct usb_interface *intf, pm_message_t message);
static int usbtouch_resume(struct usb_interface *intf);
static int usbtouch_reset_resume(struct usb_interface *intf);
static void usbtouch_free_buffers(struct usb_device *udev,struct usbtouch_usb *usbtouch);
static struct usb_endpoint_descriptor *usbtouch_get_input_endpoint(struct usb_host_interface *interface);
static int usbtouch_probe(struct usb_interface *intf,const struct usb_device_id *id);
static void usbtouch_disconnect(struct usb_interface *intf);
static int __init usbtouch_init(void);
static void __exit usbtouch_cleanup(void);
int mypow(int t);


static struct usbtouch_device_info usbtouch_dev_info[] = {
	[DEVTYPE_METOUCH] = {
		.min_xc		= 0x0,
		//	.max_xc		= 0x0fff,
		.max_xc		= 32767,
		.min_yc		= 0x0,
		//	.max_yc		= 0x0fff,
		.max_yc		= 32767,
		.rept_size	= 8,
		.read_data	= metouch_read_data,
	},
};

static struct usb_device_id metusb_table [] = {
	{ USB_DEVICE(METOUCH_VENDOR_ID, METOUCH_PRODUCT_ID) },
	{ }					/* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, metusb_table);

static int device_reset(void)
{
	mm_segment_t fs;
	struct file *nef_filp = NULL;
	int ret = -1;
	fs = get_fs();
	set_fs(KERNEL_DS);
	nef_filp = filp_open("/etc/bontouchcalibParam", O_TRUNC|O_CREAT, 0666);
	if(IS_ERR(nef_filp))
	{
		printk(KERN_ERR "filp_open error: "
               " %s - Unable to open bontouch /etc/bontouchcalibParam file!\n", __func__);
		goto end;
	}
	filp_close(nef_filp, current->files);
	ret = 1;
end:
	set_fs(fs);
	return ret;
}

static void bontouch_translatePoint(struct bontouch_point pt, int *x, int *y)
{
	*x = ((bontouchdev_context->calibX.A01 * pt.y) / 10000) + \
    ((bontouchdev_context->calibX.A10 * pt.x) / 10000) + \
    bontouchdev_context->calibX.A00;
    
	*y = ((bontouchdev_context->calibY.A01 * pt.y) / 10000) + \
    ((bontouchdev_context->calibY.A10 * pt.x) / 10000) + \
    bontouchdev_context->calibY.A00;
}

static void bontouch_get_device_param(struct usb_device * udev)
{
	mm_segment_t fs;
	struct file *nef_filp = NULL;
	struct inode *inode = NULL;
	loff_t nef_size = 0;
	u8 buf[256];
	u8 tmp[256];
	loff_t pos = 0;
	ssize_t nread;
	int i = 0, j = 0, k = 0;
	long  cdata[6];
    
	fs = get_fs();
	set_fs(KERNEL_DS);
    
	//nef_filp = filp_open("/sdcard/bontouch/calibParam", O_RDONLY, 0);
	nef_filp = filp_open("/etc/bontouchcalibParam", O_CREAT|O_RDONLY, 0666);
	if (IS_ERR(nef_filp))
	{
		printk(KERN_ERR " %s - filp_open error: "
               "Unable to open bontouch /etc/bontouchcalibParam file!\n", __func__);
		goto end;
	}
    
	if (nef_filp->f_path.dentry)
	{
		inode = nef_filp->f_path.dentry->d_inode;
	}
	else
	{
		printk(KERN_ERR " %s - Can not get valid inode!\n", __func__);
		goto out;
	}
    
	nef_size = i_size_read(inode->i_mapping->host);
	if (nef_size <= 0)
	{
		printk(KERN_ERR " %s - Invalid bontouch /etc/bontouchcalibParam file size: "
               "0x%llx\n",__func__ ,nef_size);
		goto out;
	}
    
	nread = vfs_read(nef_filp, buf, 256, &pos);
	if(nread == nef_size)
	{
		for(i=0; i<nread; i++)
		{
			if(buf[i] != '\n' || buf[i] != 0x0a)
				tmp[j++] = buf[i];
			else
			{
				tmp[j] = '\0';
				cdata[k] = simple_strtol(tmp, NULL, 0);
				k++;
				j = 0;
			}
		}
		bontouchdev_context->calibX.A00 = cdata[0];
		printk(" %s - calibX.A00=%ld\n",__func__, bontouchdev_context->calibX.A00);
		bontouchdev_context->calibX.A01 = cdata[1];
		printk(" %s - calibX.A01=%ld\n",__func__, bontouchdev_context->calibX.A01);
		bontouchdev_context->calibX.A10 = cdata[2];
		printk(" %s - calibX.A10=%ld\n",__func__, bontouchdev_context->calibX.A10);
		bontouchdev_context->calibY.A00 = cdata[3];
		printk(" %s - calibY.A00=%ld\n",__func__, bontouchdev_context->calibY.A00);
		bontouchdev_context->calibY.A01 = cdata[4];
		printk(" %s - calibY.A01=%ld\n",__func__, bontouchdev_context->calibY.A01);
		bontouchdev_context->calibY.A10 = cdata[5];
		printk(" %s - calibY.A10=%ld\n",__func__, bontouchdev_context->calibY.A10);
		bontouchdev_context->devConfig.calibrateStatus = 1;
	}
    
out:
	filp_close(nef_filp, current->files);
end:
	set_fs(fs);
}

static int copy_to_buf(long p, char * buf, int* pos)
{
	char tbuf[15];
    
	int i = 0;
	int n = 10;
	int t = *pos;
	if(p == 0)
	{
		buf[(*pos)++] = '0';
		buf[(*pos)++] = '\n';
		goto end;
	}
	if(p < 0)
	{
		buf[(*pos)++] = '-';
		p = -p;
	}
    
	for(i=0;p != 0;)
	{
		tbuf[i++] = p%n+'0';
		p = p/n;
	}
    
	for(;i>0;)
	{
		buf[(*pos)++] = tbuf[--i];
	}
	buf[(*pos)++] = '\n';
    
end:
	return (*pos-t);
}

static int save_param(void)
{
	mm_segment_t fs;
	struct file *nef_filp = NULL;
	//loff_t nef_size = 0;
	char buf[256];
	ssize_t nwrite = 0;
	int i = 0;
	ssize_t len = 0;
	loff_t pos = 0;
	len += copy_to_buf(bontouchdev_context->calibX.A00, buf, &i);
	len += copy_to_buf(bontouchdev_context->calibX.A01, buf, &i);
	len += copy_to_buf(bontouchdev_context->calibX.A10, buf, &i);
	len += copy_to_buf(bontouchdev_context->calibY.A00, buf, &i);
	len += copy_to_buf(bontouchdev_context->calibY.A01, buf, &i);
	len += copy_to_buf(bontouchdev_context->calibY.A10, buf, &i);
    
	printk(" %s - len = %d\n",__func__,len);
	fs = get_fs();
	set_fs(KERNEL_DS);
    
	nef_filp = filp_open("/etc/bontouchcalibParam", O_CREAT|O_RDWR|O_TRUNC, 0666);
	if (IS_ERR(nef_filp))
	{
		printk(KERN_ERR " %s - filp_open error: "
               "Unable to open bontouch /etc/bontouchcalibParam file!\n", __func__);
		goto end;
	}
	nwrite = vfs_write(nef_filp, buf, len, &pos);
    
	filp_close(nef_filp, current->files);
    
end:
	set_fs(fs);
	return nwrite;
}

/*****************************************************************************
 * METOUCH Part
 */
/*
 ****************************************************************************/
static int metouch_read_data(struct usbtouch_usb *dev, unsigned char *pkt)
{
    
	int x = 0, y = 0, touch = 0;
	struct bontouch_point raw_point;
    
	x = ((pkt[3] << 8) | pkt[2]) << 3;
	y = ((pkt[5] << 8) | pkt[4]) << 3;
	touch = (pkt[1] & 0x03) ? 1 : 0;
    
	//printk("x = %d, y = %d, touch = %d\n", x, y, touch);
    
	if(touch)
	{
		if(bontouchdev_context->startCalib)
		{
			bontouchdev_context->points.x = x;
			bontouchdev_context->points.y = y;
		}
	}
    
	if(bontouchdev_context->devConfig.calibrateStatus && (!bontouchdev_context->startCalib))
	{
		raw_point.x = x;
		raw_point.y = y;
		bontouch_translatePoint(raw_point,&x,&y);
	}
    
	//  printk("status=%d, x=%d, y=%d!\n", touch, x, y);
	tflag = touch;
	dev->x = x;
	dev->y = y;
	dev->touch = touch;
    
	return 1;
}


/*****************************************************************************
 * Generic Part
 */
static void usbtouch_process_pkt(struct usbtouch_usb *usbtouch,
                                 unsigned char *pkt, int len)
{
	struct usbtouch_device_info *type = usbtouch->type;
    
	if (!type->read_data(usbtouch, pkt))
		return;
    
	input_report_key(usbtouch->input, BTN_TOUCH, usbtouch->touch);
	//printk("status = %d, x = %d, y = %d\n", usbtouch->touch, usbtouch->x, usbtouch->y);
    
	input_report_abs(usbtouch->input, ABS_X, usbtouch->x);
	input_report_abs(usbtouch->input, ABS_Y, usbtouch->y);
	if (type->max_press)
	{
		printk(" %s - max_press = %d\n",__func__, type->max_press);
		input_report_abs(usbtouch->input, ABS_PRESSURE, usbtouch->press);
	}
	input_sync(usbtouch->input);
}


static void usbtouch_irq(struct urb *urb)
{
	struct usbtouch_usb *usbtouch = urb->context;
	int retval;
    
	switch (urb->status) {
		case 0:
			/* success */
			break;
		case -ETIME:
			/* this urb is timing out */
			dbg("%s - urb timed out - was the device unplugged?",
                __func__);
			return;
		case -ECONNRESET:
		case -ENOENT:
		case -ESHUTDOWN:
		case -EPIPE:
			/* this urb is terminated, clean up */
			dbg("%s - urb shutting down with status: %d",
                __func__, urb->status);
			return;
		default:
			dbg("%s - nonzero urb status received: %d",
                __func__, urb->status);
			goto exit;
	}
    
	usbtouch->type->process_pkt(usbtouch, usbtouch->data, urb->actual_length);
    
exit:
	usb_mark_last_busy(interface_to_usbdev(usbtouch->interface));
    
    
	retval = usb_submit_urb(urb, GFP_ATOMIC);
	if (retval)
		err("%s - usb_submit_urb failed with result: %d",
            __func__, retval);
}

static int usbtouch_open(struct input_dev *input)
{
	struct usbtouch_usb *usbtouch = input_get_drvdata(input);
	int r;
    
	printk(KERN_ERR " %s - open.\n", __func__);
	usbtouch->irq->dev = interface_to_usbdev(usbtouch->interface);
    
	r = usb_autopm_get_interface(usbtouch->interface);
    
	printk(KERN_ERR " %s - r = %d\n", __func__, r);
	r = r ? -EIO : 0;
	if (r < 0)
		goto out;
    
	if (!usbtouch->type->irq_always) {
		if (usb_submit_urb(usbtouch->irq, GFP_KERNEL)) {
			r = -EIO;
			goto out_put;
		}
	}
    
	usbtouch->interface->needs_remote_wakeup = 1;
out_put:
	usb_autopm_put_interface(usbtouch->interface);
out:
	return r;
}

static void usbtouch_close(struct input_dev *input)
{
	struct usbtouch_usb *usbtouch = input_get_drvdata(input);
	int r;
    
	printk(KERN_ERR " %s - close.\n",__func__);
	if (!usbtouch->type->irq_always)
		usb_kill_urb(usbtouch->irq);
	r = usb_autopm_get_interface(usbtouch->interface);
	usbtouch->interface->needs_remote_wakeup = 0;
	if (!r)
		usb_autopm_put_interface(usbtouch->interface);
}

static int usbtouch_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct usbtouch_usb *usbtouch = usb_get_intfdata(intf);
    
	printk(" %s - suspend.\n",__func__);
	usb_kill_urb(usbtouch->irq);
    
	return 0;
}

static int usbtouch_resume(struct usb_interface *intf)
{
	struct usbtouch_usb *usbtouch = usb_get_intfdata(intf);
	struct input_dev *input = usbtouch->input;
	int result = 0;
    
	printk(" %s - resume.\n",__func__);
	mutex_lock(&input->mutex);
	if (input->users || usbtouch->type->irq_always)
		result = usb_submit_urb(usbtouch->irq, GFP_NOIO);
	mutex_unlock(&input->mutex);
    
	return result;
}

static int usbtouch_reset_resume(struct usb_interface *intf)
{
	struct usbtouch_usb *usbtouch = usb_get_intfdata(intf);
	struct input_dev *input = usbtouch->input;
	int err = 0;
    
	printk(" %s - reset_resume.\n",__func__);
	/* reinit the device */
	if (usbtouch->type->init) {
		err = usbtouch->type->init(usbtouch);
		if (err) {
			dbg("%s - type->init() failed, err: %d",
                __func__, err);
			return err;
		}
	}
    
	/* restart IO if needed */
	mutex_lock(&input->mutex);
	if (input->users)
		err = usb_submit_urb(usbtouch->irq, GFP_NOIO);
	mutex_unlock(&input->mutex);
    
	return err;
}

static void usbtouch_free_buffers(struct usb_device *udev,
                                  struct usbtouch_usb *usbtouch)
{
	usb_free_coherent(udev, usbtouch->type->rept_size,
                      usbtouch->data, usbtouch->data_dma);
	kfree(usbtouch->buffer);
}

static struct usb_endpoint_descriptor *usbtouch_get_input_endpoint(struct usb_host_interface *interface)
{
	int i;
    
	for (i = 0; i < interface->desc.bNumEndpoints; i++)
		if (usb_endpoint_dir_in(&interface->endpoint[i].desc))
			return &interface->endpoint[i].desc;
    
	return NULL;
}

static int bontouch_open(struct inode * inode, struct file * filp)
{
	struct bontouchusb *bontouch;
	struct usb_interface *intf;
    
	dbg("bontouch:  init_open");
	printk(" %s - open.\n", __func__);
    
	intf = usb_find_interface(&usbtouch_driver, MINOR_DEVICE_NUMBER);
	if(!intf)
	{
		err("bontouch:  %s usb_find_interface ERROR!",__func__);
		return -1;
	}
    
	bontouch = usb_get_intfdata(intf);
	if(!bontouch)
	{
		err("bontouch:  %s touch is NULL!",__func__);
		return -1;
	}
    
	filp->private_data = bontouch;
    
	return 0;
}
static int bontouch_release(struct inode * inode, struct file * filp)
{
	dbg("bontouch:  bontouch_release");
	printk(" %s - bontouch:  bontouch_release\n", __func__);
	return 0;
}
static long bontouch_ioctl(struct file * filp, unsigned int ctl_code, unsigned long ctl_param)
{
	unsigned char value = 0;
	int ret = -1;
	struct bontouchusb *bontouch;
	int isave = 0;
    
	dbg("bontouch:  bontouch_ioctl");
    
	if(NULL == filp->private_data)
	{
		err("bontouch:  %s private_data is NULL!",__func__);
		printk("bontouch:  %s private_data is NULL!\n",__func__);
		return ret;
	}
    
	bontouch = filp->private_data;
    
	//printk("ctl_code : %x\n", ctl_code);
	switch(ctl_code)
	{
		case CTLCODE_SAVE_PARAM:
			isave = save_param();
			//printk("save ret = %d\n", isave);
			ret = copy_to_user((int*)ctl_param, &isave, sizeof(int));
			if(ret != 0)
			{
				err("bontouch:  %s <CTLCODE_IS_TOUCHED>copy_to_user failed!",__func__);
			}
			//printk("save param !!!\n");
			break;
            
		case CTLCODE_IS_TOUCHED:
			ret = copy_to_user((unsigned char *)ctl_param, (void*)&tflag, sizeof(int));
			if(ret != 0)
			{
				err("bontouch:  %s <CTLCODE_IS_TOUCHED>copy_to_user failed!",__func__);
			}
			break;
            
		case CTLCODE_START_CALIB:
			ret = copy_from_user(&value, (unsigned char *)ctl_param, sizeof(unsigned char));
			if(ret == 0)
			{
				if(value == 0x01)
				{
					bontouchdev_context->startCalib = true;
				}
				else
				{
					bontouchdev_context->startCalib = false;
				}
			}
			break;
            
		case CTLCODE_GET_COORDINATE:
			ret = copy_to_user((struct points *)ctl_param, &bontouchdev_context->points, sizeof(struct points));
			if(ret != 0)
			{
				err("bontouch:  %s <CTLCODE_GET_COORDINATE>copy_to_user failed!",__func__);
			}
			break;
            
		case CTLCODE_GET_CALIB_STATUS:
        {
            int status = bontouchdev_context->devConfig.calibrateStatus;
            ret = copy_to_user((int *)ctl_param, &status, sizeof(int));
            if(ret != 0)
            {
                err("bontouch:  %s <CTLCODE_GET_CALIB_STATUS>copy_to_user failed!",__func__);
            }
            break;
        }
            
		case CTLCODE_SET_CALIB_PARA_X:
			ret = copy_from_user(&bontouchdev_context->calibX, (struct calib_param *)ctl_param, sizeof(struct calib_param));
			if(ret != 0)
			{
				err("bontouch:  %s <CTLCODE_SET_CALIB_PARA_X>copy_to_user failed!",__func__);
			}
			else
			{
				//printk("XA00 = %ld\n",bontouchdev_context->calibX.A00);
				//printk("XA01 = %ld\n",bontouchdev_context->calibX.A01);
				//printk("XA10 = %ld\n",bontouchdev_context->calibX.A10);
				bontouchdev_context->isCalibX = true;
			}
			break;
            
		case CTLCODE_SET_CALIB_PARA_Y:
			ret = copy_from_user(&bontouchdev_context->calibY, (struct calib_param *)ctl_param, sizeof(struct calib_param));
			if(ret != 0)
			{
				err("bontouch:  %s <CTLCODE_SET_CALIB_PARA_Y>copy_to_user failed!",__func__);
			}
			else
			{
				//printk("XA00 = %ld\n",bontouchdev_context->calibY.A00);
				//printk("XA01 = %ld\n",bontouchdev_context->calibY.A01);
				//printk("XA10 = %ld\n",bontouchdev_context->calibY.A10);
				bontouchdev_context->isCalibY = true;
			}
			break;
            
		case CTLCODE_GET_CALIB_PARA_X:
			ret = copy_to_user((struct calib_param *)ctl_param, &bontouchdev_context->calibX, sizeof(struct calib_param));
			if(ret != 0)
			{
				err("bontouch:  %s <CTLCODE_GET_CALIB_PARA>copy_to_user failed!",__func__);
			}
			break;
            
		case CTLCODE_GET_CALIB_PARA_Y:
			ret = copy_to_user((struct calib_param *)ctl_param, &bontouchdev_context->calibY, sizeof(struct calib_param));
			if(ret != 0)
			{
				err("bontouch:  %s <CTLCODE_GET_CALIB_PARA>copy_to_user failed!",__func__);
			}
			break;
            
		case CTLCODE_GET_PRODUCT_ID:
        {
            int status = bontouchdev_context->productid;
            ret = copy_to_user((int *)ctl_param, &status, sizeof(int));
            if(ret != 0)
            {
                err("bontouch:  %s <CTLCODE_GET_PRODUCT_ID>copy_to_user failed!",__func__);
            }
            break;
        }
            
		case CTLCODE_DEVICE_RESET:
			ret = copy_from_user(&value, (unsigned char *)ctl_param, sizeof(unsigned char));
			if(ret == 0)
			{
				if(value == 0x01)
				{
					bontouchdev_context->devConfig.calibrateStatus = 0;
					ret = device_reset();
					ret = copy_to_user((int *)ctl_param, &ret, sizeof(int));
					if(ret != 0)
					{
						err("bontouch:  %s <CTLCODE_DEVICE_RESET>copy_to_user failed!",__func__);
					}
				}
			}
			break;
            
		default:
			break;
	}
    
	if(bontouchdev_context->isCalibX && bontouchdev_context->isCalibY)
	{
		bontouchdev_context->isCalibX = false;
		bontouchdev_context->isCalibY = false;
		bontouchdev_context->devConfig.calibrateStatus = 1;
	}
	return 0;
}
static struct usb_class_driver bontouch_class_driver =
{
	.name = "bontouch",
	.fops = &bontouch_fops,
	.minor_base = MINOR_DEVICE_NUMBER,
};
static struct file_operations bontouch_fops = {
	.owner = THIS_MODULE,
	.open  = bontouch_open,
	.unlocked_ioctl = bontouch_ioctl,
	.release = bontouch_release,
};


static bool bontouch_mkdev(void)
{
	int retval;
    
	//create device node
	dev_t devno = MKDEV(MAJOR_DEVICE_NUMBER,MINOR_DEVICE_NUMBER);
    
	retval = register_chrdev_region(devno,1,TOUCH_DEVICE_NAME);
	if(retval < 0)
	{
		err("bontouch:  %s register chrdev error.",__func__);
		return false;
	}
    
	cdev_init(&bontouch_cdev,&bontouch_fops);
	bontouch_cdev.owner = THIS_MODULE;
	bontouch_cdev.ops = &bontouch_fops;
	retval = cdev_add(&bontouch_cdev,devno,1);
    
	if(retval)
	{
		err("bontouch:  %s  Adding char_reg_setup_cdev error=%d",__func__,retval);
		return false;
	}
    
	irser_class = class_create(THIS_MODULE, TOUCH_DEVICE_NAME);
	if(IS_ERR(irser_class))
	{
		err("bontouch:  %s class create failed.",__func__);
		return false;
	}
    
	device_create(irser_class,NULL,MKDEV(MAJOR_DEVICE_NUMBER,MINOR_DEVICE_NUMBER),NULL,TOUCH_DEVICE_NAME);
    
	printk("bontouch:  %s success!\n",__func__);
	return true;
}

static int usbtouch_probe(struct usb_interface *intf,
                          const struct usb_device_id *id)
{
	struct usb_device *dev = interface_to_usbdev(intf);
    
	struct usbtouch_usb *usbtouch;
	struct input_dev *input_dev;
	struct usb_endpoint_descriptor *endpoint;
	struct usb_device *udev = interface_to_usbdev(intf);
	struct usbtouch_device_info *type;
	int err = -ENOMEM;
    
	//GetUserCfg();
    
	/* some devices are ignored */
	if (id->driver_info == -1)
		return -ENODEV;
    
	endpoint = usbtouch_get_input_endpoint(intf->cur_altsetting);
	if (!endpoint)
		return -ENXIO;
	//--->
	if (!(bontouchdev_context = kmalloc(sizeof(struct device_context), GFP_KERNEL)))
		return -ENOMEM;
	memset(bontouchdev_context,0,sizeof(struct device_context));
	//---
    
	usbtouch = kzalloc(sizeof(struct usbtouch_usb), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!usbtouch || !input_dev)
		goto out_free;
    
	type = &usbtouch_dev_info[id->driver_info];
	usbtouch->type = type;
	if (!type->process_pkt)
		type->process_pkt = usbtouch_process_pkt;
    
	usbtouch->data = usb_alloc_coherent(udev, type->rept_size,
                                        GFP_KERNEL, &usbtouch->data_dma);
	if (!usbtouch->data)
		goto out_free;
    
	if (type->get_pkt_len) {
		usbtouch->buffer = kmalloc(type->rept_size, GFP_KERNEL);
		if (!usbtouch->buffer)
			goto out_free_buffers;
	}
    
	usbtouch->irq = usb_alloc_urb(0, GFP_KERNEL);
	if (!usbtouch->irq) {
		dbg("%s - usb_alloc_urb failed: usbtouch->irq", __func__);
		goto out_free_buffers;
	}
    
	usbtouch->interface = intf;
	usbtouch->input = input_dev;
    
	if (udev->manufacturer)
		strlcpy(usbtouch->name, udev->manufacturer, sizeof(usbtouch->name));
    
	if (udev->product) {
		if (udev->manufacturer)
			strlcat(usbtouch->name, " ", sizeof(usbtouch->name));
		strlcat(usbtouch->name, udev->product, sizeof(usbtouch->name));
	}
    
	if (!strlen(usbtouch->name))
		snprintf(usbtouch->name, sizeof(usbtouch->name),
                 "BonXone single-touch Touchscreen %04x:%04x",
                 le16_to_cpu(udev->descriptor.idVendor),
                 le16_to_cpu(udev->descriptor.idProduct));
    
	usb_make_path(udev, usbtouch->phys, sizeof(usbtouch->phys));
	strlcat(usbtouch->phys, "/input0", sizeof(usbtouch->phys));
    
	input_dev->name = usbtouch->name;
	input_dev->phys = usbtouch->phys;
	usb_to_input_id(udev, &input_dev->id);
	input_dev->dev.parent = &intf->dev;
    
	input_set_drvdata(input_dev, usbtouch);
    
	input_dev->open = usbtouch_open;
	input_dev->close = usbtouch_close;
    
	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
    
	input_set_abs_params(input_dev, ABS_X, 0, 32767, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, 32767, 0, 0);
	//	input_set_abs_params(input_dev, ABS_X, type->min_xc, type->max_xc, 0, 0);
	//	input_set_abs_params(input_dev, ABS_Y, type->min_yc, type->max_yc, 0, 0);
    
	if (type->max_press)
		input_set_abs_params(input_dev, ABS_PRESSURE, type->min_press, type->max_press, 0, 0);
    
	if (usb_endpoint_type(endpoint) == USB_ENDPOINT_XFER_INT)
		usb_fill_int_urb(usbtouch->irq, udev, usb_rcvintpipe(udev, endpoint->bEndpointAddress),
                         usbtouch->data, type->rept_size,
                         usbtouch_irq, usbtouch, endpoint->bInterval);
	else
		usb_fill_bulk_urb(usbtouch->irq, udev,
                          usb_rcvbulkpipe(udev, endpoint->bEndpointAddress),
                          usbtouch->data, type->rept_size,
                          usbtouch_irq, usbtouch);
    
	usbtouch->irq->dev = udev;
	usbtouch->irq->transfer_dma = usbtouch->data_dma;
	usbtouch->irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
    
	/* device specific allocations */
	if (type->alloc) {
		err = type->alloc(usbtouch);
		if (err) {
			dbg("%s - type->alloc() failed, err: %d", __func__, err);
			goto out_free_urb;
		}
	}
    
	/* device specific initialisation*/
	if (type->init) {
		err = type->init(usbtouch);
		if (err) {
			dbg("%s - type->init() failed, err: %d", __func__, err);
			goto out_do_exit;
		}
	}
    
	err = input_register_device(usbtouch->input);
	if (err) {
		dbg("%s - input_register_device failed, err: %d", __func__, err);
		goto out_do_exit;
	}
    
	usb_set_intfdata(intf, usbtouch);
    
	if (usbtouch->type->irq_always) {
		/* this can't fail */
		usb_autopm_get_interface(intf);
		err = usb_submit_urb(usbtouch->irq, GFP_KERNEL);
		if (err) {
			usb_autopm_put_interface(intf);
			err("%s - usb_submit_urb failed with result: %d",
                __func__, err);
			goto out_unregister_input;
		}
	}
    
    
	//--->
	// By Franky, Tue Dec 10.
	if(usb_register_dev(intf, &bontouch_class_driver))
	{
		err("bontouch:   %s Not able to get a minor for this device.",__func__);
		usb_set_intfdata(intf,NULL);
		goto out_unregister_input;
	}
	bontouch_get_device_param(dev);
	bontouch_mkdev();
	bontouchdev_context->productid = dev->descriptor.idProduct;
	printk("bontouch id=0x%x: %s success!\n", bontouchdev_context->productid, __func__);
	//---
    
	return 0;
    
    
out_unregister_input:
	input_unregister_device(input_dev);
	input_dev = NULL;
out_do_exit:
	if (type->exit)
		type->exit(usbtouch);
out_free_urb:
	usb_free_urb(usbtouch->irq);
out_free_buffers:
	usbtouch_free_buffers(udev, usbtouch);
out_free:
	input_free_device(input_dev);
	kfree(usbtouch);
	kfree(bontouchdev_context);
	return err;
}

static void usbtouch_disconnect(struct usb_interface *intf)
{
	struct usbtouch_usb *usbtouch = usb_get_intfdata(intf);
    
    
	//--->
	// By Franky, Tue Dec 10.
	dev_t devno = MKDEV(MAJOR_DEVICE_NUMBER,MINOR_DEVICE_NUMBER);
    
	cdev_del(&bontouch_cdev);
	unregister_chrdev_region(devno,1);
    
	device_destroy(irser_class,devno);
	class_destroy(irser_class);
    
	dbg("%s - called", __func__);
	usb_deregister_dev(intf, &bontouch_class_driver);
	if(bontouchdev_context)
	{
		kfree(bontouchdev_context);
	}
    
	//---
	if (!usbtouch)
		return;
    
	dbg("%s - usbtouch is initialized, cleaning up", __func__);
	usb_set_intfdata(intf, NULL);
	/* this will stop IO via close */
	input_unregister_device(usbtouch->input);
	usb_free_urb(usbtouch->irq);
	if (usbtouch->type->exit)
		usbtouch->type->exit(usbtouch);
	usbtouch_free_buffers(interface_to_usbdev(intf), usbtouch);
	kfree(usbtouch);
}

static struct usb_driver usbtouch_driver = {
	.name		= "bontouch",
	.probe		= usbtouch_probe,
	.disconnect	= usbtouch_disconnect,
	.suspend	= usbtouch_suspend,
	.resume		= usbtouch_resume,
	.reset_resume	= usbtouch_reset_resume,
	.id_table	= metusb_table,
	.supports_autosuspend = 1,
};


static int __init usbtouch_init(void)
{
	return usb_register(&usbtouch_driver);
}

static void __exit usbtouch_cleanup(void)
{
	usb_deregister(&usbtouch_driver);
}

module_init(usbtouch_init);
module_exit(usbtouch_cleanup);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

MODULE_ALIAS("bontouch");
