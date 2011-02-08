#ifndef _LINUX_IRQ_H
#define _LINUX_IRQ_H

/*
 * Please do not include this file in generic code.  There is currently
 * no requirement for any architecture to implement anything held
 * within this file.
 *
 * Thanks. --rmk
 */

#include <linux/smp.h>

#ifndef CONFIG_S390

#include <linux/linkage.h>
#include <linux/cache.h>
#include <linux/spinlock.h>
#include <linux/cpumask.h>
#include <linux/gfp.h>
#include <linux/irqreturn.h>
#include <linux/irqnr.h>
#include <linux/errno.h>
#include <linux/topology.h>
#include <linux/wait.h>

#include <asm/irq.h>
#include <asm/ptrace.h>
#include <asm/irq_regs.h>

struct irq_desc;
typedef	void (*irq_flow_handler_t)(unsigned int irq,
					    struct irq_desc *desc);


/*
 * IRQ line status.
 *
 * Bits 0-7 are the same as the IRQF_* bits in linux/interrupt.h
 *
 * IRQ_TYPE_NONE		- default, unspecified type
 * IRQ_TYPE_EDGE_RISING		- rising edge triggered
 * IRQ_TYPE_EDGE_FALLING	- falling edge triggered
 * IRQ_TYPE_EDGE_BOTH		- rising and falling edge triggered
 * IRQ_TYPE_LEVEL_HIGH		- high level triggered
 * IRQ_TYPE_LEVEL_LOW		- low level triggered
 * IRQ_TYPE_LEVEL_MASK		- Mask to filter out the level bits
 * IRQ_TYPE_SENSE_MASK		- Mask for all the above bits
 * IRQ_TYPE_PROBE		- Special flag for probing in progress
 *
 * Bits which can be modified via irq_set/clear/modify_status_flags()
 * IRQ_LEVEL			- Interrupt is level type. Will be also
 *				  updated in the code when the above trigger
 *				  bits are modified via set_irq_type()
 * IRQ_PER_CPU			- Mark an interrupt PER_CPU. Will protect
 *				  it from affinity setting
 * IRQ_NOPROBE			- Interrupt cannot be probed by autoprobing
 * IRQ_NOREQUEST		- Interrupt cannot be requested via
 *				  request_irq()
 * IRQ_NOAUTOEN			- Interrupt is not automatically enabled in
 *				  request/setup_irq()
 * IRQ_NO_BALANCING		- Interrupt cannot be balanced (affinity set)
 * IRQ_MOVE_PCNTXT		- Interrupt can be migrated from process context
 * IRQ_NESTED_TRHEAD		- Interrupt nests into another thread
 *
 * Deprecated bits. They are kept updated as long as
 * CONFIG_GENERIC_HARDIRQS_NO_COMPAT is not set. Will go away soon. These bits
 * are internal state of the core code and if you really need to acces
 * them then talk to the genirq maintainer instead of hacking
 * something weird.
 *
 */
enum {
	IRQ_TYPE_NONE		= 0x00000000,
	IRQ_TYPE_EDGE_RISING	= 0x00000001,
	IRQ_TYPE_EDGE_FALLING	= 0x00000002,
	IRQ_TYPE_EDGE_BOTH	= (IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING),
	IRQ_TYPE_LEVEL_HIGH	= 0x00000004,
	IRQ_TYPE_LEVEL_LOW	= 0x00000008,
	IRQ_TYPE_LEVEL_MASK	= (IRQ_TYPE_LEVEL_LOW | IRQ_TYPE_LEVEL_HIGH),
	IRQ_TYPE_SENSE_MASK	= 0x0000000f,

	IRQ_TYPE_PROBE		= 0x00000010,

	IRQ_LEVEL		= (1 <<  8),
	IRQ_PER_CPU		= (1 <<  9),
	IRQ_NOPROBE		= (1 << 10),
	IRQ_NOREQUEST		= (1 << 11),
	IRQ_NOAUTOEN		= (1 << 12),
	IRQ_NO_BALANCING	= (1 << 13),
	IRQ_MOVE_PCNTXT		= (1 << 14),
	IRQ_NESTED_THREAD	= (1 << 15),

#ifndef CONFIG_GENERIC_HARDIRQS_NO_COMPAT
	IRQ_INPROGRESS		= (1 << 16),
	IRQ_REPLAY		= (1 << 17),
	IRQ_WAITING		= (1 << 18),
	IRQ_DISABLED		= (1 << 19),
	IRQ_PENDING		= (1 << 20),
	IRQ_MASKED		= (1 << 21),
	IRQ_MOVE_PENDING	= (1 << 22),
	IRQ_AFFINITY_SET	= (1 << 23),
	IRQ_WAKEUP		= (1 << 24),
#endif
};

