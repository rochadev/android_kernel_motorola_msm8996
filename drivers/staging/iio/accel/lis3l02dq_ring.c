#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <linux/export.h>

#include "../iio.h"
#include "../ring_sw.h"
#include "../kfifo_buf.h"
#include "../trigger.h"
#include "../trigger_consumer.h"
#include "lis3l02dq.h"

/**
 * combine_8_to_16() utility function to munge to u8s into u16
 **/
static inline u16 combine_8_to_16(u8 lower, u8 upper)
{
	u16 _lower = lower;
	u16 _upper = upper;
	return _lower | (_upper << 8);
}

/**
 * lis3l02dq_data_rdy_trig_poll() the event handler for the data rdy trig
 **/
irqreturn_t lis3l02dq_data_rdy_trig_poll(int irq, void *private)
{
	struct iio_dev *indio_dev = private;
	struct lis3l02dq_state *st = iio_priv(indio_dev);

	if (st->trigger_on) {
		iio_trigger_poll(st->trig, iio_get_time_ns());
		return IRQ_HANDLED;
	} else
		return IRQ_WAKE_THREAD;
}

/**
 * lis3l02dq_read_accel_from_buffer() individual acceleration read from buffer
 **/
ssize_t lis3l02dq_read_accel_from_buffer(struct iio_buffer *buffer,
					 int index,
					 int *val)
{
	int ret;
	s16 *data;

	if (!iio_scan_mask_query(buffer, index))
		return -EINVAL;

	if (!buffer->access->read_last)
		return -EBUSY;

	data = kmalloc(buffer->access->get_bytes_per_datum(buffer),
		       GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;

	ret = buffer->access->read_last(buffer, (u8 *)data);
	if (ret)
		goto error_free_data;
	*val = data[bitmap_weight(buffer->scan_mask, index)];
error_free_data:

	kfree(data);

	return ret;
}

static const u8 read_all_tx_array[] = {
	LIS3L02DQ_READ_REG(LIS3L02DQ_REG_OUT_X_L_ADDR), 0,
	LIS3L02DQ_READ_REG(LIS3L02DQ_REG_OUT_X_H_ADDR), 0,
	LIS3L02DQ_READ_REG(LIS3L02DQ_REG_OUT_Y_L_ADDR), 0,
	LIS3L02DQ_READ_REG(LIS3L02DQ_REG_OUT_Y_H_ADDR), 0,
	LIS3L02DQ_READ_REG(LIS3L02DQ_REG_OUT_Z_L_ADDR), 0,
	LIS3L02DQ_READ_REG(LIS3L02DQ_REG_OUT_Z_H_ADDR), 0,
};

/**
 * lis3l02dq_read_all() Reads all channels currently selected
 * @st:		device specific state
 * @rx_array:	(dma capable) receive array, must be at least
 *		4*number of channels
 **/
static int lis3l02dq_read_all(struct iio_dev *indio_dev, u8 *rx_array)
{
	struct lis3l02dq_state *st = iio_priv(indio_dev);
	struct spi_transfer *xfers;
	struct spi_message msg;
	int ret, i, j = 0;

	xfers = kcalloc(bitmap_weight(indio_dev->active_scan_mask,
				      indio_dev->masklength) * 2,
			sizeof(*xfers), GFP_KERNEL);
	if (!xfers)
		return -ENOMEM;

	mutex_lock(&st->buf_lock);

	for (i = 0; i < ARRAY_SIZE(read_all_tx_array)/4; i++)
		if (test_bit(i, indio_dev->active_scan_mask)) {
			/* lower byte */
			xfers[j].tx_buf = st->tx + 2*j;
			st->tx[2*j] = read_all_tx_array[i*4];
			st->tx[2*j + 1] = 0;
			if (rx_array)
				xfers[j].rx_buf = rx_array + j*2;
			xfers[j].bits_per_word = 8;
			xfers[j].len = 2;
			xfers[j].cs_change = 1;
			j++;

			/* upper byte */
			xfers[j].tx_buf = st->tx + 2*j;
			st->tx[2*j] = read_all_tx_array[i*4 + 2];
			st->tx[2*j + 1] = 0;
			if (rx_array)
				xfers[j].rx_buf = rx_array + j*2;
			xfers[j].bits_per_word = 8;
			xfers[j].len = 2;
			xfers[j].cs_change = 1;
			j++;
		}

	/* After these are transmitted, the rx_buff should have
	 * values in alternate bytes
	 */
	spi_message_init(&msg);
	for (j = 0; j < bitmap_weight(indio_dev->active_scan_mask,
				      indio_dev->masklength) * 2; j++)
		spi_message_add_tail(&xfers[j], &msg);

	ret = spi_sync(st->us, &msg);
	mutex_unlock(&st->buf_lock);
	kfree(xfers);

	return ret;
}

static int lis3l02dq_get_buffer_element(struct iio_dev *indio_dev,
				u8 *buf)
{
	int ret, i;
	u8 *rx_array ;
	s16 *data = (s16 *)buf;
	int scan_count = bitmap_weight(indio_dev->active_scan_mask,
				       indio_dev->masklength);

	rx_array = kzalloc(4 * scan_count, GFP_KERNEL);
	if (rx_array == NULL)
		return -ENOMEM;
	ret = lis3l02dq_read_all(indio_dev, rx_array);
	if (ret < 0)
		return ret;
	for (i = 0; i < scan_count; i++)
		data[i] = combine_8_to_16(rx_array[i*4+1],
					rx_array[i*4+3]);
	kfree(rx_array);

	return i*sizeof(data[0]);
}

static irqreturn_t lis3l02dq_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct iio_buffer *buffer = indio_dev->buffer;
	int len = 0;
	size_t datasize = buffer->access->get_bytes_per_datum(buffer);
	char *data = kmalloc(datasize, GFP_KERNEL);

	if (data == NULL) {
		dev_err(indio_dev->dev.parent,
			"memory alloc failed in buffer bh");
		return -ENOMEM;
	}

	if (!bitmap_empty(indio_dev->active_scan_mask, indio_dev->masklength))
		len = lis3l02dq_get_buffer_element(indio_dev, data);

	  /* Guaranteed to be aligned with 8 byte boundary */
	if (buffer->scan_timestamp)
		*(s64 *)(((phys_addr_t)data + len
				+ sizeof(s64) - 1) & ~(sizeof(s64) - 1))
			= pf->timestamp;
	buffer->access->store_to(buffer, (u8 *)data, pf->timestamp);

	iio_trigger_notify_done(indio_dev->trig);
	kfree(data);
	return IRQ_HANDLED;
}

