/*
 * Windfarm PowerMac thermal control. Core
 *
 * (c) Copyright 2005 Benjamin Herrenschmidt, IBM Corp.
 *                    <benh@kernel.crashing.org>
 *
 * Released under the term of the GNU GPL v2.
 *
 * This core code tracks the list of sensors & controls, register
 * clients, and holds the kernel thread used for control.
 *
 * TODO:
 *
 * Add some information about sensor/control type and data format to
 * sensors/controls, and have the sysfs attribute stuff be moved
 * generically here instead of hard coded in the platform specific
 * driver as it us currently
 *
 * This however requires solving some annoying lifetime issues with
 * sysfs which doesn't seem to have lifetime rules for struct attribute,
 * I may have to create full features kobjects for every sensor/control
 * instead which is a bit of an overkill imho
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/smp_lock.h>
#include <linux/kthread.h>
#include <linux/jiffies.h>
#include <linux/reboot.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include "windfarm.h"

#define VERSION "0.2"

#undef DEBUG

#ifdef DEBUG
#define DBG(args...)	printk(args)
#else
#define DBG(args...)	do { } while(0)
#endif

static LIST_HEAD(wf_controls);
static LIST_HEAD(wf_sensors);
static DECLARE_MUTEX(wf_lock);
static struct notifier_block *wf_client_list;
static int wf_client_count;
static unsigned int wf_overtemp;
static unsigned int wf_overtemp_counter;
struct task_struct *wf_thread;

/*
 * Utilities & tick thread
 */

static inline void wf_notify(int event, void *param)
{
	notifier_call_chain(&wf_client_list, event, param);
}

int wf_critical_overtemp(void)
{
	static char * critical_overtemp_path = "/sbin/critical_overtemp";
	char *argv[] = { critical_overtemp_path, NULL };
	static char *envp[] = { "HOME=/",
				"TERM=linux",
				"PATH=/sbin:/usr/sbin:/bin:/usr/bin",
				NULL };

	return call_usermodehelper(critical_overtemp_path, argv, envp, 0);
}
EXPORT_SYMBOL_GPL(wf_critical_overtemp);

static int wf_thread_func(void *data)
{
	unsigned long next, delay;

	next = jiffies;

	DBG("wf: thread started\n");

	while(!kthread_should_stop()) {
		try_to_freeze();

		if (time_after_eq(jiffies, next)) {
			wf_notify(WF_EVENT_TICK, NULL);
			if (wf_overtemp) {
				wf_overtemp_counter++;
				/* 10 seconds overtemp, notify userland */
				if (wf_overtemp_counter > 10)
					wf_critical_overtemp();
				/* 30 seconds, shutdown */
				if (wf_overtemp_counter > 30) {
					printk(KERN_ERR "windfarm: Overtemp "
					       "for more than 30"
					       " seconds, shutting down\n");
					machine_power_off();
				}
			}
			next += HZ;
		}

		delay = next - jiffies;
		if (delay <= HZ)
			schedule_timeout_interruptible(delay);

		/* there should be no signal, but oh well */
		if (signal_pending(current)) {
			printk(KERN_WARNING "windfarm: thread got sigl !\n");
			break;
		}
	}

	DBG("wf: thread stopped\n");

	return 0;
}

static void wf_start_thread(void)
{
	wf_thread = kthread_run(wf_thread_func, NULL, "kwindfarm");
	if (IS_ERR(wf_thread)) {
		printk(KERN_ERR "windfarm: failed to create thread,err %ld\n",
		       PTR_ERR(wf_thread));
		wf_thread = NULL;
	}
}


static void wf_stop_thread(void)
{
	if (wf_thread)
		kthread_stop(wf_thread);
	wf_thread = NULL;
}

/*
 * Controls
 */

static void wf_control_release(struct kref *kref)
{
	struct wf_control *ct = container_of(kref, struct wf_control, ref);

	DBG("wf: Deleting control %s\n", ct->name);

	if (ct->ops && ct->ops->release)
		ct->ops->release(ct);
	else
		kfree(ct);
}

