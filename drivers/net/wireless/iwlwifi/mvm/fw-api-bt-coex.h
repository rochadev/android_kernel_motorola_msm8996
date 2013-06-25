/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2013 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * BSD LICENSE
 *
 * Copyright(c) 2013 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

#ifndef __fw_api_bt_coex_h__
#define __fw_api_bt_coex_h__

#include <linux/types.h>
#include <linux/bitops.h>

#define BITS(nb) (BIT(nb) - 1)

/**
 * enum iwl_bt_coex_flags - flags for BT_COEX command
 * @BT_CH_PRIMARY_EN:
 * @BT_CH_SECONDARY_EN:
 * @BT_NOTIF_COEX_OFF:
 * @BT_COEX_MODE_POS:
 * @BT_COEX_MODE_MSK:
 * @BT_COEX_DISABLE:
 * @BT_COEX_2W:
 * @BT_COEX_3W:
 * @BT_COEX_NW:
 * @BT_USE_DEFAULTS:
 * @BT_SYNC_2_BT_DISABLE:
 * @BT_COEX_CORUNNING_TBL_EN:
 */
enum iwl_bt_coex_flags {
	BT_CH_PRIMARY_EN		= BIT(0),
	BT_CH_SECONDARY_EN		= BIT(1),
	BT_NOTIF_COEX_OFF		= BIT(2),
	BT_COEX_MODE_POS		= 3,
	BT_COEX_MODE_MSK		= BITS(3) << BT_COEX_MODE_POS,
	BT_COEX_DISABLE			= 0x0 << BT_COEX_MODE_POS,
	BT_COEX_2W			= 0x1 << BT_COEX_MODE_POS,
	BT_COEX_3W			= 0x2 << BT_COEX_MODE_POS,
	BT_COEX_NW			= 0x3 << BT_COEX_MODE_POS,
	BT_USE_DEFAULTS			= BIT(6),
	BT_SYNC_2_BT_DISABLE		= BIT(7),
	BT_COEX_CORUNNING_TBL_EN	= BIT(8),
	BT_COEX_MPLUT_TBL_EN		= BIT(9),
	/* Bit 10 is reserved */
	BT_COEX_WF_PRIO_BOOST_CHECK_EN	= BIT(11),
};

/*
 * indicates what has changed in the BT_COEX command.
 */
enum iwl_bt_coex_valid_bit_msk {
	BT_VALID_ENABLE			= BIT(0),
	BT_VALID_BT_PRIO_BOOST		= BIT(1),
	BT_VALID_MAX_KILL		= BIT(2),
	BT_VALID_3W_TMRS		= BIT(3),
	BT_VALID_KILL_ACK		= BIT(4),
	BT_VALID_KILL_CTS		= BIT(5),
	BT_VALID_REDUCED_TX_POWER	= BIT(6),
	BT_VALID_LUT			= BIT(7),
	BT_VALID_WIFI_RX_SW_PRIO_BOOST	= BIT(8),
	BT_VALID_WIFI_TX_SW_PRIO_BOOST	= BIT(9),
	BT_VALID_MULTI_PRIO_LUT		= BIT(10),
	BT_VALID_TRM_KICK_FILTER	= BIT(11),
	BT_VALID_CORUN_LUT_20		= BIT(12),
	BT_VALID_CORUN_LUT_40		= BIT(13),
	BT_VALID_ANT_ISOLATION		= BIT(14),
	BT_VALID_ANT_ISOLATION_THRS	= BIT(15),
	BT_VALID_TXTX_DELTA_FREQ_THRS	= BIT(16),
	BT_VALID_TXRX_MAX_FREQ_0	= BIT(17),
};

/**
 * enum iwl_bt_reduced_tx_power - allows to reduce txpower for WiFi frames.
 * @BT_REDUCED_TX_POWER_CTL: reduce Tx power for control frames
 * @BT_REDUCED_TX_POWER_DATA: reduce Tx power for data frames
 *
 * This mechanism allows to have BT and WiFi run concurrently. Since WiFi
 * reduces its Tx power, it can work along with BT, hence reducing the amount
 * of WiFi frames being killed by BT.
 */
enum iwl_bt_reduced_tx_power {
	BT_REDUCED_TX_POWER_CTL		= BIT(0),
	BT_REDUCED_TX_POWER_DATA	= BIT(1),
};

enum iwl_bt_coex_lut_type {
	BT_COEX_TIGHT_LUT = 0,
	BT_COEX_LOOSE_LUT,
	BT_COEX_TX_DIS_LUT,

	BT_COEX_MAX_LUT,
};

#define BT_COEX_LUT_SIZE (12)
#define BT_COEX_CORUN_LUT_SIZE (32)
#define BT_COEX_MULTI_PRIO_LUT_SIZE (2)
#define BT_COEX_BOOST_SIZE (4)
#define BT_REDUCED_TX_POWER_BIT BIT(7)

