/* This file provides simplified substitutes for many Linux variables and
 * functions, in order to allow Homa unit tests to be run outside a Linux
 * kernel.
 */

#include <stdio.h>

#include "homa_impl.h"
#include "ccutils.h"
#include "mock.h"

#define KSELFTEST_NOT_MAIN 1
#include "kselftest_harness.h"

/* It isn't safe to include some header files, such as stdlib, because
 * they conflict with kernel header files. The explicit declarations
 * below replace those header files.
 */

extern void       free(void *ptr);
extern void      *malloc(size_t size);
extern void      *memcpy(void *dest, const void *src, size_t n);

/* The variables below can be set to non-zero values by unit tests in order
 * to simulate error returns from various functions. If bit 0 is set to 1,
 * the next call to the function will fail; bit 1 corresponds to the next
 * call after that, and so on.
 */
int mock_alloc_skb_errors = 0;
int mock_copy_data_errors = 0;
int mock_copy_to_user_errors = 0;
int mock_ip_queue_xmit_errors = 0;
int mock_kmalloc_errors = 0;
int mock_route_errors = 0;
int mock_vmalloc_errors = 0;

/* If a test since this variable to non-NULL, this function will be invoked
 * during future calls to spin_lock or spin_lock_bh.
 */
void (*mock_spin_lock_hook)(void) = NULL;

/* If a test sets this variable to nonzero, ip_queue_xmit will log
 * outgoing packets using the long format rather than short.
 *  */
int mock_xmit_log_verbose = 0;

/* Keeps track of all sk_buffs that are alive in the current test.
 * Reset for each test.
 */
static struct unit_hash *buffs_in_use = NULL;

/* Keeps track of all the blocks of memory that have been allocated by
 * kmalloc but not yet freed by kfree. Reset for each test.
 */
static struct unit_hash *kmallocs_in_use = NULL;

/* Keeps track of all the results returned by ip_route_output_flow that
 * have not yet been freed. Reset for each test. */
static struct unit_hash *routes_in_use = NULL;

/* Keeps track of all the blocks of memory that have been allocated by
 * vmalloc but not yet freed by vfree. Reset for each test.
 */
static struct unit_hash *vmallocs_in_use = NULL;

/* The number of locks that have been acquired but not yet released. 
 * Should be 0 at the end of each test.
 */
static int mock_active_locks = 0;

/* Linux's idea of the current CPU number. */
int cpu_number = 1;

struct task_struct *current_task = NULL;
unsigned long ex_handler_refcount = 0;
unsigned long phys_base = 0;
struct net init_net;

extern void add_wait_queue(struct wait_queue_head *wq_head,
		struct wait_queue_entry *wq_entry) {}

struct sk_buff *__alloc_skb(unsigned int size, gfp_t priority, int flags,
		int node)
{
	if (mock_check_error(&mock_alloc_skb_errors))
		return NULL;
	struct sk_buff *skb = malloc(sizeof(struct sk_buff));
	memset(skb, 0, sizeof(*skb));
	if (!buffs_in_use)
		buffs_in_use = unit_hash_new();
	unit_hash_set(buffs_in_use, skb, "used");
	skb->head = malloc(size);
	skb->data = skb->head;
	skb_reset_tail_pointer(skb);
	skb->network_header = 0;
	skb->transport_header = 0;
	skb->data_len = 0;
	skb->len = 0;
	skb->users.refs.counter = 1;
	skb->_skb_refdst = 0;
	ip_hdr(skb)->saddr = 0;
	return skb;
}

bool _copy_from_iter_full(void *addr, size_t bytes, struct iov_iter *i)
{
	if (mock_check_error(&mock_copy_data_errors))
		return false;
	unit_log_add_separator("; ");
	unit_log_printf("_copy_from_iter_full copied %lu bytes", bytes);
	return true;
}

bool _copy_from_iter_full_nocache(void *addr, size_t bytes, struct iov_iter *i)
{
	if (mock_check_error(&mock_copy_data_errors))
		return false;
	unit_log_add_separator("; ");
	unit_log_printf("_copy_from_iter_full_nocache copid %lu bytes", bytes);
	return true;
}

