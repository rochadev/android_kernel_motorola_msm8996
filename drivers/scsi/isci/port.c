/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2008 - 2011 Intel Corporation. All rights reserved.
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
 * Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * BSD LICENSE
 *
 * Copyright(c) 2008 - 2011 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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
 */

#include <linux/workqueue.h>
#include "isci.h"
#include "scic_phy.h"
#include "scic_sds_phy.h"
#include "scic_port.h"
#include "port.h"
#include "request.h"

static void isci_port_change_state(struct isci_port *iport, enum isci_status status)
{
	unsigned long flags;

	dev_dbg(&iport->isci_host->pdev->dev,
		"%s: iport = %p, state = 0x%x\n",
		__func__, iport, status);

	/* XXX pointless lock */
	spin_lock_irqsave(&iport->state_lock, flags);
	iport->status = status;
	spin_unlock_irqrestore(&iport->state_lock, flags);
}

void isci_port_init(struct isci_port *iport, struct isci_host *ihost, int index)
{
	INIT_LIST_HEAD(&iport->remote_dev_list);
	INIT_LIST_HEAD(&iport->domain_dev_list);
	spin_lock_init(&iport->state_lock);
	init_completion(&iport->start_complete);
	iport->isci_host = ihost;
	isci_port_change_state(iport, isci_freed);
}

/**
 * isci_port_get_state() - This function gets the status of the port object.
 * @isci_port: This parameter points to the isci_port object
 *
 * status of the object as a isci_status enum.
 */
enum isci_status isci_port_get_state(
	struct isci_port *isci_port)
{
	return isci_port->status;
}

void isci_port_bc_change_received(struct isci_host *ihost,
				  struct scic_sds_port *sci_port,
				  struct scic_sds_phy *sci_phy)
{
	struct isci_phy *iphy = sci_phy_to_iphy(sci_phy);

	dev_dbg(&ihost->pdev->dev, "%s: iphy = %p, sas_phy = %p\n",
		__func__, iphy, &iphy->sas_phy);

	ihost->sas_ha.notify_port_event(&iphy->sas_phy, PORTE_BROADCAST_RCVD);
	scic_port_enable_broadcast_change_notification(sci_port);
}

void isci_port_link_up(struct isci_host *isci_host,
		       struct scic_sds_port *port,
		       struct scic_sds_phy *phy)
{
	unsigned long flags;
	struct scic_port_properties properties;
	struct isci_phy *isci_phy = sci_phy_to_iphy(phy);
	struct isci_port *isci_port = sci_port_to_iport(port);
	unsigned long success = true;

	BUG_ON(isci_phy->isci_port != NULL);

	isci_phy->isci_port = isci_port;

	dev_dbg(&isci_host->pdev->dev,
		"%s: isci_port = %p\n",
		__func__, isci_port);

	spin_lock_irqsave(&isci_phy->sas_phy.frame_rcvd_lock, flags);

	isci_port_change_state(isci_phy->isci_port, isci_starting);

	scic_port_get_properties(port, &properties);

	if (phy->protocol == SCIC_SDS_PHY_PROTOCOL_SATA) {
		u64 attached_sas_address;

		isci_phy->sas_phy.oob_mode = SATA_OOB_MODE;
		isci_phy->sas_phy.frame_rcvd_size = sizeof(struct dev_to_host_fis);

		/*
		 * For direct-attached SATA devices, the SCI core will
		 * automagically assign a SAS address to the end device
		 * for the purpose of creating a port. This SAS address
		 * will not be the same as assigned to the PHY and needs
		 * to be obtained from struct scic_port_properties properties.
		 */
		attached_sas_address = properties.remote.sas_address.high;
		attached_sas_address <<= 32;
		attached_sas_address |= properties.remote.sas_address.low;
		swab64s(&attached_sas_address);

		memcpy(&isci_phy->sas_phy.attached_sas_addr,
		       &attached_sas_address, sizeof(attached_sas_address));
	} else if (phy->protocol == SCIC_SDS_PHY_PROTOCOL_SAS) {
		isci_phy->sas_phy.oob_mode = SAS_OOB_MODE;
		isci_phy->sas_phy.frame_rcvd_size = sizeof(struct sas_identify_frame);

		/* Copy the attached SAS address from the IAF */
		memcpy(isci_phy->sas_phy.attached_sas_addr,
		       isci_phy->frame_rcvd.iaf.sas_addr, SAS_ADDR_SIZE);
	} else {
		dev_err(&isci_host->pdev->dev, "%s: unkown target\n", __func__);
		success = false;
	}

	isci_phy->sas_phy.phy->negotiated_linkrate = sci_phy_linkrate(phy);

	spin_unlock_irqrestore(&isci_phy->sas_phy.frame_rcvd_lock, flags);

	/* Notify libsas that we have an address frame, if indeed
	 * we've found an SSP, SMP, or STP target */
	if (success)
		isci_host->sas_ha.notify_port_event(&isci_phy->sas_phy,
						    PORTE_BYTES_DMAED);
}


/**
 * isci_port_link_down() - This function is called by the sci core when a link
 *    becomes inactive.
 * @isci_host: This parameter specifies the isci host object.
 * @phy: This parameter specifies the isci phy with the active link.
 * @port: This parameter specifies the isci port with the active link.
 *
 */
void isci_port_link_down(struct isci_host *isci_host, struct isci_phy *isci_phy,
			 struct isci_port *isci_port)
{
	struct isci_remote_device *isci_device;

	dev_dbg(&isci_host->pdev->dev,
		"%s: isci_port = %p\n", __func__, isci_port);

