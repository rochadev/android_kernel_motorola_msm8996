/*

   comedi/drivers/me_daq.c

   Hardware driver for Meilhaus data acquisition cards:

     ME-2000i, ME-2600i, ME-3000vm1

   Copyright (C) 2002 Michael Hillmann <hillmann@syscongroup.de>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
Driver: me_daq
Description: Meilhaus PCI data acquisition cards
Author: Michael Hillmann <hillmann@syscongroup.de>
Devices: [Meilhaus] ME-2600i (me_daq), ME-2000i
Status: experimental

Supports:

    Analog Output

Configuration options:

    [0] - PCI bus number (optional)
    [1] - PCI slot number (optional)

    If bus/slot is not specified, the first available PCI
    device will be used.
*/

#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/firmware.h>
#include "../comedidev.h"

#define ME2600_FIRMWARE		"me2600_firmware.bin"

#define ME2000_DEVICE_ID	0x2000
#define ME2600_DEVICE_ID	0x2600

#define PLX_INTCSR		0x4C	/* PLX interrupt status register */
#define XILINX_DOWNLOAD_RESET	0x42	/* Xilinx registers */

#define ME_CONTROL_1			0x0000	/* - | W */
#define   INTERRUPT_ENABLE		(1<<15)
#define   COUNTER_B_IRQ			(1<<12)
#define   COUNTER_A_IRQ			(1<<11)
#define   CHANLIST_READY_IRQ		(1<<10)
#define   EXT_IRQ			(1<<9)
#define   ADFIFO_HALFFULL_IRQ		(1<<8)
#define   SCAN_COUNT_ENABLE		(1<<5)
#define   SIMULTANEOUS_ENABLE		(1<<4)
#define   TRIGGER_FALLING_EDGE		(1<<3)
#define   CONTINUOUS_MODE		(1<<2)
#define   DISABLE_ADC			(0<<0)
#define   SOFTWARE_TRIGGERED_ADC	(1<<0)
#define   SCAN_TRIGGERED_ADC		(2<<0)
#define   EXT_TRIGGERED_ADC		(3<<0)
#define ME_ADC_START			0x0000	/* R | - */
#define ME_CONTROL_2			0x0002	/* - | W */
#define   ENABLE_ADFIFO			(1<<10)
#define   ENABLE_CHANLIST		(1<<9)
#define   ENABLE_PORT_B			(1<<7)
#define   ENABLE_PORT_A			(1<<6)
#define   ENABLE_COUNTER_B		(1<<4)
#define   ENABLE_COUNTER_A		(1<<3)
#define   ENABLE_DAC			(1<<1)
#define   BUFFERED_DAC			(1<<0)
#define ME_DAC_UPDATE			0x0002	/* R | - */
#define ME_STATUS			0x0004	/* R | - */
#define   COUNTER_B_IRQ_PENDING		(1<<12)
#define   COUNTER_A_IRQ_PENDING		(1<<11)
#define   CHANLIST_READY_IRQ_PENDING	(1<<10)
#define   EXT_IRQ_PENDING		(1<<9)
#define   ADFIFO_HALFFULL_IRQ_PENDING	(1<<8)
#define   ADFIFO_FULL			(1<<4)
#define   ADFIFO_HALFFULL		(1<<3)
#define   ADFIFO_EMPTY			(1<<2)
#define   CHANLIST_FULL			(1<<1)
#define   FST_ACTIVE			(1<<0)
#define ME_RESET_INTERRUPT		0x0004	/* - | W */
#define ME_DIO_PORT_A			0x0006	/* R | W */
#define ME_DIO_PORT_B			0x0008	/* R | W */
#define ME_TIMER_DATA_0			0x000A	/* - | W */
#define ME_TIMER_DATA_1			0x000C	/* - | W */
#define ME_TIMER_DATA_2			0x000E	/* - | W */
#define ME_CHANNEL_LIST			0x0010	/* - | W */
#define   ADC_UNIPOLAR			(1<<6)
#define   ADC_GAIN_0			(0<<4)
#define   ADC_GAIN_1			(1<<4)
#define   ADC_GAIN_2			(2<<4)
#define   ADC_GAIN_3			(3<<4)
#define ME_READ_AD_FIFO			0x0010	/* R | - */
#define ME_DAC_CONTROL			0x0012	/* - | W */
#define   DAC_UNIPOLAR_D		(0<<4)
#define   DAC_BIPOLAR_D			(1<<4)
#define   DAC_UNIPOLAR_C		(0<<5)
#define   DAC_BIPOLAR_C			(1<<5)
#define   DAC_UNIPOLAR_B		(0<<6)
#define   DAC_BIPOLAR_B			(1<<6)
#define   DAC_UNIPOLAR_A		(0<<7)
#define   DAC_BIPOLAR_A			(1<<7)
#define   DAC_GAIN_0_D			(0<<8)
#define   DAC_GAIN_1_D			(1<<8)
#define   DAC_GAIN_0_C			(0<<9)
#define   DAC_GAIN_1_C			(1<<9)
#define   DAC_GAIN_0_B			(0<<10)
#define   DAC_GAIN_1_B			(1<<10)
#define   DAC_GAIN_0_A			(0<<11)
#define   DAC_GAIN_1_A			(1<<11)
#define ME_DAC_CONTROL_UPDATE		0x0012	/* R | - */
#define ME_DAC_DATA_A			0x0014	/* - | W */
#define ME_DAC_DATA_B			0x0016	/* - | W */
#define ME_DAC_DATA_C			0x0018	/* - | W */
#define ME_DAC_DATA_D			0x001A	/* - | W */
#define ME_COUNTER_ENDDATA_A		0x001C	/* - | W */
#define ME_COUNTER_ENDDATA_B		0x001E	/* - | W */
#define ME_COUNTER_STARTDATA_A		0x0020	/* - | W */
#define ME_COUNTER_VALUE_A		0x0020	/* R | - */
#define ME_COUNTER_STARTDATA_B		0x0022	/* - | W */
#define ME_COUNTER_VALUE_B		0x0022	/* R | - */

