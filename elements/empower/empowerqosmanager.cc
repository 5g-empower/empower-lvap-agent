/*
 * empowerqosmanager.{cc,hh} -- encapsultates 802.11 packets (EmPOWER Access Point)
 * John Bicket, Roberto Riggio
 *
 * Copyright (c) 2017 CREATE-NET
 * Copyright (c) 2004 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "empowerqosmanager.hh"
#include <click/etheraddress.hh>
#include <click/algorithm.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <click/packet_anno.hh>
#include <clicknet/llc.h>
#include <elements/wifi/wirelessinfo.hh>
#include <elements/wifi/transmissionpolicy.hh>
#include "empowerlvapmanager.hh"
CLICK_DECLS

EmpowerQOSManager::EmpowerQOSManager() :
		_el(0), _rc(0), _sleepiness(0), _iface_id(0), _debug(false) {
}

EmpowerQOSManager::~EmpowerQOSManager() {
}

int EmpowerQOSManager::configure(Vector<String> &conf,
		ErrorHandler *errh) {

	return Args(conf, this, errh)
			.read_m("EL", ElementCastArg("EmpowerLVAPManager"), _el)
			.read_m("RC", ElementCastArg("Minstrel"), _rc)
			.read_m("IFACE_ID", _iface_id)
			.read("DEBUG", _debug)
			.complete();

}

void * EmpowerQOSManager::cast(const char *n) {
	if (strcmp(n, "EmpowerQOSManager") == 0)
		return (EmpowerQOSManager *) this;
	else if (strcmp(n, Notifier::EMPTY_NOTIFIER) == 0)
		return static_cast<Notifier *>(&_empty_note);
	else
		return Element::cast(n);
}

void
EmpowerQOSManager::push(int, Packet *p) {

	if (p->length() < sizeof(struct click_ether)) {
		click_chatter("%{element} :: %s :: packet too small: %d vs %d",
				      this,
				      __func__,
				      p->length(),
				      sizeof(struct click_ether));
		p->kill();
		return;
	}

	Timestamp now = Timestamp::now();
	p->set_timestamp_anno(now);

	int dscp = 0;
	uint8_t iface_id = PAINT_ANNO(p);

	click_ether *eh = (click_ether *) p->data();

	if (ntohs(eh->ether_type) == 0x0800) {
		const click_ip *ip = p->ip_header();
		dscp = ip->ip_tos >> 2;
	}

	EtherAddress dst = EtherAddress(eh->ether_dhost);

	// If traffic is unicast we need to check if the lvap is active
	if (!dst.is_broadcast() && !dst.is_group()) {
		EmpowerStationState *ess = _el->get_ess(dst);
		if (!ess) {
			p->kill();
			return;
		}
		_el->lock()->acquire_read();
		if (!ess->is_valid(iface_id)){
			p->kill();
		} else {
	        _el->get_txp(ess->_sta)->update_tx(p->length());
	        store(ess->_ssid, dscp, p, dst, ess->_bssid);
		}
		_el->lock()->release_read();
		return;
	}

	if (_rc->tx_policies()->lookup(dst)->_tx_mcast == TX_MCAST_DMS) {

		/*
		 * DMS mcast policy. Duplicate the frame for each station in the mcast group
		 * and use unicast destination addresses.
		 */

		Vector<EtherAddress> *mcast_receivers = _el->get_mcast_receivers(dst);

		if (!mcast_receivers) {
			p->kill();
			return;
		}

		Vector<EtherAddress>::iterator itr;
		for (itr = mcast_receivers->begin(); itr != mcast_receivers->end(); itr++) {
			EmpowerStationState * ess = _el->lvaps()->get_pointer(*itr);
			if (!ess) {
				continue;
			}
			_el->lock()->acquire_read();
			if (!ess->is_valid(iface_id)){
				continue;
			} else {
				Packet *q = p->clone();
				if (!q) {
					continue;
				}
				store(ess->_ssid, dscp, q, *itr, ess->_bssid);
			}
			_el->lock()->release_read();
		}

	} else {

		/*
		 * Legacy mcast policy. Frames must still be duplicated for unique tenants
		 * since the lvap model does not support broadcasting. After frames must be
		 * send also to the shared tenants.
		 *
		 * Notice that here we can have both mcast and bcast address.
		 */

		TxPolicyInfo *mcast_tx_policy = _rc->tx_policies()->supported(dst);

		// Mcast address unknown, report to the controller
		if (!dst.is_broadcast() && dst.is_group() && !mcast_tx_policy) {
			if (_debug){
				click_chatter("%{element} :: %s :: Missing transmission policy for multicast address %s on interface %d. Sending request to the controller.",
																 this,
																 __func__,
																 dst.unparse().c_str(),
																 iface_id);
			}
			_el->send_incoming_mcast_address(dst, iface_id);
		}

		if (mcast_tx_policy) {

			/* If there is a transmission policy, it means that destination is a multicast address
			 * and the policy is set to legacy, so frames must be sent just to the bssids of the
			 * multicast receptors that subscribed that multicast group.
			 */

			Vector<EtherAddress> *mcast_receivers = _el->get_mcast_receivers(dst);

			if (!mcast_receivers) {
				p->kill();
				return;
			}

			if (mcast_receivers->size() == 0) {
				p->kill();
				return;
			}

			EtherAddress sta = mcast_receivers->front();
			EmpowerStationState *first = _el->lvaps()->get_pointer(sta);

			if (!first) {
				p->kill();
				return;
			}

			Packet *q = p->clone();
			store(first->_ssid, dscp, q, dst, first->_bssid);

		} else {

			/* If there is no transmission policy for the multicast address or it is a broadcast
			 * destination, all of the lvaps and vaps have to receive it.
			 */

			// handle unique LVAPs
			_el->lock()->acquire_read();
			for (LVAPIter it = _el->lvaps()->begin(); it.live(); it++) {
				if (!it.value().is_valid(iface_id)) {
					continue;
				}
				if (!_el->is_unique_lvap(it.value()._sta)) {
					continue;
				}
				Packet *q = p->clone();
				if (!q) {
					continue;
				}
				store(it.value()._ssid, dscp, q, it.value()._sta, it.value()._bssid);
			}
			_el->lock()->release_read();

			// handle VAPs
			for (VAPIter it = _el->vaps()->begin(); it.live(); it++) {
				if (it.value()._iface_id != iface_id) {
					continue;
				}
				Packet *q = p->clone();
				store(it.value()._ssid, dscp, q, dst, it.value()._bssid);
			}
		}
	}

	p->kill();
	return;

}