/* Caller responsible for locking as necessary. */
static int
__lis3l02dq_write_data_ready_config(struct device *dev, bool state)
{
	int ret;
	u8 valold;
	bool currentlyset;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct lis3l02dq_state *st = iio_priv(indio_dev);

/* Get the current event mask register */
	ret = lis3l02dq_spi_read_reg_8(indio_dev,
				       LIS3L02DQ_REG_CTRL_2_ADDR,
				       &valold);
	if (ret)
		goto error_ret;
/* Find out if data ready is already on */
	currentlyset
		= valold & LIS3L02DQ_REG_CTRL_2_ENABLE_DATA_READY_GENERATION;

/* Disable requested */
	if (!state && currentlyset) {
		/* disable the data ready signal */
		valold &= ~LIS3L02DQ_REG_CTRL_2_ENABLE_DATA_READY_GENERATION;

		/* The double write is to overcome a hardware bug?*/
		ret = lis3l02dq_spi_write_reg_8(indio_dev,
						LIS3L02DQ_REG_CTRL_2_ADDR,
						valold);
		if (ret)
			goto error_ret;
		ret = lis3l02dq_spi_write_reg_8(indio_dev,
						LIS3L02DQ_REG_CTRL_2_ADDR,
						valold);
		if (ret)
			goto error_ret;
		st->trigger_on = false;
/* Enable requested */
	} else if (state && !currentlyset) {
		/* if not set, enable requested */
		/* first disable all events */
		ret = lis3l02dq_disable_all_events(indio_dev);
		if (ret < 0)
			goto error_ret;

		valold = ret |
			LIS3L02DQ_REG_CTRL_2_ENABLE_DATA_READY_GENERATION;

		st->trigger_on = true;
		ret = lis3l02dq_spi_write_reg_8(indio_dev,
						LIS3L02DQ_REG_CTRL_2_ADDR,
						valold);
		if (ret)
			goto error_ret;
	}

	return 0;
error_ret:
	return ret;
}