static const struct comedi_lrange me2000_ai_range = {
	8, {
		BIP_RANGE(10),
		BIP_RANGE(5),
		BIP_RANGE(2.5),
		BIP_RANGE(1.25),
		UNI_RANGE(10),
		UNI_RANGE(5),
		UNI_RANGE(2.5),
		UNI_RANGE(1.25)
	}
};

static const struct comedi_lrange me2600_ai_range = {
	8, {
		BIP_RANGE(10),
		BIP_RANGE(5),
		BIP_RANGE(2.5),
		BIP_RANGE(1.25),
		UNI_RANGE(10),
		UNI_RANGE(5),
		UNI_RANGE(2.5),
		UNI_RANGE(1.25)
	}
};

static const struct comedi_lrange me2600_ao_range = {
	3, {
		BIP_RANGE(10),
		BIP_RANGE(5),
		UNI_RANGE(10)
	}
};

struct me_board {
	const char *name;
	int device_id;
	int ao_chans;
	int ao_bits;
	const struct comedi_lrange *ao_range;
	int ai_chans;
	int ai_bits;
	const struct comedi_lrange *ai_range;
};

static const struct me_board me_boards[] = {
	{
		.name		= "me-2600i",
		.device_id	= ME2600_DEVICE_ID,
		.ao_chans	= 4,
		.ao_bits	= 12,
		.ao_range	= &me2600_ao_range,
		.ai_chans	= 16,
		.ai_bits	= 12,
		.ai_range	= &me2600_ai_range,
	}, {
		.name		= "me-2000i",
		.device_id	= ME2000_DEVICE_ID,
		.ai_chans	= 16,
		.ai_bits	= 12,
		.ai_range	= &me2000_ai_range,
	}
};

/* Private data structure */
struct me_private_data {
	void __iomem *plx_regbase;	/* PLX configuration base address */
	void __iomem *me_regbase;	/* Base address of the Meilhaus card */

	unsigned short control_1;	/* Mirror of CONTROL_1 register */
	unsigned short control_2;	/* Mirror of CONTROL_2 register */
	unsigned short dac_control;	/* Mirror of the DAC_CONTROL register */
	int ao_readback[4];	/* Mirror of analog output data */
};

/*
 * ------------------------------------------------------------------
 *
 * Helpful functions
 *
 * ------------------------------------------------------------------
 */
static inline void sleep(unsigned sec)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule_timeout(sec * HZ);
}

/*
 * ------------------------------------------------------------------
 *
 * DIGITAL INPUT/OUTPUT SECTION
 *
 * ------------------------------------------------------------------
 */
