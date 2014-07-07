/*
 * max14577-private.h - Common API for the Maxim 14577/77836 internal sub chip
 *
 * Copyright (C) 2014 Samsung Electrnoics
 * Chanwoo Choi <cw00.choi@samsung.com>
 * Krzysztof Kozlowski <k.kozlowski@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MAX14577_PRIVATE_H__
#define __MAX14577_PRIVATE_H__

#include <linux/i2c.h>
#include <linux/regmap.h>

#define I2C_ADDR_PMIC	(0x46 >> 1)
#define I2C_ADDR_MUIC	(0x4A >> 1)
#define I2C_ADDR_FG	(0x6C >> 1)

enum maxim_device_type {
	MAXIM_DEVICE_TYPE_UNKNOWN	= 0,
	MAXIM_DEVICE_TYPE_MAX14577,
	MAXIM_DEVICE_TYPE_MAX77836,

	MAXIM_DEVICE_TYPE_NUM,
};

/* Slave addr = 0x4A: MUIC and Charger */
enum max14577_reg {
	MAX14577_REG_DEVICEID		= 0x00,
	MAX14577_REG_INT1		= 0x01,
	MAX14577_REG_INT2		= 0x02,
	MAX14577_REG_INT3		= 0x03,
	MAX14577_REG_STATUS1		= 0x04,
	MAX14577_REG_STATUS2		= 0x05,
	MAX14577_REG_STATUS3		= 0x06,
	MAX14577_REG_INTMASK1		= 0x07,
	MAX14577_REG_INTMASK2		= 0x08,
	MAX14577_REG_INTMASK3		= 0x09,
	MAX14577_REG_CDETCTRL1		= 0x0A,
	MAX14577_REG_RFU		= 0x0B,
	MAX14577_REG_CONTROL1		= 0x0C,
	MAX14577_REG_CONTROL2		= 0x0D,
	MAX14577_REG_CONTROL3		= 0x0E,
	MAX14577_REG_CHGCTRL1		= 0x0F,
	MAX14577_REG_CHGCTRL2		= 0x10,
	MAX14577_REG_CHGCTRL3		= 0x11,
	MAX14577_REG_CHGCTRL4		= 0x12,
	MAX14577_REG_CHGCTRL5		= 0x13,
	MAX14577_REG_CHGCTRL6		= 0x14,
	MAX14577_REG_CHGCTRL7		= 0x15,

	MAX14577_REG_END,
};

/* Slave addr = 0x4A: MUIC */
enum max14577_muic_reg {
	MAX14577_MUIC_REG_STATUS1	= 0x04,
	MAX14577_MUIC_REG_STATUS2	= 0x05,
	MAX14577_MUIC_REG_CONTROL1	= 0x0C,
	MAX14577_MUIC_REG_CONTROL3	= 0x0E,

	MAX14577_MUIC_REG_END,
};

enum max14577_muic_charger_type {
	MAX14577_CHARGER_TYPE_NONE = 0,
	MAX14577_CHARGER_TYPE_USB,
	MAX14577_CHARGER_TYPE_DOWNSTREAM_PORT,
	MAX14577_CHARGER_TYPE_DEDICATED_CHG,
	MAX14577_CHARGER_TYPE_SPECIAL_500MA,
	MAX14577_CHARGER_TYPE_SPECIAL_1A,
	MAX14577_CHARGER_TYPE_RESERVED,
	MAX14577_CHARGER_TYPE_DEAD_BATTERY = 7,
};

/* MAX14577 interrupts */
#define MAX14577_INT1_ADC_MASK		BIT(0)
#define MAX14577_INT1_ADCLOW_MASK	BIT(1)
#define MAX14577_INT1_ADCERR_MASK	BIT(2)
#define MAX77836_INT1_ADC1K_MASK	BIT(3)

#define MAX14577_INT2_CHGTYP_MASK	BIT(0)
#define MAX14577_INT2_CHGDETRUN_MASK	BIT(1)
#define MAX14577_INT2_DCDTMR_MASK	BIT(2)
#define MAX14577_INT2_DBCHG_MASK	BIT(3)
#define MAX14577_INT2_VBVOLT_MASK	BIT(4)
#define MAX77836_INT2_VIDRM_MASK	BIT(5)

