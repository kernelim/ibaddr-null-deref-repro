// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause

#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sunrpc/addr.h>
#include <rdma/rdma_cm.h>
#include <net/netevent.h>
#include <asm/swab.h>

MODULE_AUTHOR("Dan Aloni <dan@kernelim.com>");
MODULE_DESCRIPTION("Test case reproduction");
MODULE_LICENSE("GPL");

static unsigned int timeout_ms = 5000;
static atomic_t pending;
module_param(timeout_ms, uint, 0644);
static bool reclaim = true;
module_param(reclaim, bool, 0644);

#define logprint(fmt, ...) do { } while (0)

struct main_context {
	struct completion re_done;
	int res;
};

static int
event_handler(struct rdma_cm_id *id, struct rdma_cm_event *event)
{
	struct main_context *ep = id->context;

	switch (event->event) {
	case RDMA_CM_EVENT_ADDR_RESOLVED:
	case RDMA_CM_EVENT_ROUTE_RESOLVED:
		ep->res = 0;
		complete(&ep->re_done);
		return 0;
	case RDMA_CM_EVENT_ADDR_ERROR:
		complete(&ep->re_done);
		return 0;
	case RDMA_CM_EVENT_ROUTE_ERROR:
		complete(&ep->re_done);
		return 0;
	default:
		break;
	}

	return 0;
}


static int trigger_callback(const char *val, const struct kernel_param *kp)
{
	struct neighbour n = {};
	n.nud_state = NUD_VALID;
	call_netevent_notifiers(NETEVENT_NEIGH_UPDATE, &n);
	return 0;
}

static int trigger_read(char *buf, const struct kernel_param *kp)
{
	return sprintf(buf, "\n");
}

static int trigger;
module_param_call(trigger, trigger_callback, trigger_read, &trigger, S_IRUGO | S_IWUSR);


static int callme_callback(const char *val, const struct kernel_param *kp);
static int callme_read(char *buf, const struct kernel_param *kp)
{
	return sprintf(buf, "%d\n", atomic_read(&pending));
}


static int counter = 0;
module_param_call(callme, callme_callback, callme_read, &counter, S_IRUGO | S_IWUSR);

static void main_resolve(struct sockaddr_in *localport, struct sockaddr_in *remoteport)
{
	struct rdma_cm_id *id;
	struct main_context *ctx;
	unsigned long wtimeout = msecs_to_jiffies(timeout_ms) + 1;
	int rc;

	atomic_inc(&pending);

	ctx = kmalloc(sizeof(*ctx), GFP_KERNEL);
	BUG_ON(!ctx);

	ctx->res = -1;
	init_completion(&ctx->re_done);

	id = rdma_create_id(current->nsproxy->net_ns, event_handler, ctx,
			    RDMA_PS_TCP, IB_QPT_RC);
	BUG_ON(!id);

	rc = rdma_resolve_addr(id, (struct sockaddr *)localport,
			       (struct sockaddr *)remoteport,
			       timeout_ms);
	if (rc)
		goto out;
	rc = wait_for_completion_interruptible_timeout(&ctx->re_done,
						       wtimeout);
	if (rc < 0)
		goto out;
	if (ctx->res) {
		rc = ctx->res;
		goto out;
	}

	ctx->res = -1;
	rc = rdma_resolve_route(id, timeout_ms);
	if (rc)
		goto out;
	rc = wait_for_completion_interruptible_timeout(&ctx->re_done, wtimeout);
	if (rc < 0)
		goto out;
	if (ctx->res) {
		rc = ctx->res;
		goto out;
	}

	rc = 0;

out:
	rdma_destroy_id(id);
	kfree(ctx);

	counter++;

	if (rc == 0) {
		logprint(KERN_DEBUG "ibtest: resolve successful\n");
	} else {
		logprint(KERN_DEBUG "ibtest: resolve error: %d\n", rc);
	}

	atomic_dec(&pending);
}

struct workqueue_struct *main_test_workqueue __read_mostly;

struct main_work_item {
	struct work_struct work;
	struct sockaddr_storage localport;
	struct sockaddr_storage remoteport;
};

static void main_work_execute(struct work_struct *work)
{
	struct main_work_item *item =
		container_of(work, struct main_work_item, work);

	main_resolve((struct sockaddr_in *)&item->localport,
		     (struct sockaddr_in *)&item->remoteport);
	kfree(item);
}

static int
callme_callback(const char *val, const struct kernel_param *kp)
{
	struct main_work_item *new_item;
	int res;
	const char *p;
	const char *remoteport_str;
	const char *localport_str;
	char buf_localport[0x40] = {};
	char buf_remoteport[0x40] = {};
	int total_len = strlen(val), remote_len;

	p = strchr(val, ',');
	if (p == NULL)
		return -EINVAL;
	if (p - val >= sizeof(buf_localport))
		return -EINVAL;
	remote_len = total_len - (p - val + 1);
	if (remote_len >= sizeof(buf_remoteport))
		return -EINVAL;

	memcpy(buf_localport, val, p - val);
	memcpy(buf_remoteport, &p[1], remote_len);

	/* Trim newlines */
	while (1) {
		int n = strlen(buf_remoteport);
		if (n == 0)
			break;

		if (buf_remoteport[n - 1] == '\n') {
			buf_remoteport[n - 1] = '\0';
			n -= 1;
		} else {
			break;
		}
	}

	remoteport_str = &buf_remoteport[0];
	localport_str = &buf_localport[0];

	new_item = kmalloc(sizeof(*new_item), GFP_KERNEL);
	if (!new_item)
		return -ENOMEM;

	printk(KERN_DEBUG "ibtest: local %s, remote %s\n",
	       localport_str, remoteport_str);

	res = rpc_pton(current->nsproxy->net_ns,
		       localport_str, strlen(localport_str),
		       (struct sockaddr *)&new_item->localport,
		       sizeof(new_item->localport));
	if (res == 0) {
		kfree(new_item);
		return -EINVAL;
	}

	res = rpc_pton(current->nsproxy->net_ns,
		       remoteport_str, strlen(remoteport_str),
		       (struct sockaddr *)&new_item->remoteport,
		       sizeof(new_item->remoteport));
	if (res == 0) {
		kfree(new_item);
		return -EINVAL;
	}

	INIT_WORK(&new_item->work, main_work_execute);
	queue_work(main_test_workqueue, &new_item->work);
	return 0;
}

static int __init ibtest_init(void)
{
	struct workqueue_struct *wq;
	int flags = WQ_UNBOUND;

	if (reclaim)
		flags |= WQ_MEM_RECLAIM;

	wq = alloc_workqueue("ibtestwq", flags, 0);
	if (!wq)
		return -ENOMEM;

	main_test_workqueue = wq;
	return 0;
}

static void __exit ibtest_cleanup(void)
{
	destroy_workqueue(main_test_workqueue);
}

module_init(ibtest_init);
module_exit(ibtest_cleanup);