	if (isci_port) {

		/* check to see if this is the last phy on this port. */
		if (isci_phy->sas_phy.port
		    && isci_phy->sas_phy.port->num_phys == 1) {

			/* change the state for all devices on this port.
			 * The next task sent to this device will be returned
			 * as SAS_TASK_UNDELIVERED, and the scsi mid layer
			 * will remove the target
			 */
			list_for_each_entry(isci_device,
					    &isci_port->remote_dev_list,
					    node) {
				dev_dbg(&isci_host->pdev->dev,
					"%s: isci_device = %p\n",
					__func__, isci_device);
				isci_remote_device_change_state(isci_device,
								isci_stopping);
			}
		}
		isci_port_change_state(isci_port, isci_stopping);
	}

	/* Notify libsas of the borken link, this will trigger calls to our
	 * isci_port_deformed and isci_dev_gone functions.
	 */
	sas_phy_disconnected(&isci_phy->sas_phy);
	isci_host->sas_ha.notify_phy_event(&isci_phy->sas_phy,
					   PHYE_LOSS_OF_SIGNAL);

	isci_phy->isci_port = NULL;

	dev_dbg(&isci_host->pdev->dev,
		"%s: isci_port = %p - Done\n", __func__, isci_port);
}


/**
 * isci_port_deformed() - This function is called by libsas when a port becomes
 *    inactive.
 * @phy: This parameter specifies the libsas phy with the inactive port.
 *
 */
void isci_port_deformed(
	struct asd_sas_phy *phy)
{
	pr_debug("%s: sas_phy = %p\n", __func__, phy);
}

/**
 * isci_port_formed() - This function is called by libsas when a port becomes
 *    active.
 * @phy: This parameter specifies the libsas phy with the active port.
 *
 */
void isci_port_formed(
	struct asd_sas_phy *phy)
{
	pr_debug("%s: sas_phy = %p, sas_port = %p\n", __func__, phy, phy->port);
}

/**
 * isci_port_ready() - This function is called by the sci core when a link
 *    becomes ready.
 * @isci_host: This parameter specifies the isci host object.
 * @port: This parameter specifies the sci port with the active link.
 *
 */
void isci_port_ready(struct isci_host *isci_host, struct isci_port *isci_port)
{
	dev_dbg(&isci_host->pdev->dev,
		"%s: isci_port = %p\n", __func__, isci_port);

	complete_all(&isci_port->start_complete);
	isci_port_change_state(isci_port, isci_ready);
	return;
}

/**
 * isci_port_not_ready() - This function is called by the sci core when a link
 *    is not ready. All remote devices on this link will be removed if they are
 *    in the stopping state.
 * @isci_host: This parameter specifies the isci host object.
 * @port: This parameter specifies the sci port with the active link.
 *
 */
void isci_port_not_ready(struct isci_host *isci_host, struct isci_port *isci_port)
{
	dev_dbg(&isci_host->pdev->dev,
		"%s: isci_port = %p\n", __func__, isci_port);
}

/**
 * isci_port_hard_reset_complete() - This function is called by the sci core
 *    when the hard reset complete notification has been received.
 * @port: This parameter specifies the sci port with the active link.
 * @completion_status: This parameter specifies the core status for the reset
 *    process.
 *
 */
void isci_port_hard_reset_complete(struct isci_port *isci_port,
				   enum sci_status completion_status)
{
	dev_dbg(&isci_port->isci_host->pdev->dev,
		"%s: isci_port = %p, completion_status=%x\n",
		     __func__, isci_port, completion_status);

	/* Save the status of the hard reset from the port. */
	isci_port->hard_reset_status = completion_status;

	complete_all(&isci_port->hard_reset_complete);
}

int isci_port_perform_hard_reset(struct isci_host *ihost, struct isci_port *iport,
				 struct isci_phy *iphy)
{
	unsigned long flags;
	enum sci_status status;
	int ret = TMF_RESP_FUNC_COMPLETE;

	dev_dbg(&ihost->pdev->dev, "%s: iport = %p\n",
		__func__, iport);

	init_completion(&iport->hard_reset_complete);

	spin_lock_irqsave(&ihost->scic_lock, flags);

	#define ISCI_PORT_RESET_TIMEOUT SCIC_SDS_SIGNATURE_FIS_TIMEOUT
	status = scic_port_hard_reset(&iport->sci, ISCI_PORT_RESET_TIMEOUT);

	spin_unlock_irqrestore(&ihost->scic_lock, flags);

	if (status == SCI_SUCCESS) {
		wait_for_completion(&iport->hard_reset_complete);

		dev_dbg(&ihost->pdev->dev,
			"%s: iport = %p; hard reset completion\n",
			__func__, iport);

		if (iport->hard_reset_status != SCI_SUCCESS)
			ret = TMF_RESP_FUNC_FAILED;
	} else {
		ret = TMF_RESP_FUNC_FAILED;

		dev_err(&ihost->pdev->dev,
			"%s: iport = %p; scic_port_hard_reset call"
			" failed 0x%x\n",
			__func__, iport, status);

	}

	/* If the hard reset for the port has failed, consider this
	 * the same as link failures on all phys in the port.
	 */
	if (ret != TMF_RESP_FUNC_COMPLETE) {
		dev_err(&ihost->pdev->dev,
			"%s: iport = %p; hard reset failed "
			"(0x%x) - sending link down to libsas for phy %p\n",
			__func__, iport, iport->hard_reset_status, iphy);

		isci_port_link_down(ihost, iphy, iport);
	}

	return ret;
}

void isci_port_stop_complete(struct scic_sds_controller *scic,
					  struct scic_sds_port *sci_port,
					  enum sci_status completion_status)
{
	dev_dbg(&scic_to_ihost(scic)->pdev->dev, "Port stop complete\n");
}