#define MAX14577_INT3_EOC_MASK		BIT(0)
#define MAX14577_INT3_CGMBC_MASK	BIT(1)
#define MAX14577_INT3_OVP_MASK		BIT(2)
#define MAX14577_INT3_MBCCHGERR_MASK	BIT(3)

/* MAX14577 DEVICE ID register */
#define DEVID_VENDORID_SHIFT		0
#define DEVID_DEVICEID_SHIFT		3
#define DEVID_VENDORID_MASK		(0x07 << DEVID_VENDORID_SHIFT)
#define DEVID_DEVICEID_MASK		(0x1f << DEVID_DEVICEID_SHIFT)

/* MAX14577 STATUS1 register */
#define STATUS1_ADC_SHIFT		0
#define STATUS1_ADCLOW_SHIFT		5
#define STATUS1_ADCERR_SHIFT		6
#define MAX77836_STATUS1_ADC1K_SHIFT	7
#define STATUS1_ADC_MASK		(0x1f << STATUS1_ADC_SHIFT)
#define STATUS1_ADCLOW_MASK		BIT(STATUS1_ADCLOW_SHIFT)
#define STATUS1_ADCERR_MASK		BIT(STATUS1_ADCERR_SHIFT)
#define MAX77836_STATUS1_ADC1K_MASK	BIT(MAX77836_STATUS1_ADC1K_SHIFT)

/* MAX14577 STATUS2 register */
#define STATUS2_CHGTYP_SHIFT		0
#define STATUS2_CHGDETRUN_SHIFT		3
#define STATUS2_DCDTMR_SHIFT		4
#define STATUS2_DBCHG_SHIFT		5
#define STATUS2_VBVOLT_SHIFT		6
#define MAX77836_STATUS2_VIDRM_SHIFT	7
#define STATUS2_CHGTYP_MASK		(0x7 << STATUS2_CHGTYP_SHIFT)
#define STATUS2_CHGDETRUN_MASK		BIT(STATUS2_CHGDETRUN_SHIFT)
#define STATUS2_DCDTMR_MASK		BIT(STATUS2_DCDTMR_SHIFT)
#define STATUS2_DBCHG_MASK		BIT(STATUS2_DBCHG_SHIFT)
#define STATUS2_VBVOLT_MASK		BIT(STATUS2_VBVOLT_SHIFT)
#define MAX77836_STATUS2_VIDRM_MASK	BIT(MAX77836_STATUS2_VIDRM_SHIFT)

/* MAX14577 CONTROL1 register */
#define COMN1SW_SHIFT			0
#define COMP2SW_SHIFT			3
#define MICEN_SHIFT			6
#define IDBEN_SHIFT			7
#define COMN1SW_MASK			(0x7 << COMN1SW_SHIFT)
#define COMP2SW_MASK			(0x7 << COMP2SW_SHIFT)
#define MICEN_MASK			BIT(MICEN_SHIFT)
#define IDBEN_MASK			BIT(IDBEN_SHIFT)
#define CLEAR_IDBEN_MICEN_MASK		(COMN1SW_MASK | COMP2SW_MASK)
#define CTRL1_SW_USB			((1 << COMP2SW_SHIFT) \
						| (1 << COMN1SW_SHIFT))
#define CTRL1_SW_AUDIO			((2 << COMP2SW_SHIFT) \
						| (2 << COMN1SW_SHIFT))
#define CTRL1_SW_UART			((3 << COMP2SW_SHIFT) \
						| (3 << COMN1SW_SHIFT))
#define CTRL1_SW_OPEN			((0 << COMP2SW_SHIFT) \
						| (0 << COMN1SW_SHIFT))