#define IRQF_MODIFY_MASK	\
	(IRQ_TYPE_SENSE_MASK | IRQ_NOPROBE | IRQ_NOREQUEST | \
	 IRQ_NOAUTOEN | IRQ_MOVE_PCNTXT | IRQ_LEVEL | IRQ_NO_BALANCING | \
	 IRQ_PER_CPU | IRQ_NESTED_THREAD)

#define IRQ_NO_BALANCING_MASK	(IRQ_PER_CPU | IRQ_NO_BALANCING)

static inline __deprecated bool CHECK_IRQ_PER_CPU(unsigned int status)
{
	return status & IRQ_PER_CPU;
}

/*
 * Return value for chip->irq_set_affinity()
 *
 * IRQ_SET_MASK_OK	- OK, core updates irq_data.affinity
 * IRQ_SET_MASK_NOCPY	- OK, chip did update irq_data.affinity
 */
enum {
	IRQ_SET_MASK_OK = 0,
	IRQ_SET_MASK_OK_NOCOPY,
};

struct msi_desc;

/**
 * struct irq_data - per irq and irq chip data passed down to chip functions
 * @irq:		interrupt number
 * @node:		node index useful for balancing
 * @state_use_accessor: status information for irq chip functions.
 *			Use accessor functions to deal with it
 * @chip:		low level interrupt hardware access
 * @handler_data:	per-IRQ data for the irq_chip methods
 * @chip_data:		platform-specific per-chip private data for the chip
 *			methods, to allow shared chip implementations
 * @msi_desc:		MSI descriptor
 * @affinity:		IRQ affinity on SMP
 *
 * The fields here need to overlay the ones in irq_desc until we
 * cleaned up the direct references and switched everything over to
 * irq_data.
 */
struct irq_data {
	unsigned int		irq;
	unsigned int		node;
	unsigned int		state_use_accessors;
	struct irq_chip		*chip;
	void			*handler_data;
	void			*chip_data;
	struct msi_desc		*msi_desc;
#ifdef CONFIG_SMP
	cpumask_var_t		affinity;
#endif
};

/*
 * Bit masks for irq_data.state
 *
 * IRQD_TRIGGER_MASK		- Mask for the trigger type bits
 * IRQD_SETAFFINITY_PENDING	- Affinity setting is pending
 * IRQD_NO_BALANCING		- Balancing disabled for this IRQ
 * IRQD_PER_CPU			- Interrupt is per cpu
 * IRQD_AFFINITY_SET		- Interrupt affinity was set
 * IRQD_LEVEL			- Interrupt is level triggered
 */
enum {
	IRQD_TRIGGER_MASK		= 0xf,
	IRQD_SETAFFINITY_PENDING	= (1 <<  8),
	IRQD_NO_BALANCING		= (1 << 10),
	IRQD_PER_CPU			= (1 << 11),
	IRQD_AFFINITY_SET		= (1 << 12),
	IRQD_LEVEL			= (1 << 13),
};

static inline bool irqd_is_setaffinity_pending(struct irq_data *d)
{
	return d->state_use_accessors & IRQD_SETAFFINITY_PENDING;
}

static inline bool irqd_is_per_cpu(struct irq_data *d)
{
	return d->state_use_accessors & IRQD_PER_CPU;
}

static inline bool irqd_can_balance(struct irq_data *d)
{
	return !(d->state_use_accessors & (IRQD_PER_CPU | IRQD_NO_BALANCING));
}

static inline bool irqd_affinity_was_set(struct irq_data *d)
{
	return d->state_use_accessors & IRQD_AFFINITY_SET;
}

static inline u32 irqd_get_trigger_type(struct irq_data *d)
{
	return d->state_use_accessors & IRQD_TRIGGER_MASK;
}

/*
 * Must only be called inside irq_chip.irq_set_type() functions.
 */
static inline void irqd_set_trigger_type(struct irq_data *d, u32 type)
{
	d->state_use_accessors &= ~IRQD_TRIGGER_MASK;
	d->state_use_accessors |= type & IRQD_TRIGGER_MASK;
}

static inline bool irqd_is_level_type(struct irq_data *d)
{
	return d->state_use_accessors & IRQD_LEVEL;
}

