/*
 * Definitions for IDT RC323434 CPU.
 */

#ifndef _ASM_RC32434_RC32434_H_
#define _ASM_RC32434_RC32434_H_

#include <linux/delay.h>
#include <linux/io.h>

#define IDT_CLOCK_MULT		2

/* cpu pipeline flush */
static inline void rc32434_sync(void)
{
	__asm__ volatile ("sync");
}

static inline void rc32434_sync_udelay(int us)
{
	__asm__ volatile ("sync");
	udelay(us);
}

static inline void rc32434_sync_delay(int ms)
{
	__asm__ volatile ("sync");
	mdelay(ms);
}

#endif  /* _ASM_RC32434_RC32434_H_ */