/* MAX14577 CONTROL2 register */
#define CTRL2_LOWPWR_SHIFT		(0)
#define CTRL2_ADCEN_SHIFT		(1)
#define CTRL2_CPEN_SHIFT		(2)
#define CTRL2_SFOUTASRT_SHIFT		(3)
#define CTRL2_SFOUTORD_SHIFT		(4)
#define CTRL2_ACCDET_SHIFT		(5)
#define CTRL2_USBCPINT_SHIFT		(6)
#define CTRL2_RCPS_SHIFT		(7)
#define CTRL2_LOWPWR_MASK		BIT(CTRL2_LOWPWR_SHIFT)
#define CTRL2_ADCEN_MASK		BIT(CTRL2_ADCEN_SHIFT)
#define CTRL2_CPEN_MASK			BIT(CTRL2_CPEN_SHIFT)
#define CTRL2_SFOUTASRT_MASK		BIT(CTRL2_SFOUTASRT_SHIFT)
#define CTRL2_SFOUTORD_MASK		BIT(CTRL2_SFOUTORD_SHIFT)
#define CTRL2_ACCDET_MASK		BIT(CTRL2_ACCDET_SHIFT)
#define CTRL2_USBCPINT_MASK		BIT(CTRL2_USBCPINT_SHIFT)
#define CTRL2_RCPS_MASK			BIT(CTRL2_RCPS_SHIFT)

#define CTRL2_CPEN1_LOWPWR0 ((1 << CTRL2_CPEN_SHIFT) | \
				(0 << CTRL2_LOWPWR_SHIFT))
#define CTRL2_CPEN0_LOWPWR1 ((0 << CTRL2_CPEN_SHIFT) | \
				(1 << CTRL2_LOWPWR_SHIFT))

/* MAX14577 CONTROL3 register */
#define CTRL3_JIGSET_SHIFT		0
#define CTRL3_BOOTSET_SHIFT		2
#define CTRL3_ADCDBSET_SHIFT		4
#define CTRL3_JIGSET_MASK		(0x3 << CTRL3_JIGSET_SHIFT)
#define CTRL3_BOOTSET_MASK		(0x3 << CTRL3_BOOTSET_SHIFT)
#define CTRL3_ADCDBSET_MASK		(0x3 << CTRL3_ADCDBSET_SHIFT)

/* Slave addr = 0x4A: Charger */
enum max14577_charger_reg {
	MAX14577_CHG_REG_STATUS3	= 0x06,
	MAX14577_CHG_REG_CHG_CTRL1	= 0x0F,
	MAX14577_CHG_REG_CHG_CTRL2	= 0x10,
	MAX14577_CHG_REG_CHG_CTRL3	= 0x11,
	MAX14577_CHG_REG_CHG_CTRL4	= 0x12,
	MAX14577_CHG_REG_CHG_CTRL5	= 0x13,
	MAX14577_CHG_REG_CHG_CTRL6	= 0x14,
	MAX14577_CHG_REG_CHG_CTRL7	= 0x15,

	MAX14577_CHG_REG_END,
};

/* MAX14577 STATUS3 register */
#define STATUS3_EOC_SHIFT		0
#define STATUS3_CGMBC_SHIFT		1
#define STATUS3_OVP_SHIFT		2
#define STATUS3_MBCCHGERR_SHIFT		3
#define STATUS3_EOC_MASK		(0x1 << STATUS3_EOC_SHIFT)
#define STATUS3_CGMBC_MASK		(0x1 << STATUS3_CGMBC_SHIFT)
#define STATUS3_OVP_MASK		(0x1 << STATUS3_OVP_SHIFT)
#define STATUS3_MBCCHGERR_MASK		(0x1 << STATUS3_MBCCHGERR_SHIFT)

/* MAX14577 CDETCTRL1 register */
#define CDETCTRL1_CHGDETEN_SHIFT	0
#define CDETCTRL1_CHGTYPMAN_SHIFT	1
#define CDETCTRL1_DCDEN_SHIFT		2
#define CDETCTRL1_DCD2SCT_SHIFT		3
#define CDETCTRL1_DCHKTM_SHIFT		4
#define CDETCTRL1_DBEXIT_SHIFT		5
#define CDETCTRL1_DBIDLE_SHIFT		6
#define CDETCTRL1_CDPDET_SHIFT		7
#define CDETCTRL1_CHGDETEN_MASK		BIT(CDETCTRL1_CHGDETEN_SHIFT)
#define CDETCTRL1_CHGTYPMAN_MASK	BIT(CDETCTRL1_CHGTYPMAN_SHIFT)
#define CDETCTRL1_DCDEN_MASK		BIT(CDETCTRL1_DCDEN_SHIFT)
#define CDETCTRL1_DCD2SCT_MASK		BIT(CDETCTRL1_DCD2SCT_SHIFT)
#define CDETCTRL1_DCHKTM_MASK		BIT(CDETCTRL1_DCHKTM_SHIFT)
#define CDETCTRL1_DBEXIT_MASK		BIT(CDETCTRL1_DBEXIT_SHIFT)
#define CDETCTRL1_DBIDLE_MASK		BIT(CDETCTRL1_DBIDLE_SHIFT)
#define CDETCTRL1_CDPDET_MASK		BIT(CDETCTRL1_CDPDET_SHIFT)

