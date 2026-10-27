/*
 *  HID driver for Sony PS3 Sixaxis and PS4 DualShock 4 devices
 */

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/input/mt.h>
#include <linux/workqueue.h>

#include "hid-ids.h"

#define VAIO_RDESC_CONSTANT       (1 << 0)
#define SIXAXIS_CONTROLLER_USB    (1 << 1)
#define SIXAXIS_CONTROLLER_BT     (1 << 2)
#define DUALSHOCK4_CONTROLLER_USB (1 << 5)
#define DUALSHOCK4_CONTROLLER_BT  (1 << 6)

#define DUALSHOCK4_CONTROLLER (DUALSHOCK4_CONTROLLER_USB | DUALSHOCK4_CONTROLLER_BT)

static const u8 sixaxis_rdesc_fixup[] = {
	0x95, 0x13, 0x09, 0x01, 0x81, 0x02, 0x95, 0x0C,
	0x81, 0x01, 0x75, 0x10, 0x95, 0x04, 0x26, 0xFF,
	0x03, 0x46, 0xFF, 0x03, 0x09, 0x01, 0x81, 0x02
};

static const u8 sixaxis_rdesc_fixup2[] = {
	0x05, 0x01, 0x09, 0x04, 0xa1, 0x01, 0xa1, 0x02,
	0x85, 0x01, 0x75, 0x08, 0x95, 0x01, 0x15, 0x00,
	0x26, 0xff, 0x00, 0x81, 0x03, 0x75, 0x01, 0x95,
	0x13, 0x15, 0x00, 0x25, 0x01, 0x35, 0x00, 0x45,
	0x01, 0x05, 0x09, 0x19, 0x01, 0x29, 0x13, 0x81,
	0x02, 0x75, 0x01, 0x95, 0x0d, 0x06, 0x00, 0xff,
	0x81, 0x03, 0x15, 0x00, 0x26, 0xff, 0x00, 0x05,
	0x01, 0x09, 0x01, 0xa1, 0x00, 0x75, 0x08, 0x95,
	0x04, 0x35, 0x00, 0x46, 0xff, 0x00, 0x09, 0x30,
	0x09, 0x31, 0x09, 0x32, 0x09, 0x35, 0x81, 0x02,
	0xc0, 0x05, 0x01, 0x95, 0x13, 0x09, 0x01, 0x81,
	0x02, 0x95, 0x0c, 0x81, 0x01, 0x75, 0x10, 0x95,
	0x04, 0x26, 0xff, 0x03, 0x46, 0xff, 0x03, 0x09,
	0x01, 0x81, 0x02, 0xc0, 0xa1, 0x02, 0x85, 0x02,
	0x75, 0x08, 0x95, 0x30, 0x09, 0x01, 0xb1, 0x02,
	0xc0, 0xa1, 0x02, 0x85, 0xee, 0x75, 0x08, 0x95,
	0x30, 0x09, 0x01, 0xb1, 0x02, 0xc0, 0xa1, 0x02,
	0x85, 0xef, 0x75, 0x08, 0x95, 0x30, 0x09, 0x01,
	0xb1, 0x02, 0xc0, 0xc0,
};

