/*
 * Marvell Wireless LAN device driver: 802.11n
 *
 * Copyright (C) 2011, Marvell International Ltd.
 *
 * This software file (the "File") is distributed by Marvell International
 * Ltd. under the terms of the GNU General Public License Version 2, June 1991
 * (the "License").  You may use, redistribute and/or modify this File in
 * accordance with the terms and conditions of the License, a copy of which
 * is available by writing to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or on the
 * worldwide web at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
 *
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
 * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
 * this warranty disclaimer.
 */

#include "decl.h"
#include "ioctl.h"
#include "util.h"
#include "fw.h"
#include "main.h"
#include "wmm.h"
#include "11n.h"

/*
 * Fills HT capability information field, AMPDU Parameters field, HT extended
 * capability field, and supported MCS set fields.
 *
 * Only the following HT capability information fields are used, all other
 * fields are always turned off.
 *
 *  Bit 1 : Supported channel width (0: 20MHz, 1: Both 20 and 40 MHz)
 *  Bit 4 : Greenfield support (0: Not supported, 1: Supported)
 *  Bit 5 : Short GI for 20 MHz support (0: Not supported, 1: Supported)
 *  Bit 6 : Short GI for 40 MHz support (0: Not supported, 1: Supported)
 *  Bit 7 : Tx STBC (0: Not supported, 1: Supported)
 *  Bit 8-9 : Rx STBC (0: Not supported, X: Support for up to X spatial streams)
 *  Bit 10 : Delayed BA support (0: Not supported, 1: Supported)
 *  Bit 11 : Maximum AMSDU length (0: 3839 octets, 1: 7935 octets)
 *  Bit 14 : 40-Mhz intolerant support (0: Not supported, 1: Supported)
 *
 *  In addition, the following AMPDU Parameters are set -
 *      - Maximum AMPDU length exponent (set to 3)
 *      - Minimum AMPDU start spacing (set to 0 - No restrictions)
 *
 *  MCS is set for 1x1, with MSC32 for infra mode or ad-hoc mode with 40 MHz
 *  support.
 *
 *  RD responder bit to set to clear in the extended capability header.
 */
void
mwifiex_fill_cap_info(struct mwifiex_private *priv,
		      struct mwifiex_ie_types_htcap *ht_cap)
{
	struct mwifiex_adapter *adapter = priv->adapter;
	u8 *mcs;
	int rx_mcs_supp;
	uint16_t ht_cap_info = le16_to_cpu(ht_cap->ht_cap.cap_info);
	uint16_t ht_ext_cap = le16_to_cpu(ht_cap->ht_cap.extended_ht_cap_info);

	/* Convert dev_cap to IEEE80211_HT_CAP */
	if (ISSUPP_CHANWIDTH40(adapter->hw_dot_11n_dev_cap))
		ht_cap_info |= IEEE80211_HT_CAP_SUP_WIDTH_20_40;
	else
		ht_cap_info &= ~IEEE80211_HT_CAP_SUP_WIDTH_20_40;

	if (ISSUPP_SHORTGI20(adapter->hw_dot_11n_dev_cap))
		ht_cap_info |= IEEE80211_HT_CAP_SGI_20;
	else
		ht_cap_info &= ~IEEE80211_HT_CAP_SGI_20;

	if (ISSUPP_SHORTGI40(adapter->hw_dot_11n_dev_cap))
		ht_cap_info |= IEEE80211_HT_CAP_SGI_40;
	else
		ht_cap_info &= ~IEEE80211_HT_CAP_SGI_40;

	if (ISSUPP_TXSTBC(adapter->hw_dot_11n_dev_cap))
		ht_cap_info |= IEEE80211_HT_CAP_TX_STBC;
	else
		ht_cap_info &= ~IEEE80211_HT_CAP_TX_STBC;

	if (ISSUPP_RXSTBC(adapter->hw_dot_11n_dev_cap))
		ht_cap_info |= 1 << IEEE80211_HT_CAP_RX_STBC_SHIFT;
	else
		ht_cap_info &= ~(3 << IEEE80211_HT_CAP_RX_STBC_SHIFT);

	if (ISSUPP_GREENFIELD(adapter->hw_dot_11n_dev_cap))
		ht_cap_info |= IEEE80211_HT_CAP_GRN_FLD;
	else
		ht_cap_info &= ~IEEE80211_HT_CAP_GRN_FLD;

	ht_cap_info &= ~IEEE80211_HT_CAP_MAX_AMSDU;
	ht_cap_info |= IEEE80211_HT_CAP_SM_PS;

