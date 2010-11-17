/*
 * Anysee remote controller keytable
 *
 * Copyright (C) 2010 Antti Palosaari <crope@iki.fi>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License along
 *    with this program; if not, write to the Free Software Foundation, Inc.,
 *    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <media/rc-map.h>

static struct ir_scancode anysee[] = {
	{ 0x0800, KEY_0 },
	{ 0x0801, KEY_1 },
	{ 0x0802, KEY_2 },
	{ 0x0803, KEY_3 },
	{ 0x0804, KEY_4 },
	{ 0x0805, KEY_5 },
	{ 0x0806, KEY_6 },
	{ 0x0807, KEY_7 },
	{ 0x0808, KEY_8 },
	{ 0x0809, KEY_9 },
	{ 0x080a, KEY_POWER2 },          /* [red power button] */
	{ 0x080b, KEY_VIDEO },           /* [*] MODE */
	{ 0x080c, KEY_CHANNEL },         /* [symbol counterclockwise arrow] */
	{ 0x080d, KEY_NEXT },            /* [>>|] */
	{ 0x080e, KEY_MENU },            /* MENU */
	{ 0x080f, KEY_EPG },             /* [EPG] */
	{ 0x0810, KEY_CLEAR },           /* EXIT */
	{ 0x0811, KEY_CHANNELUP },
	{ 0x0812, KEY_VOLUMEDOWN },
	{ 0x0813, KEY_VOLUMEUP },
	{ 0x0814, KEY_CHANNELDOWN },
	{ 0x0815, KEY_OK },
	{ 0x0816, KEY_RADIO },           /* [symbol TV/radio] */
	{ 0x0817, KEY_INFO },            /* [i] */
	{ 0x0818, KEY_PREVIOUS },        /* [|<<] */
	{ 0x0819, KEY_FAVORITES },       /* FAV. */
	{ 0x081a, KEY_SUBTITLE },        /* Subtitle */
	{ 0x081b, KEY_CAMERA },          /* [symbol camera] */
	{ 0x081c, KEY_YELLOW },
	{ 0x081d, KEY_RED },
	{ 0x081e, KEY_LANGUAGE },        /* [symbol Second Audio Program] */
	{ 0x081f, KEY_GREEN },
	{ 0x0820, KEY_SLEEP },           /* Sleep */
	{ 0x0821, KEY_SCREEN },          /* 16:9 / 4:3 */
	{ 0x0822, KEY_ZOOM },            /* SIZE */
	{ 0x0824, KEY_FN },              /* [F1] */
	{ 0x0825, KEY_FN },              /* [F2] */
	{ 0x0842, KEY_MUTE },            /* symbol mute */
	{ 0x0844, KEY_BLUE },
	{ 0x0847, KEY_TEXT },            /* TEXT */
	{ 0x0848, KEY_STOP },
	{ 0x0849, KEY_RECORD },
	{ 0x0850, KEY_PLAY },
	{ 0x0851, KEY_PAUSE },
};

static struct rc_keymap anysee_map = {
	.map = {
		.scan    = anysee,
		.size    = ARRAY_SIZE(anysee),
		.rc_type = RC_TYPE_NEC,
		.name    = RC_MAP_ANYSEE,
	}
};

static int __init init_rc_map_anysee(void)
{
	return ir_register_map(&anysee_map);
}

static void __exit exit_rc_map_anysee(void)
{
	ir_unregister_map(&anysee_map);
}

module_init(init_rc_map_anysee)
module_exit(exit_rc_map_anysee)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Antti Palosaari <crope@iki.fi>");