unsigned long _copy_to_user(void __user *to, const void *from, unsigned long n)
{
	if (mock_check_error(&mock_copy_to_user_errors))
		return -1;
	unit_log_add_separator("; ");
	unit_log_printf("_copy_to_user copied %lu bytes", n);
	return 0;
}

bool csum_and_copy_from_iter_full(void *addr, size_t bytes, __wsum *csum,
			       struct iov_iter *i)
{
	if (mock_check_error(&mock_copy_data_errors))
		return false;
	unit_log_add_separator("; ");
	unit_log_printf("csum_and_copy_from_iter_full copied %lu bytes", bytes);
	return true;
}

unsigned long _copy_from_user(void *to, const void __user *from,
		unsigned long n)
{
	if (mock_check_error(&mock_copy_data_errors))
		return false;
	unit_log_add_separator("; ");
	unit_log_printf("_copy_from_user copyed %lu bytes", n);
	return 0;
}

int ip4_datagram_connect(struct sock *sk, struct sockaddr *uaddr,
		int addr_len)
{
	return 0;
}

void ip4_datagram_release_cb(struct sock *sk) {}

void dst_release(struct dst_entry *dst)
{
	if (!dst)
		return;
	dst->__refcnt.counter--;
	if (dst->__refcnt.counter > 0)
		return;
	if (!routes_in_use || unit_hash_get(routes_in_use, dst) == NULL) {
		FAIL("dst_release on unknown route");
		return;
	}
	unit_hash_erase(routes_in_use, dst);
	free(dst);
}

int import_single_range(int type, void __user *buf, size_t len,
		struct iovec *iov, struct iov_iter *i)
{
	return 0;
}

int inet_add_protocol(const struct net_protocol *prot, unsigned char num)
{
	return 0;
}

int inet_del_protocol(const struct net_protocol *prot, unsigned char num)
{
	return 0;
}

int inet_dgram_connect(struct socket *sock, struct sockaddr *uaddr,
		       int addr_len, int flags)
{
	return 0;
}

int inet_getname(struct socket *sock, struct sockaddr *uaddr, int *uaddr_len,
		int peer)
{
	return 0;
}

int inet_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg)
{
	return 0;
}

int inet_recvmsg(struct socket *sock, struct msghdr *msg, size_t size,
		int flags)
{
	return 0;
}
struct ctl_table_header *register_net_sysctl(struct net *net,
	const char *path, struct ctl_table *table)
{
	return NULL;
}

void inet_register_protosw(struct inet_protosw *p) {}

int inet_release(struct socket *sock)
{
	return 0;
}

int inet_sendmsg(struct socket *sock, struct msghdr *msg, size_t size)
{
	return 0;
}

void inet_unregister_protosw(struct inet_protosw *p) {}

int ip_queue_xmit(struct sock *sk, struct sk_buff *skb, struct flowi *fl)
{
	char buffer[200];
	if (mock_check_error(&mock_ip_queue_xmit_errors))
		return -ENETDOWN;
	if (mock_xmit_log_verbose)
		homa_print_packet(skb, buffer, sizeof(buffer));
	else
		homa_print_packet_short(skb, buffer, sizeof(buffer));
	unit_log_add_separator("; ");
	unit_log_printf("xmit %s", buffer);
	kfree_skb(skb);
	return 0;
}

struct rtable *ip_route_output_flow(struct net *net, struct flowi4 *flp4,
		const struct sock *sk)
{
	struct rtable *route;
	if (mock_check_error(&mock_route_errors))
		return ERR_PTR(-EHOSTUNREACH);
	route = malloc(sizeof(struct rtable));
	if (!route) {
		FAIL("malloc failed");
		return ERR_PTR(-ENOMEM);
	}
	route->dst.__refcnt.counter = 1;
	if (!routes_in_use)
		routes_in_use = unit_hash_new();
	unit_hash_set(routes_in_use, route, "used");
	return route;
}