static int me_dio_insn_config(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      struct comedi_insn *insn, unsigned int *data)
{
	struct me_private_data *dev_private = dev->private;
	int bits;
	int mask = 1 << CR_CHAN(insn->chanspec);

	/* calculate port */
	if (mask & 0x0000ffff) {	/* Port A in use */
		bits = 0x0000ffff;

		/* Enable Port A */
		dev_private->control_2 |= ENABLE_PORT_A;
		writew(dev_private->control_2,
		       dev_private->me_regbase + ME_CONTROL_2);
	} else {		/* Port B in use */

		bits = 0xffff0000;

		/* Enable Port B */
		dev_private->control_2 |= ENABLE_PORT_B;
		writew(dev_private->control_2,
		       dev_private->me_regbase + ME_CONTROL_2);
	}

	if (data[0]) {
		/* Config port as output */
		s->io_bits |= bits;
	} else {
		/* Config port as input */
		s->io_bits &= ~bits;
	}

	return 1;
}

/* Digital instant input/outputs */
static int me_dio_insn_bits(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    struct comedi_insn *insn, unsigned int *data)
{
	struct me_private_data *dev_private = dev->private;
	unsigned int mask = data[0];

	s->state &= ~mask;
	s->state |= (mask & data[1]);

	mask &= s->io_bits;
	if (mask & 0x0000ffff) {	/* Port A */
		writew((s->state & 0xffff),
		       dev_private->me_regbase + ME_DIO_PORT_A);
	} else {
		data[1] &= ~0x0000ffff;
		data[1] |= readw(dev_private->me_regbase + ME_DIO_PORT_A);
	}

	if (mask & 0xffff0000) {	/* Port B */
		writew(((s->state >> 16) & 0xffff),
		       dev_private->me_regbase + ME_DIO_PORT_B);
	} else {
		data[1] &= ~0xffff0000;
		data[1] |= readw(dev_private->me_regbase + ME_DIO_PORT_B) << 16;
	}

	return insn->n;
}

/*
 * ------------------------------------------------------------------
 *
 * ANALOG INPUT SECTION
 *
 * ------------------------------------------------------------------
 */

/* Analog instant input */
static int me_ai_insn_read(struct comedi_device *dev,
			   struct comedi_subdevice *s,
			   struct comedi_insn *insn, unsigned int *data)
{
	struct me_private_data *dev_private = dev->private;
	unsigned short value;
	int chan = CR_CHAN((&insn->chanspec)[0]);
	int rang = CR_RANGE((&insn->chanspec)[0]);
	int aref = CR_AREF((&insn->chanspec)[0]);
	int i;

	/* stop any running conversion */
	dev_private->control_1 &= 0xFFFC;
	writew(dev_private->control_1, dev_private->me_regbase + ME_CONTROL_1);

	/* clear chanlist and ad fifo */
	dev_private->control_2 &= ~(ENABLE_ADFIFO | ENABLE_CHANLIST);
	writew(dev_private->control_2, dev_private->me_regbase + ME_CONTROL_2);

	/* reset any pending interrupt */
	writew(0x00, dev_private->me_regbase + ME_RESET_INTERRUPT);

	/* enable the chanlist and ADC fifo */
	dev_private->control_2 |= (ENABLE_ADFIFO | ENABLE_CHANLIST);
	writew(dev_private->control_2, dev_private->me_regbase + ME_CONTROL_2);

	/* write to channel list fifo */
	/* b3:b0 are the channel number */
	value = chan & 0x0f;
	/* b5:b4 are the channel gain */
	value |= (rang & 0x03) << 4;
	/* b6 channel polarity */
	value |= (rang & 0x04) << 4;
	/* b7 single or differential */
	value |= ((aref & AREF_DIFF) ? 0x80 : 0);
	writew(value & 0xff, dev_private->me_regbase + ME_CHANNEL_LIST);

	/* set ADC mode to software trigger */
	dev_private->control_1 |= SOFTWARE_TRIGGERED_ADC;
	writew(dev_private->control_1, dev_private->me_regbase + ME_CONTROL_1);

	/* start conversion by reading from ADC_START */
	readw(dev_private->me_regbase + ME_ADC_START);

	/* wait for ADC fifo not empty flag */
	for (i = 100000; i > 0; i--)
		if (!(readw(dev_private->me_regbase + ME_STATUS) & 0x0004))
			break;

