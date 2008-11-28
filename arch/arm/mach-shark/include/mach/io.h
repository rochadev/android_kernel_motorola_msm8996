/*
 * arch/arm/mach-shark/include/mach/io.h
 *
 * by Alexander Schulz
 *
 * derived from:
 * arch/arm/mach-ebsa110/include/mach/io.h
 * Copyright (C) 1997,1998 Russell King
 */

#ifndef __ASM_ARM_ARCH_IO_H
#define __ASM_ARM_ARCH_IO_H

#include <mach/hardware.h>

#define IO_SPACE_LIMIT 0xffffffff

/*
 * We use two different types of addressing - PC style addresses, and ARM
 * addresses.  PC style accesses the PC hardware with the normal PC IO
 * addresses, eg 0x3f8 for serial#1.  ARM addresses are 0x80000000+
 * and are translated to the start of IO.
 */
#define __PORT_PCIO(x)	(!((x) & 0x80000000))

#define __io(a)                 ((void __iomem *)(PCIO_BASE + (a)))


static inline unsigned int __ioaddr (unsigned int port)			\
{										\
	if (__PORT_PCIO(port))							\
		return (unsigned int)(PCIO_BASE + (port));			\
	else									\
		return (unsigned int)(IO_BASE + (port));			\
}

#define __mem_pci(addr) (addr)

#endif