	ht_cap->ht_cap.ampdu_params_info |= IEEE80211_HT_AMPDU_PARM_FACTOR;
	ht_cap->ht_cap.ampdu_params_info &= ~IEEE80211_HT_AMPDU_PARM_DENSITY;

	rx_mcs_supp = GET_RXMCSSUPP(adapter->hw_dev_mcs_support);

	mcs = (u8 *)&ht_cap->ht_cap.mcs;

	/* Set MCS for 1x1 */
	memset(mcs, 0xff, rx_mcs_supp);

	/* Clear all the other values */
	memset(&mcs[rx_mcs_supp], 0,
			sizeof(struct ieee80211_mcs_info) - rx_mcs_supp);

	if (priv->bss_mode == NL80211_IFTYPE_STATION ||
			(ht_cap_info & IEEE80211_HT_CAP_SUP_WIDTH_20_40))
		/* Set MCS32 for infra mode or ad-hoc mode with 40MHz support */
		SETHT_MCS32(ht_cap->ht_cap.mcs.rx_mask);

	/* Clear RD responder bit */
	RESETHT_EXTCAP_RDG(ht_ext_cap);

	ht_cap->ht_cap.cap_info = cpu_to_le16(ht_cap_info);
	ht_cap->ht_cap.extended_ht_cap_info = cpu_to_le16(ht_ext_cap);
}

/*
 * This function returns the pointer to an entry in BA Stream
 * table which matches the requested BA status.
 */
static struct mwifiex_tx_ba_stream_tbl *
mwifiex_11n_get_tx_ba_stream_status(struct mwifiex_private *priv,
				  enum mwifiex_ba_status ba_status)
{
	struct mwifiex_tx_ba_stream_tbl *tx_ba_tsr_tbl;
	unsigned long flags;

	spin_lock_irqsave(&priv->tx_ba_stream_tbl_lock, flags);
	list_for_each_entry(tx_ba_tsr_tbl, &priv->tx_ba_stream_tbl_ptr, list) {
		if (tx_ba_tsr_tbl->ba_status == ba_status) {
			spin_unlock_irqrestore(&priv->tx_ba_stream_tbl_lock,
					       flags);
			return tx_ba_tsr_tbl;
		}
	}
	spin_unlock_irqrestore(&priv->tx_ba_stream_tbl_lock, flags);
	return NULL;
}

/*
 * This function handles the command response of delete a block
 * ack request.
 *
 * The function checks the response success status and takes action
 * accordingly (send an add BA request in case of success, or recreate
 * the deleted stream in case of failure, if the add BA was also
 * initiated by us).
 */
int mwifiex_ret_11n_delba(struct mwifiex_private *priv,
			  struct host_cmd_ds_command *resp)
{
	int tid;
	struct mwifiex_tx_ba_stream_tbl *tx_ba_tbl;
	struct host_cmd_ds_11n_delba *del_ba =
		(struct host_cmd_ds_11n_delba *) &resp->params.del_ba;
	uint16_t del_ba_param_set = le16_to_cpu(del_ba->del_ba_param_set);

	tid = del_ba_param_set >> DELBA_TID_POS;
	if (del_ba->del_result == BA_RESULT_SUCCESS) {
		mwifiex_11n_delete_ba_stream_tbl(priv, tid,
				del_ba->peer_mac_addr, TYPE_DELBA_SENT,
				INITIATOR_BIT(del_ba_param_set));

		tx_ba_tbl = mwifiex_11n_get_tx_ba_stream_status(priv,
						BA_STREAM_SETUP_INPROGRESS);
		if (tx_ba_tbl)
			mwifiex_send_addba(priv, tx_ba_tbl->tid,
					   tx_ba_tbl->ra);
	} else { /*
		  * In case of failure, recreate the deleted stream in case
		  * we initiated the ADDBA
		  */
		if (INITIATOR_BIT(del_ba_param_set)) {
			mwifiex_11n_create_tx_ba_stream_tbl(priv,
					del_ba->peer_mac_addr, tid,
					BA_STREAM_SETUP_INPROGRESS);

			tx_ba_tbl = mwifiex_11n_get_tx_ba_stream_status(priv,
					BA_STREAM_SETUP_INPROGRESS);
			if (tx_ba_tbl)
				mwifiex_11n_delete_ba_stream_tbl(priv,
						tx_ba_tbl->tid, tx_ba_tbl->ra,
						TYPE_DELBA_SENT, true);
		}
	}

