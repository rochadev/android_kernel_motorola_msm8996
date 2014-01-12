/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */


/*
 * This file contains the API definitions for the Unified Wireless Module Interface (WMI).
 */
#ifndef _WMI_UNIFIED_PRIV_H_
#define _WMI_UNIFIED_PRIV_H_
#include <osdep.h>
#include "a_types.h"
#include "wmi.h"
#include "wmi_unified.h"
#include "adf_os_atomic.h"

#define WMI_UNIFIED_MAX_EVENT 0x100
#define WMI_MAX_CMDS  1024

typedef adf_nbuf_t wmi_buf_t;

#ifdef WLAN_OPEN_SOURCE
struct fwdebug {
       struct sk_buff_head fwlog_queue;
       struct completion fwlog_completion;
       A_BOOL fwlog_open;
};
#endif /* WLAN_OPEN_SOURCE */

struct wmi_unified {
	ol_scn_t scn_handle; /* handle to device */
	adf_os_atomic_t pending_cmds;
	HTC_ENDPOINT_ID wmi_endpoint_id;
	WMI_EVT_ID event_id[WMI_UNIFIED_MAX_EVENT];
	wmi_unified_event_handler event_handler[WMI_UNIFIED_MAX_EVENT];
	u_int32_t max_event_idx;
	void *htc_handle;
#ifndef QCA_WIFI_ISOC
	adf_os_spinlock_t eventq_lock;
	adf_nbuf_queue_t event_queue;
	struct work_struct rx_event_work;
#endif
#ifdef WLAN_OPEN_SOURCE
       struct fwdebug dbglog;
       struct dentry *debugfs_phy;
#endif /* WLAN_OPEN_SOURCE */
};
#endif
