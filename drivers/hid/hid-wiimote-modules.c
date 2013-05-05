/*
 * Device Modules for Nintendo Wii / Wii U HID Driver
 * Copyright (c) 2011-2013 David Herrmann <dh.herrmann@gmail.com>
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

/*
 * Wiimote Modules
 * Nintendo devices provide different peripherals and many new devices lack
 * initial features like the IR camera. Therefore, each peripheral device is
 * implemented as an independent module and we probe on each device only the
 * modules for the hardware that really is available.
 *
 * Module registration is sequential. Unregistration is done in reverse order.
 * After device detection, the needed modules are loaded. Users can trigger
 * re-detection which causes all modules to be unloaded and then reload the
 * modules for the new detected device.
 *
 * wdata->input is a shared input device. It is always initialized prior to
 * module registration. If at least one registered module is marked as
 * WIIMOD_FLAG_INPUT, then the input device will get registered after all
 * modules were registered.
 * Please note that it is unregistered _before_ the "remove" callbacks are
 * called. This guarantees that no input interaction is done, anymore. However,
 * the wiimote core keeps a reference to the input device so it is freed only
 * after all modules were removed. It is safe to send events to unregistered
 * input devices.
 */

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/input.h>
#include <linux/spinlock.h>
#include "hid-wiimote.h"

/*
 * Keys
 * The initial Wii Remote provided a bunch of buttons that are reported as
 * part of the core protocol. Many later devices dropped these and report
 * invalid data in the core button reports. Load this only on devices which
 * correctly send button reports.
 * It uses the shared input device.
 */

static const __u16 wiimod_keys_map[] = {
	KEY_LEFT,	/* WIIPROTO_KEY_LEFT */
	KEY_RIGHT,	/* WIIPROTO_KEY_RIGHT */
	KEY_UP,		/* WIIPROTO_KEY_UP */
	KEY_DOWN,	/* WIIPROTO_KEY_DOWN */
	KEY_NEXT,	/* WIIPROTO_KEY_PLUS */
	KEY_PREVIOUS,	/* WIIPROTO_KEY_MINUS */
	BTN_1,		/* WIIPROTO_KEY_ONE */
	BTN_2,		/* WIIPROTO_KEY_TWO */
	BTN_A,		/* WIIPROTO_KEY_A */
	BTN_B,		/* WIIPROTO_KEY_B */
	BTN_MODE,	/* WIIPROTO_KEY_HOME */
};

static void wiimod_keys_in_keys(struct wiimote_data *wdata, const __u8 *keys)
{
	input_report_key(wdata->input, wiimod_keys_map[WIIPROTO_KEY_LEFT],
							!!(keys[0] & 0x01));
	input_report_key(wdata->input, wiimod_keys_map[WIIPROTO_KEY_RIGHT],
							!!(keys[0] & 0x02));
	input_report_key(wdata->input, wiimod_keys_map[WIIPROTO_KEY_DOWN],
							!!(keys[0] & 0x04));
	input_report_key(wdata->input, wiimod_keys_map[WIIPROTO_KEY_UP],
							!!(keys[0] & 0x08));
	input_report_key(wdata->input, wiimod_keys_map[WIIPROTO_KEY_PLUS],
							!!(keys[0] & 0x10));
	input_report_key(wdata->input, wiimod_keys_map[WIIPROTO_KEY_TWO],
							!!(keys[1] & 0x01));
	input_report_key(wdata->input, wiimod_keys_map[WIIPROTO_KEY_ONE],
							!!(keys[1] & 0x02));
	input_report_key(wdata->input, wiimod_keys_map[WIIPROTO_KEY_B],
							!!(keys[1] & 0x04));
	input_report_key(wdata->input, wiimod_keys_map[WIIPROTO_KEY_A],
							!!(keys[1] & 0x08));
	input_report_key(wdata->input, wiimod_keys_map[WIIPROTO_KEY_MINUS],
							!!(keys[1] & 0x10));
	input_report_key(wdata->input, wiimod_keys_map[WIIPROTO_KEY_HOME],
							!!(keys[1] & 0x80));
	input_sync(wdata->input);
}

static int wiimod_keys_probe(const struct wiimod_ops *ops,
			     struct wiimote_data *wdata)
{
	unsigned int i;

	set_bit(EV_KEY, wdata->input->evbit);
	for (i = 0; i < WIIPROTO_KEY_COUNT; ++i)
		set_bit(wiimod_keys_map[i], wdata->input->keybit);

	return 0;
}

static const struct wiimod_ops wiimod_keys = {
	.flags = WIIMOD_FLAG_INPUT,
	.arg = 0,
	.probe = wiimod_keys_probe,
	.remove = NULL,
	.in_keys = wiimod_keys_in_keys,
};

