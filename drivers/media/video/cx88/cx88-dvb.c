/*
 *
 * device driver for Conexant 2388x based TV cards
 * MPEG Transport Stream (DVB) routines
 *
 * (c) 2004, 2005 Chris Pascoe <c.pascoe@itee.uq.edu.au>
 * (c) 2004 Gerd Knorr <kraxel@bytesex.org> [SuSE Labs]
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/file.h>
#include <linux/suspend.h>

#include "cx88.h"
#include "dvb-pll.h"
#include <media/v4l2-common.h>

#include "mt352.h"
#include "mt352_priv.h"
#include "cx88-vp3054-i2c.h"
#include "zl10353.h"
#include "cx22702.h"
#include "or51132.h"
#include "lgdt330x.h"
#include "s5h1409.h"
#include "xc5000.h"
#include "nxt200x.h"
#include "cx24123.h"
#include "isl6421.h"
#include "tuner-simple.h"
#include "tda9887.h"
#include "s5h1411.h"
#include "stv0299.h"
#include "z0194a.h"
#include "stv0288.h"
#include "stb6000.h"
#include "cx24116.h"

MODULE_DESCRIPTION("driver for cx2388x based DVB cards");
MODULE_AUTHOR("Chris Pascoe <c.pascoe@itee.uq.edu.au>");
MODULE_AUTHOR("Gerd Knorr <kraxel@bytesex.org> [SuSE Labs]");
MODULE_LICENSE("GPL");

static unsigned int debug;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug,"enable debug messages [dvb]");

DVB_DEFINE_MOD_OPT_ADAPTER_NR(adapter_nr);

#define dprintk(level,fmt, arg...)	if (debug >= level) \
	printk(KERN_DEBUG "%s/2-dvb: " fmt, core->name, ## arg)

/* ------------------------------------------------------------------ */

static int dvb_buf_setup(struct videobuf_queue *q,
			 unsigned int *count, unsigned int *size)
{
	struct cx8802_dev *dev = q->priv_data;

	dev->ts_packet_size  = 188 * 4;
	dev->ts_packet_count = 32;

	*size  = dev->ts_packet_size * dev->ts_packet_count;
	*count = 32;
	return 0;
}

static int dvb_buf_prepare(struct videobuf_queue *q,
			   struct videobuf_buffer *vb, enum v4l2_field field)
{
	struct cx8802_dev *dev = q->priv_data;
	return cx8802_buf_prepare(q, dev, (struct cx88_buffer*)vb,field);
}

static void dvb_buf_queue(struct videobuf_queue *q, struct videobuf_buffer *vb)
{
	struct cx8802_dev *dev = q->priv_data;
	cx8802_buf_queue(dev, (struct cx88_buffer*)vb);
}

static void dvb_buf_release(struct videobuf_queue *q,
			    struct videobuf_buffer *vb)
{
	cx88_free_buffer(q, (struct cx88_buffer*)vb);
}

static struct videobuf_queue_ops dvb_qops = {
	.buf_setup    = dvb_buf_setup,
	.buf_prepare  = dvb_buf_prepare,
	.buf_queue    = dvb_buf_queue,
	.buf_release  = dvb_buf_release,
};

/* ------------------------------------------------------------------ */

static int cx88_dvb_bus_ctrl(struct dvb_frontend* fe, int acquire)
{
	struct cx8802_dev *dev= fe->dvb->priv;
	struct cx8802_driver *drv = NULL;
	int ret = 0;
	int fe_id;

	fe_id = videobuf_dvb_find_frontend(&dev->frontends, fe);
	if (!fe_id) {
		printk(KERN_ERR "%s() No frontend found\n", __func__);
		return -EINVAL;
	}

	drv = cx8802_get_driver(dev, CX88_MPEG_DVB);
	if (drv) {
		if (acquire){
			dev->frontends.active_fe_id = fe_id;
			ret = drv->request_acquire(drv);
		} else {
			ret = drv->request_release(drv);
			dev->frontends.active_fe_id = 0;
		}
	}

	return ret;
}

/* ------------------------------------------------------------------ */

static int dvico_fusionhdtv_demod_init(struct dvb_frontend* fe)
{
	static u8 clock_config []  = { CLOCK_CTL,  0x38, 0x39 };
	static u8 reset []         = { RESET,      0x80 };
	static u8 adc_ctl_1_cfg [] = { ADC_CTL_1,  0x40 };
	static u8 agc_cfg []       = { AGC_TARGET, 0x24, 0x20 };
	static u8 gpp_ctl_cfg []   = { GPP_CTL,    0x33 };
	static u8 capt_range_cfg[] = { CAPT_RANGE, 0x32 };

	mt352_write(fe, clock_config,   sizeof(clock_config));
	udelay(200);
	mt352_write(fe, reset,          sizeof(reset));
	mt352_write(fe, adc_ctl_1_cfg,  sizeof(adc_ctl_1_cfg));

	mt352_write(fe, agc_cfg,        sizeof(agc_cfg));
	mt352_write(fe, gpp_ctl_cfg,    sizeof(gpp_ctl_cfg));
	mt352_write(fe, capt_range_cfg, sizeof(capt_range_cfg));
	return 0;
}

static int dvico_dual_demod_init(struct dvb_frontend *fe)
{
	static u8 clock_config []  = { CLOCK_CTL,  0x38, 0x38 };
	static u8 reset []         = { RESET,      0x80 };
	static u8 adc_ctl_1_cfg [] = { ADC_CTL_1,  0x40 };
	static u8 agc_cfg []       = { AGC_TARGET, 0x28, 0x20 };
	static u8 gpp_ctl_cfg []   = { GPP_CTL,    0x33 };
	static u8 capt_range_cfg[] = { CAPT_RANGE, 0x32 };

	mt352_write(fe, clock_config,   sizeof(clock_config));
	udelay(200);
	mt352_write(fe, reset,          sizeof(reset));
	mt352_write(fe, adc_ctl_1_cfg,  sizeof(adc_ctl_1_cfg));

	mt352_write(fe, agc_cfg,        sizeof(agc_cfg));
	mt352_write(fe, gpp_ctl_cfg,    sizeof(gpp_ctl_cfg));
	mt352_write(fe, capt_range_cfg, sizeof(capt_range_cfg));

	return 0;
}