	/* get value from ADC fifo */
	if (i) {
		data[0] =
		    (readw(dev_private->me_regbase +
			   ME_READ_AD_FIFO) ^ 0x800) & 0x0FFF;
	} else {
		dev_err(dev->class_dev, "Cannot get single value\n");
		return -EIO;
	}

	/* stop any running conversion */
	dev_private->control_1 &= 0xFFFC;
	writew(dev_private->control_1, dev_private->me_regbase + ME_CONTROL_1);

	return 1;
}

static int me_ao_insn_write(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    struct comedi_insn *insn,
			    unsigned int *data)
{
	struct me_private_data *dev_private = dev->private;
	unsigned int chan = CR_CHAN(insn->chanspec);
	unsigned int rang = CR_RANGE(insn->chanspec);
	int i;

	/* Enable all DAC */
	dev_private->control_2 |= ENABLE_DAC;
	writew(dev_private->control_2, dev_private->me_regbase + ME_CONTROL_2);

	/* and set DAC to "buffered" mode */
	dev_private->control_2 |= BUFFERED_DAC;
	writew(dev_private->control_2, dev_private->me_regbase + ME_CONTROL_2);

	/* Set dac-control register */
	for (i = 0; i < insn->n; i++) {
		/* clear bits for this channel */
		dev_private->dac_control &= ~(0x0880 >> chan);
		if (rang == 0)
			dev_private->dac_control |=
			    ((DAC_BIPOLAR_A | DAC_GAIN_1_A) >> chan);
		else if (rang == 1)
			dev_private->dac_control |=
			    ((DAC_BIPOLAR_A | DAC_GAIN_0_A) >> chan);
	}
	writew(dev_private->dac_control,
	       dev_private->me_regbase + ME_DAC_CONTROL);

	/* Update dac-control register */
	readw(dev_private->me_regbase + ME_DAC_CONTROL_UPDATE);

	/* Set data register */
	for (i = 0; i < insn->n; i++) {
		writew((data[0] & s->maxdata),
		       dev_private->me_regbase + ME_DAC_DATA_A + (chan << 1));
		dev_private->ao_readback[chan] = (data[0] & s->maxdata);
	}

	/* Update dac with data registers */
	readw(dev_private->me_regbase + ME_DAC_UPDATE);

	return insn->n;
}

static int me_ao_insn_read(struct comedi_device *dev,
			   struct comedi_subdevice *s,
			   struct comedi_insn *insn,
			   unsigned int *data)
{
	struct me_private_data *dev_private = dev->private;
	unsigned int chan = CR_CHAN(insn->chanspec);
	int i;

	for (i = 0; i < insn->n; i++)
		data[i] = dev_private->ao_readback[chan];

	return insn->n;
}

/*
 * ------------------------------------------------------------------
 *
 * INITIALISATION SECTION
 *
 * ------------------------------------------------------------------
 */

/* Xilinx firmware download for card: ME-2600i */
static int me2600_xilinx_download(struct comedi_device *dev,
				  const u8 *data, size_t size)
{
	struct me_private_data *dev_private = dev->private;
	unsigned int value;
	unsigned int file_length;
	unsigned int i;

	/* disable irq's on PLX */
	writel(0x00, dev_private->plx_regbase + PLX_INTCSR);

	/* First, make a dummy read to reset xilinx */
	value = readw(dev_private->me_regbase + XILINX_DOWNLOAD_RESET);

	/* Wait until reset is over */
	sleep(1);

	/* Write a dummy value to Xilinx */
	writeb(0x00, dev_private->me_regbase + 0x0);
	sleep(1);

	/*
	 * Format of the firmware
	 * Build longs from the byte-wise coded header
	 * Byte 1-3:   length of the array
	 * Byte 4-7:   version
	 * Byte 8-11:  date
	 * Byte 12-15: reserved
	 */
	if (size < 16)
		return -EINVAL;

	file_length = (((unsigned int)data[0] & 0xff) << 24) +
	    (((unsigned int)data[1] & 0xff) << 16) +
	    (((unsigned int)data[2] & 0xff) << 8) +
	    ((unsigned int)data[3] & 0xff);

	/*
	 * Loop for writing firmware byte by byte to xilinx
	 * Firmware data start at offfset 16
	 */
	for (i = 0; i < file_length; i++)
		writeb((data[16 + i] & 0xff),
		       dev_private->me_regbase + 0x0);

	/* Write 5 dummy values to xilinx */
	for (i = 0; i < 5; i++)
		writeb(0x00, dev_private->me_regbase + 0x0);

	/* Test if there was an error during download -> INTB was thrown */
	value = readl(dev_private->plx_regbase + PLX_INTCSR);
	if (value & 0x20) {
		/* Disable interrupt */
		writel(0x00, dev_private->plx_regbase + PLX_INTCSR);
		dev_err(dev->class_dev, "Xilinx download failed\n");
		return -EIO;
	}

	/* Wait until the Xilinx is ready for real work */
	sleep(1);

	/* Enable PLX-Interrupts */
	writel(0x43, dev_private->plx_regbase + PLX_INTCSR);

	return 0;
}