	return 0;
}

/*
 * This function handles the command response of add a block
 * ack request.
 *
 * Handling includes changing the header fields to CPU formats, checking
 * the response success status and taking actions accordingly (delete the
 * BA stream table in case of failure).
 */
int mwifiex_ret_11n_addba_req(struct mwifiex_private *priv,
			      struct host_cmd_ds_command *resp)
{
	int tid;
	struct host_cmd_ds_11n_addba_rsp *add_ba_rsp =
		(struct host_cmd_ds_11n_addba_rsp *) &resp->params.add_ba_rsp;
	struct mwifiex_tx_ba_stream_tbl *tx_ba_tbl;

	add_ba_rsp->ssn = cpu_to_le16((le16_to_cpu(add_ba_rsp->ssn))
			& SSN_MASK);

	tid = (le16_to_cpu(add_ba_rsp->block_ack_param_set)
		& IEEE80211_ADDBA_PARAM_TID_MASK)
		>> BLOCKACKPARAM_TID_POS;
	if (le16_to_cpu(add_ba_rsp->status_code) == BA_RESULT_SUCCESS) {
		tx_ba_tbl = mwifiex_11n_get_tx_ba_stream_tbl(priv, tid,
						add_ba_rsp->peer_mac_addr);
		if (tx_ba_tbl) {
			dev_dbg(priv->adapter->dev, "info: BA stream complete\n");
			tx_ba_tbl->ba_status = BA_STREAM_SETUP_COMPLETE;
		} else {
			dev_err(priv->adapter->dev, "BA stream not created\n");
		}
	} else {
		mwifiex_11n_delete_ba_stream_tbl(priv, tid,
						add_ba_rsp->peer_mac_addr,
						TYPE_DELBA_SENT, true);
		if (add_ba_rsp->add_rsp_result != BA_RESULT_TIMEOUT)
			priv->aggr_prio_tbl[tid].ampdu_ap =
				BA_STREAM_NOT_ALLOWED;
	}

	return 0;
}

/*
 * This function handles the command response of 11n configuration request.
 *
 * Handling includes changing the header fields into CPU format.
 */
int mwifiex_ret_11n_cfg(struct mwifiex_private *priv,
			struct host_cmd_ds_command *resp,
			void *data_buf)
{
	struct mwifiex_ds_11n_tx_cfg *tx_cfg = NULL;
	struct host_cmd_ds_11n_cfg *htcfg = &resp->params.htcfg;

	if (data_buf) {
		tx_cfg = (struct mwifiex_ds_11n_tx_cfg *) data_buf;
		tx_cfg->tx_htcap = le16_to_cpu(htcfg->ht_tx_cap);
		tx_cfg->tx_htinfo = le16_to_cpu(htcfg->ht_tx_info);
	}
	return 0;
}

/*
 * This function prepares command of reconfigure Tx buffer.
 *
 * Preparation includes -
 *      - Setting command ID, action and proper size
 *      - Setting Tx buffer size (for SET only)
 *      - Ensuring correct endian-ness
 */
int mwifiex_cmd_recfg_tx_buf(struct mwifiex_private *priv,
			     struct host_cmd_ds_command *cmd, int cmd_action,
			     void *data_buf)
{
	struct host_cmd_ds_txbuf_cfg *tx_buf = &cmd->params.tx_buf;
	u16 action = (u16) cmd_action;
	u16 buf_size = *((u16 *) data_buf);

	cmd->command = cpu_to_le16(HostCmd_CMD_RECONFIGURE_TX_BUFF);
	cmd->size =
		cpu_to_le16(sizeof(struct host_cmd_ds_txbuf_cfg) + S_DS_GEN);
	tx_buf->action = cpu_to_le16(action);
	switch (action) {
	case HostCmd_ACT_GEN_SET:
		dev_dbg(priv->adapter->dev, "cmd: set tx_buf=%d\n", buf_size);
		tx_buf->buff_size = cpu_to_le16(buf_size);
		break;
	case HostCmd_ACT_GEN_GET:
	default:
		tx_buf->buff_size = 0;
		break;
	}
	return 0;
}

/*
 * This function prepares command of AMSDU aggregation control.
 *
 * Preparation includes -
 *      - Setting command ID, action and proper size
 *      - Setting AMSDU control parameters (for SET only)
 *      - Ensuring correct endian-ness
 */