/**
 * lis3l02dq_data_rdy_trigger_set_state() set datardy interrupt state
 *
 * If disabling the interrupt also does a final read to ensure it is clear.
 * This is only important in some cases where the scan enable elements are
 * switched before the buffer is reenabled.
 **/
static int lis3l02dq_data_rdy_trigger_set_state(struct iio_trigger *trig,
						bool state)
{
	struct iio_dev *indio_dev = trig->private_data;
	int ret = 0;
	u8 t;

	__lis3l02dq_write_data_ready_config(&indio_dev->dev, state);
	if (state == false) {
		/*
		 * A possible quirk with teh handler is currently worked around
		 *  by ensuring outstanding read events are cleared.
		 */
		ret = lis3l02dq_read_all(indio_dev, NULL);
	}
	lis3l02dq_spi_read_reg_8(indio_dev,
				 LIS3L02DQ_REG_WAKE_UP_SRC_ADDR,
				 &t);
	return ret;
}

/**
 * lis3l02dq_trig_try_reen() try renabling irq for data rdy trigger
 * @trig:	the datardy trigger
 */
static int lis3l02dq_trig_try_reen(struct iio_trigger *trig)
{
	struct iio_dev *indio_dev = trig->private_data;
	struct lis3l02dq_state *st = iio_priv(indio_dev);
	int i;

	/* If gpio still high (or high again) */
	/* In theory possible we will need to do this several times */
	for (i = 0; i < 5; i++)
		if (gpio_get_value(irq_to_gpio(st->us->irq)))
			lis3l02dq_read_all(indio_dev, NULL);
		else
			break;
	if (i == 5)
		printk(KERN_INFO
		       "Failed to clear the interrupt for lis3l02dq\n");

	/* irq reenabled so success! */
	return 0;
}

static const struct iio_trigger_ops lis3l02dq_trigger_ops = {
	.owner = THIS_MODULE,
	.set_trigger_state = &lis3l02dq_data_rdy_trigger_set_state,
	.try_reenable = &lis3l02dq_trig_try_reen,
};

int lis3l02dq_probe_trigger(struct iio_dev *indio_dev)
{
	int ret;
	struct lis3l02dq_state *st = iio_priv(indio_dev);

	st->trig = iio_allocate_trigger("lis3l02dq-dev%d", indio_dev->id);
	if (!st->trig) {
		ret = -ENOMEM;
		goto error_ret;
	}

	st->trig->dev.parent = &st->us->dev;
	st->trig->ops = &lis3l02dq_trigger_ops;
	st->trig->private_data = indio_dev;
	ret = iio_trigger_register(st->trig);
	if (ret)
		goto error_free_trig;

	return 0;

error_free_trig:
	iio_free_trigger(st->trig);
error_ret:
	return ret;
}

void lis3l02dq_remove_trigger(struct iio_dev *indio_dev)
{
	struct lis3l02dq_state *st = iio_priv(indio_dev);

	iio_trigger_unregister(st->trig);
	iio_free_trigger(st->trig);
}

void lis3l02dq_unconfigure_buffer(struct iio_dev *indio_dev)
{
	iio_dealloc_pollfunc(indio_dev->pollfunc);
	lis3l02dq_free_buf(indio_dev->buffer);
}