static int dntv_live_dvbt_demod_init(struct dvb_frontend* fe)
{
	static u8 clock_config []  = { 0x89, 0x38, 0x39 };
	static u8 reset []         = { 0x50, 0x80 };
	static u8 adc_ctl_1_cfg [] = { 0x8E, 0x40 };
	static u8 agc_cfg []       = { 0x67, 0x10, 0x23, 0x00, 0xFF, 0xFF,
				       0x00, 0xFF, 0x00, 0x40, 0x40 };
	static u8 dntv_extra[]     = { 0xB5, 0x7A };
	static u8 capt_range_cfg[] = { 0x75, 0x32 };

	mt352_write(fe, clock_config,   sizeof(clock_config));
	udelay(2000);
	mt352_write(fe, reset,          sizeof(reset));
	mt352_write(fe, adc_ctl_1_cfg,  sizeof(adc_ctl_1_cfg));

	mt352_write(fe, agc_cfg,        sizeof(agc_cfg));
	udelay(2000);
	mt352_write(fe, dntv_extra,     sizeof(dntv_extra));
	mt352_write(fe, capt_range_cfg, sizeof(capt_range_cfg));

	return 0;
}

static struct mt352_config dvico_fusionhdtv = {
	.demod_address = 0x0f,
	.demod_init    = dvico_fusionhdtv_demod_init,
};

static struct mt352_config dntv_live_dvbt_config = {
	.demod_address = 0x0f,
	.demod_init    = dntv_live_dvbt_demod_init,
};

static struct mt352_config dvico_fusionhdtv_dual = {
	.demod_address = 0x0f,
	.demod_init    = dvico_dual_demod_init,
};

#if defined(CONFIG_VIDEO_CX88_VP3054) || (defined(CONFIG_VIDEO_CX88_VP3054_MODULE) && defined(MODULE))
static int dntv_live_dvbt_pro_demod_init(struct dvb_frontend* fe)
{
	static u8 clock_config []  = { 0x89, 0x38, 0x38 };
	static u8 reset []         = { 0x50, 0x80 };
	static u8 adc_ctl_1_cfg [] = { 0x8E, 0x40 };
	static u8 agc_cfg []       = { 0x67, 0x10, 0x20, 0x00, 0xFF, 0xFF,
				       0x00, 0xFF, 0x00, 0x40, 0x40 };
	static u8 dntv_extra[]     = { 0xB5, 0x7A };
	static u8 capt_range_cfg[] = { 0x75, 0x32 };

	mt352_write(fe, clock_config,   sizeof(clock_config));
	udelay(2000);
	mt352_write(fe, reset,          sizeof(reset));
	mt352_write(fe, adc_ctl_1_cfg,  sizeof(adc_ctl_1_cfg));

	mt352_write(fe, agc_cfg,        sizeof(agc_cfg));
	udelay(2000);
	mt352_write(fe, dntv_extra,     sizeof(dntv_extra));
	mt352_write(fe, capt_range_cfg, sizeof(capt_range_cfg));

	return 0;
}

static struct mt352_config dntv_live_dvbt_pro_config = {
	.demod_address = 0x0f,
	.no_tuner      = 1,
	.demod_init    = dntv_live_dvbt_pro_demod_init,
};
#endif

static struct zl10353_config dvico_fusionhdtv_hybrid = {
	.demod_address = 0x0f,
	.no_tuner      = 1,
};

static struct zl10353_config dvico_fusionhdtv_xc3028 = {
	.demod_address = 0x0f,
	.if2           = 45600,
	.no_tuner      = 1,
};

static struct mt352_config dvico_fusionhdtv_mt352_xc3028 = {
	.demod_address = 0x0f,
	.if2 = 4560,
	.no_tuner = 1,
	.demod_init = dvico_fusionhdtv_demod_init,
};

static struct zl10353_config dvico_fusionhdtv_plus_v1_1 = {
	.demod_address = 0x0f,
};

static struct cx22702_config connexant_refboard_config = {
	.demod_address = 0x43,
	.output_mode   = CX22702_SERIAL_OUTPUT,
};

static struct cx22702_config hauppauge_hvr_config = {
	.demod_address = 0x63,
	.output_mode   = CX22702_SERIAL_OUTPUT,
};

static int or51132_set_ts_param(struct dvb_frontend* fe, int is_punctured)
{
	struct cx8802_dev *dev= fe->dvb->priv;
	dev->ts_gen_cntrl = is_punctured ? 0x04 : 0x00;
	return 0;
}

static struct or51132_config pchdtv_hd3000 = {
	.demod_address = 0x15,
	.set_ts_params = or51132_set_ts_param,
};

static int lgdt330x_pll_rf_set(struct dvb_frontend* fe, int index)
{
	struct cx8802_dev *dev= fe->dvb->priv;
	struct cx88_core *core = dev->core;

	dprintk(1, "%s: index = %d\n", __func__, index);
	if (index == 0)
		cx_clear(MO_GP0_IO, 8);
	else
		cx_set(MO_GP0_IO, 8);
	return 0;
}

static int lgdt330x_set_ts_param(struct dvb_frontend* fe, int is_punctured)
{
	struct cx8802_dev *dev= fe->dvb->priv;
	if (is_punctured)
		dev->ts_gen_cntrl |= 0x04;
	else
		dev->ts_gen_cntrl &= ~0x04;
	return 0;
}

static struct lgdt330x_config fusionhdtv_3_gold = {
	.demod_address = 0x0e,
	.demod_chip    = LGDT3302,
	.serial_mpeg   = 0x04, /* TPSERIAL for 3302 in TOP_CONTROL */
	.set_ts_params = lgdt330x_set_ts_param,
};

static struct lgdt330x_config fusionhdtv_5_gold = {
	.demod_address = 0x0e,
	.demod_chip    = LGDT3303,
	.serial_mpeg   = 0x40, /* TPSERIAL for 3303 in TOP_CONTROL */
	.set_ts_params = lgdt330x_set_ts_param,
};

static struct lgdt330x_config pchdtv_hd5500 = {
	.demod_address = 0x59,
	.demod_chip    = LGDT3303,
	.serial_mpeg   = 0x40, /* TPSERIAL for 3303 in TOP_CONTROL */
	.set_ts_params = lgdt330x_set_ts_param,
};

static int nxt200x_set_ts_param(struct dvb_frontend* fe, int is_punctured)
{
	struct cx8802_dev *dev= fe->dvb->priv;
	dev->ts_gen_cntrl = is_punctured ? 0x04 : 0x00;
	return 0;
}

static struct nxt200x_config ati_hdtvwonder = {
	.demod_address = 0x0a,
	.set_ts_params = nxt200x_set_ts_param,
};