int mwifiex_cmd_amsdu_aggr_ctrl(struct mwifiex_private *priv,
				struct host_cmd_ds_command *cmd,
				int cmd_action, void *data_buf)
{
	struct host_cmd_ds_amsdu_aggr_ctrl *amsdu_ctrl =
		&cmd->params.amsdu_aggr_ctrl;
	u16 action = (u16) cmd_action;
	struct mwifiex_ds_11n_amsdu_aggr_ctrl *aa_ctrl =
		(struct mwifiex_ds_11n_amsdu_aggr_ctrl *) data_buf;

	cmd->command = cpu_to_le16(HostCmd_CMD_AMSDU_AGGR_CTRL);
	cmd->size = cpu_to_le16(sizeof(struct host_cmd_ds_amsdu_aggr_ctrl)
				+ S_DS_GEN);
	amsdu_ctrl->action = cpu_to_le16(action);
	switch (action) {
	case HostCmd_ACT_GEN_SET:
		amsdu_ctrl->enable = cpu_to_le16(aa_ctrl->enable);
		amsdu_ctrl->curr_buf_size = 0;
		break;
	case HostCmd_ACT_GEN_GET:
	default:
		amsdu_ctrl->curr_buf_size = 0;
		break;
	}
	return 0;
}

/*
 * This function handles the command response of AMSDU aggregation
 * control request.
 *
 * Handling includes changing the header fields into CPU format.
 */
int mwifiex_ret_amsdu_aggr_ctrl(struct mwifiex_private *priv,
				struct host_cmd_ds_command *resp,
				void *data_buf)
{
	struct mwifiex_ds_11n_amsdu_aggr_ctrl *amsdu_aggr_ctrl = NULL;
	struct host_cmd_ds_amsdu_aggr_ctrl *amsdu_ctrl =
		&resp->params.amsdu_aggr_ctrl;

	if (data_buf) {
		amsdu_aggr_ctrl =
			(struct mwifiex_ds_11n_amsdu_aggr_ctrl *) data_buf;
		amsdu_aggr_ctrl->enable = le16_to_cpu(amsdu_ctrl->enable);
		amsdu_aggr_ctrl->curr_buf_size =
			le16_to_cpu(amsdu_ctrl->curr_buf_size);
	}
	return 0;
}

/*
 * This function prepares 11n configuration command.
 *
 * Preparation includes -
 *      - Setting command ID, action and proper size
 *      - Setting HT Tx capability and HT Tx information fields
 *      - Ensuring correct endian-ness
 */
int mwifiex_cmd_11n_cfg(struct mwifiex_private *priv,
			struct host_cmd_ds_command *cmd,
			u16 cmd_action, void *data_buf)
{
	struct host_cmd_ds_11n_cfg *htcfg = &cmd->params.htcfg;
	struct mwifiex_ds_11n_tx_cfg *txcfg =
		(struct mwifiex_ds_11n_tx_cfg *) data_buf;

	cmd->command = cpu_to_le16(HostCmd_CMD_11N_CFG);
	cmd->size = cpu_to_le16(sizeof(struct host_cmd_ds_11n_cfg) + S_DS_GEN);
	htcfg->action = cpu_to_le16(cmd_action);
	htcfg->ht_tx_cap = cpu_to_le16(txcfg->tx_htcap);
	htcfg->ht_tx_info = cpu_to_le16(txcfg->tx_htinfo);
	return 0;
}

/*
 * This function appends an 11n TLV to a buffer.
 *
 * Buffer allocation is responsibility of the calling
 * function. No size validation is made here.
 *
 * The function fills up the following sections, if applicable -
 *      - HT capability IE
 *      - HT information IE (with channel list)
 *      - 20/40 BSS Coexistence IE
 *      - HT Extended Capabilities IE
 */