static int lis3l02dq_buffer_postenable(struct iio_dev *indio_dev)
{
	/* Disable unwanted channels otherwise the interrupt will not clear */
	u8 t;
	int ret;
	bool oneenabled = false;

	ret = lis3l02dq_spi_read_reg_8(indio_dev,
				       LIS3L02DQ_REG_CTRL_1_ADDR,
				       &t);
	if (ret)
		goto error_ret;

	if (test_bit(0, indio_dev->active_scan_mask)) {
		t |= LIS3L02DQ_REG_CTRL_1_AXES_X_ENABLE;
		oneenabled = true;
	} else
		t &= ~LIS3L02DQ_REG_CTRL_1_AXES_X_ENABLE;
	if (test_bit(1, indio_dev->active_scan_mask)) {
		t |= LIS3L02DQ_REG_CTRL_1_AXES_Y_ENABLE;
		oneenabled = true;
	} else
		t &= ~LIS3L02DQ_REG_CTRL_1_AXES_Y_ENABLE;
	if (test_bit(2, indio_dev->active_scan_mask)) {
		t |= LIS3L02DQ_REG_CTRL_1_AXES_Z_ENABLE;
		oneenabled = true;
	} else
		t &= ~LIS3L02DQ_REG_CTRL_1_AXES_Z_ENABLE;

	if (!oneenabled) /* what happens in this case is unknown */
		return -EINVAL;
	ret = lis3l02dq_spi_write_reg_8(indio_dev,
					LIS3L02DQ_REG_CTRL_1_ADDR,
					t);
	if (ret)
		goto error_ret;

	return iio_triggered_buffer_postenable(indio_dev);
error_ret:
	return ret;
}

/* Turn all channels on again */
static int lis3l02dq_buffer_predisable(struct iio_dev *indio_dev)
{
	u8 t;
	int ret;

	ret = iio_triggered_buffer_predisable(indio_dev);
	if (ret)
		goto error_ret;

	ret = lis3l02dq_spi_read_reg_8(indio_dev,
				       LIS3L02DQ_REG_CTRL_1_ADDR,
				       &t);
	if (ret)
		goto error_ret;
	t |= LIS3L02DQ_REG_CTRL_1_AXES_X_ENABLE |
		LIS3L02DQ_REG_CTRL_1_AXES_Y_ENABLE |
		LIS3L02DQ_REG_CTRL_1_AXES_Z_ENABLE;

	ret = lis3l02dq_spi_write_reg_8(indio_dev,
					LIS3L02DQ_REG_CTRL_1_ADDR,
					t);

error_ret:
	return ret;
}

static const struct iio_buffer_setup_ops lis3l02dq_buffer_setup_ops = {
	.preenable = &iio_sw_buffer_preenable,
	.postenable = &lis3l02dq_buffer_postenable,
	.predisable = &lis3l02dq_buffer_predisable,
};

int lis3l02dq_configure_buffer(struct iio_dev *indio_dev)
{
	int ret;
	struct iio_buffer *buffer;

	buffer = lis3l02dq_alloc_buf(indio_dev);
	if (!buffer)
		return -ENOMEM;

	indio_dev->buffer = buffer;
	/* Effectively select the buffer implementation */
	indio_dev->buffer->access = &lis3l02dq_access_funcs;

	buffer->scan_timestamp = true;
	indio_dev->setup_ops = &lis3l02dq_buffer_setup_ops;

	/* Functions are NULL as we set handler below */
	indio_dev->pollfunc = iio_alloc_pollfunc(&iio_pollfunc_store_time,
						 &lis3l02dq_trigger_handler,
						 0,
						 indio_dev,
						 "lis3l02dq_consumer%d",
						 indio_dev->id);

	if (indio_dev->pollfunc == NULL) {
		ret = -ENOMEM;
		goto error_iio_sw_rb_free;
	}

	indio_dev->modes |= INDIO_BUFFER_TRIGGERED;
	return 0;

error_iio_sw_rb_free:
	lis3l02dq_free_buf(indio_dev->buffer);
	return ret;
}