static u8 dualshock4_usb_rdesc[] = {
	0x05, 0x01,         /*  Usage Page (Desktop),               */
	0x09, 0x05,         /*  Usage (Gamepad),                    */
	0xA1, 0x01,         /*  Collection (Application),           */
	0x85, 0x01,         /*      Report ID (1),                  */
	0x09, 0x30,         /*      Usage (X),                      */
	0x09, 0x31,         /*      Usage (Y),                      */
	0x09, 0x32,         /*      Usage (Z),                      */
	0x09, 0x35,         /*      Usage (Rz),                     */
	0x15, 0x00,         /*      Logical Minimum (0),            */
	0x26, 0xFF, 0x00,   /*      Logical Maximum (255),          */
	0x75, 0x08,         /*      Report Size (8),                */
	0x95, 0x04,         /*      Report Count (4),               */
	0x81, 0x02,         /*      Input (Variable),               */
	0x09, 0x39,         /*      Usage (Hat Switch),             */
	0x15, 0x00,         /*      Logical Minimum (0),            */
	0x25, 0x07,         /*      Logical Maximum (7),            */
	0x35, 0x00,         /*      Physical Minimum (0),           */
	0x46, 0x3B, 0x01,   /*      Physical Maximum (315),         */
	0x65, 0x14,         /*      Unit (Degrees),                 */
	0x75, 0x04,         /*      Report Size (4),                */
	0x95, 0x01,         /*      Report Count (1),               */
	0x81, 0x42,         /*      Input (Variable, Null State),   */
	0x65, 0x00,         /*      Unit,                           */
	0x05, 0x09,         /*      Usage Page (Button),            */
	0x19, 0x01,         /*      Usage Minimum (01h),            */
	0x29, 0x0E,         /*      Usage Maximum (0Eh),            */
	0x15, 0x00,         /*      Logical Minimum (0),            */
	0x25, 0x01,         /*      Logical Maximum (1),            */
	0x75, 0x01,         /*      Report Size (1),                */
	0x95, 0x0E,         /*      Report Count (14),              */
	0x81, 0x02,         /*      Input (Variable),               */
	0x06, 0x00, 0xFF,   /*      Usage Page (FF00h),             */
	0x09, 0x20,         /*      Usage (20h),                    */
	0x75, 0x06,         /*      Report Size (6),                */
	0x95, 0x01,         /*      Report Count (1),               */
	0x15, 0x00,         /*      Logical Minimum (0),            */
	0x25, 0x3F,         /*      Logical Maximum (63),           */
	0x81, 0x02,         /*      Input (Variable),               */
	0x05, 0x01,         /*      Usage Page (Desktop),           */
	0x09, 0x33,         /*      Usage (Rx),                     */
	0x09, 0x34,         /*      Usage (Ry),                     */
	0x15, 0x00,         /*      Logical Minimum (0),            */
	0x26, 0xFF, 0x00,   /*      Logical Maximum (255),          */
	0x75, 0x08,         /*      Report Size (8),                */
	0x95, 0x02,         /*      Report Count (2),               */
	0x81, 0x02,         /*      Input (Variable),               */
	0x06, 0x00, 0xFF,   /*      Usage Page (FF00h),             */
	0x09, 0x21,         /*      Usage (21h),                    */
	0x95, 0x03,         /*      Report Count (3),               */
	0x81, 0x02,         /*      Input (Variable),               */
	0x05, 0x01,         /*      Usage Page (Desktop),           */
	0x19, 0x40,         /*      Usage Minimum (40h),            */
	0x29, 0x42,         /*      Usage Maximum (42h),            */
	0x16, 0x00, 0x80,   /*      Logical Minimum (-32768),       */
	0x26, 0x00, 0x7F,   /*      Logical Maximum (32767),        */
	0x75, 0x10,         /*      Report Size (16),               */
	0x95, 0x03,         /*      Report Count (3),               */
	0x81, 0x02,         /*      Input (Variable),               */
	0x19, 0x43,         /*      Usage Minimum (43h),            */
	0x29, 0x45,         /*      Usage Maximum (45h),            */
	0x16, 0x00, 0xE0,   /*      Logical Minimum (-8192),        */
	0x26, 0xFF, 0x1F,   /*      Logical Maximum (8191),         */
	0x95, 0x03,         /*      Report Count (3),               */
	0x81, 0x02,         /*      Input (Variable),               */
	0x06, 0x00, 0xFF,   /*      Usage Page (FF00h),             */
	0x09, 0x21,         /*      Usage (21h),                    */
	0x15, 0x00,         /*      Logical Minimum (0),            */
	0x26, 0xFF, 0x00,   /*      Logical Maximum (255),          */
	0x75, 0x08,         /*      Report Size (8),                */
	0x95, 0x27,         /*      Report Count (39),              */
	0x81, 0x02,         /*      Input (Variable),               */
	0x85, 0x05,         /*      Report ID (5),                  */
	0x09, 0x22,         /*      Usage (22h),                    */
	0x95, 0x1F,         /*      Report Count (31),              */
	0x91, 0x02,         /*      Output (Variable),              */
	0x85, 0x04,         /*      Report ID (4),                  */
	0x09, 0x23,         /*      Usage (23h),                    */
	0x95, 0x24,         /*      Report Count (36),              */
	0xB1, 0x02,         /*      Feature (Variable),             */
	0x85, 0x02,         /*      Report ID (2),                  */
	0x09, 0x24,         /*      Usage (24h),                    */
	0x95, 0x24,         /*      Report Count (36),              */
	0xB1, 0x02,         /*      Feature (Variable),             */
	0xC0                /*  End Collection                      */
};