int
mwifiex_cmd_append_11n_tlv(struct mwifiex_private *priv,
			   struct mwifiex_bssdescriptor *bss_desc,
			   u8 **buffer)
{
	struct mwifiex_ie_types_htcap *ht_cap;
	struct mwifiex_ie_types_htinfo *ht_info;
	struct mwifiex_ie_types_chan_list_param_set *chan_list;
	struct mwifiex_ie_types_2040bssco *bss_co_2040;
	struct mwifiex_ie_types_extcap *ext_cap;
	int ret_len = 0;

	if (!buffer || !*buffer)
		return ret_len;

	if (bss_desc->bcn_ht_cap) {
		ht_cap = (struct mwifiex_ie_types_htcap *) *buffer;
		memset(ht_cap, 0, sizeof(struct mwifiex_ie_types_htcap));
		ht_cap->header.type = cpu_to_le16(WLAN_EID_HT_CAPABILITY);
		ht_cap->header.len =
				cpu_to_le16(sizeof(struct ieee80211_ht_cap));
		memcpy((u8 *) ht_cap + sizeof(struct mwifiex_ie_types_header),
		       (u8 *) bss_desc->bcn_ht_cap +
		       sizeof(struct ieee_types_header),
		       le16_to_cpu(ht_cap->header.len));

		mwifiex_fill_cap_info(priv, ht_cap);

		*buffer += sizeof(struct mwifiex_ie_types_htcap);
		ret_len += sizeof(struct mwifiex_ie_types_htcap);
	}

	if (bss_desc->bcn_ht_info) {
		if (priv->bss_mode == NL80211_IFTYPE_ADHOC) {
			ht_info = (struct mwifiex_ie_types_htinfo *) *buffer;
			memset(ht_info, 0,
			       sizeof(struct mwifiex_ie_types_htinfo));
			ht_info->header.type =
					cpu_to_le16(WLAN_EID_HT_INFORMATION);
			ht_info->header.len =
				cpu_to_le16(sizeof(struct ieee80211_ht_info));

			memcpy((u8 *) ht_info +
			       sizeof(struct mwifiex_ie_types_header),
			       (u8 *) bss_desc->bcn_ht_info +
			       sizeof(struct ieee_types_header),
			       le16_to_cpu(ht_info->header.len));

			if (!ISSUPP_CHANWIDTH40
					(priv->adapter->hw_dot_11n_dev_cap))
				ht_info->ht_info.ht_param &=
					~(IEEE80211_HT_PARAM_CHAN_WIDTH_ANY |
					IEEE80211_HT_PARAM_CHA_SEC_OFFSET);

			*buffer += sizeof(struct mwifiex_ie_types_htinfo);
			ret_len += sizeof(struct mwifiex_ie_types_htinfo);
		}

		chan_list =
			(struct mwifiex_ie_types_chan_list_param_set *) *buffer;
		memset(chan_list, 0,
		       sizeof(struct mwifiex_ie_types_chan_list_param_set));
		chan_list->header.type = cpu_to_le16(TLV_TYPE_CHANLIST);
		chan_list->header.len = cpu_to_le16(
			sizeof(struct mwifiex_ie_types_chan_list_param_set) -
			sizeof(struct mwifiex_ie_types_header));
		chan_list->chan_scan_param[0].chan_number =
			bss_desc->bcn_ht_info->control_chan;
		chan_list->chan_scan_param[0].radio_type =
			mwifiex_band_to_radio_type((u8) bss_desc->bss_band);

		if (ISSUPP_CHANWIDTH40(priv->adapter->hw_dot_11n_dev_cap)
			&& (bss_desc->bcn_ht_info->ht_param &
				IEEE80211_HT_PARAM_CHAN_WIDTH_ANY))
			SET_SECONDARYCHAN(chan_list->chan_scan_param[0].
					  radio_type,
					  (bss_desc->bcn_ht_info->ht_param &
					  IEEE80211_HT_PARAM_CHA_SEC_OFFSET));

		*buffer += sizeof(struct mwifiex_ie_types_chan_list_param_set);
		ret_len += sizeof(struct mwifiex_ie_types_chan_list_param_set);
	}

	if (bss_desc->bcn_bss_co_2040) {
		bss_co_2040 = (struct mwifiex_ie_types_2040bssco *) *buffer;
		memset(bss_co_2040, 0,
		       sizeof(struct mwifiex_ie_types_2040bssco));
		bss_co_2040->header.type = cpu_to_le16(WLAN_EID_BSS_COEX_2040);
		bss_co_2040->header.len =
		       cpu_to_le16(sizeof(bss_co_2040->bss_co_2040));

		memcpy((u8 *) bss_co_2040 +
		       sizeof(struct mwifiex_ie_types_header),
		       (u8 *) bss_desc->bcn_bss_co_2040 +
		       sizeof(struct ieee_types_header),
		       le16_to_cpu(bss_co_2040->header.len));

		*buffer += sizeof(struct mwifiex_ie_types_2040bssco);
		ret_len += sizeof(struct mwifiex_ie_types_2040bssco);
	}

	if (bss_desc->bcn_ext_cap) {
		ext_cap = (struct mwifiex_ie_types_extcap *) *buffer;
		memset(ext_cap, 0, sizeof(struct mwifiex_ie_types_extcap));
		ext_cap->header.type = cpu_to_le16(WLAN_EID_EXT_CAPABILITY);
		ext_cap->header.len = cpu_to_le16(sizeof(ext_cap->ext_cap));

		memcpy((u8 *) ext_cap +
		       sizeof(struct mwifiex_ie_types_header),
		       (u8 *) bss_desc->bcn_ext_cap +
		       sizeof(struct ieee_types_header),
		       le16_to_cpu(ext_cap->header.len));

		*buffer += sizeof(struct mwifiex_ie_types_extcap);
		ret_len += sizeof(struct mwifiex_ie_types_extcap);
	}

	return ret_len;
}