void EmpowerQOSManager::store(String ssid, int dscp, Packet *q, EtherAddress ra, EtherAddress ta) {

	_lock.acquire_write();

	SliceKey slice_key = SliceKey(ssid, dscp);
	Slice *slice;

	SlicePairKey slice_pair_key = SlicePairKey(ra, ta);
	SlicePair *slice_pair;

	if (_slices.find(slice_key) == _slices.end()) {
		slice_key = SliceKey(ssid, 0);
	}
	slice = _slices.get(slice_key);

	if (slice->_slice_pairs.find(slice_pair_key) == slice->_slice_pairs.end()) {
		slice_pair = new SlicePair(ra, ta, 12000);
		slice->_slice_pairs.set(slice_pair_key, slice_pair);
		slice->set_aggr_queues_quantum(_rc);
	} else {
		slice_pair = slice->_slice_pairs.get(slice_pair_key);
	}

	SlicePairQueueKey queue_key = SlicePairQueueKey(slice_key, slice_pair_key);
	SlicePairQueue *queue = &slice_pair->_queue;

	if (queue->push(q)) {
		// check if queue was empty and no packet in buffer
		if (queue->_nb_pkts == 1 && _head_table.find(queue_key) == _head_table.end()) {
			queue->_deficit = 0;
			_active_list.push_back(queue_key);
		}
		// wake up queue
		_empty_note.wake();
		// reset sleepiness
		_sleepiness = 0;
	} else {
		q->kill();
	}

	_lock.release_write();

}