void kfree(const void *block)
{
	if (!kmallocs_in_use || unit_hash_get(kmallocs_in_use, block) == NULL) {
		FAIL("kfree on unknown block");
		return;
	}
	unit_hash_erase(kmallocs_in_use, block);
	free((void *) block);
}

void kfree_skb(struct sk_buff *skb)
{
	skb_dst_drop(skb);
	skb->users.refs.counter--;
	if (skb->users.refs.counter > 0)
		return;
	if (!buffs_in_use || unit_hash_get(buffs_in_use, skb) == NULL) {
		FAIL("kfree_skb on unknown sk_buff");
		return;
	}
	unit_hash_erase(buffs_in_use, skb);
	free(skb->head);
	free(skb);
}

void *__kmalloc(size_t size, gfp_t flags)
{
	if (mock_check_error(&mock_kmalloc_errors))
		return NULL;
	void *block = malloc(size);
	if (!block) {
		FAIL("malloc failed");
		return NULL;
	}
	if (!kmallocs_in_use)
		kmallocs_in_use = unit_hash_new();
	unit_hash_set(kmallocs_in_use, block, "used");
	return block;
}

void __lockfunc _raw_spin_lock_bh(raw_spinlock_t *lock)
{
	if (mock_spin_lock_hook) {
		mock_spin_lock_hook();
	}
	mock_active_locks++;
}

void __lockfunc _raw_spin_unlock_bh(raw_spinlock_t *lock)
{
	mock_active_locks--;
}

void lock_sock_nested(struct sock *sk, int subclass)
{
	mock_active_locks++;
	sk->sk_lock.owned = 1;
}

ssize_t __modver_version_show(struct module_attribute *a,
		struct module_kobject *b, char *c)
{
	return 0;
}

void __mutex_init(struct mutex *lock, const char *name,
			 struct lock_class_key *key)
{
	
}

void mutex_lock(struct mutex *lock)
{
	mock_active_locks++;
}

void mutex_unlock(struct mutex *lock)
{
	mock_active_locks--;
}

int printk(const char *s, ...)
{
	return 0;
}

struct proc_dir_entry *proc_create(const char *name, umode_t mode,
				   struct proc_dir_entry *parent,
				   const struct file_operations *proc_fops)
{
	return NULL;
}

void proc_remove(struct proc_dir_entry *de)
{
	
}

int proto_register(struct proto *prot, int alloc_slab)
{
	return 0;
}

void proto_unregister(struct proto *prot) {}

void release_sock(struct sock *sk)
{
	mock_active_locks--;
	sk->sk_lock.owned = 0;
}

void remove_wait_queue(struct wait_queue_head *wq_head,
		struct wait_queue_entry *wq_entry) {}

void security_sk_classify_flow(struct sock *sk, struct flowi *fl) {}

void sk_common_release(struct sock *sk) {}

int sk_set_peek_off(struct sock *sk, int val)
{
	return 0;
}

int skb_copy_datagram_iter(const struct sk_buff *from, int offset,
		struct iov_iter *to, int size)
{
	unit_log_add_separator("; ");
	unit_log_printf("skb_copy_datagram_iter ");
	unit_log_data(from->data + offset, size);
	return 0;
}

void *skb_put(struct sk_buff *skb, unsigned int len)
{
	unsigned char *result = skb_tail_pointer(skb);
	skb->tail += len;
	skb->len += len;
	return result;
}

int sock_common_getsockopt(struct socket *sock, int level, int optname,
		char __user *optval, int __user *optlen)
{
	return 0;
}

int sock_common_setsockopt(struct socket *sock, int level, int optname,
		char __user *optval, unsigned int optlen)
{
	return 0;
}

int sock_no_accept(struct socket *sock, struct socket *newsock, int flags,
		bool kern)
{
	return 0;
}

int sock_no_listen(struct socket *sock, int backlog)
{
	return 0;
}

