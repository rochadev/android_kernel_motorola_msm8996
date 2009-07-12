/* dvb-usb-ids.h is part of the DVB USB library.
 *
 * Copyright (C) 2004-5 Patrick Boettcher (patrick.boettcher@desy.de) see
 * dvb-usb-init.c for copyright information.
 *
 * a header file containing define's for the USB device supported by the
 * various drivers.
 */
#ifndef _DVB_USB_IDS_H_
#define _DVB_USB_IDS_H_

/* Vendor IDs */
#define USB_VID_ADSTECH				0x06e1
#define USB_VID_AFATECH				0x15a4
#define USB_VID_ALCOR_MICRO			0x058f
#define USB_VID_ALINK				0x05e3
#define USB_VID_AMT				0x1c73
#define USB_VID_ANCHOR				0x0547
#define USB_VID_ANSONIC				0x10b9
#define USB_VID_ANUBIS_ELECTRONIC		0x10fd
#define USB_VID_ASUS				0x0b05
#define USB_VID_AVERMEDIA			0x07ca
#define USB_VID_COMPRO				0x185b
#define USB_VID_COMPRO_UNK			0x145f
#define USB_VID_CONEXANT			0x0572
#define USB_VID_CYPRESS				0x04b4
#define USB_VID_DIBCOM				0x10b8
#define USB_VID_DPOSH				0x1498
#define USB_VID_DVICO				0x0fe9
#define USB_VID_ELGATO				0x0fd9
#define USB_VID_EMPIA				0xeb1a
#define USB_VID_GENPIX				0x09c0
#define USB_VID_GRANDTEC			0x5032
#define USB_VID_HANFTEK				0x15f4
#define USB_VID_HAUPPAUGE			0x2040
#define USB_VID_HYPER_PALTEK			0x1025
#define USB_VID_INTEL				0x8086
#define USB_VID_KWORLD				0xeb2a
#define USB_VID_KWORLD_2			0x1b80
#define USB_VID_KYE				0x0458
#define USB_VID_LEADTEK				0x0413
#define USB_VID_LITEON				0x04ca
#define USB_VID_MEDION				0x1660
#define USB_VID_MIGLIA				0x18f3
#define USB_VID_MSI				0x0db0
#define USB_VID_MSI_2				0x1462
#define USB_VID_OPERA1				0x695c
#define USB_VID_PINNACLE			0x2304
#define USB_VID_TECHNOTREND			0x0b48
#define USB_VID_TERRATEC			0x0ccd
#define USB_VID_TELESTAR			0x10b9
#define USB_VID_VISIONPLUS			0x13d3
#define USB_VID_SONY				0x1415
#define USB_VID_TWINHAN				0x1822
#define USB_VID_ULTIMA_ELECTRONIC		0x05d8
#define USB_VID_UNIWILL				0x1584
#define USB_VID_WIDEVIEW			0x14aa
#define USB_VID_GIGABYTE			0x1044
#define USB_VID_YUAN				0x1164
#define USB_VID_XTENSIONS			0x1ae7
#define USB_VID_HUMAX_COEX			0x10b9