Packet * EmpowerQOSManager::pull(int) {

	if (_active_list.empty()) {
		if (++_sleepiness == SLEEPINESS_TRIGGER) {
			_empty_note.sleep();
		}
		return 0;
	}

	_lock.acquire_write();

	SlicePairQueueKey queue_key = _active_list[0];
	_active_list.pop_front();

	Slice *slice = _slices.get(queue_key.first);
	SlicePair *slice_pair = slice->_slice_pairs.get(queue_key.second);
	SlicePairQueue *queue = &slice_pair->_queue;

	Packet *p = 0;
	if (_head_table.find(queue_key) == _head_table.end()) {
		p = queue->pull();
		assert(p);

		click_ether *eh = (click_ether *) p->data();
		EtherAddress src = EtherAddress(eh->ether_shost);
		p = wifi_encap(p, slice_pair->_key._ra, src, slice_pair->_key._ta);
	} else {
		p = _head_table.get(queue_key);
		_head_table.erase(queue_key);
	}

	uint32_t deficit = _rc->estimate_usecs_wifi_packet(p);

	if (deficit <= queue->_deficit) {
		queue->_deficit -= deficit;
		queue->_deficit_used += deficit;
		queue->_tx_bytes += p->length();
		queue->_tx_packets++;
		if (queue->_nb_pkts > 0) {
			_active_list.push_front(queue_key);
		} else {
			queue->_deficit = 0;
		}
		_lock.release_write();
		return p;
	} else {
		_head_table.set(queue_key, p);
		_active_list.push_back(queue_key);
		queue->_deficit += slice_pair->_quantum;
	}

	_lock.release_write();

	return 0;
}

void EmpowerQOSManager::set_default_slice(String ssid) {
	set_slice(ssid, 0, 12000, false, 0);
}

void EmpowerQOSManager::set_slice(String ssid, int dscp, uint32_t quantum, bool amsdu_aggregation, uint32_t scheduler) {

	_lock.acquire_write();

	SliceKey slice_key = SliceKey(ssid, dscp);

	if (_slices.find(slice_key) == _slices.end()) {
		if (_debug) {
			click_chatter("%{element} :: %s :: Creating new slice queue for ssid %s dscp %u quantum %u A-MSDU %s scheduler %u",
						  this,
						  __func__,
						  slice_key._ssid.c_str(),
						  slice_key._dscp,
						  quantum,
						  amsdu_aggregation ? "yes" : "no",
						  scheduler);
		}

		uint32_t tr_quantum = (quantum == 0) ? 12000 : quantum;
		Slice *slice = new Slice(ssid, dscp, tr_quantum, amsdu_aggregation, scheduler);
		_slices.set(slice_key, slice);

		slice->set_aggr_queues_quantum(_rc);
	} else {
		if (_debug) {
			click_chatter("%{element} :: %s :: Updating slice queue for ssid %s dscp %u quantum %u A-MSDU %s scheduler %u",
						  this,
						  __func__,
						  slice_key._ssid.c_str(),
						  slice_key._dscp,
						  quantum,
						  amsdu_aggregation ? "yes" : "no",
						  scheduler);
		}

		Slice *slice = _slices.get(slice_key);
		slice->_quantum = quantum;
		slice->_amsdu_aggregation = amsdu_aggregation;
		slice->_scheduler = scheduler;

		slice->set_aggr_queues_quantum(_rc);
	}

	_el->send_status_slice(ssid, dscp, _iface_id);

	_lock.release_write();
}