/*
 * DualShock 4 Bluetooth descriptor supporting BOTH:
 * - Standard 10-byte Report ID 0x01 (sent before Feature Report 0x02 or on clones)
 * - Extended 78-byte Report ID 0x11 (sent after Feature Report 0x02)
 */
static u8 dualshock4_bt_rdesc[] = {
	0x05, 0x01,         /*  Usage Page (Desktop),               */
	0x09, 0x05,         /*  Usage (Gamepad),                    */
	0xA1, 0x01,         /*  Collection (Application),           */
	0x85, 0x01,         /*      Report ID (1),                  */
	0x09, 0x30,         /*      Usage (X),                      */
	0x09, 0x31,         /*      Usage (Y),                      */
	0x09, 0x32,         /*      Usage (Z),                      */
	0x09, 0x35,         /*      Usage (Rz),                     */
	0x15, 0x00,         /*      Logical Minimum (0),            */
	0x26, 0xFF, 0x00,   /*      Logical Maximum (255),          */
	0x75, 0x08,         /*      Report Size (8),                */
	0x95, 0x04,         /*      Report Count (4),               */
	0x81, 0x02,         /*      Input (Variable),               */
	0x09, 0x39,         /*      Usage (Hat Switch),             */
	0x15, 0x00,         /*      Logical Minimum (0),            */
	0x25, 0x07,         /*      Logical Maximum (7),            */
	0x75, 0x04,         /*      Report Size (4),                */
	0x95, 0x01,         /*      Report Count (1),               */
	0x81, 0x42,         /*      Input (Variable, Null State),   */
	0x05, 0x09,         /*      Usage Page (Button),            */
	0x19, 0x01,         /*      Usage Minimum (01h),            */
	0x29, 0x0E,         /*      Usage Maximum (0Eh),            */
	0x15, 0x00,         /*      Logical Minimum (0),            */
	0x25, 0x01,         /*      Logical Maximum (1),            */
	0x75, 0x01,         /*      Report Size (1),                */
	0x95, 0x0E,         /*      Report Count (14),              */
	0x81, 0x02,         /*      Input (Variable),               */
	0x75, 0x06,         /*      Report Size (6),                */
	0x95, 0x01,         /*      Report Count (1),               */
	0x81, 0x01,         /*      Input (Constant),               */
	0x05, 0x01,         /*      Usage Page (Desktop),           */
	0x09, 0x33,         /*      Usage (Rx),                     */
	0x09, 0x34,         /*      Usage (Ry),                     */
	0x15, 0x00,         /*      Logical Minimum (0),            */
	0x26, 0xFF, 0x00,   /*      Logical Maximum (255),          */
	0x75, 0x08,         /*      Report Size (8),                */
	0x95, 0x02,         /*      Report Count (2),               */
	0x81, 0x02,         /*      Input (Variable),               */
	0x06, 0x04, 0xFF,   /*      Usage Page (FF04h),             */
	0x85, 0x02,         /*      Report ID (2),                  */
	0x09, 0x24,         /*      Usage (24h),                    */
	0x95, 0x24,         /*      Report Count (36),              */
	0xB1, 0x02,         /*      Feature (Variable),             */
	0x85, 0x11,         /*      Report ID (17),                 */
	0x06, 0x00, 0xFF,   /*      Usage Page (FF00h),             */
	0x09, 0x20,         /*      Usage (20h),                    */
	0x95, 0x02,         /*      Report Count (2),               */
	0x81, 0x02,         /*      Input (Variable),               */
	0x05, 0x01,         /*      Usage Page (Desktop),           */
	0x09, 0x30,         /*      Usage (X),                      */
	0x09, 0x31,         /*      Usage (Y),                      */
	0x09, 0x32,         /*      Usage (Z),                      */
	0x09, 0x35,         /*      Usage (Rz),                     */
	0x15, 0x00,         /*      Logical Minimum (0),            */
	0x26, 0xFF, 0x00,   /*      Logical Maximum (255),          */
	0x75, 0x08,         /*      Report Size (8),                */
	0x95, 0x04,         /*      Report Count (4),               */
	0x81, 0x02,         /*      Input (Variable),               */
	0x09, 0x39,         /*      Usage (Hat Switch),             */
	0x15, 0x00,         /*      Logical Minimum (0),            */
	0x25, 0x07,         /*      Logical Maximum (7),            */
	0x75, 0x04,         /*      Report Size (4),                */
	0x95, 0x01,         /*      Report Count (1),               */
	0x81, 0x42,         /*      Input (Variable, Null State),   */
	0x05, 0x09,         /*      Usage Page (Button),            */
	0x19, 0x01,         /*      Usage Minimum (01h),            */
	0x29, 0x0E,         /*      Usage Maximum (0Eh),            */
	0x15, 0x00,         /*      Logical Minimum (0),            */
	0x25, 0x01,         /*      Logical Maximum (1),            */
	0x75, 0x01,         /*      Report Size (1),                */
	0x95, 0x0E,         /*      Report Count (14),              */
	0x81, 0x02,         /*      Input (Variable),               */
	0x75, 0x06,         /*      Report Size (6),                */
	0x95, 0x01,         /*      Report Count (1),               */
	0x81, 0x01,         /*      Input (Constant),               */
	0x05, 0x01,         /*      Usage Page (Desktop),           */
	0x09, 0x33,         /*      Usage (Rx),                     */
	0x09, 0x34,         /*      Usage (Ry),                     */
	0x15, 0x00,         /*      Logical Minimum (0),            */
	0x26, 0xFF, 0x00,   /*      Logical Maximum (255),          */
	0x75, 0x08,         /*      Report Size (8),                */
	0x95, 0x02,         /*      Report Count (2),               */
	0x81, 0x02,         /*      Input (Variable),               */
	0x06, 0x00, 0xFF,   /*      Usage Page (FF00h),             */
	0x09, 0x20,         /*      Usage (20h),                    */
	0x95, 0x03,         /*      Report Count (3),               */
	0x81, 0x02,         /*      Input (Variable),               */
	0x05, 0x01,         /*      Usage Page (Desktop),           */
	0x19, 0x40,         /*      Usage Minimum (40h),            */
	0x29, 0x42,         /*      Usage Maximum (42h),            */
	0x16, 0x00, 0x80,   /*      Logical Minimum (-32768),       */
	0x26, 0x00, 0x7F,   /*      Logical Maximum (32767),        */
	0x75, 0x10,         /*      Report Size (16),               */
	0x95, 0x03,         /*      Report Count (3),               */
	0x81, 0x02,         /*      Input (Variable),               */
	0x19, 0x43,         /*      Usage Minimum (43h),            */
	0x29, 0x45,         /*      Usage Maximum (45h),            */
	0x16, 0x00, 0xE0,   /*      Logical Minimum (-8192),        */
	0x26, 0xFF, 0x1F,   /*      Logical Maximum (8191),         */
	0x95, 0x03,         /*      Report Count (3),               */
	0x81, 0x02,         /*      Input (Variable),               */
	0x06, 0x00, 0xFF,   /*      Usage Page (FF00h),             */
	0x09, 0x20,         /*      Usage (20h),                    */
	0x15, 0x00,         /*      Logical Minimum (0),            */
	0x26, 0xFF, 0x00,   /*      Logical Maximum (255),          */
	0x75, 0x08,         /*      Report Size (8),                */
	0x95, 0x31,         /*      Report Count (51),              */
	0x81, 0x02,         /*      Input (Variable),               */
	0x09, 0x21,         /*      Usage (21h),                    */
	0x75, 0x08,         /*      Report Size (8),                */
	0x95, 0x4D,         /*      Report Count (77),              */
	0x91, 0x02,         /*      Output (Variable),              */
	0xC0                /*  End Collection                      */
};

