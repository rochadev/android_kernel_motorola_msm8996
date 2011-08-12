/* The industrial I/O core
 *
 *Copyright (c) 2008 Jonathan Cameron
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * General attributes
 */

#ifndef _INDUSTRIAL_IO_SYSFS_H_
#define _INDUSTRIAL_IO_SYSFS_H_

struct iio_chan_spec;

/**
 * struct iio_dev_attr - iio specific device attribute
 * @dev_attr:	underlying device attribute
 * @address:	associated register address
 * @l:		list head for maintaining list of dynamically created attrs.
 */
struct iio_dev_attr {
	struct device_attribute dev_attr;
	int address;
	struct list_head l;
	struct iio_chan_spec const *c;
};

#define to_iio_dev_attr(_dev_attr)				\
	container_of(_dev_attr, struct iio_dev_attr, dev_attr)

ssize_t iio_read_const_attr(struct device *dev,
			    struct device_attribute *attr,
			    char *len);

/**
 * struct iio_const_attr - constant device specific attribute
 *                         often used for things like available modes
 * @string:	attribute string
 * @dev_attr:	underlying device attribute
 */
struct iio_const_attr {
	const char *string;
	struct device_attribute dev_attr;
};

#define to_iio_const_attr(_dev_attr) \
	container_of(_dev_attr, struct iio_const_attr, dev_attr)

/* Some attributes will be hard coded (device dependent) and not require an
   address, in these cases pass a negative */
#define IIO_ATTR(_name, _mode, _show, _store, _addr)		\
	{ .dev_attr = __ATTR(_name, _mode, _show, _store),	\
	  .address = _addr }

#define IIO_DEVICE_ATTR(_name, _mode, _show, _store, _addr)	\
	struct iio_dev_attr iio_dev_attr_##_name		\
	= IIO_ATTR(_name, _mode, _show, _store, _addr)

#define IIO_DEVICE_ATTR_NAMED(_vname, _name, _mode, _show, _store, _addr) \
	struct iio_dev_attr iio_dev_attr_##_vname			\
	= IIO_ATTR(_name, _mode, _show, _store, _addr)

#define IIO_CONST_ATTR(_name, _string)					\
	struct iio_const_attr iio_const_attr_##_name			\
	= { .string = _string,						\
	    .dev_attr = __ATTR(_name, S_IRUGO, iio_read_const_attr, NULL)}

#define IIO_CONST_ATTR_NAMED(_vname, _name, _string)			\
	struct iio_const_attr iio_const_attr_##_vname			\
	= { .string = _string,						\
	    .dev_attr = __ATTR(_name, S_IRUGO, iio_read_const_attr, NULL)}

/* Generic attributes of onetype or another */
/**
 * IIO_DEV_ATTR_RESET: resets the device
 **/
#define IIO_DEV_ATTR_RESET(_store)			\
	IIO_DEVICE_ATTR(reset, S_IWUSR, NULL, _store, 0)

/**
 * IIO_DEV_ATTR_SAMP_FREQ - sets any internal clock frequency
 * @_mode: sysfs file mode/permissions
 * @_show: output method for the attribute
 * @_store: input method for the attribute
 **/
#define IIO_DEV_ATTR_SAMP_FREQ(_mode, _show, _store)			\
	IIO_DEVICE_ATTR(sampling_frequency, _mode, _show, _store, 0)

/**
 * IIO_DEV_ATTR_SAMP_FREQ_AVAIL - list available sampling frequencies
 * @_show: output method for the attribute
 *
 * May be mode dependent on some devices
 **/
#define IIO_DEV_ATTR_SAMP_FREQ_AVAIL(_show)				\
	IIO_DEVICE_ATTR(sampling_frequency_available, S_IRUGO, _show, NULL, 0)
/**
 * IIO_CONST_ATTR_AVAIL_SAMP_FREQ - list available sampling frequencies
 * @_string: frequency string for the attribute
 *
 * Constant version
 **/
#define IIO_CONST_ATTR_SAMP_FREQ_AVAIL(_string)			\
	IIO_CONST_ATTR(sampling_frequency_available, _string)

#define IIO_DEV_ATTR_TEMP_RAW(_show)			\
	IIO_DEVICE_ATTR(temp_raw, S_IRUGO, _show, NULL, 0)

#define IIO_CONST_ATTR_TEMP_OFFSET(_string)		\
	IIO_CONST_ATTR(temp_offset, _string)

#define IIO_CONST_ATTR_TEMP_SCALE(_string)		\
	IIO_CONST_ATTR(temp_scale, _string)

#define IIO_EV_TYPE_THRESH		0
#define IIO_EV_TYPE_MAG			1
#define IIO_EV_TYPE_ROC			2

#define IIO_EV_DIR_EITHER		0
#define IIO_EV_DIR_RISING		1
#define IIO_EV_DIR_FALLING		2

#define IIO_EV_TYPE_MAX 8
#define IIO_EV_BIT(type, direction)			\
	(1 << (type*IIO_EV_TYPE_MAX + direction))

#define IIO_EVENT_CODE(channelclass, orient_bit, number,		\
		       modifier, type, direction)			\
	(channelclass | (orient_bit << 8) | ((number) << 9) |		\
	 ((modifier) << 13) | ((type) << 16) | ((direction) << 24))

#define IIO_MOD_EVENT_CODE(channelclass, number, modifier,		\
			   type, direction)				\
	IIO_EVENT_CODE(channelclass, 1, number, modifier, type, direction)

#define IIO_UNMOD_EVENT_CODE(channelclass, number, type, direction)	\
	IIO_EVENT_CODE(channelclass, 0, number, 0, type, direction)

#define IIO_EVENT_CODE_EXTRACT_DIR(mask) ((mask >> 24) & 0xf)

/* Event code number extraction depends on which type of event we have.
 * Perhaps review this function in the future*/
#define IIO_EVENT_CODE_EXTRACT_NUM(mask) ((mask >> 9) & 0x0f)

#define IIO_EVENT_CODE_EXTRACT_MODIFIER(mask) ((mask >> 13) & 0x7)

#endif /* _INDUSTRIAL_IO_SYSFS_H_ */