static int cx24123_set_ts_param(struct dvb_frontend* fe,
	int is_punctured)
{
	struct cx8802_dev *dev= fe->dvb->priv;
	dev->ts_gen_cntrl = 0x02;
	return 0;
}

static int kworld_dvbs_100_set_voltage(struct dvb_frontend* fe,
				       fe_sec_voltage_t voltage)
{
	struct cx8802_dev *dev= fe->dvb->priv;
	struct cx88_core *core = dev->core;

	if (voltage == SEC_VOLTAGE_OFF)
		cx_write(MO_GP0_IO, 0x000006fb);
	else
		cx_write(MO_GP0_IO, 0x000006f9);

	if (core->prev_set_voltage)
		return core->prev_set_voltage(fe, voltage);
	return 0;
}

static int geniatech_dvbs_set_voltage(struct dvb_frontend *fe,
				      fe_sec_voltage_t voltage)
{
	struct cx8802_dev *dev= fe->dvb->priv;
	struct cx88_core *core = dev->core;

	if (voltage == SEC_VOLTAGE_OFF) {
		dprintk(1,"LNB Voltage OFF\n");
		cx_write(MO_GP0_IO, 0x0000efff);
	}

	if (core->prev_set_voltage)
		return core->prev_set_voltage(fe, voltage);
	return 0;
}

static int tevii_dvbs_set_voltage(struct dvb_frontend *fe,
				      fe_sec_voltage_t voltage)
{
	struct cx8802_dev *dev= fe->dvb->priv;
	struct cx88_core *core = dev->core;

	switch (voltage) {
		case SEC_VOLTAGE_13:
			printk("LNB Voltage SEC_VOLTAGE_13\n");
			cx_write(MO_GP0_IO, 0x00006040);
			break;
		case SEC_VOLTAGE_18:
			printk("LNB Voltage SEC_VOLTAGE_18\n");
			cx_write(MO_GP0_IO, 0x00006060);
			break;
		case SEC_VOLTAGE_OFF:
		   	printk("LNB Voltage SEC_VOLTAGE_off\n");
			break;
	}

	if (core->prev_set_voltage)
		return core->prev_set_voltage(fe, voltage);
	return 0;
}

static struct cx24123_config geniatech_dvbs_config = {
	.demod_address = 0x55,
	.set_ts_params = cx24123_set_ts_param,
};

static struct cx24123_config hauppauge_novas_config = {
	.demod_address = 0x55,
	.set_ts_params = cx24123_set_ts_param,
};

static struct cx24123_config kworld_dvbs_100_config = {
	.demod_address = 0x15,
	.set_ts_params = cx24123_set_ts_param,
	.lnb_polarity  = 1,
};

static struct s5h1409_config pinnacle_pctv_hd_800i_config = {
	.demod_address = 0x32 >> 1,
	.output_mode   = S5H1409_PARALLEL_OUTPUT,
	.gpio	       = S5H1409_GPIO_ON,
	.qam_if	       = 44000,
	.inversion     = S5H1409_INVERSION_OFF,
	.status_mode   = S5H1409_DEMODLOCKING,
	.mpeg_timing   = S5H1409_MPEGTIMING_NONCONTINOUS_NONINVERTING_CLOCK,
};

static struct s5h1409_config dvico_hdtv5_pci_nano_config = {
	.demod_address = 0x32 >> 1,
	.output_mode   = S5H1409_SERIAL_OUTPUT,
	.gpio          = S5H1409_GPIO_OFF,
	.inversion     = S5H1409_INVERSION_OFF,
	.status_mode   = S5H1409_DEMODLOCKING,
	.mpeg_timing   = S5H1409_MPEGTIMING_CONTINOUS_NONINVERTING_CLOCK,
};

static struct s5h1409_config kworld_atsc_120_config = {
	.demod_address = 0x32 >> 1,
	.output_mode   = S5H1409_SERIAL_OUTPUT,
	.gpio	       = S5H1409_GPIO_OFF,
	.inversion     = S5H1409_INVERSION_OFF,
	.status_mode   = S5H1409_DEMODLOCKING,
	.mpeg_timing   = S5H1409_MPEGTIMING_CONTINOUS_NONINVERTING_CLOCK,
};

static struct xc5000_config pinnacle_pctv_hd_800i_tuner_config = {
	.i2c_address	= 0x64,
	.if_khz		= 5380,
};

static struct zl10353_config cx88_pinnacle_hybrid_pctv = {
	.demod_address = (0x1e >> 1),
	.no_tuner      = 1,
	.if2           = 45600,
};

static struct zl10353_config cx88_geniatech_x8000_mt = {
       .demod_address = (0x1e >> 1),
       .no_tuner = 1,
};

static struct s5h1411_config dvico_fusionhdtv7_config = {
	.output_mode   = S5H1411_SERIAL_OUTPUT,
	.gpio          = S5H1411_GPIO_ON,
	.mpeg_timing   = S5H1411_MPEGTIMING_CONTINOUS_NONINVERTING_CLOCK,
	.qam_if        = S5H1411_IF_44000,
	.vsb_if        = S5H1411_IF_44000,
	.inversion     = S5H1411_INVERSION_OFF,
	.status_mode   = S5H1411_DEMODLOCKING
};

static struct xc5000_config dvico_fusionhdtv7_tuner_config = {
	.i2c_address    = 0xc2 >> 1,
	.if_khz         = 5380,
};

static int attach_xc3028(u8 addr, struct cx8802_dev *dev)
{
	struct dvb_frontend *fe;
	struct videobuf_dvb_frontend *fe0 = NULL;
	struct xc2028_ctrl ctl;
	struct xc2028_config cfg = {
		.i2c_adap  = &dev->core->i2c_adap,
		.i2c_addr  = addr,
		.ctrl      = &ctl,
	};

	/* Get the first frontend */
	fe0 = videobuf_dvb_get_frontend(&dev->frontends, 1);
	if (!fe0)
		return -EINVAL;

	if (!fe0->dvb.frontend) {
		printk(KERN_ERR "%s/2: dvb frontend not attached. "
				"Can't attach xc3028\n",
		       dev->core->name);
		return -EINVAL;
	}

	/*
	 * Some xc3028 devices may be hidden by an I2C gate. This is known
	 * to happen with some s5h1409-based devices.
	 * Now that I2C gate is open, sets up xc3028 configuration
	 */
	cx88_setup_xc3028(dev->core, &ctl);

	fe = dvb_attach(xc2028_attach, fe0->dvb.frontend, &cfg);
	if (!fe) {
		printk(KERN_ERR "%s/2: xc3028 attach failed\n",
		       dev->core->name);
		dvb_frontend_detach(fe0->dvb.frontend);
		dvb_unregister_frontend(fe0->dvb.frontend);
		fe0->dvb.frontend = NULL;
		return -EINVAL;
	}

	printk(KERN_INFO "%s/2: xc3028 attached\n",
	       dev->core->name);

	return 0;
}