struct sony_sc {
	struct hid_device *hdev;
	unsigned long quirks;
	struct work_struct bt_init_work;
	bool work_inited;
};

static __u8 *sony_report_fixup(struct hid_device *hdev, __u8 *rdesc,
		unsigned int *rsize)
{
	struct sony_sc *sc = hid_get_drvdata(hdev);

	if ((sc->quirks & VAIO_RDESC_CONSTANT) && *rsize >= 56 &&
	    rdesc[2] == 0x09 && rdesc[3] == 0x02 &&
	    rdesc[54] == 0x81 && rdesc[55] == 0x07) {
		hid_info(hdev, "Fixing up Sony RF Receiver report descriptor\n");
		rdesc[55] = 0x06;
	}

	if ((sc->quirks & DUALSHOCK4_CONTROLLER_USB) && (*rsize == 467 || *rsize == 507)) {
		hid_info(hdev, "Using modified Dualshock 4 USB report descriptor\n");
		rdesc = dualshock4_usb_rdesc;
		*rsize = sizeof(dualshock4_usb_rdesc);
	} else if ((sc->quirks & DUALSHOCK4_CONTROLLER_BT) && *rsize >= 300) {
		hid_info(hdev, "Using modified Dualshock 4 Bluetooth report descriptor (orig size %u)\n", *rsize);
		rdesc = dualshock4_bt_rdesc;
		*rsize = sizeof(dualshock4_bt_rdesc);
	}

	if ((((sc->quirks & SIXAXIS_CONTROLLER_USB) && *rsize == 148) ||
			((sc->quirks & SIXAXIS_CONTROLLER_BT) && *rsize == 149)) &&
			rdesc[83] == 0x75) {
		hid_info(hdev, "Fixing up Sony Sixaxis report descriptor\n");
		memcpy((void *)&rdesc[83], (void *)&sixaxis_rdesc_fixup,
			sizeof(sixaxis_rdesc_fixup));
	} else if ((sc->quirks & SIXAXIS_CONTROLLER_USB) &&
		   *rsize > sizeof(sixaxis_rdesc_fixup2)) {
		*rsize = sizeof(sixaxis_rdesc_fixup2);
		memcpy(rdesc, &sixaxis_rdesc_fixup2, *rsize);
	}
	return rdesc;
}