static int me2600_upload_firmware(struct comedi_device *dev)
{
	struct pci_dev *pcidev = comedi_to_pci_dev(dev);
	const struct firmware *fw;
	int ret;

	ret = request_firmware(&fw, ME2600_FIRMWARE, &pcidev->dev);
	if (ret)
		return ret;

	ret = me2600_xilinx_download(dev, fw->data, fw->size);
	release_firmware(fw);

	return ret;
}

/* Reset device */
static int me_reset(struct comedi_device *dev)
{
	struct me_private_data *dev_private = dev->private;

	/* Reset board */
	writew(0x00, dev_private->me_regbase + ME_CONTROL_1);
	writew(0x00, dev_private->me_regbase + ME_CONTROL_2);
	writew(0x00, dev_private->me_regbase + ME_RESET_INTERRUPT);
	writew(0x00, dev_private->me_regbase + ME_DAC_CONTROL);

	/* Save values in the board context */
	dev_private->dac_control = 0;
	dev_private->control_1 = 0;
	dev_private->control_2 = 0;

	return 0;
}

static int me_plx_bug_check(struct comedi_device *dev,
			    struct pci_dev *pcidev)
{
	resource_size_t plx_regbase_tmp = pci_resource_start(pcidev, 0);
	resource_size_t swap_regbase_tmp = pci_resource_start(pcidev, 5);
	resource_size_t regbase_tmp;
	int ret;

	if (!swap_regbase_tmp)
		dev_err(dev->class_dev, "Swap not present\n");

	if (plx_regbase_tmp & 0x0080) {
		dev_err(dev->class_dev, "PLX-Bug detected\n");

		if (swap_regbase_tmp) {
			regbase_tmp = plx_regbase_tmp;
			plx_regbase_tmp = swap_regbase_tmp;
			swap_regbase_tmp = regbase_tmp;

			ret = pci_write_config_dword(pcidev,
						     PCI_BASE_ADDRESS_0,
						     plx_regbase_tmp);
			if (ret != PCIBIOS_SUCCESSFUL)
				return -EIO;

			ret = pci_write_config_dword(pcidev,
						     PCI_BASE_ADDRESS_5,
						     swap_regbase_tmp);
			if (ret != PCIBIOS_SUCCESSFUL)
				return -EIO;
		} else {
			plx_regbase_tmp -= 0x80;
			ret = pci_write_config_dword(pcidev,
						     PCI_BASE_ADDRESS_0,
						     plx_regbase_tmp);
			if (ret != PCIBIOS_SUCCESSFUL)
				return -EIO;
		}
	}
	return 0;
}

static const void *me_find_boardinfo(struct comedi_device *dev,
				     struct pci_dev *pcidev)
{
	const struct me_board *board;
	int i;

	for (i = 0; i < ARRAY_SIZE(me_boards); i++) {
		board = &me_boards[i];
		if (board->device_id == pcidev->device)
			return board;
	}
	return NULL;
}