void EmpowerQOSManager::del_slice(String ssid, int dscp) {

	_lock.acquire_write();

	SliceKey slice_key = SliceKey(ssid, dscp);

	if (_debug) {
		click_chatter("%{element} :: %s :: Deleting slice queue for ssid %s dscp %u",
					  this,
					  __func__,
					  ssid.c_str(),
					  dscp);
	}

	// remove from active list
	Vector<SlicePairQueueKey>::iterator it = _active_list.begin();
	while (it != _active_list.end()) {
		if (it->first == slice_key) {
			it = _active_list.erase(it);

			if (_head_table.find(*it) != _head_table.end()) {
				_head_table.erase(*it);
			}
		} else {
			it++;
		}
	}

	Slice *slice = _slices.get(slice_key);

	for (SlicePairsIter it = slice->_slice_pairs.begin(); it.live(); it++) {
		delete it.value();
	}

	_slices.erase(slice_key);
	delete slice;

	_lock.release_write();
}

String EmpowerQOSManager::list_slices() {

	StringAccum result;

	for (SlicesIter it = _slices.begin(); it.live(); it++) {
		Slice *slice = it.value();
		result << slice->unparse() << "\n";
	} // end while

	return result.take_string();
}

Packet * EmpowerQOSManager::wifi_encap(Packet *p, EtherAddress ra, EtherAddress sa, EtherAddress ta) {

    WritablePacket *q = p->uniqueify();

	if (!q) {
		return 0;
	}

	uint8_t mode = WIFI_FC1_DIR_FROMDS;
	uint16_t ethtype;

	memcpy(&ethtype, q->data() + 12, 2);

	q->pull(sizeof(struct click_ether));
	q = q->push(sizeof(struct click_llc));

	if (!q) {
		q->kill();
		return 0;
	}

	memcpy(q->data(), WIFI_LLC_HEADER, WIFI_LLC_HEADER_LEN);
	memcpy(q->data() + 6, &ethtype, 2);

	q = q->push(sizeof(struct click_wifi));

	if (!q) {
		q->kill();
		return 0;
	}

	struct click_wifi *w = (struct click_wifi *) q->data();

	memset(q->data(), 0, sizeof(click_wifi));

	w->i_fc[0] = (uint8_t) (WIFI_FC0_VERSION_0 | WIFI_FC0_TYPE_DATA);
	w->i_fc[1] = 0;
	w->i_fc[1] |= (uint8_t) (WIFI_FC1_DIR_MASK & mode);

	memcpy(w->i_addr1, ra.data(), 6);
	memcpy(w->i_addr2, ta.data(), 6);
	memcpy(w->i_addr3, sa.data(), 6);

	return q;

}

enum {
	H_DEBUG, H_SLICES
};

String EmpowerQOSManager::read_handler(Element *e, void *thunk) {
	EmpowerQOSManager *td = (EmpowerQOSManager *) e;
	switch ((uintptr_t) thunk) {
	case H_SLICES:
		return (td->list_slices());
	case H_DEBUG:
		return String(td->_debug) + "\n";
	default:
		return String();
	}
}

int EmpowerQOSManager::write_handler(const String &in_s, Element *e, void *vparam, ErrorHandler *errh) {
	EmpowerQOSManager *f = (EmpowerQOSManager *) e;
	String s = cp_uncomment(in_s);
	switch ((intptr_t) vparam) {
	case H_DEBUG: {    //debug
		bool debug;
		if (!BoolArg().parse(s, debug))
			return errh->error("debug parameter must be boolean");
		f->_debug = debug;
		break;
	}
	}
	return 0;
}

void EmpowerQOSManager::add_handlers() {
	add_read_handler("debug", read_handler, (void *) H_DEBUG);
	add_read_handler("slices", read_handler, (void *) H_SLICES);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EmpowerQOSManager)
ELEMENT_REQUIRES(userlevel)