static void dualshock4_parse_report(struct sony_sc *sc, __u8 *rd, int size)
{
	struct hid_input *hidinput;
	struct input_dev *input_dev;
	int n, offset;

	if (list_empty(&sc->hdev->inputs))
		return;

	hidinput = list_entry(sc->hdev->inputs.next, struct hid_input, list);
	input_dev = hidinput->input;
	offset = (sc->quirks & DUALSHOCK4_CONTROLLER_USB) ? 35 : 37;
	if (size < offset + 8)
		return;

	for (n = 0; n < 2; n++) {
		__u16 x, y;
		x = rd[offset+1] | ((rd[offset+2] & 0xF) << 8);
		y = ((rd[offset+2] & 0xF0) >> 4) | (rd[offset+3] << 4);

		input_mt_slot(input_dev, n);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER,
					!(rd[offset] >> 7));
		input_report_abs(input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(input_dev, ABS_MT_POSITION_Y, y);

		offset += 4;
	}
}

static int sony_raw_event(struct hid_device *hdev, struct hid_report *report,
		__u8 *rd, int size)
{
	struct sony_sc *sc = hid_get_drvdata(hdev);

	if ((sc->quirks & (SIXAXIS_CONTROLLER_USB | SIXAXIS_CONTROLLER_BT)) &&
			rd[0] == 0x01 && size == 49) {
		swap(rd[41], rd[42]);
		swap(rd[43], rd[44]);
		swap(rd[45], rd[46]);
		swap(rd[47], rd[48]);
	} else if (((sc->quirks & DUALSHOCK4_CONTROLLER_USB) && rd[0] == 0x01 && size == 64) ||
		   ((sc->quirks & DUALSHOCK4_CONTROLLER_BT) && rd[0] == 0x11 && size >= 78)) {
		dualshock4_parse_report(sc, rd, size);
	}

	return 0;
}

