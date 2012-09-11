/*
 * arch/arm/mach-kirkwood/include/mach/kirkwood.h
 *
 * Generic definitions for Marvell Kirkwood SoC flavors:
 *  88F6180, 88F6192 and 88F6281.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef __ASM_ARCH_KIRKWOOD_H
#define __ASM_ARCH_KIRKWOOD_H

/*
 * Marvell Kirkwood address maps.
 *
 * phys
 * e0000000	PCIe #0 Memory space
 * e8000000	PCIe #1 Memory space
 * f1000000	on-chip peripheral registers
 * f2000000	PCIe #0 I/O space
 * f3000000	PCIe #1 I/O space
 * f4000000	NAND controller address window
 * f5000000	Security Accelerator SRAM
 *
 * virt		phys		size
 * fed00000	f1000000	1M	on-chip peripheral registers
 * fee00000	f2000000	1M	PCIe #0 I/O space
 * fef00000	f3000000	1M	PCIe #1 I/O space
 */

#define KIRKWOOD_SRAM_PHYS_BASE		0xf5000000
#define KIRKWOOD_SRAM_SIZE		SZ_2K

#define KIRKWOOD_NAND_MEM_PHYS_BASE	0xf4000000
#define KIRKWOOD_NAND_MEM_SIZE		SZ_1K

#define KIRKWOOD_PCIE1_IO_PHYS_BASE	0xf3000000
#define KIRKWOOD_PCIE1_IO_VIRT_BASE	0xfef00000
#define KIRKWOOD_PCIE1_IO_BUS_BASE	0x00100000
#define KIRKWOOD_PCIE1_IO_SIZE		SZ_1M

#define KIRKWOOD_PCIE_IO_PHYS_BASE	0xf2000000
#define KIRKWOOD_PCIE_IO_VIRT_BASE	0xfee00000
#define KIRKWOOD_PCIE_IO_BUS_BASE	0x00000000
#define KIRKWOOD_PCIE_IO_SIZE		SZ_1M

#define KIRKWOOD_REGS_PHYS_BASE		0xf1000000
#define KIRKWOOD_REGS_VIRT_BASE		0xfed00000
#define KIRKWOOD_REGS_SIZE		SZ_1M

#define KIRKWOOD_PCIE_MEM_PHYS_BASE	0xe0000000
#define KIRKWOOD_PCIE_MEM_BUS_BASE	0xe0000000
#define KIRKWOOD_PCIE_MEM_SIZE		SZ_128M

#define KIRKWOOD_PCIE1_MEM_PHYS_BASE	0xe8000000
#define KIRKWOOD_PCIE1_MEM_BUS_BASE	0xe8000000
#define KIRKWOOD_PCIE1_MEM_SIZE		SZ_128M

/*
 * Register Map
 */
#define DDR_VIRT_BASE		(KIRKWOOD_REGS_VIRT_BASE + 0x00000)
#define  DDR_WINDOW_CPU_BASE	(DDR_VIRT_BASE + 0x1500)
#define DDR_OPERATION_BASE	(DDR_VIRT_BASE + 0x1418)

#define DEV_BUS_PHYS_BASE	(KIRKWOOD_REGS_PHYS_BASE + 0x10000)
#define DEV_BUS_VIRT_BASE	(KIRKWOOD_REGS_VIRT_BASE + 0x10000)
#define  SAMPLE_AT_RESET	(DEV_BUS_VIRT_BASE + 0x0030)
#define  DEVICE_ID		(DEV_BUS_VIRT_BASE + 0x0034)
#define  GPIO_LOW_VIRT_BASE	(DEV_BUS_VIRT_BASE + 0x0100)
#define  GPIO_HIGH_VIRT_BASE	(DEV_BUS_VIRT_BASE + 0x0140)
#define  RTC_PHYS_BASE		(DEV_BUS_PHYS_BASE + 0x0300)
#define  SPI_PHYS_BASE		(DEV_BUS_PHYS_BASE + 0x0600)
#define  I2C_PHYS_BASE		(DEV_BUS_PHYS_BASE + 0x1000)
#define  UART0_PHYS_BASE	(DEV_BUS_PHYS_BASE + 0x2000)
#define  UART0_VIRT_BASE	(DEV_BUS_VIRT_BASE + 0x2000)
#define  UART1_PHYS_BASE	(DEV_BUS_PHYS_BASE + 0x2100)
#define  UART1_VIRT_BASE	(DEV_BUS_VIRT_BASE + 0x2100)