/* MAX14577 CHGCTRL1 register */
#define CHGCTRL1_TCHW_SHIFT		4
#define CHGCTRL1_TCHW_MASK		(0x7 << CHGCTRL1_TCHW_SHIFT)

/* MAX14577 CHGCTRL2 register */
#define CHGCTRL2_MBCHOSTEN_SHIFT	6
#define CHGCTRL2_MBCHOSTEN_MASK		BIT(CHGCTRL2_MBCHOSTEN_SHIFT)
#define CHGCTRL2_VCHGR_RC_SHIFT		7
#define CHGCTRL2_VCHGR_RC_MASK		BIT(CHGCTRL2_VCHGR_RC_SHIFT)

/* MAX14577 CHGCTRL3 register */
#define CHGCTRL3_MBCCVWRC_SHIFT		0
#define CHGCTRL3_MBCCVWRC_MASK		(0xf << CHGCTRL3_MBCCVWRC_SHIFT)

/* MAX14577 CHGCTRL4 register */
#define CHGCTRL4_MBCICHWRCH_SHIFT	0
#define CHGCTRL4_MBCICHWRCH_MASK	(0xf << CHGCTRL4_MBCICHWRCH_SHIFT)
#define CHGCTRL4_MBCICHWRCL_SHIFT	4
#define CHGCTRL4_MBCICHWRCL_MASK	BIT(CHGCTRL4_MBCICHWRCL_SHIFT)

/* MAX14577 CHGCTRL5 register */
#define CHGCTRL5_EOCS_SHIFT		0
#define CHGCTRL5_EOCS_MASK		(0xf << CHGCTRL5_EOCS_SHIFT)

/* MAX14577 CHGCTRL6 register */
#define CHGCTRL6_AUTOSTOP_SHIFT		5
#define CHGCTRL6_AUTOSTOP_MASK		BIT(CHGCTRL6_AUTOSTOP_SHIFT)

/* MAX14577 CHGCTRL7 register */
#define CHGCTRL7_OTPCGHCVS_SHIFT	0
#define CHGCTRL7_OTPCGHCVS_MASK		(0x3 << CHGCTRL7_OTPCGHCVS_SHIFT)

/* MAX14577 regulator current limits (as in CHGCTRL4 register), uA */
#define MAX14577_REGULATOR_CURRENT_LIMIT_MIN		 90000
#define MAX14577_REGULATOR_CURRENT_LIMIT_HIGH_START	200000
#define MAX14577_REGULATOR_CURRENT_LIMIT_HIGH_STEP	 50000
#define MAX14577_REGULATOR_CURRENT_LIMIT_MAX		950000

/* MAX77836 regulator current limits (as in CHGCTRL4 register), uA */
#define MAX77836_REGULATOR_CURRENT_LIMIT_MIN		 45000
#define MAX77836_REGULATOR_CURRENT_LIMIT_HIGH_START	100000
#define MAX77836_REGULATOR_CURRENT_LIMIT_HIGH_STEP	 25000
#define MAX77836_REGULATOR_CURRENT_LIMIT_MAX		475000

/* MAX14577 regulator SFOUT LDO voltage, fixed, uV */
#define MAX14577_REGULATOR_SAFEOUT_VOLTAGE		4900000

/* MAX77836 regulator LDOx voltage, uV */
#define MAX77836_REGULATOR_LDO_VOLTAGE_MIN		800000
#define MAX77836_REGULATOR_LDO_VOLTAGE_MAX		3950000
#define MAX77836_REGULATOR_LDO_VOLTAGE_STEP		50000
#define MAX77836_REGULATOR_LDO_VOLTAGE_STEPS_NUM	64