/**
 * struct irq_chip - hardware interrupt chip descriptor
 *
 * @name:		name for /proc/interrupts
 * @startup:		deprecated, replaced by irq_startup
 * @shutdown:		deprecated, replaced by irq_shutdown
 * @enable:		deprecated, replaced by irq_enable
 * @disable:		deprecated, replaced by irq_disable
 * @ack:		deprecated, replaced by irq_ack
 * @mask:		deprecated, replaced by irq_mask
 * @mask_ack:		deprecated, replaced by irq_mask_ack
 * @unmask:		deprecated, replaced by irq_unmask
 * @eoi:		deprecated, replaced by irq_eoi
 * @end:		deprecated, will go away with __do_IRQ()
 * @set_affinity:	deprecated, replaced by irq_set_affinity
 * @retrigger:		deprecated, replaced by irq_retrigger
 * @set_type:		deprecated, replaced by irq_set_type
 * @set_wake:		deprecated, replaced by irq_wake
 * @bus_lock:		deprecated, replaced by irq_bus_lock
 * @bus_sync_unlock:	deprecated, replaced by irq_bus_sync_unlock
 *
 * @irq_startup:	start up the interrupt (defaults to ->enable if NULL)
 * @irq_shutdown:	shut down the interrupt (defaults to ->disable if NULL)
 * @irq_enable:		enable the interrupt (defaults to chip->unmask if NULL)
 * @irq_disable:	disable the interrupt
 * @irq_ack:		start of a new interrupt
 * @irq_mask:		mask an interrupt source
 * @irq_mask_ack:	ack and mask an interrupt source
 * @irq_unmask:		unmask an interrupt source
 * @irq_eoi:		end of interrupt
 * @irq_set_affinity:	set the CPU affinity on SMP machines
 * @irq_retrigger:	resend an IRQ to the CPU
 * @irq_set_type:	set the flow type (IRQ_TYPE_LEVEL/etc.) of an IRQ
 * @irq_set_wake:	enable/disable power-management wake-on of an IRQ
 * @irq_bus_lock:	function to lock access to slow bus (i2c) chips
 * @irq_bus_sync_unlock:function to sync and unlock slow bus (i2c) chips
 *
 * @release:		release function solely used by UML
 */
struct irq_chip {
	const char	*name;
#ifndef CONFIG_GENERIC_HARDIRQS_NO_DEPRECATED
	unsigned int	(*startup)(unsigned int irq);
	void		(*shutdown)(unsigned int irq);
	void		(*enable)(unsigned int irq);
	void		(*disable)(unsigned int irq);

	void		(*ack)(unsigned int irq);
	void		(*mask)(unsigned int irq);
	void		(*mask_ack)(unsigned int irq);
	void		(*unmask)(unsigned int irq);
	void		(*eoi)(unsigned int irq);

	void		(*end)(unsigned int irq);
	int		(*set_affinity)(unsigned int irq,
					const struct cpumask *dest);
	int		(*retrigger)(unsigned int irq);
	int		(*set_type)(unsigned int irq, unsigned int flow_type);
	int		(*set_wake)(unsigned int irq, unsigned int on);

	void		(*bus_lock)(unsigned int irq);
	void		(*bus_sync_unlock)(unsigned int irq);
#endif
	unsigned int	(*irq_startup)(struct irq_data *data);
	void		(*irq_shutdown)(struct irq_data *data);
	void		(*irq_enable)(struct irq_data *data);
	void		(*irq_disable)(struct irq_data *data);

	void		(*irq_ack)(struct irq_data *data);
	void		(*irq_mask)(struct irq_data *data);
	void		(*irq_mask_ack)(struct irq_data *data);
	void		(*irq_unmask)(struct irq_data *data);
	void		(*irq_eoi)(struct irq_data *data);

	int		(*irq_set_affinity)(struct irq_data *data, const struct cpumask *dest, bool force);
	int		(*irq_retrigger)(struct irq_data *data);
	int		(*irq_set_type)(struct irq_data *data, unsigned int flow_type);
	int		(*irq_set_wake)(struct irq_data *data, unsigned int on);

	void		(*irq_bus_lock)(struct irq_data *data);
	void		(*irq_bus_sync_unlock)(struct irq_data *data);

	/* Currently used only by UML, might disappear one day.*/
#ifdef CONFIG_IRQ_RELEASE_METHOD
	void		(*release)(unsigned int irq, void *dev_id);
#endif
};

/* This include will go away once we isolated irq_desc usage to core code */
#include <linux/irqdesc.h>