static int sony_input_configured(struct hid_device *hdev,
				 struct hid_input *hidinput)
{
	struct sony_sc *sc = hid_get_drvdata(hdev);
	struct input_dev *input_dev = hidinput->input;
	int ret;

	if (sc->quirks & DUALSHOCK4_CONTROLLER) {
		ret = input_mt_init_slots(input_dev, 2, 0);
		if (ret < 0)
			return 0;
		input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, 1920, 0, 0);
		input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, 942, 0, 0);
	}
	return 0;
}

static int sixaxis_usb_output_raw_report(struct hid_device *hid, __u8 *buf,
		size_t count, unsigned char report_type)
{
	struct usb_interface *intf = to_usb_interface(hid->dev.parent);
	struct usb_device *dev = interface_to_usbdev(intf);
	struct usb_host_interface *interface = intf->cur_altsetting;
	int report_id = buf[0];
	int ret;

	if (report_type == HID_OUTPUT_REPORT) {
		buf++;
		count--;
	}

	ret = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
		HID_REQ_SET_REPORT,
		USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
		((report_type + 1) << 8) | report_id,
		interface->desc.bInterfaceNumber, buf, count,
		USB_CTRL_SET_TIMEOUT);

	if (ret > 0 && report_type == HID_OUTPUT_REPORT)
		ret++;

	return ret;
}

static int sixaxis_set_operational_usb(struct hid_device *hdev)
{
	struct usb_interface *intf = to_usb_interface(hdev->dev.parent);
	struct usb_device *dev = interface_to_usbdev(intf);
	__u16 ifnum = intf->cur_altsetting->desc.bInterfaceNumber;
	int ret;
	char *buf = kmalloc(18, GFP_KERNEL);

	if (!buf)
		return -ENOMEM;

	ret = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
				 HID_REQ_GET_REPORT,
				 USB_DIR_IN | USB_TYPE_CLASS |
				 USB_RECIP_INTERFACE,
				 (3 << 8) | 0xf2, ifnum, buf, 17,
				 USB_CTRL_GET_TIMEOUT);
	if (ret < 0)
		hid_err(hdev, "can't set operational mode\n");

	kfree(buf);
	return ret;
}

static int sixaxis_set_operational_bt(struct hid_device *hdev)
{
	unsigned char buf[] = { 0xf4, 0x42, 0x03, 0x00, 0x00 };
	if (!hdev->hid_output_raw_report)
		return 0;
	return hdev->hid_output_raw_report(hdev, buf, sizeof(buf), HID_FEATURE_REPORT);
}

static void dualshock4_bt_init_worker(struct work_struct *work)
{
	struct sony_sc *sc = container_of(work, struct sony_sc, bt_init_work);
	struct hid_device *hdev = sc->hdev;
	__u8 *buf;

	if (!hdev || !hdev->hid_get_raw_report)
		return;

	buf = kzalloc(37, GFP_KERNEL);
	if (!buf)
		return;

	hdev->hid_get_raw_report(hdev, 0x02, buf, 37, HID_FEATURE_REPORT);
	kfree(buf);
}

