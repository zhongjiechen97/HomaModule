/* This file manages homa_peertab objects and is responsible for creating
 * and deleting homa_peer objects.
 */

#include "homa_impl.h"

/**
 * homa_peertab_init() - Constructor for homa_peertabs.
 * @peertab:  The object to initialize; previous contents are discarded.
 * 
 * Return:    0 in the normal case, or a negative errno if there was a problem.
 */
int homa_peertab_init(struct homa_peertab *peertab)
{
	int i;
	spin_lock_init(&peertab->write_lock);
	peertab->buckets = (struct hlist_head *) vmalloc(
			HOMA_PEERTAB_BUCKETS * sizeof(*peertab->buckets));
	if (!peertab->buckets)
		return -ENOMEM;
	for (i = 0; i < HOMA_PEERTAB_BUCKETS; i++) {
		INIT_HLIST_HEAD(&peertab->buckets[i]);
	}
	return 0;
}

/**
 * homa_peertab_destroy() - Destructor for homa_peertabs. After this
 * function returns, it is unsafe to use any results from previous calls
 * to homa_peer_find, since all existing homa_peer objects will have been
 * destroyed.
 * @peertab:  The table to destroy.
 */
void homa_peertab_destroy(struct homa_peertab *peertab)
{
	int i;
	struct homa_peer *peer;
	struct hlist_node *next;
	if (!peertab->buckets)
		return;
	
	for (i = 0; i <HOMA_PEERTAB_BUCKETS; i++) {
		hlist_for_each_entry_safe(peer, next, &peertab->buckets[i],
				peertab_links) {
			dst_release(peer->dst);
			kfree(peer);
		}
	}
	vfree(peertab->buckets);
}

/**
 * homa_peer_find() - Returns the peer associated with a given host; creates
 * a new homa_peer if one doesn't already exist.
 * @peertab:    Peer table in which to perform lookup.
 * @addr:       IPV4 address of the desired host.
 * @inet:       Socket that will be used for sending packets.
 * Return:      The peer associated with @addr, or a negative errno if an
 *              error occurred. The caller can retain this pointer
 *              indefinitely: peer entries are never deleted except in
 *              home_peertab_destroy.
 */
struct homa_peer *homa_peer_find(struct homa_peertab *peertab, __be32 addr,
	struct inet_sock *inet)
{
	/* Note: this function uses RCU operators to ensure safety even
	 * if a concurrent call is adding a new entry.
	 */
	struct homa_peer *peer;
	struct rtable *rt;
	__u32 bucket = hash_32(addr, HOMA_PEERTAB_BUCKET_BITS);
	hlist_for_each_entry_rcu(peer, &peertab->buckets[bucket],
			peertab_links) {
		if (peer->addr == addr) {
			return peer;
		}
		INC_METRIC(peer_hash_links, 1);
	}
	
	/* No existing entry; create a new one.
	 * 
	 * Note: after we acquire the lock, we have to check again to
	 * make sure the entry still doesn't exist after grabbing
	 * the lock (it might have been created by a concurrent invocation
	 * of this function). */
	spin_lock_bh(&peertab->write_lock);
	hlist_for_each_entry_rcu(peer, &peertab->buckets[bucket],
			peertab_links) {
		if (peer->addr == addr)
			goto done;
	}
	peer = kmalloc(sizeof(*peer), GFP_ATOMIC);
	if (!peer) {
		peer = (struct homa_peer *) ERR_PTR(-ENOMEM);
		INC_METRIC(peer_kmalloc_errors, 1);
		goto done;
	}
	peer->addr = addr;
	flowi4_init_output(&peer->flow.u.ip4, inet->sk.sk_bound_dev_if,
			inet->sk.sk_mark, inet->tos, RT_SCOPE_UNIVERSE,
			inet->sk.sk_protocol, 0, addr, inet->inet_saddr,
			0, 0, inet->sk.sk_uid);
	security_sk_classify_flow(&inet->sk, &peer->flow);
	rt = ip_route_output_flow(sock_net(&inet->sk), &peer->flow.u.ip4,
			&inet->sk);
	if (IS_ERR(rt)) {
		kfree(peer);
		peer = (struct homa_peer *) PTR_ERR(rt);
		INC_METRIC(peer_route_errors, 1);
		goto done;
	}
	peer->dst = &rt->dst;
	peer->unsched_cutoffs[HOMA_NUM_PRIORITIES-1] = 0;
	peer->unsched_cutoffs[HOMA_NUM_PRIORITIES-2] = INT_MAX;
	peer->cutoff_version = 0;
	hlist_add_head_rcu(&peer->peertab_links, &peertab->buckets[bucket]);
	INC_METRIC(peer_new_entries, 1);
	
    done:
	spin_unlock_bh(&peertab->write_lock);
	return peer;
}