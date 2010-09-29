#ifndef _ASM_GENERIC_BITOPS_FIND_H_
#define _ASM_GENERIC_BITOPS_FIND_H_

#ifndef CONFIG_GENERIC_FIND_NEXT_BIT
extern unsigned long find_next_bit(const unsigned long *addr, unsigned long
		size, unsigned long offset);

extern unsigned long find_next_zero_bit(const unsigned long *addr, unsigned
		long size, unsigned long offset);
#endif

#ifdef CONFIG_GENERIC_FIND_FIRST_BIT

/**
 * find_first_bit - find the first set bit in a memory region
 * @addr: The address to start the search at
 * @size: The maximum size to search
 *
 * Returns the bit number of the first set bit.
 */
extern unsigned long find_first_bit(const unsigned long *addr,
				    unsigned long size);

/**
 * find_first_zero_bit - find the first cleared bit in a memory region
 * @addr: The address to start the search at
 * @size: The maximum size to search
 *
 * Returns the bit number of the first cleared bit.
 */
extern unsigned long find_first_zero_bit(const unsigned long *addr,
					 unsigned long size);
#else /* CONFIG_GENERIC_FIND_FIRST_BIT */

#define find_first_bit(addr, size) find_next_bit((addr), (size), 0)
#define find_first_zero_bit(addr, size) find_next_zero_bit((addr), (size), 0)

#endif /* CONFIG_GENERIC_FIND_FIRST_BIT */

#endif /*_ASM_GENERIC_BITOPS_FIND_H_ */