static int me_attach_pci(struct comedi_device *dev, struct pci_dev *pcidev)
{
	const struct me_board *board;
	struct me_private_data *dev_private;
	struct comedi_subdevice *s;
	int ret;

	board = me_find_boardinfo(dev, pcidev);
	if (!board)
		return -ENODEV;
	dev->board_ptr = board;
	dev->board_name = board->name;

	dev_private = kzalloc(sizeof(*dev_private), GFP_KERNEL);
	if (!dev_private)
		return -ENOMEM;
	dev->private = dev_private;

	ret = comedi_pci_enable(pcidev, dev->board_name);
	if (ret)
		return ret;
	dev->iobase = 1;	/* detach needs this */

	dev_private->plx_regbase = ioremap(pci_resource_start(pcidev, 0),
					   pci_resource_len(pcidev, 0));
	if (!dev_private->plx_regbase)
		return -ENOMEM;

	ret = me_plx_bug_check(dev, pcidev);
	if (ret)
		return ret;

	dev_private->me_regbase = ioremap(pci_resource_start(pcidev, 2),
					  pci_resource_len(pcidev, 2));
	if (!dev_private->me_regbase)
		return -ENOMEM;

	/* Download firmware and reset card */
	if (board->device_id == ME2600_DEVICE_ID) {
		ret = me2600_upload_firmware(dev);
		if (ret < 0)
			return ret;
	}
	me_reset(dev);

	ret = comedi_alloc_subdevices(dev, 3);
	if (ret)
		return ret;

	s = &dev->subdevices[0];
	s->type		= COMEDI_SUBD_AI;
	s->subdev_flags	= SDF_READABLE | SDF_COMMON;
	s->n_chan	= board->ai_chans;
	s->maxdata	= (1 << board->ai_bits) - 1;
	s->len_chanlist	= board->ai_chans;
	s->range_table	= board->ai_range;
	s->insn_read	= me_ai_insn_read;

	s = &dev->subdevices[1];
	if (board->ao_chans) {
		s->type		= COMEDI_SUBD_AO;
		s->subdev_flags	= SDF_WRITEABLE | SDF_COMMON;
		s->n_chan	= board->ao_chans;
		s->maxdata	= (1 << board->ao_bits) - 1;
		s->len_chanlist	= board->ao_chans;
		s->range_table	= board->ao_range;
		s->insn_read	= me_ao_insn_read;
		s->insn_write	= me_ao_insn_write;
	} else {
		s->type = COMEDI_SUBD_UNUSED;
	}

	s = &dev->subdevices[2];
	s->type		= COMEDI_SUBD_DIO;
	s->subdev_flags	= SDF_READABLE | SDF_WRITEABLE;
	s->n_chan	= 32;
	s->maxdata	= 1;
	s->len_chanlist	= 32;
	s->range_table	= &range_digital;
	s->insn_bits	= me_dio_insn_bits;
	s->insn_config	= me_dio_insn_config;
	s->io_bits	= 0;

	dev_info(dev->class_dev, "%s: %s attached\n",
		dev->driver->driver_name, dev->board_name);

	return 0;
}

static void me_detach(struct comedi_device *dev)
{
	struct pci_dev *pcidev = comedi_to_pci_dev(dev);
	struct me_private_data *dev_private = dev->private;

	if (dev_private) {
		if (dev_private->me_regbase) {
			me_reset(dev);
			iounmap(dev_private->me_regbase);
		}
		if (dev_private->plx_regbase)
			iounmap(dev_private->plx_regbase);
	}
	if (pcidev) {
		if (dev->iobase)
			comedi_pci_disable(pcidev);
	}
}

static struct comedi_driver me_daq_driver = {
	.driver_name	= "me_daq",
	.module		= THIS_MODULE,
	.attach_pci	= me_attach_pci,
	.detach		= me_detach,
};

static int __devinit me_daq_pci_probe(struct pci_dev *dev,
				      const struct pci_device_id *ent)
{
	return comedi_pci_auto_config(dev, &me_daq_driver);
}

static void __devexit me_daq_pci_remove(struct pci_dev *dev)
{
	comedi_pci_auto_unconfig(dev);
}

static DEFINE_PCI_DEVICE_TABLE(me_daq_pci_table) = {
	{ PCI_DEVICE(PCI_VENDOR_ID_MEILHAUS, ME2600_DEVICE_ID) },
	{ PCI_DEVICE(PCI_VENDOR_ID_MEILHAUS, ME2000_DEVICE_ID) },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, me_daq_pci_table);

static struct pci_driver me_daq_pci_driver = {
	.name		= "me_daq",
	.id_table	= me_daq_pci_table,
	.probe		= me_daq_pci_probe,
	.remove		= __devexit_p(me_daq_pci_remove),
};
module_comedi_pci_driver(me_daq_driver, me_daq_pci_driver);

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
MODULE_FIRMWARE(ME2600_FIRMWARE);