/**
 * struct iwl_bt_coex_cmd - bt coex configuration command
 * @flags:&enum iwl_bt_coex_flags
 * @max_kill:
 * @bt_reduced_tx_power: enum %iwl_bt_reduced_tx_power
 * @bt4_antenna_isolation:
 * @bt4_antenna_isolation_thr:
 * @bt4_tx_tx_delta_freq_thr:
 * @bt4_tx_rx_max_freq0:
 * @bt_prio_boost:
 * @wifi_tx_prio_boost: SW boost of wifi tx priority
 * @wifi_rx_prio_boost: SW boost of wifi rx priority
 * @kill_ack_msk:
 * @kill_cts_msk:
 * @decision_lut:
 * @bt4_multiprio_lut:
 * @bt4_corun_lut20:
 * @bt4_corun_lut40:
 * @valid_bit_msk: enum %iwl_bt_coex_valid_bit_msk
 *
 * The structure is used for the BT_COEX command.
 */
struct iwl_bt_coex_cmd {
	__le32 flags;
	u8 max_kill;
	u8 bt_reduced_tx_power;
	u8 reserved[2];

	u8 bt4_antenna_isolation;
	u8 bt4_antenna_isolation_thr;
	u8 bt4_tx_tx_delta_freq_thr;
	u8 bt4_tx_rx_max_freq0;

	__le32 bt_prio_boost[BT_COEX_BOOST_SIZE];
	__le32 wifi_tx_prio_boost;
	__le32 wifi_rx_prio_boost;
	__le32 kill_ack_msk;
	__le32 kill_cts_msk;

	__le32 decision_lut[BT_COEX_MAX_LUT][BT_COEX_LUT_SIZE];
	__le32 bt4_multiprio_lut[BT_COEX_MULTI_PRIO_LUT_SIZE];
	__le32 bt4_corun_lut20[BT_COEX_CORUN_LUT_SIZE];
	__le32 bt4_corun_lut40[BT_COEX_CORUN_LUT_SIZE];

	__le32 valid_bit_msk;
} __packed; /* BT_COEX_CMD_API_S_VER_3 */

/**
 * struct iwl_bt_coex_ci_cmd - bt coex channel inhibition command
 * @bt_primary_ci:
 * @bt_secondary_ci:
 * @co_run_bw_primary:
 * @co_run_bw_secondary:
 * @primary_ch_phy_id:
 * @secondary_ch_phy_id:
 *
 * Used for BT_COEX_CI command
 */
struct iwl_bt_coex_ci_cmd {
	__le64 bt_primary_ci;
	__le64 bt_secondary_ci;

	u8 co_run_bw_primary;
	u8 co_run_bw_secondary;
	u8 primary_ch_phy_id;
	u8 secondary_ch_phy_id;
} __packed; /* BT_CI_MSG_API_S_VER_1 */

#define BT_MBOX(n_dw, _msg, _pos, _nbits)	\
	BT_MBOX##n_dw##_##_msg##_POS = (_pos),	\
	BT_MBOX##n_dw##_##_msg = BITS(_nbits) << BT_MBOX##n_dw##_##_msg##_POS

enum iwl_bt_mxbox_dw0 {
	BT_MBOX(0, LE_SLAVE_LAT, 0, 3),
	BT_MBOX(0, LE_PROF1, 3, 1),
	BT_MBOX(0, LE_PROF2, 4, 1),
	BT_MBOX(0, LE_PROF_OTHER, 5, 1),
	BT_MBOX(0, CHL_SEQ_N, 8, 4),
	BT_MBOX(0, INBAND_S, 13, 1),
	BT_MBOX(0, LE_MIN_RSSI, 16, 4),
	BT_MBOX(0, LE_SCAN, 20, 1),
	BT_MBOX(0, LE_ADV, 21, 1),
	BT_MBOX(0, LE_MAX_TX_POWER, 24, 4),
	BT_MBOX(0, OPEN_CON_1, 28, 2),
};

enum iwl_bt_mxbox_dw1 {
	BT_MBOX(1, BR_MAX_TX_POWER, 0, 4),
	BT_MBOX(1, IP_SR, 4, 1),
	BT_MBOX(1, LE_MSTR, 5, 1),
	BT_MBOX(1, AGGR_TRFC_LD, 8, 6),
	BT_MBOX(1, MSG_TYPE, 16, 3),
	BT_MBOX(1, SSN, 19, 2),
};

enum iwl_bt_mxbox_dw2 {
	BT_MBOX(2, SNIFF_ACT, 0, 3),
	BT_MBOX(2, PAG, 3, 1),
	BT_MBOX(2, INQUIRY, 4, 1),
	BT_MBOX(2, CONN, 5, 1),
	BT_MBOX(2, SNIFF_INTERVAL, 8, 5),
	BT_MBOX(2, DISC, 13, 1),
	BT_MBOX(2, SCO_TX_ACT, 16, 2),
	BT_MBOX(2, SCO_RX_ACT, 18, 2),
	BT_MBOX(2, ESCO_RE_TX, 20, 2),
	BT_MBOX(2, SCO_DURATION, 24, 6),
};