/* Slave addr = 0x46: PMIC */
enum max77836_pmic_reg {
	MAX77836_PMIC_REG_PMIC_ID		= 0x20,
	MAX77836_PMIC_REG_PMIC_REV		= 0x21,
	MAX77836_PMIC_REG_INTSRC		= 0x22,
	MAX77836_PMIC_REG_INTSRC_MASK		= 0x23,
	MAX77836_PMIC_REG_TOPSYS_INT		= 0x24,
	MAX77836_PMIC_REG_TOPSYS_INT_MASK	= 0x26,
	MAX77836_PMIC_REG_TOPSYS_STAT		= 0x28,
	MAX77836_PMIC_REG_MRSTB_CNTL		= 0x2A,
	MAX77836_PMIC_REG_LSCNFG		= 0x2B,

	MAX77836_LDO_REG_CNFG1_LDO1		= 0x51,
	MAX77836_LDO_REG_CNFG2_LDO1		= 0x52,
	MAX77836_LDO_REG_CNFG1_LDO2		= 0x53,
	MAX77836_LDO_REG_CNFG2_LDO2		= 0x54,
	MAX77836_LDO_REG_CNFG_LDO_BIAS		= 0x55,

	MAX77836_COMP_REG_COMP1			= 0x60,

	MAX77836_PMIC_REG_END,
};

#define MAX77836_INTSRC_MASK_TOP_INT_SHIFT	1
#define MAX77836_INTSRC_MASK_MUIC_CHG_INT_SHIFT	3
#define MAX77836_INTSRC_MASK_TOP_INT_MASK	BIT(MAX77836_INTSRC_MASK_TOP_INT_SHIFT)
#define MAX77836_INTSRC_MASK_MUIC_CHG_INT_MASK	BIT(MAX77836_INTSRC_MASK_MUIC_CHG_INT_SHIFT)

/* MAX77836 PMIC interrupts */
#define MAX77836_TOPSYS_INT_T120C_SHIFT		0
#define MAX77836_TOPSYS_INT_T140C_SHIFT		1
#define MAX77836_TOPSYS_INT_T120C_MASK		BIT(MAX77836_TOPSYS_INT_T120C_SHIFT)
#define MAX77836_TOPSYS_INT_T140C_MASK		BIT(MAX77836_TOPSYS_INT_T140C_SHIFT)

/* LDO1/LDO2 CONFIG1 register */
#define MAX77836_CNFG1_LDO_PWRMD_SHIFT		6
#define MAX77836_CNFG1_LDO_TV_SHIFT		0
#define MAX77836_CNFG1_LDO_PWRMD_MASK		(0x3 << MAX77836_CNFG1_LDO_PWRMD_SHIFT)
#define MAX77836_CNFG1_LDO_TV_MASK		(0x3f << MAX77836_CNFG1_LDO_TV_SHIFT)

/* LDO1/LDO2 CONFIG2 register */
#define MAX77836_CNFG2_LDO_OVCLMPEN_SHIFT	7
#define MAX77836_CNFG2_LDO_ALPMEN_SHIFT		6
#define MAX77836_CNFG2_LDO_COMP_SHIFT		4
#define MAX77836_CNFG2_LDO_POK_SHIFT		3
#define MAX77836_CNFG2_LDO_ADE_SHIFT		1
#define MAX77836_CNFG2_LDO_SS_SHIFT		0
#define MAX77836_CNFG2_LDO_OVCLMPEN_MASK	BIT(MAX77836_CNFG2_LDO_OVCLMPEN_SHIFT)
#define MAX77836_CNFG2_LDO_ALPMEN_MASK		BIT(MAX77836_CNFG2_LDO_ALPMEN_SHIFT)
#define MAX77836_CNFG2_LDO_COMP_MASK		(0x3 << MAX77836_CNFG2_LDO_COMP_SHIFT)
#define MAX77836_CNFG2_LDO_POK_MASK		BIT(MAX77836_CNFG2_LDO_POK_SHIFT)
#define MAX77836_CNFG2_LDO_ADE_MASK		BIT(MAX77836_CNFG2_LDO_ADE_SHIFT)
#define MAX77836_CNFG2_LDO_SS_MASK		BIT(MAX77836_CNFG2_LDO_SS_SHIFT)

