/*
 * Copyright © 2013 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#include "i915_drv.h"
#include "intel_drv.h"

/* IOSF sideband */
static int vlv_punit_rw(struct drm_i915_private *dev_priv, u32 port, u8 opcode,
			u8 addr, u32 *val)
{
	u32 cmd, devfn, be, bar;

	bar = 0;
	be = 0xf;
	devfn = PCI_DEVFN(2, 0);

	cmd = (devfn << IOSF_DEVFN_SHIFT) | (opcode << IOSF_OPCODE_SHIFT) |
		(port << IOSF_PORT_SHIFT) | (be << IOSF_BYTE_ENABLES_SHIFT) |
		(bar << IOSF_BAR_SHIFT);

	WARN_ON(!mutex_is_locked(&dev_priv->rps.hw_lock));

	if (I915_READ(VLV_IOSF_DOORBELL_REQ) & IOSF_SB_BUSY) {
		DRM_DEBUG_DRIVER("warning: pcode (%s) mailbox access failed\n",
				 opcode == PUNIT_OPCODE_REG_READ ?
				 "read" : "write");
		return -EAGAIN;
	}

	I915_WRITE(VLV_IOSF_ADDR, addr);
	if (opcode == PUNIT_OPCODE_REG_WRITE)
		I915_WRITE(VLV_IOSF_DATA, *val);
	I915_WRITE(VLV_IOSF_DOORBELL_REQ, cmd);

	if (wait_for((I915_READ(VLV_IOSF_DOORBELL_REQ) & IOSF_SB_BUSY) == 0,
		     5)) {
		DRM_ERROR("timeout waiting for pcode %s (%d) to finish\n",
			  opcode == PUNIT_OPCODE_REG_READ ? "read" : "write",
			  addr);
		return -ETIMEDOUT;
	}

	if (opcode == PUNIT_OPCODE_REG_READ)
		*val = I915_READ(VLV_IOSF_DATA);
	I915_WRITE(VLV_IOSF_DATA, 0);

	return 0;
}

int valleyview_punit_read(struct drm_i915_private *dev_priv, u8 addr, u32 *val)
{
	return vlv_punit_rw(dev_priv, IOSF_PORT_PUNIT, PUNIT_OPCODE_REG_READ,
			    addr, val);
}

int valleyview_punit_write(struct drm_i915_private *dev_priv, u8 addr, u32 val)
{
	return vlv_punit_rw(dev_priv, IOSF_PORT_PUNIT, PUNIT_OPCODE_REG_WRITE,
			    addr, &val);
}

int valleyview_nc_read(struct drm_i915_private *dev_priv, u8 addr, u32 *val)
{
	return vlv_punit_rw(dev_priv, IOSF_PORT_NC, PUNIT_OPCODE_REG_READ,
			    addr, val);
}

u32 intel_dpio_read(struct drm_i915_private *dev_priv, int reg)
{
	WARN_ON(!mutex_is_locked(&dev_priv->dpio_lock));

	if (wait_for_atomic_us((I915_READ(DPIO_PKT) & DPIO_BUSY) == 0, 100)) {
		DRM_ERROR("DPIO idle wait timed out\n");
		return 0;
	}

	I915_WRITE(DPIO_REG, reg);
	I915_WRITE(DPIO_PKT, DPIO_RID | DPIO_OP_READ | DPIO_PORTID |
		   DPIO_BYTE);
	if (wait_for_atomic_us((I915_READ(DPIO_PKT) & DPIO_BUSY) == 0, 100)) {
		DRM_ERROR("DPIO read wait timed out\n");
		return 0;
	}

	return I915_READ(DPIO_DATA);
}

void intel_dpio_write(struct drm_i915_private *dev_priv, int reg, u32 val)
{
	WARN_ON(!mutex_is_locked(&dev_priv->dpio_lock));

	if (wait_for_atomic_us((I915_READ(DPIO_PKT) & DPIO_BUSY) == 0, 100)) {
		DRM_ERROR("DPIO idle wait timed out\n");
		return;
	}

	I915_WRITE(DPIO_DATA, val);
	I915_WRITE(DPIO_REG, reg);
	I915_WRITE(DPIO_PKT, DPIO_RID | DPIO_OP_WRITE | DPIO_PORTID |
		   DPIO_BYTE);
	if (wait_for_atomic_us((I915_READ(DPIO_PKT) & DPIO_BUSY) == 0, 100))
		DRM_ERROR("DPIO write wait timed out\n");
}

/* SBI access */
u32 intel_sbi_read(struct drm_i915_private *dev_priv, u16 reg,
		   enum intel_sbi_destination destination)
{
	u32 value = 0;
	WARN_ON(!mutex_is_locked(&dev_priv->dpio_lock));

	if (wait_for((I915_READ(SBI_CTL_STAT) & SBI_BUSY) == 0,
				100)) {
		DRM_ERROR("timeout waiting for SBI to become ready\n");
		return 0;
	}

	I915_WRITE(SBI_ADDR, (reg << 16));

	if (destination == SBI_ICLK)
		value = SBI_CTL_DEST_ICLK | SBI_CTL_OP_CRRD;
	else
		value = SBI_CTL_DEST_MPHY | SBI_CTL_OP_IORD;
	I915_WRITE(SBI_CTL_STAT, value | SBI_BUSY);

	if (wait_for((I915_READ(SBI_CTL_STAT) & (SBI_BUSY | SBI_RESPONSE_FAIL)) == 0,
				100)) {
		DRM_ERROR("timeout waiting for SBI to complete read transaction\n");
		return 0;
	}

	return I915_READ(SBI_DATA);
}

void intel_sbi_write(struct drm_i915_private *dev_priv, u16 reg, u32 value,
		     enum intel_sbi_destination destination)
{
	u32 tmp;

	WARN_ON(!mutex_is_locked(&dev_priv->dpio_lock));

	if (wait_for((I915_READ(SBI_CTL_STAT) & SBI_BUSY) == 0,
				100)) {
		DRM_ERROR("timeout waiting for SBI to become ready\n");
		return;
	}

	I915_WRITE(SBI_ADDR, (reg << 16));
	I915_WRITE(SBI_DATA, value);

	if (destination == SBI_ICLK)
		tmp = SBI_CTL_DEST_ICLK | SBI_CTL_OP_CRWR;
	else
		tmp = SBI_CTL_DEST_MPHY | SBI_CTL_OP_IOWR;
	I915_WRITE(SBI_CTL_STAT, SBI_BUSY | tmp);

	if (wait_for((I915_READ(SBI_CTL_STAT) & (SBI_BUSY | SBI_RESPONSE_FAIL)) == 0,
				100)) {
		DRM_ERROR("timeout waiting for SBI to complete write transaction\n");
		return;
	}
}