static int sony_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret;
	unsigned long quirks = id->driver_data;
	struct sony_sc *sc;
	unsigned int connect_mask = HID_CONNECT_DEFAULT;

	sc = kzalloc(sizeof(*sc), GFP_KERNEL);
	if (sc == NULL) {
		hid_err(hdev, "can't alloc sony descriptor\n");
		return -ENOMEM;
	}

	sc->quirks = quirks;
	sc->hdev = hdev;
	hid_set_drvdata(hdev, sc);

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "parse failed\n");
		goto err_free;
	}

	if (sc->quirks & (VAIO_RDESC_CONSTANT | SIXAXIS_CONTROLLER_USB | SIXAXIS_CONTROLLER_BT))
		connect_mask |= HID_CONNECT_HIDDEV_FORCE;

	ret = hid_hw_start(hdev, connect_mask);
	if (ret) {
		hid_err(hdev, "hw start failed\n");
		goto err_free;
	}

	if (sc->quirks & SIXAXIS_CONTROLLER_USB) {
		hdev->hid_output_raw_report = sixaxis_usb_output_raw_report;
		ret = sixaxis_set_operational_usb(hdev);
	} else if (sc->quirks & SIXAXIS_CONTROLLER_BT) {
		ret = sixaxis_set_operational_bt(hdev);
	} else if (sc->quirks & DUALSHOCK4_CONTROLLER_BT) {
		INIT_WORK(&sc->bt_init_work, dualshock4_bt_init_worker);
		sc->work_inited = true;
		schedule_work(&sc->bt_init_work);
		ret = 0;
	} else {
		ret = 0;
	}

	if (ret < 0)
		goto err_stop;

	return 0;
err_stop:
	hid_hw_stop(hdev);
err_free:
	kfree(sc);
	return ret;
}

static void sony_remove(struct hid_device *hdev)
{
	struct sony_sc *sc = hid_get_drvdata(hdev);
	if (sc && sc->work_inited)
		cancel_work_sync(&sc->bt_init_work);
	hid_hw_stop(hdev);
	kfree(sc);
}

static const struct hid_device_id sony_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_SONY, USB_DEVICE_ID_SONY_PS3_CONTROLLER),
		.driver_data = SIXAXIS_CONTROLLER_USB },
	{ HID_USB_DEVICE(USB_VENDOR_ID_SONY, USB_DEVICE_ID_SONY_NAVIGATION_CONTROLLER),
		.driver_data = SIXAXIS_CONTROLLER_USB },
	{ HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_SONY, USB_DEVICE_ID_SONY_PS3_CONTROLLER),
		.driver_data = SIXAXIS_CONTROLLER_BT },
	{ HID_USB_DEVICE(USB_VENDOR_ID_SONY, USB_DEVICE_ID_SONY_VAIO_VGX_MOUSE),
		.driver_data = VAIO_RDESC_CONSTANT },
	{ HID_USB_DEVICE(USB_VENDOR_ID_SONY, USB_DEVICE_ID_SONY_VAIO_VGP_MOUSE),
		.driver_data = VAIO_RDESC_CONSTANT },
	{ HID_USB_DEVICE(USB_VENDOR_ID_SONY, USB_DEVICE_ID_SONY_PS4_CONTROLLER),
		.driver_data = DUALSHOCK4_CONTROLLER_USB },
	{ HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_SONY, USB_DEVICE_ID_SONY_PS4_CONTROLLER),
		.driver_data = DUALSHOCK4_CONTROLLER_BT },
	{ HID_USB_DEVICE(USB_VENDOR_ID_SONY, USB_DEVICE_ID_SONY_PS4_CONTROLLER_2),
		.driver_data = DUALSHOCK4_CONTROLLER_USB },
	{ HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_SONY, USB_DEVICE_ID_SONY_PS4_CONTROLLER_2),
		.driver_data = DUALSHOCK4_CONTROLLER_BT },
	{ HID_USB_DEVICE(USB_VENDOR_ID_SONY, USB_DEVICE_ID_SONY_PS4_CONTROLLER_DONGLE),
		.driver_data = DUALSHOCK4_CONTROLLER_USB },
	{ }
};
MODULE_DEVICE_TABLE(hid, sony_devices);

static struct hid_driver sony_driver = {
	.name = "sony",
	.id_table = sony_devices,
	.input_configured = sony_input_configured,
	.probe = sony_probe,
	.remove = sony_remove,
	.report_fixup = sony_report_fixup,
	.raw_event = sony_raw_event
};
module_hid_driver(sony_driver);

MODULE_LICENSE("GPL");