static int cx24116_set_ts_param(struct dvb_frontend *fe,
	int is_punctured)
{
	struct cx8802_dev *dev = fe->dvb->priv;
	dev->ts_gen_cntrl = 0x2;

	return 0;
}

static int cx24116_reset_device(struct dvb_frontend *fe)
{
	struct cx8802_dev *dev = fe->dvb->priv;
	struct cx88_core *core = dev->core;

	/* Reset the part */
	/* Put the cx24116 into reset */
	cx_write(MO_SRST_IO, 0);
	msleep(10);
	/* Take the cx24116 out of reset */
	cx_write(MO_SRST_IO, 1);
	msleep(10);

	return 0;
}

static struct cx24116_config hauppauge_hvr4000_config = {
	.demod_address          = 0x05,
	.set_ts_params          = cx24116_set_ts_param,
	.reset_device           = cx24116_reset_device,
};

static struct cx24116_config tevii_s460_config = {
	.demod_address = 0x55,
	.set_ts_params = cx24116_set_ts_param,
	.reset_device  = cx24116_reset_device,
};

static struct stv0299_config tevii_tuner_sharp_config = {
	.demod_address = 0x68,
	.inittab = sharp_z0194a_inittab,
	.mclk = 88000000UL,
	.invert = 1,
	.skip_reinit = 0,
	.lock_output = 1,
	.volt13_op0_op1 = STV0299_VOLT13_OP1,
	.min_delay_ms = 100,
	.set_symbol_rate = sharp_z0194a_set_symbol_rate,
	.set_ts_params = cx24116_set_ts_param,
};

static struct stv0288_config tevii_tuner_earda_config = {
	.demod_address = 0x68,
	.min_delay_ms = 100,
	.set_ts_params = cx24116_set_ts_param,
};