/*
 * This function reconfigures the Tx buffer size in firmware.
 *
 * This function prepares a firmware command and issues it, if
 * the current Tx buffer size is different from the one requested.
 * Maximum configurable Tx buffer size is limited by the HT capability
 * field value.
 */
void
mwifiex_cfg_tx_buf(struct mwifiex_private *priv,
		   struct mwifiex_bssdescriptor *bss_desc)
{
	u16 max_amsdu = MWIFIEX_TX_DATA_BUF_SIZE_2K;
	u16 tx_buf = 0;
	u16 curr_tx_buf_size = 0;

	if (bss_desc->bcn_ht_cap) {
		if (le16_to_cpu(bss_desc->bcn_ht_cap->cap_info) &
				IEEE80211_HT_CAP_MAX_AMSDU)
			max_amsdu = MWIFIEX_TX_DATA_BUF_SIZE_8K;
		else
			max_amsdu = MWIFIEX_TX_DATA_BUF_SIZE_4K;
	}

	tx_buf = min(priv->adapter->max_tx_buf_size, max_amsdu);

	dev_dbg(priv->adapter->dev, "info: max_amsdu=%d, max_tx_buf=%d\n",
			max_amsdu, priv->adapter->max_tx_buf_size);

	if (priv->adapter->curr_tx_buf_size <= MWIFIEX_TX_DATA_BUF_SIZE_2K)
		curr_tx_buf_size = MWIFIEX_TX_DATA_BUF_SIZE_2K;
	else if (priv->adapter->curr_tx_buf_size <= MWIFIEX_TX_DATA_BUF_SIZE_4K)
		curr_tx_buf_size = MWIFIEX_TX_DATA_BUF_SIZE_4K;
	else if (priv->adapter->curr_tx_buf_size <= MWIFIEX_TX_DATA_BUF_SIZE_8K)
		curr_tx_buf_size = MWIFIEX_TX_DATA_BUF_SIZE_8K;
	if (curr_tx_buf_size != tx_buf)
		mwifiex_send_cmd_async(priv, HostCmd_CMD_RECONFIGURE_TX_BUFF,
				       HostCmd_ACT_GEN_SET, 0, &tx_buf);
}

/*
 * This function checks if the given pointer is valid entry of
 * Tx BA Stream table.
 */
static int mwifiex_is_tx_ba_stream_ptr_valid(struct mwifiex_private *priv,
				struct mwifiex_tx_ba_stream_tbl *tx_tbl_ptr)
{
	struct mwifiex_tx_ba_stream_tbl *tx_ba_tsr_tbl;

	list_for_each_entry(tx_ba_tsr_tbl, &priv->tx_ba_stream_tbl_ptr, list) {
		if (tx_ba_tsr_tbl == tx_tbl_ptr)
			return true;
	}

	return false;
}

/*
 * This function deletes the given entry in Tx BA Stream table.
 *
 * The function also performs a validity check on the supplied
 * pointer before trying to delete.
 */
void mwifiex_11n_delete_tx_ba_stream_tbl_entry(struct mwifiex_private *priv,
				struct mwifiex_tx_ba_stream_tbl *tx_ba_tsr_tbl)
{
	if (!tx_ba_tsr_tbl &&
			mwifiex_is_tx_ba_stream_ptr_valid(priv, tx_ba_tsr_tbl))
		return;

	dev_dbg(priv->adapter->dev, "info: tx_ba_tsr_tbl %p\n", tx_ba_tsr_tbl);

	list_del(&tx_ba_tsr_tbl->list);

	kfree(tx_ba_tsr_tbl);
}

/*
 * This function deletes all the entries in Tx BA Stream table.
 */