/*
 * Pick up the arch-dependent methods:
 */
#include <asm/hw_irq.h>

#ifndef NR_IRQS_LEGACY
# define NR_IRQS_LEGACY 0
#endif

#ifndef ARCH_IRQ_INIT_FLAGS
# define ARCH_IRQ_INIT_FLAGS	0
#endif

#define IRQ_DEFAULT_INIT_FLAGS	ARCH_IRQ_INIT_FLAGS

struct irqaction;
extern int setup_irq(unsigned int irq, struct irqaction *new);
extern void remove_irq(unsigned int irq, struct irqaction *act);

#ifdef CONFIG_GENERIC_HARDIRQS

#if defined(CONFIG_SMP) && defined(CONFIG_GENERIC_PENDING_IRQ)
void move_native_irq(int irq);
void move_masked_irq(int irq);
#else
static inline void move_native_irq(int irq) { }
static inline void move_masked_irq(int irq) { }
#endif

extern int no_irq_affinity;

/* Handle irq action chains: */
extern irqreturn_t handle_IRQ_event(unsigned int irq, struct irqaction *action);

/*
 * Built-in IRQ handlers for various IRQ types,
 * callable via desc->handle_irq()
 */
extern void handle_level_irq(unsigned int irq, struct irq_desc *desc);
extern void handle_fasteoi_irq(unsigned int irq, struct irq_desc *desc);
extern void handle_edge_irq(unsigned int irq, struct irq_desc *desc);
extern void handle_simple_irq(unsigned int irq, struct irq_desc *desc);
extern void handle_percpu_irq(unsigned int irq, struct irq_desc *desc);
extern void handle_bad_irq(unsigned int irq, struct irq_desc *desc);
extern void handle_nested_irq(unsigned int irq);

/* Handling of unhandled and spurious interrupts: */
extern void note_interrupt(unsigned int irq, struct irq_desc *desc,
			   irqreturn_t action_ret);


/* Enable/disable irq debugging output: */
extern int noirqdebug_setup(char *str);

/* Checks whether the interrupt can be requested by request_irq(): */
extern int can_request_irq(unsigned int irq, unsigned long irqflags);

/* Dummy irq-chip implementations: */
extern struct irq_chip no_irq_chip;
extern struct irq_chip dummy_irq_chip;

extern void
set_irq_chip_and_handler(unsigned int irq, struct irq_chip *chip,
			 irq_flow_handler_t handle);
extern void
set_irq_chip_and_handler_name(unsigned int irq, struct irq_chip *chip,
			      irq_flow_handler_t handle, const char *name);

extern void
__set_irq_handler(unsigned int irq, irq_flow_handler_t handle, int is_chained,
		  const char *name);

/*
 * Set a highlevel flow handler for a given IRQ:
 */
static inline void
set_irq_handler(unsigned int irq, irq_flow_handler_t handle)
{
	__set_irq_handler(irq, handle, 0, NULL);
}

/*
 * Set a highlevel chained flow handler for a given IRQ.
 * (a chained handler is automatically enabled and set to
 *  IRQ_NOREQUEST and IRQ_NOPROBE)
 */
static inline void
set_irq_chained_handler(unsigned int irq, irq_flow_handler_t handle)
{
	__set_irq_handler(irq, handle, 1, NULL);
}

void irq_modify_status(unsigned int irq, unsigned long clr, unsigned long set);

static inline void irq_set_status_flags(unsigned int irq, unsigned long set)
{
	irq_modify_status(irq, 0, set);
}

static inline void irq_clear_status_flags(unsigned int irq, unsigned long clr)
{
	irq_modify_status(irq, clr, 0);
}

static inline void irq_set_noprobe(unsigned int irq)
{
	irq_modify_status(irq, 0, IRQ_NOPROBE);
}

static inline void irq_set_probe(unsigned int irq)
{
	irq_modify_status(irq, IRQ_NOPROBE, 0);
}

static inline void irq_set_nested_thread(unsigned int irq, bool nest)
{
	if (nest)
		irq_set_status_flags(irq, IRQ_NESTED_THREAD);
	else
		irq_clear_status_flags(irq, IRQ_NESTED_THREAD);
}

/* Handle dynamic irq creation and destruction */
extern unsigned int create_irq_nr(unsigned int irq_want, int node);
extern int create_irq(void);
extern void destroy_irq(unsigned int irq);

/*
 * Dynamic irq helper functions. Obsolete. Use irq_alloc_desc* and
 * irq_free_desc instead.
 */