static int dvb_register(struct cx8802_dev *dev)
{
	struct cx88_core *core = dev->core;
	struct videobuf_dvb_frontend *fe0, *fe1 = NULL;
	int mfe_shared = 0; /* bus not shared by default */

	/* Get the first frontend */
	fe0 = videobuf_dvb_get_frontend(&dev->frontends, 1);
	if (!fe0)
		return -EINVAL;

	/* multi-frontend gate control is undefined or defaults to fe0 */
	dev->frontends.gate = 0;

	/* init frontend(s) */
	switch (core->boardnr) {
	case CX88_BOARD_HAUPPAUGE_DVB_T1:
		fe0->dvb.frontend = dvb_attach(cx22702_attach,
					       &connexant_refboard_config,
					       &core->i2c_adap);
		if (fe0->dvb.frontend != NULL) {
			if (!dvb_attach(dvb_pll_attach, fe0->dvb.frontend,
					0x61, &core->i2c_adap,
					DVB_PLL_THOMSON_DTT759X))
				goto frontend_detach;
		}
		break;
	case CX88_BOARD_TERRATEC_CINERGY_1400_DVB_T1:
	case CX88_BOARD_CONEXANT_DVB_T1:
	case CX88_BOARD_KWORLD_DVB_T_CX22702:
	case CX88_BOARD_WINFAST_DTV1000:
		fe0->dvb.frontend = dvb_attach(cx22702_attach,
					       &connexant_refboard_config,
					       &core->i2c_adap);
		if (fe0->dvb.frontend != NULL) {
			if (!dvb_attach(dvb_pll_attach, fe0->dvb.frontend,
					0x60, &core->i2c_adap,
					DVB_PLL_THOMSON_DTT7579))
				goto frontend_detach;
		}
		break;
	case CX88_BOARD_WINFAST_DTV2000H:
	case CX88_BOARD_HAUPPAUGE_HVR1100:
	case CX88_BOARD_HAUPPAUGE_HVR1100LP:
	case CX88_BOARD_HAUPPAUGE_HVR1300:
		fe0->dvb.frontend = dvb_attach(cx22702_attach,
					       &hauppauge_hvr_config,
					       &core->i2c_adap);
		if (fe0->dvb.frontend != NULL) {
			if (!dvb_attach(simple_tuner_attach, fe0->dvb.frontend,
				   &core->i2c_adap, 0x61,
				   TUNER_PHILIPS_FMD1216ME_MK3))
				goto frontend_detach;
		}
		break;
	case CX88_BOARD_HAUPPAUGE_HVR3000:
		/* DVB-S init */
		fe0->dvb.frontend = dvb_attach(cx24123_attach,
			       &hauppauge_novas_config,
			       &dev->core->i2c_adap);
		if (fe0->dvb.frontend) {
			if (!dvb_attach(isl6421_attach, fe0->dvb.frontend,
			&dev->core->i2c_adap, 0x08, ISL6421_DCL, 0x00)) {
				dprintk( 1, "%s(): HVR3000 - DVB-S LNB Init: failed\n", __func__);
			}
		} else {
			dprintk( 1, "%s(): HVR3000 - DVB-S Init: failed\n", __func__);
		}
		/* DVB-T init */
		fe1 = videobuf_dvb_get_frontend(&dev->frontends, 2);
		if (fe1) {
			dev->frontends.gate = 2;
			mfe_shared = 1;
			fe1->dvb.frontend = dvb_attach(cx22702_attach,
				&hauppauge_hvr_config,
				&dev->core->i2c_adap);
			if (fe1->dvb.frontend) {
				fe1->dvb.frontend->id = 1;
				if(!dvb_attach(simple_tuner_attach, fe1->dvb.frontend,
						&dev->core->i2c_adap, 0x61,
						TUNER_PHILIPS_FMD1216ME_MK3)) {
					dprintk( 1, "%s(): HVR3000 - DVB-T misc Init: failed\n", __func__);
				}
			} else {
				dprintk( 1, "%s(): HVR3000 - DVB-T Init: failed\n", __func__);
			}
		} else {
			dprintk( 1, "%s(): HVR3000 - DVB-T Init: can't find frontend 2.\n", __func__);
		}
		break;
	case CX88_BOARD_DVICO_FUSIONHDTV_DVB_T_PLUS:
		fe0->dvb.frontend = dvb_attach(mt352_attach,
					       &dvico_fusionhdtv,
					       &core->i2c_adap);
		if (fe0->dvb.frontend != NULL) {
			if (!dvb_attach(dvb_pll_attach, fe0->dvb.frontend,
					0x60, NULL, DVB_PLL_THOMSON_DTT7579))
				goto frontend_detach;
			break;
		}
		/* ZL10353 replaces MT352 on later cards */
		fe0->dvb.frontend = dvb_attach(zl10353_attach,
					       &dvico_fusionhdtv_plus_v1_1,
					       &core->i2c_adap);
		if (fe0->dvb.frontend != NULL) {
			if (!dvb_attach(dvb_pll_attach, fe0->dvb.frontend,
					0x60, NULL, DVB_PLL_THOMSON_DTT7579))
				goto frontend_detach;
		}
		break;
	case CX88_BOARD_DVICO_FUSIONHDTV_DVB_T_DUAL:
		/* The tin box says DEE1601, but it seems to be DTT7579
		 * compatible, with a slightly different MT352 AGC gain. */
		fe0->dvb.frontend = dvb_attach(mt352_attach,
					       &dvico_fusionhdtv_dual,
					       &core->i2c_adap);
		if (fe0->dvb.frontend != NULL) {
			if (!dvb_attach(dvb_pll_attach, fe0->dvb.frontend,
					0x61, NULL, DVB_PLL_THOMSON_DTT7579))
				goto frontend_detach;
			break;
		}
		/* ZL10353 replaces MT352 on later cards */
		fe0->dvb.frontend = dvb_attach(zl10353_attach,
					       &dvico_fusionhdtv_plus_v1_1,
					       &core->i2c_adap);
		if (fe0->dvb.frontend != NULL) {
			if (!dvb_attach(dvb_pll_attach, fe0->dvb.frontend,
					0x61, NULL, DVB_PLL_THOMSON_DTT7579))
				goto frontend_detach;
		}
		break;
	case CX88_BOARD_DVICO_FUSIONHDTV_DVB_T1:
		fe0->dvb.frontend = dvb_attach(mt352_attach,
					       &dvico_fusionhdtv,
					       &core->i2c_adap);
		if (fe0->dvb.frontend != NULL) {
			if (!dvb_attach(dvb_pll_attach, fe0->dvb.frontend,
					0x61, NULL, DVB_PLL_LG_Z201))
				goto frontend_detach;
		}
		break;
	case CX88_BOARD_KWORLD_DVB_T:
	case CX88_BOARD_DNTV_LIVE_DVB_T:
	case CX88_BOARD_ADSTECH_DVB_T_PCI:
		fe0->dvb.frontend = dvb_attach(mt352_attach,
					       &dntv_live_dvbt_config,
					       &core->i2c_adap);
		if (fe0->dvb.frontend != NULL) {
			if (!dvb_attach(dvb_pll_attach, fe0->dvb.frontend,
					0x61, NULL, DVB_PLL_UNKNOWN_1))
				goto frontend_detach;
		}
		break;
	case CX88_BOARD_DNTV_LIVE_DVB_T_PRO:
#if defined(CONFIG_VIDEO_CX88_VP3054) || (defined(CONFIG_VIDEO_CX88_VP3054_MODULE) && defined(MODULE))
		/* MT352 is on a secondary I2C bus made from some GPIO lines */
		fe0->dvb.frontend = dvb_attach(mt352_attach, &dntv_live_dvbt_pro_config,
					       &dev->vp3054->adap);
		if (fe0->dvb.frontend != NULL) {
			if (!dvb_attach(simple_tuner_attach, fe0->dvb.frontend,
					&core->i2c_adap, 0x61,
					TUNER_PHILIPS_FMD1216ME_MK3))
				goto frontend_detach;
		}
#else
		printk(KERN_ERR "%s/2: built without vp3054 support\n",
				core->name);
#endif
		break;
	case CX88_BOARD_DVICO_FUSIONHDTV_DVB_T_HYBRID:
		fe0->dvb.frontend = dvb_attach(zl10353_attach,
					       &dvico_fusionhdtv_hybrid,
					       &core->i2c_adap);
		if (fe0->dvb.frontend != NULL) {
			if (!dvb_attach(simple_tuner_attach, fe0->dvb.frontend,
				   &core->i2c_adap, 0x61,
				   TUNER_THOMSON_FE6600))
				goto frontend_detach;
		}
		break;
	case CX88_BOARD_DVICO_FUSIONHDTV_DVB_T_PRO:
		fe0->dvb.frontend = dvb_attach(zl10353_attach,
					       &dvico_fusionhdtv_xc3028,
					       &core->i2c_adap);
		if (fe0->dvb.frontend == NULL)
			fe0->dvb.frontend = dvb_attach(mt352_attach,
						&dvico_fusionhdtv_mt352_xc3028,
						&core->i2c_adap);
		/*
		 * On this board, the demod provides the I2C bus pullup.
		 * We must not permit gate_ctrl to be performed, or
		 * the xc3028 cannot communicate on the bus.
		 */
		if (fe0->dvb.frontend)
			fe0->dvb.frontend->ops.i2c_gate_ctrl = NULL;
		if (attach_xc3028(0x61, dev) < 0)
			goto frontend_detach;
		break;
	case CX88_BOARD_PCHDTV_HD3000:
		fe0->dvb.frontend = dvb_attach(or51132_attach, &pchdtv_hd3000,
					       &core->i2c_adap);
		if (fe0->dvb.frontend != NULL) {
			if (!dvb_attach(simple_tuner_attach, fe0->dvb.frontend,
					&core->i2c_adap, 0x61,
					TUNER_THOMSON_DTT761X))
				goto frontend_detach;
		}
		break;
	case CX88_BOARD_DVICO_FUSIONHDTV_3_GOLD_Q:
		dev->ts_gen_cntrl = 0x08;

		/* Do a hardware reset of chip before using it. */
		cx_clear(MO_GP0_IO, 1);
		mdelay(100);
		cx_set(MO_GP0_IO, 1);
		mdelay(200);

		/* Select RF connector callback */
		fusionhdtv_3_gold.pll_rf_set = lgdt330x_pll_rf_set;
		fe0->dvb.frontend = dvb_attach(lgdt330x_attach,
					       &fusionhdtv_3_gold,
					       &core->i2c_adap);
		if (fe0->dvb.frontend != NULL) {
			if (!dvb_attach(simple_tuner_attach, fe0->dvb.frontend,
					&core->i2c_adap, 0x61,
					TUNER_MICROTUNE_4042FI5))
				goto frontend_detach;
		}
		break;
	case CX88_BOARD_DVICO_FUSIONHDTV_3_GOLD_T:
		dev->ts_gen_cntrl = 0x08;

		/* Do a hardware reset of chip before using it. */
		cx_clear(MO_GP0_IO, 1);
		mdelay(100);
		cx_set(MO_GP0_IO, 9);
		mdelay(200);
		fe0->dvb.frontend = dvb_attach(lgdt330x_attach,
					       &fusionhdtv_3_gold,
					       &core->i2c_adap);
		if (fe0->dvb.frontend != NULL) {
			if (!dvb_attach(simple_tuner_attach, fe0->dvb.frontend,
					&core->i2c_adap, 0x61,
					TUNER_THOMSON_DTT761X))
				goto frontend_detach;
		}
		break;
	case CX88_BOARD_DVICO_FUSIONHDTV_5_GOLD:
		dev->ts_gen_cntrl = 0x08;

		/* Do a hardware reset of chip before using it. */
		cx_clear(MO_GP0_IO, 1);
		mdelay(100);
		cx_set(MO_GP0_IO, 1);
		mdelay(200);
		fe0->dvb.frontend = dvb_attach(lgdt330x_attach,
					       &fusionhdtv_5_gold,
					       &core->i2c_adap);
		if (fe0->dvb.frontend != NULL) {
			if (!dvb_attach(simple_tuner_attach, fe0->dvb.frontend,
					&core->i2c_adap, 0x61,
					TUNER_LG_TDVS_H06XF))
				goto frontend_detach;
			if (!dvb_attach(tda9887_attach, fe0->dvb.frontend,
				   &core->i2c_adap, 0x43))
				goto frontend_detach;
		}
		break;
	case CX88_BOARD_PCHDTV_HD5500:
		dev->ts_gen_cntrl = 0x08;

		/* Do a hardware reset of chip before using it. */
		cx_clear(MO_GP0_IO, 1);
		mdelay(100);
		cx_set(MO_GP0_IO, 1);
		mdelay(200);
		fe0->dvb.frontend = dvb_attach(lgdt330x_attach,
					       &pchdtv_hd5500,
					       &core->i2c_adap);
		if (fe0->dvb.frontend != NULL) {
			if (!dvb_attach(simple_tuner_attach, fe0->dvb.frontend,
					&core->i2c_adap, 0x61,
					TUNER_LG_TDVS_H06XF))
				goto frontend_detach;
			if (!dvb_attach(tda9887_attach, fe0->dvb.frontend,
				   &core->i2c_adap, 0x43))
				goto frontend_detach;
		}
		break;
	case CX88_BOARD_ATI_HDTVWONDER:
		fe0->dvb.frontend = dvb_attach(nxt200x_attach,
					       &ati_hdtvwonder,
					       &core->i2c_adap);
		if (fe0->dvb.frontend != NULL) {
			if (!dvb_attach(simple_tuner_attach, fe0->dvb.frontend,
					&core->i2c_adap, 0x61,
					TUNER_PHILIPS_TUV1236D))
				goto frontend_detach;
		}
		break;
	case CX88_BOARD_HAUPPAUGE_NOVASPLUS_S1:
	case CX88_BOARD_HAUPPAUGE_NOVASE2_S1:
		fe0->dvb.frontend = dvb_attach(cx24123_attach,
					       &hauppauge_novas_config,
					       &core->i2c_adap);
		if (fe0->dvb.frontend) {
			if (!dvb_attach(isl6421_attach, fe0->dvb.frontend,
					&core->i2c_adap, 0x08, ISL6421_DCL, 0x00))
				goto frontend_detach;
		}
		break;
	case CX88_BOARD_KWORLD_DVBS_100:
		fe0->dvb.frontend = dvb_attach(cx24123_attach,
					       &kworld_dvbs_100_config,
					       &core->i2c_adap);
		if (fe0->dvb.frontend) {
			core->prev_set_voltage = fe0->dvb.frontend->ops.set_voltage;
			fe0->dvb.frontend->ops.set_voltage = kworld_dvbs_100_set_voltage;
		}
		break;
	case CX88_BOARD_GENIATECH_DVBS:
		fe0->dvb.frontend = dvb_attach(cx24123_attach,
					       &geniatech_dvbs_config,
					       &core->i2c_adap);
		if (fe0->dvb.frontend) {
			core->prev_set_voltage = fe0->dvb.frontend->ops.set_voltage;
			fe0->dvb.frontend->ops.set_voltage = geniatech_dvbs_set_voltage;
		}
		break;
	case CX88_BOARD_PINNACLE_PCTV_HD_800i:
		fe0->dvb.frontend = dvb_attach(s5h1409_attach,
					       &pinnacle_pctv_hd_800i_config,
					       &core->i2c_adap);
		if (fe0->dvb.frontend != NULL) {
			if (!dvb_attach(xc5000_attach, fe0->dvb.frontend,
					&core->i2c_adap,
					&pinnacle_pctv_hd_800i_tuner_config))
				goto frontend_detach;
		}
		break;
	case CX88_BOARD_DVICO_FUSIONHDTV_5_PCI_NANO:
		fe0->dvb.frontend = dvb_attach(s5h1409_attach,
						&dvico_hdtv5_pci_nano_config,
						&core->i2c_adap);
		if (fe0->dvb.frontend != NULL) {
			struct dvb_frontend *fe;
			struct xc2028_config cfg = {
				.i2c_adap  = &core->i2c_adap,
				.i2c_addr  = 0x61,
			};
			static struct xc2028_ctrl ctl = {
				.fname       = XC2028_DEFAULT_FIRMWARE,
				.max_len     = 64,
				.scode_table = XC3028_FE_OREN538,
			};

			fe = dvb_attach(xc2028_attach,
					fe0->dvb.frontend, &cfg);
			if (fe != NULL && fe->ops.tuner_ops.set_config != NULL)
				fe->ops.tuner_ops.set_config(fe, &ctl);
		}
		break;
	 case CX88_BOARD_PINNACLE_HYBRID_PCTV:
		fe0->dvb.frontend = dvb_attach(zl10353_attach,
					       &cx88_pinnacle_hybrid_pctv,
					       &core->i2c_adap);
		if (fe0->dvb.frontend) {
			fe0->dvb.frontend->ops.i2c_gate_ctrl = NULL;
			if (attach_xc3028(0x61, dev) < 0)
				goto frontend_detach;
		}
		break;
	 case CX88_BOARD_GENIATECH_X8000_MT:
		dev->ts_gen_cntrl = 0x00;

		fe0->dvb.frontend = dvb_attach(zl10353_attach,
					       &cx88_geniatech_x8000_mt,
					       &core->i2c_adap);
		if (attach_xc3028(0x61, dev) < 0)
			goto frontend_detach;
		break;
	 case CX88_BOARD_KWORLD_ATSC_120:
		fe0->dvb.frontend = dvb_attach(s5h1409_attach,
					       &kworld_atsc_120_config,
					       &core->i2c_adap);
		if (attach_xc3028(0x61, dev) < 0)
			goto frontend_detach;
		break;
	case CX88_BOARD_DVICO_FUSIONHDTV_7_GOLD:
		fe0->dvb.frontend = dvb_attach(s5h1411_attach,
					       &dvico_fusionhdtv7_config,
					       &core->i2c_adap);
		if (fe0->dvb.frontend != NULL) {
			if (!dvb_attach(xc5000_attach, fe0->dvb.frontend,
					&core->i2c_adap,
					&dvico_fusionhdtv7_tuner_config))
				goto frontend_detach;
		}
		break;
	case CX88_BOARD_HAUPPAUGE_HVR4000:
		/* DVB-S/S2 Init */
		fe0->dvb.frontend = dvb_attach(cx24116_attach,
			&hauppauge_hvr4000_config,
			&dev->core->i2c_adap);
		if (fe0->dvb.frontend) {
			if(!dvb_attach(isl6421_attach, fe0->dvb.frontend,
				&dev->core->i2c_adap, 0x08, ISL6421_DCL, 0x00)) {
				dprintk( 1, "%s(): HVR4000 - DVB-S LNB Init: failed\n", __func__);
			}
		} else {
			dprintk( 1, "%s(): HVR4000 - DVB-S Init: failed\n", __func__);
		}
		/* DVB-T Init */
		fe1 = videobuf_dvb_get_frontend(&dev->frontends, 2);
		if (fe1) {
			dev->frontends.gate = 2;
			mfe_shared = 1;
			fe1->dvb.frontend = dvb_attach(cx22702_attach,
				&hauppauge_hvr_config,
				&dev->core->i2c_adap);
			if (fe1->dvb.frontend) {
				fe1->dvb.frontend->id = 1;
				if(!dvb_attach(simple_tuner_attach, fe1->dvb.frontend,
					&dev->core->i2c_adap, 0x61,
					TUNER_PHILIPS_FMD1216ME_MK3)) {
					dprintk( 1, "%s(): HVR4000 - DVB-T misc Init: failed\n", __func__);
				}
			} else {
				dprintk( 1, "%s(): HVR4000 - DVB-T Init: failed\n", __func__);
			}
		} else {
			dprintk( 1, "%s(): HVR4000 - DVB-T Init: can't find frontend 2.\n", __func__);
		}
		break;
	case CX88_BOARD_HAUPPAUGE_HVR4000LITE:
		fe0->dvb.frontend = dvb_attach(cx24116_attach,
			&hauppauge_hvr4000_config,
			&dev->core->i2c_adap);
		if (fe0->dvb.frontend) {
			dvb_attach(isl6421_attach, fe0->dvb.frontend,
				&dev->core->i2c_adap,
				0x08, ISL6421_DCL, 0x00);
		}
		break;
	case CX88_BOARD_TEVII_S420:
		fe0->dvb.frontend = dvb_attach(stv0299_attach,
						&tevii_tuner_sharp_config,
						&core->i2c_adap);
		if (fe0->dvb.frontend != NULL) {
			if (!dvb_attach(dvb_pll_attach, fe0->dvb.frontend, 0x60,
					&core->i2c_adap, DVB_PLL_OPERA1))
				goto frontend_detach;
			core->prev_set_voltage = fe0->dvb.frontend->ops.set_voltage;
			fe0->dvb.frontend->ops.set_voltage = tevii_dvbs_set_voltage;

		} else {
			fe0->dvb.frontend = dvb_attach(stv0288_attach,
							    &tevii_tuner_earda_config,
							    &core->i2c_adap);
				if (fe0->dvb.frontend != NULL) {
					if (!dvb_attach(stb6000_attach, fe0->dvb.frontend, 0x61,
						&core->i2c_adap))
					goto frontend_detach;
				core->prev_set_voltage = fe0->dvb.frontend->ops.set_voltage;
				fe0->dvb.frontend->ops.set_voltage = tevii_dvbs_set_voltage;
			}
		}
		break;
	case CX88_BOARD_TEVII_S460:
		fe0->dvb.frontend = dvb_attach(cx24116_attach,
					       &tevii_s460_config,
					       &core->i2c_adap);
		if (fe0->dvb.frontend != NULL) {
			core->prev_set_voltage = fe0->dvb.frontend->ops.set_voltage;
			fe0->dvb.frontend->ops.set_voltage = tevii_dvbs_set_voltage;
		}
		break;
	case CX88_BOARD_OMICOM_SS4_PCI:
	case CX88_BOARD_TBS_8920:
	case CX88_BOARD_PROF_7300:
		fe0->dvb.frontend = dvb_attach(cx24116_attach,
					       &hauppauge_hvr4000_config,
					       &core->i2c_adap);
		if (fe0->dvb.frontend != NULL) {
			core->prev_set_voltage = fe0->dvb.frontend->ops.set_voltage;
			fe0->dvb.frontend->ops.set_voltage = tevii_dvbs_set_voltage;
		}
		break;
	default:
		printk(KERN_ERR "%s/2: The frontend of your DVB/ATSC card isn't supported yet\n",
		       core->name);
		break;
	}

        if ( (NULL == fe0->dvb.frontend) || (fe1 && NULL == fe1->dvb.frontend) ) {
		printk(KERN_ERR
		       "%s/2: frontend initialization failed\n",
		       core->name);
		return -EINVAL;
	}
	/* define general-purpose callback pointer */
	fe0->dvb.frontend->callback = cx88_tuner_callback;

	/* Ensure all frontends negotiate bus access */
	fe0->dvb.frontend->ops.ts_bus_ctrl = cx88_dvb_bus_ctrl;
	if (fe1)
		fe1->dvb.frontend->ops.ts_bus_ctrl = cx88_dvb_bus_ctrl;

	/* Put the analog decoder in standby to keep it quiet */
	cx88_call_i2c_clients(core, TUNER_SET_STANDBY, NULL);

	/* register everything */
	return videobuf_dvb_register_bus(&dev->frontends, THIS_MODULE, dev,
		&dev->pci->dev, adapter_nr, mfe_shared);

frontend_detach:
	videobuf_dvb_dealloc_frontends(&dev->frontends);
	return -EINVAL;
}