enum iwl_bt_mxbox_dw3 {
	BT_MBOX(3, SCO_STATE, 0, 1),
	BT_MBOX(3, SNIFF_STATE, 1, 1),
	BT_MBOX(3, A2DP_STATE, 2, 1),
	BT_MBOX(3, ACL_STATE, 3, 1),
	BT_MBOX(3, MSTR_STATE, 4, 1),
	BT_MBOX(3, OBX_STATE, 5, 1),
	BT_MBOX(3, OPEN_CON_2, 8, 2),
	BT_MBOX(3, TRAFFIC_LOAD, 10, 2),
	BT_MBOX(3, CHL_SEQN_LSB, 12, 1),
	BT_MBOX(3, INBAND_P, 13, 1),
	BT_MBOX(3, MSG_TYPE_2, 16, 3),
	BT_MBOX(3, SSN_2, 19, 2),
	BT_MBOX(3, UPDATE_REQUEST, 21, 1),
};

#define BT_MBOX_MSG(_notif, _num, _field)				     \
	((le32_to_cpu((_notif)->mbox_msg[(_num)]) & BT_MBOX##_num##_##_field)\
	>> BT_MBOX##_num##_##_field##_POS)

enum iwl_bt_activity_grading {
	BT_OFF			= 0,
	BT_ON_NO_CONNECTION	= 1,
	BT_LOW_TRAFFIC		= 2,
	BT_HIGH_TRAFFIC		= 3,
};

/**
 * struct iwl_bt_coex_profile_notif - notification about BT coex
 * @mbox_msg: message from BT to WiFi
 * @msg_idx: the index of the message
 * @bt_status: 0 - off, 1 - on
 * @bt_open_conn: number of BT connections open
 * @bt_traffic_load: load of BT traffic
 * @bt_agg_traffic_load: aggregated load of BT traffic
 * @bt_ci_compliance: 0 - no CI compliance, 1 - CI compliant
 * @primary_ch_lut: LUT used for primary channel
 * @secondary_ch_lut: LUT used for secondary channel
 * @bt_activity_grading: the activity of BT enum %iwl_bt_activity_grading
 */
struct iwl_bt_coex_profile_notif {
	__le32 mbox_msg[4];
	__le32 msg_idx;
	u8 bt_status;
	u8 bt_open_conn;
	u8 bt_traffic_load;
	u8 bt_agg_traffic_load;
	u8 bt_ci_compliance;
	u8 reserved[3];

	__le32 primary_ch_lut;
	__le32 secondary_ch_lut;
	__le32 bt_activity_grading;
} __packed; /* BT_COEX_PROFILE_NTFY_API_S_VER_2 */

enum iwl_bt_coex_prio_table_event {
	BT_COEX_PRIO_TBL_EVT_INIT_CALIB1		= 0,
	BT_COEX_PRIO_TBL_EVT_INIT_CALIB2		= 1,
	BT_COEX_PRIO_TBL_EVT_PERIODIC_CALIB_LOW1	= 2,
	BT_COEX_PRIO_TBL_EVT_PERIODIC_CALIB_LOW2	= 3,
	BT_COEX_PRIO_TBL_EVT_PERIODIC_CALIB_HIGH1	= 4,
	BT_COEX_PRIO_TBL_EVT_PERIODIC_CALIB_HIGH2	= 5,
	BT_COEX_PRIO_TBL_EVT_DTIM			= 6,
	BT_COEX_PRIO_TBL_EVT_SCAN52			= 7,
	BT_COEX_PRIO_TBL_EVT_SCAN24			= 8,
	BT_COEX_PRIO_TBL_EVT_IDLE			= 9,
	BT_COEX_PRIO_TBL_EVT_MAX			= 16,
}; /* BT_COEX_PRIO_TABLE_EVENTS_API_E_VER_1 */

enum iwl_bt_coex_prio_table_prio {
	BT_COEX_PRIO_TBL_DISABLED	= 0,
	BT_COEX_PRIO_TBL_PRIO_LOW	= 1,
	BT_COEX_PRIO_TBL_PRIO_HIGH	= 2,
	BT_COEX_PRIO_TBL_PRIO_BYPASS	= 3,
	BT_COEX_PRIO_TBL_PRIO_COEX_OFF	= 4,
	BT_COEX_PRIO_TBL_PRIO_COEX_ON	= 5,
	BT_COEX_PRIO_TBL_PRIO_COEX_IDLE = 6,
	BT_COEX_PRIO_TBL_MAX		= 8,
}; /* BT_COEX_PRIO_TABLE_PRIORITIES_API_E_VER_1 */

#define BT_COEX_PRIO_TBL_SHRD_ANT_POS     (0)
#define BT_COEX_PRIO_TBL_PRIO_POS         (1)
#define BT_COEX_PRIO_TBL_RESERVED_POS     (4)

/**
 * struct iwl_bt_coex_prio_tbl_cmd - priority table for BT coex
 * @prio_tbl:
 */
struct iwl_bt_coex_prio_tbl_cmd {
	u8 prio_tbl[BT_COEX_PRIO_TBL_EVT_MAX];
} __packed;

#endif /* __fw_api_bt_coex_h__ */