#define BRIDGE_VIRT_BASE	(KIRKWOOD_REGS_VIRT_BASE + 0x20000)
#define BRIDGE_PHYS_BASE	(KIRKWOOD_REGS_PHYS_BASE + 0x20000)

#define CRYPTO_PHYS_BASE	(KIRKWOOD_REGS_PHYS_BASE + 0x30000)

#define PCIE_VIRT_BASE		(KIRKWOOD_REGS_VIRT_BASE + 0x40000)
#define PCIE_LINK_CTRL		(PCIE_VIRT_BASE + 0x70)
#define PCIE_STATUS		(PCIE_VIRT_BASE + 0x1a04)
#define PCIE1_VIRT_BASE		(KIRKWOOD_REGS_VIRT_BASE + 0x44000)
#define PCIE1_LINK_CTRL		(PCIE1_VIRT_BASE + 0x70)
#define PCIE1_STATUS		(PCIE1_VIRT_BASE + 0x1a04)

#define USB_PHYS_BASE		(KIRKWOOD_REGS_PHYS_BASE + 0x50000)

#define XOR0_PHYS_BASE		(KIRKWOOD_REGS_PHYS_BASE + 0x60800)
#define XOR0_VIRT_BASE		(KIRKWOOD_REGS_VIRT_BASE + 0x60800)
#define XOR1_PHYS_BASE		(KIRKWOOD_REGS_PHYS_BASE + 0x60900)
#define XOR1_VIRT_BASE		(KIRKWOOD_REGS_VIRT_BASE + 0x60900)
#define XOR0_HIGH_PHYS_BASE	(KIRKWOOD_REGS_PHYS_BASE + 0x60A00)
#define XOR0_HIGH_VIRT_BASE	(KIRKWOOD_REGS_VIRT_BASE + 0x60A00)
#define XOR1_HIGH_PHYS_BASE	(KIRKWOOD_REGS_PHYS_BASE + 0x60B00)
#define XOR1_HIGH_VIRT_BASE	(KIRKWOOD_REGS_VIRT_BASE + 0x60B00)

#define GE00_PHYS_BASE		(KIRKWOOD_REGS_PHYS_BASE + 0x70000)
#define GE01_PHYS_BASE		(KIRKWOOD_REGS_PHYS_BASE + 0x74000)

#define SATA_PHYS_BASE		(KIRKWOOD_REGS_PHYS_BASE + 0x80000)
#define SATA_VIRT_BASE		(KIRKWOOD_REGS_VIRT_BASE + 0x80000)
#define SATA0_IF_CTRL		(SATA_VIRT_BASE + 0x2050)
#define SATA0_PHY_MODE_2	(SATA_VIRT_BASE + 0x2330)
#define SATA1_IF_CTRL		(SATA_VIRT_BASE + 0x4050)
#define SATA1_PHY_MODE_2	(SATA_VIRT_BASE + 0x4330)

#define SDIO_PHYS_BASE		(KIRKWOOD_REGS_PHYS_BASE + 0x90000)

#define AUDIO_PHYS_BASE		(KIRKWOOD_REGS_PHYS_BASE + 0xA0000)
#define AUDIO_VIRT_BASE		(KIRKWOOD_REGS_VIRT_BASE + 0xA0000)

/*
 * Supported devices and revisions.
 */
#define MV88F6281_DEV_ID	0x6281
#define MV88F6281_REV_Z0	0
#define MV88F6281_REV_A0	2
#define MV88F6281_REV_A1	3

#define MV88F6192_DEV_ID	0x6192
#define MV88F6192_REV_Z0	0
#define MV88F6192_REV_A0	2
#define MV88F6192_REV_A1	3

#define MV88F6180_DEV_ID	0x6180
#define MV88F6180_REV_A0	2
#define MV88F6180_REV_A1	3

#define MV88F6282_DEV_ID	0x6282
#define MV88F6282_REV_A0	0
#define MV88F6282_REV_A1	1
#endif