void mwifiex_11n_delete_all_tx_ba_stream_tbl(struct mwifiex_private *priv)
{
	int i;
	struct mwifiex_tx_ba_stream_tbl *del_tbl_ptr, *tmp_node;
	unsigned long flags;

	spin_lock_irqsave(&priv->tx_ba_stream_tbl_lock, flags);
	list_for_each_entry_safe(del_tbl_ptr, tmp_node,
				 &priv->tx_ba_stream_tbl_ptr, list)
		mwifiex_11n_delete_tx_ba_stream_tbl_entry(priv, del_tbl_ptr);
	spin_unlock_irqrestore(&priv->tx_ba_stream_tbl_lock, flags);

	INIT_LIST_HEAD(&priv->tx_ba_stream_tbl_ptr);

	for (i = 0; i < MAX_NUM_TID; ++i)
		priv->aggr_prio_tbl[i].ampdu_ap =
			priv->aggr_prio_tbl[i].ampdu_user;
}

/*
 * This function returns the pointer to an entry in BA Stream
 * table which matches the given RA/TID pair.
 */
struct mwifiex_tx_ba_stream_tbl *
mwifiex_11n_get_tx_ba_stream_tbl(struct mwifiex_private *priv,
				 int tid, u8 *ra)
{
	struct mwifiex_tx_ba_stream_tbl *tx_ba_tsr_tbl;
	unsigned long flags;

	spin_lock_irqsave(&priv->tx_ba_stream_tbl_lock, flags);
	list_for_each_entry(tx_ba_tsr_tbl, &priv->tx_ba_stream_tbl_ptr, list) {
		if ((!memcmp(tx_ba_tsr_tbl->ra, ra, ETH_ALEN))
		    && (tx_ba_tsr_tbl->tid == tid)) {
			spin_unlock_irqrestore(&priv->tx_ba_stream_tbl_lock,
					       flags);
			return tx_ba_tsr_tbl;
		}
	}
	spin_unlock_irqrestore(&priv->tx_ba_stream_tbl_lock, flags);
	return NULL;
}

/*
 * This function creates an entry in Tx BA stream table for the
 * given RA/TID pair.
 */
void mwifiex_11n_create_tx_ba_stream_tbl(struct mwifiex_private *priv,
					 u8 *ra, int tid,
					 enum mwifiex_ba_status ba_status)
{
	struct mwifiex_tx_ba_stream_tbl *new_node;
	unsigned long flags;

	if (!mwifiex_11n_get_tx_ba_stream_tbl(priv, tid, ra)) {
		new_node = kzalloc(sizeof(struct mwifiex_tx_ba_stream_tbl),
				   GFP_ATOMIC);
		if (!new_node) {
			dev_err(priv->adapter->dev,
				"%s: failed to alloc new_node\n", __func__);
			return;
		}

		INIT_LIST_HEAD(&new_node->list);

		new_node->tid = tid;
		new_node->ba_status = ba_status;
		memcpy(new_node->ra, ra, ETH_ALEN);

		spin_lock_irqsave(&priv->tx_ba_stream_tbl_lock, flags);
		list_add_tail(&new_node->list, &priv->tx_ba_stream_tbl_ptr);
		spin_unlock_irqrestore(&priv->tx_ba_stream_tbl_lock, flags);
	}
}

/*
 * This function sends an add BA request to the given TID/RA pair.
 */
int mwifiex_send_addba(struct mwifiex_private *priv, int tid, u8 *peer_mac)
{
	struct host_cmd_ds_11n_addba_req add_ba_req;
	static u8 dialog_tok;
	int ret;

	dev_dbg(priv->adapter->dev, "cmd: %s: tid %d\n", __func__, tid);

	add_ba_req.block_ack_param_set = cpu_to_le16(
		(u16) ((tid << BLOCKACKPARAM_TID_POS) |
			 (priv->add_ba_param.
			  tx_win_size << BLOCKACKPARAM_WINSIZE_POS) |
			 IMMEDIATE_BLOCK_ACK));
	add_ba_req.block_ack_tmo = cpu_to_le16((u16)priv->add_ba_param.timeout);

	++dialog_tok;

	if (dialog_tok == 0)
		dialog_tok = 1;

	add_ba_req.dialog_token = dialog_tok;
	memcpy(&add_ba_req.peer_mac_addr, peer_mac, ETH_ALEN);

	/* We don't wait for the response of this command */
	ret = mwifiex_send_cmd_async(priv, HostCmd_CMD_11N_ADDBA_REQ,
				     0, 0, &add_ba_req);

	return ret;
}

/*
 * This function sends a delete BA request to the given TID/RA pair.
 */
