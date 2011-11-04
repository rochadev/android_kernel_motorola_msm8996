#define DPRINTK(fmt, args...)				\
	pr_debug("xenbus_probe (%s:%d) " fmt ".\n",	\
		 __func__, __LINE__, ##args)

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/notifier.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/io.h>

#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/xen/hypervisor.h>
#include <xen/xenbus.h>
#include <xen/events.h>
#include <xen/page.h>

#include <xen/platform_pci.h>

#include "xenbus_comms.h"
#include "xenbus_probe.h"


/* device/<type>/<id> => <type>-<id> */
static int frontend_bus_id(char bus_id[XEN_BUS_ID_SIZE], const char *nodename)
{
	nodename = strchr(nodename, '/');
	if (!nodename || strlen(nodename + 1) >= XEN_BUS_ID_SIZE) {
		printk(KERN_WARNING "XENBUS: bad frontend %s\n", nodename);
		return -EINVAL;
	}

	strlcpy(bus_id, nodename + 1, XEN_BUS_ID_SIZE);
	if (!strchr(bus_id, '/')) {
		printk(KERN_WARNING "XENBUS: bus_id %s no slash\n", bus_id);
		return -EINVAL;
	}
	*strchr(bus_id, '/') = '-';
	return 0;
}

/* device/<typename>/<name> */
static int xenbus_probe_frontend(struct xen_bus_type *bus, const char *type,
				 const char *name)
{
	char *nodename;
	int err;

	nodename = kasprintf(GFP_KERNEL, "%s/%s/%s", bus->root, type, name);
	if (!nodename)
		return -ENOMEM;

	DPRINTK("%s", nodename);

	err = xenbus_probe_node(bus, type, nodename);
	kfree(nodename);
	return err;
}

static int xenbus_uevent_frontend(struct device *_dev,
				  struct kobj_uevent_env *env)
{
	struct xenbus_device *dev = to_xenbus_device(_dev);

	if (add_uevent_var(env, "MODALIAS=xen:%s", dev->devicetype))
		return -ENOMEM;

	return 0;
}


static void backend_changed(struct xenbus_watch *watch,
			    const char **vec, unsigned int len)
{
	xenbus_otherend_changed(watch, vec, len, 1);
}

static const struct dev_pm_ops xenbus_pm_ops = {
	.suspend	= xenbus_dev_suspend,
	.resume		= xenbus_dev_resume,
	.freeze		= xenbus_dev_suspend,
	.thaw		= xenbus_dev_cancel,
	.restore	= xenbus_dev_resume,
};

static struct xen_bus_type xenbus_frontend = {
	.root = "device",
	.levels = 2,		/* device/type/<id> */
	.get_bus_id = frontend_bus_id,
	.probe = xenbus_probe_frontend,
	.otherend_changed = backend_changed,
	.bus = {
		.name		= "xen",
		.match		= xenbus_match,
		.uevent		= xenbus_uevent_frontend,
		.probe		= xenbus_dev_probe,
		.remove		= xenbus_dev_remove,
		.shutdown	= xenbus_dev_shutdown,
		.dev_attrs	= xenbus_dev_attrs,

		.pm		= &xenbus_pm_ops,
	},
};

static void frontend_changed(struct xenbus_watch *watch,
			     const char **vec, unsigned int len)
{
	DPRINTK("");

	xenbus_dev_changed(vec[XS_WATCH_PATH], &xenbus_frontend);
}


/* We watch for devices appearing and vanishing. */
static struct xenbus_watch fe_watch = {
	.node = "device",
	.callback = frontend_changed,
};

static int read_backend_details(struct xenbus_device *xendev)
{
	return xenbus_read_otherend_details(xendev, "backend-id", "backend");
}

static int is_device_connecting(struct device *dev, void *data)
{
	struct xenbus_device *xendev = to_xenbus_device(dev);
	struct device_driver *drv = data;
	struct xenbus_driver *xendrv;

	/*
	 * A device with no driver will never connect. We care only about
	 * devices which should currently be in the process of connecting.
	 */
	if (!dev->driver)
		return 0;

	/* Is this search limited to a particular driver? */
	if (drv && (dev->driver != drv))
		return 0;

	xendrv = to_xenbus_driver(dev->driver);
	return (xendev->state < XenbusStateConnected ||
		(xendev->state == XenbusStateConnected &&
		 xendrv->is_ready && !xendrv->is_ready(xendev)));
}

static int exists_connecting_device(struct device_driver *drv)
{
	return bus_for_each_dev(&xenbus_frontend.bus, NULL, drv,
				is_device_connecting);
}

static int print_device_status(struct device *dev, void *data)
{
	struct xenbus_device *xendev = to_xenbus_device(dev);
	struct device_driver *drv = data;

	/* Is this operation limited to a particular driver? */
	if (drv && (dev->driver != drv))
		return 0;

	if (!dev->driver) {
		/* Information only: is this too noisy? */
		printk(KERN_INFO "XENBUS: Device with no driver: %s\n",
		       xendev->nodename);
	} else if (xendev->state < XenbusStateConnected) {
		enum xenbus_state rstate = XenbusStateUnknown;
		if (xendev->otherend)
			rstate = xenbus_read_driver_state(xendev->otherend);
		printk(KERN_WARNING "XENBUS: Timeout connecting "
		       "to device: %s (local state %d, remote state %d)\n",
		       xendev->nodename, xendev->state, rstate);
	}

	return 0;
}

/* We only wait for device setup after most initcalls have run. */
static int ready_to_wait_for_devices;

/*
 * On a 5-minute timeout, wait for all devices currently configured.  We need
 * to do this to guarantee that the filesystems and / or network devices
 * needed for boot are available, before we can allow the boot to proceed.
 *
 * This needs to be on a late_initcall, to happen after the frontend device
 * drivers have been initialised, but before the root fs is mounted.
 *
 * A possible improvement here would be to have the tools add a per-device
 * flag to the store entry, indicating whether it is needed at boot time.
 * This would allow people who knew what they were doing to accelerate their
 * boot slightly, but of course needs tools or manual intervention to set up
 * those flags correctly.
 */
static void wait_for_devices(struct xenbus_driver *xendrv)
{
	unsigned long start = jiffies;
	struct device_driver *drv = xendrv ? &xendrv->driver : NULL;
	unsigned int seconds_waited = 0;

	if (!ready_to_wait_for_devices || !xen_domain())
		return;

	while (exists_connecting_device(drv)) {
		if (time_after(jiffies, start + (seconds_waited+5)*HZ)) {
			if (!seconds_waited)
				printk(KERN_WARNING "XENBUS: Waiting for "
				       "devices to initialise: ");
			seconds_waited += 5;
			printk("%us...", 300 - seconds_waited);
			if (seconds_waited == 300)
				break;
		}

		schedule_timeout_interruptible(HZ/10);
	}

	if (seconds_waited)
		printk("\n");

	bus_for_each_dev(&xenbus_frontend.bus, NULL, drv,
			 print_device_status);
}

int __xenbus_register_frontend(struct xenbus_driver *drv,
			       struct module *owner, const char *mod_name)
{
	int ret;

	drv->read_otherend_details = read_backend_details;

	ret = xenbus_register_driver_common(drv, &xenbus_frontend,
					    owner, mod_name);
	if (ret)
		return ret;

	/* If this driver is loaded as a module wait for devices to attach. */
	wait_for_devices(drv);

	return 0;
}
EXPORT_SYMBOL_GPL(__xenbus_register_frontend);

static DECLARE_WAIT_QUEUE_HEAD(backend_state_wq);
static int backend_state;

static void xenbus_reset_backend_state_changed(struct xenbus_watch *w,
					const char **v, unsigned int l)
{
	xenbus_scanf(XBT_NIL, v[XS_WATCH_PATH], "", "%i", &backend_state);
	printk(KERN_DEBUG "XENBUS: backend %s %s\n",
			v[XS_WATCH_PATH], xenbus_strstate(backend_state));
	wake_up(&backend_state_wq);
}

static void xenbus_reset_wait_for_backend(char *be, int expected)
{
	long timeout;
	timeout = wait_event_interruptible_timeout(backend_state_wq,
			backend_state == expected, 5 * HZ);
	if (timeout <= 0)
		printk(KERN_INFO "XENBUS: backend %s timed out.\n", be);
}

/*
 * Reset frontend if it is in Connected or Closed state.
 * Wait for backend to catch up.
 * State Connected happens during kdump, Closed after kexec.
 */
static void xenbus_reset_frontend(char *fe, char *be, int be_state)
{
	struct xenbus_watch be_watch;

	printk(KERN_DEBUG "XENBUS: backend %s %s\n",
			be, xenbus_strstate(be_state));

	memset(&be_watch, 0, sizeof(be_watch));
	be_watch.node = kasprintf(GFP_NOIO | __GFP_HIGH, "%s/state", be);
	if (!be_watch.node)
		return;

	be_watch.callback = xenbus_reset_backend_state_changed;
	backend_state = XenbusStateUnknown;

	printk(KERN_INFO "XENBUS: triggering reconnect on %s\n", be);
	register_xenbus_watch(&be_watch);

	/* fall through to forward backend to state XenbusStateInitialising */
	switch (be_state) {
	case XenbusStateConnected:
		xenbus_printf(XBT_NIL, fe, "state", "%d", XenbusStateClosing);
		xenbus_reset_wait_for_backend(be, XenbusStateClosing);

	case XenbusStateClosing:
		xenbus_printf(XBT_NIL, fe, "state", "%d", XenbusStateClosed);
		xenbus_reset_wait_for_backend(be, XenbusStateClosed);

	case XenbusStateClosed:
		xenbus_printf(XBT_NIL, fe, "state", "%d", XenbusStateInitialising);
		xenbus_reset_wait_for_backend(be, XenbusStateInitWait);
	}

	unregister_xenbus_watch(&be_watch);
	printk(KERN_INFO "XENBUS: reconnect done on %s\n", be);
	kfree(be_watch.node);
}

static void xenbus_check_frontend(char *class, char *dev)
{
	int be_state, fe_state, err;
	char *backend, *frontend;

	frontend = kasprintf(GFP_NOIO | __GFP_HIGH, "device/%s/%s", class, dev);
	if (!frontend)
		return;

	err = xenbus_scanf(XBT_NIL, frontend, "state", "%i", &fe_state);
	if (err != 1)
		goto out;

	switch (fe_state) {
	case XenbusStateConnected:
	case XenbusStateClosed:
		printk(KERN_DEBUG "XENBUS: frontend %s %s\n",
				frontend, xenbus_strstate(fe_state));
		backend = xenbus_read(XBT_NIL, frontend, "backend", NULL);
		if (!backend || IS_ERR(backend))
			goto out;
		err = xenbus_scanf(XBT_NIL, backend, "state", "%i", &be_state);
		if (err == 1)
			xenbus_reset_frontend(frontend, backend, be_state);
		kfree(backend);
		break;
	default:
		break;
	}
out:
	kfree(frontend);
}

static void xenbus_reset_state(void)
{
	char **devclass, **dev;
	int devclass_n, dev_n;
	int i, j;

	devclass = xenbus_directory(XBT_NIL, "device", "", &devclass_n);
	if (IS_ERR(devclass))
		return;

	for (i = 0; i < devclass_n; i++) {
		dev = xenbus_directory(XBT_NIL, "device", devclass[i], &dev_n);
		if (IS_ERR(dev))
			continue;
		for (j = 0; j < dev_n; j++)
			xenbus_check_frontend(devclass[i], dev[j]);
		kfree(dev);
	}
	kfree(devclass);
}

static int frontend_probe_and_watch(struct notifier_block *notifier,
				   unsigned long event,
				   void *data)
{
	/* reset devices in Connected or Closed state */
	if (xen_hvm_domain())
		xenbus_reset_state();
	/* Enumerate devices in xenstore and watch for changes. */
	xenbus_probe_devices(&xenbus_frontend);
	register_xenbus_watch(&fe_watch);

	return NOTIFY_DONE;
}


static int __init xenbus_probe_frontend_init(void)
{
	static struct notifier_block xenstore_notifier = {
		.notifier_call = frontend_probe_and_watch
	};
	int err;

	DPRINTK("");

	/* Register ourselves with the kernel bus subsystem */
	err = bus_register(&xenbus_frontend.bus);
	if (err)
		return err;

	register_xenstore_notifier(&xenstore_notifier);

	return 0;
}
subsys_initcall(xenbus_probe_frontend_init);

#ifndef MODULE
static int __init boot_wait_for_devices(void)
{
	if (xen_hvm_domain() && !xen_platform_pci_unplug)
		return -ENODEV;

	ready_to_wait_for_devices = 1;
	wait_for_devices(NULL);
	return 0;
}

late_initcall(boot_wait_for_devices);
#endif

MODULE_LICENSE("GPL");