/* ----------------------------------------------------------- */

/* CX8802 MPEG -> mini driver - We have been given the hardware */
static int cx8802_dvb_advise_acquire(struct cx8802_driver *drv)
{
	struct cx88_core *core = drv->core;
	int err = 0;
	dprintk( 1, "%s\n", __func__);

	switch (core->boardnr) {
	case CX88_BOARD_HAUPPAUGE_HVR1300:
		/* We arrive here with either the cx23416 or the cx22702
		 * on the bus. Take the bus from the cx23416 and enable the
		 * cx22702 demod
		 */
		cx_set(MO_GP0_IO,   0x00000080); /* cx22702 out of reset and enable */
		cx_clear(MO_GP0_IO, 0x00000004);
		udelay(1000);
		break;

	case CX88_BOARD_HAUPPAUGE_HVR3000:
	case CX88_BOARD_HAUPPAUGE_HVR4000:
		if(core->dvbdev->frontends.active_fe_id == 1) {
			/* DVB-S/S2 Enabled */

			/* Toggle reset on cx22702 leaving i2c active */
			cx_write(MO_GP0_IO, (core->board.input[0].gpio0 & 0x0000ff00) | 0x00000080);
			udelay(1000);
			cx_clear(MO_GP0_IO, 0x00000080);
			udelay(50);
			cx_set(MO_GP0_IO, 0x00000080); /* cx22702 out of reset */
			cx_set(MO_GP0_IO, 0x00000004); /* tri-state the cx22702 pins */
			udelay(1000);

			cx_write(MO_SRST_IO, 1); /* Take the cx24116/cx24123 out of reset */
			core->dvbdev->ts_gen_cntrl = 0x02; /* Parallel IO */
		} else
		if (core->dvbdev->frontends.active_fe_id == 2) {
			/* DVB-T Enabled */

			/* Put the cx24116/cx24123 into reset */
			cx_write(MO_SRST_IO, 0);

			/* cx22702 out of reset and enable it */
			cx_set(MO_GP0_IO,   0x00000080);
			cx_clear(MO_GP0_IO, 0x00000004);
			core->dvbdev->ts_gen_cntrl = 0x0c; /* Serial IO */
			udelay(1000);
		}
		break;

	default:
		err = -ENODEV;
	}
	return err;
}