extern void dynamic_irq_cleanup(unsigned int irq);
static inline void dynamic_irq_init(unsigned int irq)
{
	dynamic_irq_cleanup(irq);
}

/* Set/get chip/data for an IRQ: */
extern int irq_set_chip(unsigned int irq, struct irq_chip *chip);
extern int irq_set_handler_data(unsigned int irq, void *data);
extern int irq_set_chip_data(unsigned int irq, void *data);
extern int irq_set_irq_type(unsigned int irq, unsigned int type);
extern int irq_set_msi_desc(unsigned int irq, struct msi_desc *entry);
extern struct irq_data *irq_get_irq_data(unsigned int irq);

static inline struct irq_chip *irq_get_chip(unsigned int irq)
{
	struct irq_data *d = irq_get_irq_data(irq);
	return d ? d->chip : NULL;
}

static inline struct irq_chip *irq_data_get_irq_chip(struct irq_data *d)
{
	return d->chip;
}

static inline void *irq_get_chip_data(unsigned int irq)
{
	struct irq_data *d = irq_get_irq_data(irq);
	return d ? d->chip_data : NULL;
}

static inline void *irq_data_get_irq_chip_data(struct irq_data *d)
{
	return d->chip_data;
}

static inline void *irq_get_handler_data(unsigned int irq)
{
	struct irq_data *d = irq_get_irq_data(irq);
	return d ? d->handler_data : NULL;
}

static inline void *irq_data_get_irq_handler_data(struct irq_data *d)
{
	return d->handler_data;
}

static inline struct msi_desc *irq_get_msi_desc(unsigned int irq)
{
	struct irq_data *d = irq_get_irq_data(irq);
	return d ? d->msi_desc : NULL;
}

static inline struct msi_desc *irq_data_get_msi(struct irq_data *d)
{
	return d->msi_desc;
}

#ifndef CONFIG_GENERIC_HARDIRQS_NO_COMPAT
/* Please do not use: Use the replacement functions instead */
static inline int set_irq_chip(unsigned int irq, struct irq_chip *chip)
{
	return irq_set_chip(irq, chip);
}
static inline int set_irq_data(unsigned int irq, void *data)
{
	return irq_set_handler_data(irq, data);
}
static inline int set_irq_chip_data(unsigned int irq, void *data)
{
	return irq_set_chip_data(irq, data);
}
static inline int set_irq_type(unsigned int irq, unsigned int type)
{
	return irq_set_irq_type(irq, type);
}
static inline int set_irq_msi(unsigned int irq, struct msi_desc *entry)
{
	return irq_set_msi_desc(irq, entry);
}
static inline struct irq_chip *get_irq_chip(unsigned int irq)
{
	return irq_get_chip(irq);
}
static inline void *get_irq_chip_data(unsigned int irq)
{
	return irq_get_chip_data(irq);
}
static inline void *get_irq_data(unsigned int irq)
{
	return irq_get_handler_data(irq);
}
static inline void *irq_data_get_irq_data(struct irq_data *d)
{
	return irq_data_get_irq_handler_data(d);
}
static inline struct msi_desc *get_irq_msi(unsigned int irq)
{
	return irq_get_msi_desc(irq);
}
static inline void set_irq_noprobe(unsigned int irq)
{
	irq_set_noprobe(irq);
}
static inline void set_irq_probe(unsigned int irq)
{
	irq_set_probe(irq);
}
static inline void set_irq_nested_thread(unsigned int irq, int nest)
{
	irq_set_nested_thread(irq, nest);
}
#endif

int irq_alloc_descs(int irq, unsigned int from, unsigned int cnt, int node);
void irq_free_descs(unsigned int irq, unsigned int cnt);
int irq_reserve_irqs(unsigned int from, unsigned int cnt);

static inline int irq_alloc_desc(int node)
{
	return irq_alloc_descs(-1, 0, 1, node);
}

static inline int irq_alloc_desc_at(unsigned int at, int node)
{
	return irq_alloc_descs(at, at, 1, node);
}

static inline int irq_alloc_desc_from(unsigned int from, int node)
{
	return irq_alloc_descs(-1, from, 1, node);
}

static inline void irq_free_desc(unsigned int irq)
{
	irq_free_descs(irq, 1);
}

static inline int irq_reserve_irq(unsigned int irq)
{
	return irq_reserve_irqs(irq, 1);
}

#endif /* CONFIG_GENERIC_HARDIRQS */

#endif /* !CONFIG_S390 */

#endif /* _LINUX_IRQ_H */