int sock_no_mmap(struct file *file, struct socket *sock,
		struct vm_area_struct *vma)
{
	return 0;
}

int proc_dointvec(struct ctl_table *table, int write,
		     void __user *buffer, size_t *lenp, loff_t *ppos)
{
	return 0;
}

int sock_no_shutdown(struct socket *sock, int how)
{
	return 0;
}

void _raw_spin_lock(raw_spinlock_t *lock)
{
	mock_active_locks++;
}

ssize_t sock_no_sendpage(struct socket *sock, struct page *page, int offset,
		size_t size, int flags)
{
	return 0;
}

int sock_no_socketpair(struct socket *sock1, struct socket *sock2)
{
	return 0;
}

void unregister_net_sysctl_table(struct ctl_table_header *header) {}

long wait_woken(struct wait_queue_entry *wq_entry, unsigned mode,
		long timeout)
{
	return 0;
}

void vfree(const void *block)
{
	if (!vmallocs_in_use || unit_hash_get(vmallocs_in_use, block) == NULL) {
		FAIL("vfree on unknown block");
		return;
	}
	unit_hash_erase(vmallocs_in_use, block);
	free((void *) block);
}

void *vmalloc(size_t size)
{
	if (mock_check_error(&mock_vmalloc_errors))
		return NULL;
	void *block = malloc(size);
	if (!block) {
		FAIL("malloc failed");
		return NULL;
	}
	if (!vmallocs_in_use)
		vmallocs_in_use = unit_hash_new();
	unit_hash_set(vmallocs_in_use, block, "used");
	return block;
}

void __warn_printk(const char *s, ...) {}

int woken_wake_function(struct wait_queue_entry *wq_entry, unsigned mode,
		int sync, void *key)
{
	return 0;
}

/**
 * mock_check_error() - Determines whether a method should simulate an error
 * return.
 * @errorMask:  Address of a variable containing a bit mask, indicating
 *              which of the next calls should result in errors.
 * 
 * Return:      zero means the function should behave normally; 1 means return
 *              an eror 
 */
int mock_check_error(int *errorMask)
{
	int result = *errorMask & 1;
	*errorMask = *errorMask >> 1;
	return result;
}

/**
 * mock_data_ready() - Invoked through sk->sk_data_ready; logs a message
 * to indicate that it was invoked.
 * @sk:    Associated socket; not used here.
 */
void mock_data_ready(struct sock *sk)
{
	unit_log_printf("sk->sk_data_ready invoked");
}

/**
 * pp() - Allocate and return a packet buffer. The buffer is
 * initialized as if it just arrived from the network.
 * @saddr:        IPV4 address to use as the sender of the packet, in
 *                network byte order.
 * @h:            Header for the buffer; actual length and contents depend
 *                on the type.
 * @extra_bytes:  How much additional data to add to the buffer after
 *                the header.
 * @first_value:  Determines the data contents: the first __u32 will have
 *                this value, and each successive __u32 will increment by 4.
 * 
 * Return:        A packet buffer containing the information described above.
 *                The caller owns this buffer and is responsible for freeing it.
 */
struct sk_buff *mock_skb_new(__be32 saddr, struct common_header *h,
		int extra_bytes, int first_value)
{
	int header_size, ip_size;
	
	switch (h->type) {
	case DATA:
		header_size = sizeof(struct data_header);
		break;
	case GRANT:
		header_size = sizeof(struct grant_header);
		break;
	case RESEND:
		header_size = sizeof(struct resend_header);
		break;
	case BUSY:
		header_size = sizeof(struct busy_header);
		break;
	default:
		printf("*** Unknown packet type %d in new_buff.\n", h->type);
		header_size = sizeof(struct common_header);
		break;
	}
	struct sk_buff *skb = malloc(sizeof(struct sk_buff));
	memset(skb, 0, sizeof(*skb));
	if (!buffs_in_use)
		buffs_in_use = unit_hash_new();
	unit_hash_set(buffs_in_use, skb, "used");
	
	/* Round up sizes to whole words for convenience. */
	ip_size = (sizeof(struct iphdr) + 3) & ~3;
	/* Round up extra data space to whole words for convenience. */
	skb->head = malloc(ip_size + header_size + ((extra_bytes+3)&~3));
	skb->data = skb->head + ip_size;
	skb->network_header = ip_size - sizeof(struct iphdr);
	skb->transport_header = ip_size;
	skb->data = skb->head + ip_size;
	memcpy(skb->data, h, header_size);
	unit_fill_data(skb->data + header_size, extra_bytes, first_value);
	skb->len = header_size + extra_bytes;
	skb->users.refs.counter = 1;
	ip_hdr(skb)->saddr = saddr;
	skb->_skb_refdst = 0;
	return skb;
}