/* CX8802 MPEG -> mini driver - We no longer have the hardware */
static int cx8802_dvb_advise_release(struct cx8802_driver *drv)
{
	struct cx88_core *core = drv->core;
	int err = 0;
	dprintk( 1, "%s\n", __func__);

	switch (core->boardnr) {
	case CX88_BOARD_HAUPPAUGE_HVR1300:
		/* Do Nothing, leave the cx22702 on the bus. */
		break;
	case CX88_BOARD_HAUPPAUGE_HVR3000:
	case CX88_BOARD_HAUPPAUGE_HVR4000:
		break;
	default:
		err = -ENODEV;
	}
	return err;
}

static int cx8802_dvb_probe(struct cx8802_driver *drv)
{
	struct cx88_core *core = drv->core;
	struct cx8802_dev *dev = drv->core->dvbdev;
	int err, i;
	struct videobuf_dvb_frontend *fe;

	dprintk( 1, "%s\n", __func__);
	dprintk( 1, " ->being probed by Card=%d Name=%s, PCI %02x:%02x\n",
		core->boardnr,
		core->name,
		core->pci_bus,
		core->pci_slot);

	err = -ENODEV;
	if (!(core->board.mpeg & CX88_MPEG_DVB))
		goto fail_core;

	/* If vp3054 isn't enabled, a stub will just return 0 */
	err = vp3054_i2c_probe(dev);
	if (0 != err)
		goto fail_core;

	/* dvb stuff */
	printk(KERN_INFO "%s/2: cx2388x based DVB/ATSC card\n", core->name);
	dev->ts_gen_cntrl = 0x0c;

	for (i = 1; i <= core->board.num_frontends; i++) {
		fe = videobuf_dvb_get_frontend(&core->dvbdev->frontends, i);
		if (!fe) {
			printk(KERN_ERR "%s() failed to get frontend(%d)\n", __func__, i);
			continue;
		}
		videobuf_queue_sg_init(&fe->dvb.dvbq, &dvb_qops,
			    &dev->pci->dev, &dev->slock,
			    V4L2_BUF_TYPE_VIDEO_CAPTURE,
			    V4L2_FIELD_TOP,
			    sizeof(struct cx88_buffer),
			    dev);
		/* init struct videobuf_dvb */
		fe->dvb.name = dev->core->name;
	}
	err = dvb_register(dev);
	if (err != 0)
		printk(KERN_ERR "%s/2: dvb_register failed (err = %d)\n",
		       core->name, err);
fail_core:
	return err;
}