/*
 * Rumble
 * Nearly all devices provide a rumble feature. A small motor for
 * force-feedback effects. We provide an FF_RUMBLE memless ff device on the
 * shared input device if this module is loaded.
 * The rumble motor is controlled via a flag on almost every output report so
 * the wiimote core handles the rumble flag. But if a device doesn't provide
 * the rumble motor, this flag shouldn't be set.
 */

static int wiimod_rumble_play(struct input_dev *dev, void *data,
			      struct ff_effect *eff)
{
	struct wiimote_data *wdata = input_get_drvdata(dev);
	__u8 value;
	unsigned long flags;

	/*
	 * The wiimote supports only a single rumble motor so if any magnitude
	 * is set to non-zero then we start the rumble motor. If both are set to
	 * zero, we stop the rumble motor.
	 */

	if (eff->u.rumble.strong_magnitude || eff->u.rumble.weak_magnitude)
		value = 1;
	else
		value = 0;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wiiproto_req_rumble(wdata, value);
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	return 0;
}

static int wiimod_rumble_probe(const struct wiimod_ops *ops,
			       struct wiimote_data *wdata)
{
	set_bit(FF_RUMBLE, wdata->input->ffbit);
	if (input_ff_create_memless(wdata->input, NULL, wiimod_rumble_play))
		return -ENOMEM;

	return 0;
}

static void wiimod_rumble_remove(const struct wiimod_ops *ops,
				 struct wiimote_data *wdata)
{
	unsigned long flags;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wiiproto_req_rumble(wdata, 0);
	spin_unlock_irqrestore(&wdata->state.lock, flags);
}

static const struct wiimod_ops wiimod_rumble = {
	.flags = WIIMOD_FLAG_INPUT,
	.arg = 0,
	.probe = wiimod_rumble_probe,
	.remove = wiimod_rumble_remove,
};

/*
 * Battery
 * 1 byte of battery capacity information is sent along every protocol status
 * report. The wiimote core caches it but we try to update it on every
 * user-space request.
 * This is supported by nearly every device so it's almost always enabled.
 */

static enum power_supply_property wiimod_battery_props[] = {
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_SCOPE,
};

static int wiimod_battery_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct wiimote_data *wdata = container_of(psy, struct wiimote_data,
						  battery);
	int ret = 0, state;
	unsigned long flags;

	if (psp == POWER_SUPPLY_PROP_SCOPE) {
		val->intval = POWER_SUPPLY_SCOPE_DEVICE;
		return 0;
	} else if (psp != POWER_SUPPLY_PROP_CAPACITY) {
		return -EINVAL;
	}

	ret = wiimote_cmd_acquire(wdata);
	if (ret)
		return ret;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wiimote_cmd_set(wdata, WIIPROTO_REQ_SREQ, 0);
	wiiproto_req_status(wdata);
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	wiimote_cmd_wait(wdata);
	wiimote_cmd_release(wdata);

	spin_lock_irqsave(&wdata->state.lock, flags);
	state = wdata->state.cmd_battery;
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	val->intval = state * 100 / 255;
	return ret;
}

static int wiimod_battery_probe(const struct wiimod_ops *ops,
				struct wiimote_data *wdata)
{
	int ret;

	wdata->battery.properties = wiimod_battery_props;
	wdata->battery.num_properties = ARRAY_SIZE(wiimod_battery_props);
	wdata->battery.get_property = wiimod_battery_get_property;
	wdata->battery.type = POWER_SUPPLY_TYPE_BATTERY;
	wdata->battery.use_for_apm = 0;
	wdata->battery.name = kasprintf(GFP_KERNEL, "wiimote_battery_%s",
					wdata->hdev->uniq);
	if (!wdata->battery.name)
		return -ENOMEM;

	ret = power_supply_register(&wdata->hdev->dev, &wdata->battery);
	if (ret) {
		hid_err(wdata->hdev, "cannot register battery device\n");
		goto err_free;
	}

	power_supply_powers(&wdata->battery, &wdata->hdev->dev);
	return 0;

err_free:
	kfree(wdata->battery.name);
	wdata->battery.name = NULL;
	return ret;
}

static void wiimod_battery_remove(const struct wiimod_ops *ops,
				  struct wiimote_data *wdata)
{
	if (!wdata->battery.name)
		return;

	power_supply_unregister(&wdata->battery);
	kfree(wdata->battery.name);
	wdata->battery.name = NULL;
}

static const struct wiimod_ops wiimod_battery = {
	.flags = 0,
	.arg = 0,
	.probe = wiimod_battery_probe,
	.remove = wiimod_battery_remove,
};

/* module table */

const struct wiimod_ops *wiimod_table[WIIMOD_NUM] = {
	[WIIMOD_KEYS] = &wiimod_keys,
	[WIIMOD_RUMBLE] = &wiimod_rumble,
	[WIIMOD_BATTERY] = &wiimod_battery,
};