int mwifiex_send_delba(struct mwifiex_private *priv, int tid, u8 *peer_mac,
		       int initiator)
{
	struct host_cmd_ds_11n_delba delba;
	int ret;
	uint16_t del_ba_param_set;

	memset(&delba, 0, sizeof(delba));
	delba.del_ba_param_set = cpu_to_le16(tid << DELBA_TID_POS);

	del_ba_param_set = le16_to_cpu(delba.del_ba_param_set);
	if (initiator)
		del_ba_param_set |= IEEE80211_DELBA_PARAM_INITIATOR_MASK;
	else
		del_ba_param_set &= ~IEEE80211_DELBA_PARAM_INITIATOR_MASK;

	memcpy(&delba.peer_mac_addr, peer_mac, ETH_ALEN);

	/* We don't wait for the response of this command */
	ret = mwifiex_send_cmd_async(priv, HostCmd_CMD_11N_DELBA,
				     HostCmd_ACT_GEN_SET, 0, &delba);

	return ret;
}

/*
 * This function handles the command response of a delete BA request.
 */
void mwifiex_11n_delete_ba_stream(struct mwifiex_private *priv, u8 *del_ba)
{
	struct host_cmd_ds_11n_delba *cmd_del_ba =
		(struct host_cmd_ds_11n_delba *) del_ba;
	uint16_t del_ba_param_set = le16_to_cpu(cmd_del_ba->del_ba_param_set);
	int tid;

	tid = del_ba_param_set >> DELBA_TID_POS;

	mwifiex_11n_delete_ba_stream_tbl(priv, tid, cmd_del_ba->peer_mac_addr,
					 TYPE_DELBA_RECEIVE,
					 INITIATOR_BIT(del_ba_param_set));
}

/*
 * This function retrieves the Rx reordering table.
 */
int mwifiex_get_rx_reorder_tbl(struct mwifiex_private *priv,
			       struct mwifiex_ds_rx_reorder_tbl *buf)
{
	int i;
	struct mwifiex_ds_rx_reorder_tbl *rx_reo_tbl = buf;
	struct mwifiex_rx_reorder_tbl *rx_reorder_tbl_ptr;
	int count = 0;
	unsigned long flags;

	spin_lock_irqsave(&priv->rx_reorder_tbl_lock, flags);
	list_for_each_entry(rx_reorder_tbl_ptr, &priv->rx_reorder_tbl_ptr,
			    list) {
		rx_reo_tbl->tid = (u16) rx_reorder_tbl_ptr->tid;
		memcpy(rx_reo_tbl->ta, rx_reorder_tbl_ptr->ta, ETH_ALEN);
		rx_reo_tbl->start_win = rx_reorder_tbl_ptr->start_win;
		rx_reo_tbl->win_size = rx_reorder_tbl_ptr->win_size;
		for (i = 0; i < rx_reorder_tbl_ptr->win_size; ++i) {
			if (rx_reorder_tbl_ptr->rx_reorder_ptr[i])
				rx_reo_tbl->buffer[i] = true;
			else
				rx_reo_tbl->buffer[i] = false;
		}
		rx_reo_tbl++;
		count++;

		if (count >= MWIFIEX_MAX_RX_BASTREAM_SUPPORTED)
			break;
	}
	spin_unlock_irqrestore(&priv->rx_reorder_tbl_lock, flags);

	return count;
}

/*
 * This function retrieves the Tx BA stream table.
 */
int mwifiex_get_tx_ba_stream_tbl(struct mwifiex_private *priv,
				 struct mwifiex_ds_tx_ba_stream_tbl *buf)
{
	struct mwifiex_tx_ba_stream_tbl *tx_ba_tsr_tbl;
	struct mwifiex_ds_tx_ba_stream_tbl *rx_reo_tbl = buf;
	int count = 0;
	unsigned long flags;

	spin_lock_irqsave(&priv->tx_ba_stream_tbl_lock, flags);
	list_for_each_entry(tx_ba_tsr_tbl, &priv->tx_ba_stream_tbl_ptr, list) {
		rx_reo_tbl->tid = (u16) tx_ba_tsr_tbl->tid;
		dev_dbg(priv->adapter->dev, "data: %s tid=%d\n",
						__func__, rx_reo_tbl->tid);
		memcpy(rx_reo_tbl->ra, tx_ba_tsr_tbl->ra, ETH_ALEN);
		rx_reo_tbl++;
		count++;
		if (count >= MWIFIEX_MAX_TX_BASTREAM_SUPPORTED)
			break;
	}
	spin_unlock_irqrestore(&priv->tx_ba_stream_tbl_lock, flags);

	return count;
}