static int cx8802_dvb_remove(struct cx8802_driver *drv)
{
	struct cx88_core *core = drv->core;
	struct cx8802_dev *dev = drv->core->dvbdev;

	dprintk( 1, "%s\n", __func__);

	videobuf_dvb_unregister_bus(&dev->frontends);

	vp3054_i2c_remove(dev);

	return 0;
}

static struct cx8802_driver cx8802_dvb_driver = {
	.type_id        = CX88_MPEG_DVB,
	.hw_access      = CX8802_DRVCTL_SHARED,
	.probe          = cx8802_dvb_probe,
	.remove         = cx8802_dvb_remove,
	.advise_acquire = cx8802_dvb_advise_acquire,
	.advise_release = cx8802_dvb_advise_release,
};

static int dvb_init(void)
{
	printk(KERN_INFO "cx88/2: cx2388x dvb driver version %d.%d.%d loaded\n",
	       (CX88_VERSION_CODE >> 16) & 0xff,
	       (CX88_VERSION_CODE >>  8) & 0xff,
	       CX88_VERSION_CODE & 0xff);
#ifdef SNAPSHOT
	printk(KERN_INFO "cx2388x: snapshot date %04d-%02d-%02d\n",
	       SNAPSHOT/10000, (SNAPSHOT/100)%100, SNAPSHOT%100);
#endif
	return cx8802_register_driver(&cx8802_dvb_driver);
}

static void dvb_fini(void)
{
	cx8802_unregister_driver(&cx8802_dvb_driver);
}

module_init(dvb_init);
module_exit(dvb_fini);

/*
 * Local variables:
 * c-basic-offset: 8
 * compile-command: "make DVB=1"
 * End:
 */
