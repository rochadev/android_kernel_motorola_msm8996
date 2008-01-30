#ifndef _ASM_X8664_PROTO_H
#define _ASM_X8664_PROTO_H 1

#include <asm/ldt.h>

/* misc architecture specific prototypes */

struct cpuinfo_x86; 
struct pt_regs;

extern void early_idt_handler(void);

extern void init_memory_mapping(unsigned long start, unsigned long end);

extern void system_call(void); 
extern int kernel_syscall(void);
extern void syscall_init(void);

extern void ia32_syscall(void);
extern void ia32_cstar_target(void); 
extern void ia32_sysenter_target(void); 

extern void config_acpi_tables(void);
extern void ia32_syscall(void);

extern void reserve_bootmem_generic(unsigned long phys, unsigned len);

extern void load_gs_index(unsigned gs);

extern void exception_table_check(void);

extern void swap_low_mappings(void);

extern void syscall32_cpu_init(void);

extern void check_efer(void);

extern unsigned long table_start, table_end;

extern int exception_trace;

extern int reboot_force;

extern int gsi_irq_sharing(int gsi);

long do_arch_prctl(struct task_struct *task, int code, unsigned long addr);

#define round_up(x,y) (((x) + (y) - 1) & ~((y)-1))
#define round_down(x,y) ((x) & ~((y)-1))

#endif