/* Product IDs */
#define USB_PID_ADSTECH_USB2_COLD			0xa333
#define USB_PID_ADSTECH_USB2_WARM			0xa334
#define USB_PID_AFATECH_AF9005				0x9020
#define USB_PID_AFATECH_AF9015_9015			0x9015
#define USB_PID_AFATECH_AF9015_9016			0x9016
#define USB_PID_TREKSTOR_DVBT				0x901b
#define USB_VID_ALINK_DTU				0xf170
#define USB_PID_ANSONIC_DVBT_USB			0x6000
#define USB_PID_ANYSEE					0x861f
#define USB_PID_AZUREWAVE_AD_TU700			0x3237
#define USB_PID_AVERMEDIA_DVBT_USB_COLD			0x0001
#define USB_PID_AVERMEDIA_DVBT_USB_WARM			0x0002
#define USB_PID_AVERMEDIA_DVBT_USB2_COLD		0xa800
#define USB_PID_AVERMEDIA_DVBT_USB2_WARM		0xa801
#define USB_PID_COMPRO_DVBU2000_COLD			0xd000
#define USB_PID_COMPRO_DVBU2000_WARM			0xd001
#define USB_PID_COMPRO_DVBU2000_UNK_COLD		0x010c
#define USB_PID_COMPRO_DVBU2000_UNK_WARM		0x010d
#define USB_PID_COMPRO_VIDEOMATE_U500			0x1e78
#define USB_PID_COMPRO_VIDEOMATE_U500_PC		0x1e80
#define USB_PID_CONCEPTRONIC_CTVDIGRCU			0xe397
#define USB_PID_CONEXANT_D680_DMB			0x86d6
#define USB_PID_DIBCOM_HOOK_DEFAULT			0x0064
#define USB_PID_DIBCOM_HOOK_DEFAULT_REENUM		0x0065
#define USB_PID_DIBCOM_MOD3000_COLD			0x0bb8
#define USB_PID_DIBCOM_MOD3000_WARM			0x0bb9
#define USB_PID_DIBCOM_MOD3001_COLD			0x0bc6
#define USB_PID_DIBCOM_MOD3001_WARM			0x0bc7
#define USB_PID_DIBCOM_STK7700P				0x1e14
#define USB_PID_DIBCOM_STK7700P_PC			0x1e78
#define USB_PID_DIBCOM_STK7700D				0x1ef0
#define USB_PID_DIBCOM_STK7700_U7000			0x7001
#define USB_PID_DIBCOM_STK7070P				0x1ebc
#define USB_PID_DIBCOM_STK7070PD			0x1ebe
#define USB_PID_DIBCOM_ANCHOR_2135_COLD			0x2131
#define USB_PID_DPOSH_M9206_COLD			0x9206
#define USB_PID_DPOSH_M9206_WARM			0xa090
#define USB_PID_UNIWILL_STK7700P			0x6003
#define USB_PID_GENIUS_TVGO_DVB_T03			0x4012
#define USB_PID_GRANDTEC_DVBT_USB_COLD			0x0fa0
#define USB_PID_GRANDTEC_DVBT_USB_WARM			0x0fa1
#define USB_PID_INTEL_CE9500				0x9500
#define USB_PID_KWORLD_399U				0xe399
#define USB_PID_KWORLD_395U				0xe396
#define USB_PID_KWORLD_395U_2				0xe39b
#define USB_PID_KWORLD_395U_3				0xe395
#define USB_PID_KWORLD_MC810				0xc810
#define USB_PID_KWORLD_PC160_2T				0xc160
#define USB_PID_KWORLD_VSTREAM_COLD			0x17de
#define USB_PID_KWORLD_VSTREAM_WARM			0x17df
#define USB_PID_TERRATEC_CINERGY_T_USB_XE		0x0055
#define USB_PID_TERRATEC_CINERGY_T_USB_XE_REV2		0x0069
#define USB_PID_TWINHAN_VP7041_COLD			0x3201
#define USB_PID_TWINHAN_VP7041_WARM			0x3202
#define USB_PID_TWINHAN_VP7020_COLD			0x3203
#define USB_PID_TWINHAN_VP7020_WARM			0x3204
#define USB_PID_TWINHAN_VP7045_COLD			0x3205
#define USB_PID_TWINHAN_VP7045_WARM			0x3206
#define USB_PID_TWINHAN_VP7021_COLD			0x3207
#define USB_PID_TWINHAN_VP7021_WARM			0x3208
#define USB_PID_TINYTWIN				0x3226
#define USB_PID_DNTV_TINYUSB2_COLD			0x3223
#define USB_PID_DNTV_TINYUSB2_WARM			0x3224
#define USB_PID_ULTIMA_TVBOX_COLD			0x8105
#define USB_PID_ULTIMA_TVBOX_WARM			0x8106
#define USB_PID_ULTIMA_TVBOX_AN2235_COLD		0x8107
#define USB_PID_ULTIMA_TVBOX_AN2235_WARM		0x8108
#define USB_PID_ULTIMA_TVBOX_ANCHOR_COLD		0x2235
#define USB_PID_ULTIMA_TVBOX_USB2_COLD			0x8109
#define USB_PID_ULTIMA_TVBOX_USB2_WARM			0x810a
#define USB_PID_ARTEC_T14_COLD				0x810b
#define USB_PID_ARTEC_T14_WARM				0x810c
#define USB_PID_ARTEC_T14BR				0x810f
#define USB_PID_ULTIMA_TVBOX_USB2_FX_COLD		0x8613
#define USB_PID_ULTIMA_TVBOX_USB2_FX_WARM		0x1002
#define USB_PID_UNK_HYPER_PALTEK_COLD			0x005e
#define USB_PID_UNK_HYPER_PALTEK_WARM			0x005f
#define USB_PID_HANFTEK_UMT_010_COLD			0x0001
#define USB_PID_HANFTEK_UMT_010_WARM			0x0015
#define USB_PID_DTT200U_COLD				0x0201
#define USB_PID_DTT200U_WARM				0x0301
#define USB_PID_WT220U_ZAP250_COLD			0x0220
#define USB_PID_WT220U_COLD				0x0222
#define USB_PID_WT220U_WARM				0x0221
#define USB_PID_WT220U_FC_COLD				0x0225
#define USB_PID_WT220U_FC_WARM				0x0226
#define USB_PID_WT220U_ZL0353_COLD			0x022a
#define USB_PID_WT220U_ZL0353_WARM			0x022b
#define USB_PID_WINTV_NOVA_T_USB2_COLD			0x9300
#define USB_PID_WINTV_NOVA_T_USB2_WARM			0x9301
#define USB_PID_HAUPPAUGE_NOVA_T_500			0x9941
#define USB_PID_HAUPPAUGE_NOVA_T_500_2			0x9950
#define USB_PID_HAUPPAUGE_NOVA_T_500_3			0x8400
#define USB_PID_HAUPPAUGE_NOVA_T_STICK			0x7050
#define USB_PID_HAUPPAUGE_NOVA_T_STICK_2		0x7060
#define USB_PID_HAUPPAUGE_NOVA_T_STICK_3		0x7070
#define USB_PID_HAUPPAUGE_MYTV_T			0x7080
#define USB_PID_HAUPPAUGE_NOVA_TD_STICK			0x9580
#define USB_PID_HAUPPAUGE_NOVA_TD_STICK_52009		0x5200
#define USB_PID_HAUPPAUGE_TIGER_ATSC			0xb200
#define USB_PID_HAUPPAUGE_TIGER_ATSC_B210		0xb210
#define USB_PID_AVERMEDIA_EXPRESS			0xb568
#define USB_PID_AVERMEDIA_VOLAR				0xa807
#define USB_PID_AVERMEDIA_VOLAR_2			0xb808
#define USB_PID_AVERMEDIA_VOLAR_A868R			0xa868
#define USB_PID_AVERMEDIA_MCE_USB_M038			0x1228
#define USB_PID_AVERMEDIA_HYBRID_ULTRA_USB_M039R	0x0039
#define USB_PID_AVERMEDIA_HYBRID_ULTRA_USB_M039R_ATSC	0x1039
#define USB_PID_AVERMEDIA_HYBRID_ULTRA_USB_M039R_DVBT	0x2039
#define USB_PID_AVERMEDIA_VOLAR_X			0xa815
#define USB_PID_AVERMEDIA_VOLAR_X_2			0x8150
#define USB_PID_AVERMEDIA_A309				0xa309
#define USB_PID_AVERMEDIA_A310				0xa310
#define USB_PID_AVERMEDIA_A850				0x850a
#define USB_PID_AVERMEDIA_A805				0xa805
#define USB_PID_TECHNOTREND_CONNECT_S2400               0x3006
#define USB_PID_TERRATEC_CINERGY_DT_XS_DIVERSITY	0x005a
#define USB_PID_TERRATEC_CINERGY_DT_XS_DIVERSITY_2	0x0081
#define USB_PID_TERRATEC_CINERGY_HT_USB_XE		0x0058
#define USB_PID_TERRATEC_CINERGY_HT_EXPRESS		0x0060
#define USB_PID_TERRATEC_CINERGY_T_EXPRESS		0x0062
#define USB_PID_TERRATEC_CINERGY_T_XXS			0x0078
#define USB_PID_TERRATEC_T3				0x10a0
#define USB_PID_TERRATEC_T5				0x10a1
#define USB_PID_PINNACLE_EXPRESSCARD_320CX		0x022e
#define USB_PID_PINNACLE_PCTV2000E			0x022c
#define USB_PID_PINNACLE_PCTV_DVB_T_FLASH		0x0228
#define USB_PID_PINNACLE_PCTV_DUAL_DIVERSITY_DVB_T	0x0229
#define USB_PID_PINNACLE_PCTV71E			0x022b
#define USB_PID_PINNACLE_PCTV72E			0x0236
#define USB_PID_PINNACLE_PCTV73E			0x0237
#define USB_PID_PINNACLE_PCTV801E			0x023a
#define USB_PID_PINNACLE_PCTV801E_SE			0x023b
#define USB_PID_PCTV_200E				0x020e
#define USB_PID_PCTV_400E				0x020f
#define USB_PID_PCTV_450E				0x0222
#define USB_PID_NEBULA_DIGITV				0x0201
#define USB_PID_DVICO_BLUEBIRD_LGDT			0xd820
#define USB_PID_DVICO_BLUEBIRD_LG064F_COLD		0xd500
#define USB_PID_DVICO_BLUEBIRD_LG064F_WARM		0xd501
#define USB_PID_DVICO_BLUEBIRD_LGZ201_COLD		0xdb00
#define USB_PID_DVICO_BLUEBIRD_LGZ201_WARM		0xdb01
#define USB_PID_DVICO_BLUEBIRD_TH7579_COLD		0xdb10
#define USB_PID_DVICO_BLUEBIRD_TH7579_WARM		0xdb11
#define USB_PID_DVICO_BLUEBIRD_DUAL_1_COLD		0xdb50
#define USB_PID_DVICO_BLUEBIRD_DUAL_1_WARM		0xdb51
#define USB_PID_DVICO_BLUEBIRD_DUAL_2_COLD		0xdb58
#define USB_PID_DVICO_BLUEBIRD_DUAL_2_WARM		0xdb59
#define USB_PID_DVICO_BLUEBIRD_DUAL_4			0xdb78
#define USB_PID_DVICO_BLUEBIRD_DUAL_4_REV_2		0xdb98
#define USB_PID_DVICO_BLUEBIRD_DVB_T_NANO_2		0xdb70
#define USB_PID_DVICO_BLUEBIRD_DVB_T_NANO_2_NFW_WARM	0xdb71
#define USB_PID_DIGITALNOW_BLUEBIRD_DUAL_1_COLD		0xdb54
#define USB_PID_DIGITALNOW_BLUEBIRD_DUAL_1_WARM		0xdb55
#define USB_PID_MEDION_MD95700				0x0932
#define USB_PID_MSI_MEGASKY580				0x5580
#define USB_PID_MSI_MEGASKY580_55801			0x5581
#define USB_PID_KYE_DVB_T_COLD				0x701e
#define USB_PID_KYE_DVB_T_WARM				0x701f
#define USB_PID_LITEON_DVB_T_COLD			0xf000
#define USB_PID_LITEON_DVB_T_WARM			0xf001
#define USB_PID_DIGIVOX_MINI_SL_COLD			0xe360
#define USB_PID_DIGIVOX_MINI_SL_WARM			0xe361
#define USB_PID_GRANDTEC_DVBT_USB2_COLD			0x0bc6
#define USB_PID_GRANDTEC_DVBT_USB2_WARM			0x0bc7
#define USB_PID_WINFAST_DTV_DONGLE_COLD			0x6025
#define USB_PID_WINFAST_DTV_DONGLE_WARM			0x6026
#define USB_PID_WINFAST_DTV_DONGLE_STK7700P		0x6f00
#define USB_PID_WINFAST_DTV_DONGLE_H			0x60f6
#define USB_PID_WINFAST_DTV_DONGLE_STK7700P_2		0x6f01
#define USB_PID_WINFAST_DTV_DONGLE_GOLD			0x6029
#define USB_PID_GENPIX_8PSK_REV_1_COLD			0x0200
#define USB_PID_GENPIX_8PSK_REV_1_WARM			0x0201
#define USB_PID_GENPIX_8PSK_REV_2			0x0202
#define USB_PID_GENPIX_SKYWALKER_1			0x0203
#define USB_PID_GENPIX_SKYWALKER_CW3K			0x0204
#define USB_PID_SIGMATEK_DVB_110			0x6610
#define USB_PID_MSI_DIGI_VOX_MINI_II			0x1513
#define USB_PID_MSI_DIGIVOX_DUO				0x8801
#define USB_PID_OPERA1_COLD				0x2830
#define USB_PID_OPERA1_WARM				0x3829
#define USB_PID_LIFEVIEW_TV_WALKER_TWIN_COLD		0x0514
#define USB_PID_LIFEVIEW_TV_WALKER_TWIN_WARM		0x0513
#define USB_PID_GIGABYTE_U7000				0x7001
#define USB_PID_GIGABYTE_U8000				0x7002
#define USB_PID_ASUS_U3000				0x171f
#define USB_PID_ASUS_U3000H				0x1736
#define USB_PID_ASUS_U3100				0x173f
#define USB_PID_YUAN_EC372S				0x1edc
#define USB_PID_YUAN_STK7700PH				0x1f08
#define USB_PID_YUAN_PD378S				0x2edc
#define USB_PID_YUAN_MC770				0x0871
#define USB_PID_YUAN_STK7700D				0x1efc
#define USB_PID_DW2102					0x2102
#define USB_PID_XTENSIONS_XD_380			0x0381
#define USB_PID_TELESTAR_STARSTICK_2			0x8000
#define USB_PID_MSI_DIGI_VOX_MINI_III                   0x8807
#define USB_PID_SONY_PLAYTV				0x0003
#define USB_PID_ELGATO_EYETV_DTT			0x0021
#define USB_PID_ELGATO_EYETV_DTT_Dlx			0x0020
#define USB_PID_DVB_T_USB_STICK_HIGH_SPEED_COLD		0x5000
#define USB_PID_DVB_T_USB_STICK_HIGH_SPEED_WARM		0x5001

#endif