int wf_register_control(struct wf_control *new_ct)
{
	struct wf_control *ct;

	down(&wf_lock);
	list_for_each_entry(ct, &wf_controls, link) {
		if (!strcmp(ct->name, new_ct->name)) {
			printk(KERN_WARNING "windfarm: trying to register"
			       " duplicate control %s\n", ct->name);
			up(&wf_lock);
			return -EEXIST;
		}
	}
	kref_init(&new_ct->ref);
	list_add(&new_ct->link, &wf_controls);

	DBG("wf: Registered control %s\n", new_ct->name);

	wf_notify(WF_EVENT_NEW_CONTROL, new_ct);
	up(&wf_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(wf_register_control);

void wf_unregister_control(struct wf_control *ct)
{
	down(&wf_lock);
	list_del(&ct->link);
	up(&wf_lock);

	DBG("wf: Unregistered control %s\n", ct->name);

	kref_put(&ct->ref, wf_control_release);
}
EXPORT_SYMBOL_GPL(wf_unregister_control);

struct wf_control * wf_find_control(const char *name)
{
	struct wf_control *ct;

	down(&wf_lock);
	list_for_each_entry(ct, &wf_controls, link) {
		if (!strcmp(ct->name, name)) {
			if (wf_get_control(ct))
				ct = NULL;
			up(&wf_lock);
			return ct;
		}
	}
	up(&wf_lock);
	return NULL;
}
EXPORT_SYMBOL_GPL(wf_find_control);

int wf_get_control(struct wf_control *ct)
{
	if (!try_module_get(ct->ops->owner))
		return -ENODEV;
	kref_get(&ct->ref);
	return 0;
}
EXPORT_SYMBOL_GPL(wf_get_control);

void wf_put_control(struct wf_control *ct)
{
	struct module *mod = ct->ops->owner;
	kref_put(&ct->ref, wf_control_release);
	module_put(mod);
}
EXPORT_SYMBOL_GPL(wf_put_control);


/*
 * Sensors
 */


static void wf_sensor_release(struct kref *kref)
{
	struct wf_sensor *sr = container_of(kref, struct wf_sensor, ref);

	DBG("wf: Deleting sensor %s\n", sr->name);

	if (sr->ops && sr->ops->release)
		sr->ops->release(sr);
	else
		kfree(sr);
}

int wf_register_sensor(struct wf_sensor *new_sr)
{
	struct wf_sensor *sr;

	down(&wf_lock);
	list_for_each_entry(sr, &wf_sensors, link) {
		if (!strcmp(sr->name, new_sr->name)) {
			printk(KERN_WARNING "windfarm: trying to register"
			       " duplicate sensor %s\n", sr->name);
			up(&wf_lock);
			return -EEXIST;
		}
	}
	kref_init(&new_sr->ref);
	list_add(&new_sr->link, &wf_sensors);

	DBG("wf: Registered sensor %s\n", new_sr->name);

	wf_notify(WF_EVENT_NEW_SENSOR, new_sr);
	up(&wf_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(wf_register_sensor);

void wf_unregister_sensor(struct wf_sensor *sr)
{
	down(&wf_lock);
	list_del(&sr->link);
	up(&wf_lock);

	DBG("wf: Unregistered sensor %s\n", sr->name);

	wf_put_sensor(sr);
}
EXPORT_SYMBOL_GPL(wf_unregister_sensor);

struct wf_sensor * wf_find_sensor(const char *name)
{
	struct wf_sensor *sr;

	down(&wf_lock);
	list_for_each_entry(sr, &wf_sensors, link) {
		if (!strcmp(sr->name, name)) {
			if (wf_get_sensor(sr))
				sr = NULL;
			up(&wf_lock);
			return sr;
		}
	}
	up(&wf_lock);
	return NULL;
}
EXPORT_SYMBOL_GPL(wf_find_sensor);

int wf_get_sensor(struct wf_sensor *sr)
{
	if (!try_module_get(sr->ops->owner))
		return -ENODEV;
	kref_get(&sr->ref);
	return 0;
}
EXPORT_SYMBOL_GPL(wf_get_sensor);

void wf_put_sensor(struct wf_sensor *sr)
{
	struct module *mod = sr->ops->owner;
	kref_put(&sr->ref, wf_sensor_release);
	module_put(mod);
}
EXPORT_SYMBOL_GPL(wf_put_sensor);


/*
 * Client & notification
 */

int wf_register_client(struct notifier_block *nb)
{
	int rc;
	struct wf_control *ct;
	struct wf_sensor *sr;

	down(&wf_lock);
	rc = notifier_chain_register(&wf_client_list, nb);
	if (rc != 0)
		goto bail;
	wf_client_count++;
	list_for_each_entry(ct, &wf_controls, link)
		wf_notify(WF_EVENT_NEW_CONTROL, ct);
	list_for_each_entry(sr, &wf_sensors, link)
		wf_notify(WF_EVENT_NEW_SENSOR, sr);
	if (wf_client_count == 1)
		wf_start_thread();
 bail:
	up(&wf_lock);
	return rc;
}
EXPORT_SYMBOL_GPL(wf_register_client);

int wf_unregister_client(struct notifier_block *nb)
{
	down(&wf_lock);
	notifier_chain_unregister(&wf_client_list, nb);
	wf_client_count++;
	if (wf_client_count == 0)
		wf_stop_thread();
	up(&wf_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(wf_unregister_client);

void wf_set_overtemp(void)
{
	down(&wf_lock);
	wf_overtemp++;
	if (wf_overtemp == 1) {
		printk(KERN_WARNING "windfarm: Overtemp condition detected !\n");
		wf_overtemp_counter = 0;
		wf_notify(WF_EVENT_OVERTEMP, NULL);
	}
	up(&wf_lock);
}
EXPORT_SYMBOL_GPL(wf_set_overtemp);

void wf_clear_overtemp(void)
{
	down(&wf_lock);
	WARN_ON(wf_overtemp == 0);
	if (wf_overtemp == 0) {
		up(&wf_lock);
		return;
	}
	wf_overtemp--;
	if (wf_overtemp == 0) {
		printk(KERN_WARNING "windfarm: Overtemp condition cleared !\n");
		wf_notify(WF_EVENT_NORMALTEMP, NULL);
	}
	up(&wf_lock);
}
EXPORT_SYMBOL_GPL(wf_clear_overtemp);

int wf_is_overtemp(void)
{
	return (wf_overtemp != 0);
}
EXPORT_SYMBOL_GPL(wf_is_overtemp);

static struct platform_device wf_platform_device = {
	.name	= "windfarm",
};

static int __init windfarm_core_init(void)
{
	DBG("wf: core loaded\n");

	platform_device_register(&wf_platform_device);
	return 0;
}

static void __exit windfarm_core_exit(void)
{
	BUG_ON(wf_client_count != 0);

	DBG("wf: core unloaded\n");

	platform_device_unregister(&wf_platform_device);
}


module_init(windfarm_core_init);
module_exit(windfarm_core_exit);

MODULE_AUTHOR("Benjamin Herrenschmidt <benh@kernel.crashing.org>");
MODULE_DESCRIPTION("Core component of PowerMac thermal control");
MODULE_LICENSE("GPL");