/* Slave addr = 0x6C: Fuel-Gauge/Battery */
enum max77836_fg_reg {
	MAX77836_FG_REG_VCELL_MSB	= 0x02,
	MAX77836_FG_REG_VCELL_LSB	= 0x03,
	MAX77836_FG_REG_SOC_MSB		= 0x04,
	MAX77836_FG_REG_SOC_LSB		= 0x05,
	MAX77836_FG_REG_MODE_H		= 0x06,
	MAX77836_FG_REG_MODE_L		= 0x07,
	MAX77836_FG_REG_VERSION_MSB	= 0x08,
	MAX77836_FG_REG_VERSION_LSB	= 0x09,
	MAX77836_FG_REG_HIBRT_H		= 0x0A,
	MAX77836_FG_REG_HIBRT_L		= 0x0B,
	MAX77836_FG_REG_CONFIG_H	= 0x0C,
	MAX77836_FG_REG_CONFIG_L	= 0x0D,
	MAX77836_FG_REG_VALRT_MIN	= 0x14,
	MAX77836_FG_REG_VALRT_MAX	= 0x15,
	MAX77836_FG_REG_CRATE_MSB	= 0x16,
	MAX77836_FG_REG_CRATE_LSB	= 0x17,
	MAX77836_FG_REG_VRESET		= 0x18,
	MAX77836_FG_REG_FGID		= 0x19,
	MAX77836_FG_REG_STATUS_H	= 0x1A,
	MAX77836_FG_REG_STATUS_L	= 0x1B,
	/*
	 * TODO: TABLE registers
	 * TODO: CMD register
	 */

	MAX77836_FG_REG_END,
};

enum max14577_irq {
	/* INT1 */
	MAX14577_IRQ_INT1_ADC,
	MAX14577_IRQ_INT1_ADCLOW,
	MAX14577_IRQ_INT1_ADCERR,
	MAX77836_IRQ_INT1_ADC1K,

	/* INT2 */
	MAX14577_IRQ_INT2_CHGTYP,
	MAX14577_IRQ_INT2_CHGDETRUN,
	MAX14577_IRQ_INT2_DCDTMR,
	MAX14577_IRQ_INT2_DBCHG,
	MAX14577_IRQ_INT2_VBVOLT,
	MAX77836_IRQ_INT2_VIDRM,

	/* INT3 */
	MAX14577_IRQ_INT3_EOC,
	MAX14577_IRQ_INT3_CGMBC,
	MAX14577_IRQ_INT3_OVP,
	MAX14577_IRQ_INT3_MBCCHGERR,

	/* TOPSYS_INT, only MAX77836 */
	MAX77836_IRQ_TOPSYS_T140C,
	MAX77836_IRQ_TOPSYS_T120C,

	MAX14577_IRQ_NUM,
};

struct max14577 {
	struct device *dev;
	struct i2c_client *i2c; /* Slave addr = 0x4A */
	struct i2c_client *i2c_pmic; /* Slave addr = 0x46 */
	enum maxim_device_type dev_type;

	struct regmap *regmap; /* For MUIC and Charger */
	struct regmap *regmap_pmic;

	struct regmap_irq_chip_data *irq_data; /* For MUIC and Charger */
	struct regmap_irq_chip_data *irq_data_pmic;
	int irq;
};

/* MAX14577 shared regmap API function */
static inline int max14577_read_reg(struct regmap *map, u8 reg, u8 *dest)
{
	unsigned int val;
	int ret;

	ret = regmap_read(map, reg, &val);
	*dest = val;

	return ret;
}

static inline int max14577_bulk_read(struct regmap *map, u8 reg, u8 *buf,
		int count)
{
	return regmap_bulk_read(map, reg, buf, count);
}

static inline int max14577_write_reg(struct regmap *map, u8 reg, u8 value)
{
	return regmap_write(map, reg, value);
}

static inline int max14577_bulk_write(struct regmap *map, u8 reg, u8 *buf,
		int count)
{
	return regmap_bulk_write(map, reg, buf, count);
}

static inline int max14577_update_reg(struct regmap *map, u8 reg, u8 mask,
		u8 val)
{
	return regmap_update_bits(map, reg, mask, val);
}

#endif /* __MAX14577_PRIVATE_H__ */