/**
 * Returns the number of sk_buffs currently in use.
 */
int mock_skb_count(void)
{
	return unit_hash_size(buffs_in_use);
}

/**
 * mock_sock_destroy() - Destructor for sockets; cleans up the mocked-out
 * non-Homa parts as well as the Homa parts.
 */
void mock_sock_destroy(struct homa_sock *hsk, struct homa_socktab *socktab)
{
	homa_sock_destroy(hsk, socktab);
}

/**
 * mock_sock_init() - Constructor for sockets; initializes the Homa-specific
 * part, and mocks out the non-Homa-specific parts.
 * @hsk:          Storage area to be initialized.\
 * @homa:         Overall information about the Oma protocol.
 * @client_port:  Client-side port number to use for the socket, or 0 to
 *                use default.
 * @server_port:  Server-side port number to use for the socket (or 0).
 */
void mock_sock_init(struct homa_sock *hsk, struct homa *homa,
		int client_port, int server_port)
{
	struct sock *sk = (struct sock *) hsk;
	memset(hsk, 0, sizeof(*hsk));
	homa_sock_init(hsk, homa);
	if (client_port != 0)
		hsk->client_port = client_port;
	if (server_port != 0)
		homa_sock_bind(&homa->port_map, hsk, server_port);
	hsk->server_port = server_port;
	sk->sk_data_ready = mock_data_ready;
}

/**
 * mock_spin_unlock() - Called instead of spin_unlock when Homa is compiled
 * for unit testing.
 * @lock:   Lock to be be released (ignored).
 */
void mock_spin_unlock(spinlock_t *lock)
{
	mock_active_locks--;
}

/**
 * mock_teardown() - Invoked at the end of each unit test to check for
 * consistency issues with all of the information managed by this file.
 * This function also cleans up the mocking information, so it is ready
 * for the next unit test.
 */
void mock_teardown(void)
{
	cpu_number = 1;
	mock_alloc_skb_errors = 0;
	mock_copy_data_errors = 0;
	mock_copy_to_user_errors = 0;
	mock_kmalloc_errors = 0;
	mock_route_errors = 0;
	mock_vmalloc_errors = 0;
	mock_spin_lock_hook = NULL;
	mock_xmit_log_verbose = 0;
	
	int count = unit_hash_size(buffs_in_use);
	if (count > 0)
		FAIL("%u sk_buff(s)still in use after test", count);
	unit_hash_free(buffs_in_use);
	buffs_in_use = NULL;
	
	count = unit_hash_size(kmallocs_in_use);
	if (count > 0)
		FAIL("%u kmalloced block(s) still allocated after test", count);
	unit_hash_free(kmallocs_in_use);
	kmallocs_in_use = NULL;
	
	count = unit_hash_size(routes_in_use);
	if (count > 0)
		FAIL("%u route(s) still allocated after test", count);
	unit_hash_free(routes_in_use);
	routes_in_use = NULL;
	
	count = unit_hash_size(vmallocs_in_use);
	if (count > 0)
		FAIL("%u vmalloced block(s) still allocated after test", count);
	unit_hash_free(vmallocs_in_use);
	vmallocs_in_use = NULL;
	
	if (mock_active_locks > 0)
		FAIL("%d locks still locked after test", mock_active_locks);
	mock_active_locks = 0;
}