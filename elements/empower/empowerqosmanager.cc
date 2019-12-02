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
#include <elements/wifi/bitrate.hh>
#include "transmissionpolicy.hh"
#include "minstrel.hh"
#include "empowerlvapmanager.hh"
CLICK_DECLS

EmpowerQOSManager::EmpowerQOSManager() :
		_el(0), _rc(0), _sleepiness(0), _capacity(500), _quantum(1470), _iface_id(0), _debug(false) {
}

EmpowerQOSManager::~EmpowerQOSManager() {
}

int EmpowerQOSManager::configure(Vector<String> &conf,
		ErrorHandler *errh) {

	return Args(conf, this, errh)
			.read_m("EL", ElementCastArg("EmpowerLVAPManager"), _el)
			.read_m("RC", ElementCastArg("Minstrel"), _rc)
			.read_m("IFACE_ID", _iface_id)
			.read("QUANTUM", _quantum)
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
			_el->send_incoming_mcast_address(iface_id, dst);
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

	Slice slice = Slice(ssid, dscp);
	SliceQueue *sliceq = 0;

	if (_slices.find(slice) == _slices.end()) {
		slice = Slice(ssid, 0);
		assert(_slices.find(slice) != _slices.end());
	}

	sliceq = _slices.get(slice);

	if (sliceq->enqueue(q, ra, ta)) {
		// check if queue was empty and no packet in buffer
		if (sliceq->_size == 1 && _head_table.find(slice).value() == 0) {
			sliceq->_deficit = 0;
			_active_list.push_back(slice);
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

	Slice slice = _active_list[0];
	_active_list.pop_front();

	SIter active = _slices.find(slice);
	HItr head = _head_table.find(slice);
	SliceQueue* queue = active.value();

	Packet *p = 0;
	if (head.value()) {
		p = head.value();
		_head_table.set(slice, 0);
	} else {
		p = queue->dequeue();
	}

	if (!p) {
		queue->_deficit = 0;
	} else if (_rc->estimate_usecs_wifi_packet(p) <= queue->_deficit) {
		uint32_t deficit = _rc->estimate_usecs_wifi_packet(p);
		queue->_deficit -= deficit;
		queue->_deficit_used += deficit;
		queue->_tx_bytes += p->length();
		queue->_tx_packets++;
		if (queue->_size > 0) {
			_active_list.push_front(slice);
		}
		_lock.release_write();
		return p;
	} else {
		_head_table.set(slice, p);
		_active_list.push_back(slice);
		queue->_deficit += queue->_quantum;
	}

	_lock.release_write();

	return 0;
}

void EmpowerQOSManager::set_default_slice(String ssid) {
	set_slice(ssid, 0, 12000, false, 0);
}

void EmpowerQOSManager::set_slice(String ssid, int dscp, uint32_t quantum, bool amsdu_aggregation, uint8_t scheduler) {

	_lock.acquire_write();

	Slice slice = Slice(ssid, dscp);
	SIter itr = _slices.find(slice);

	if (itr == _slices.end()) {
		if (_debug) {
			click_chatter("%{element} :: %s :: Creating new slice queue for ssid %s dscp %u quantum %u A-MSDU %s scheduler %u",
						  this,
						  __func__,
						  slice._ssid.c_str(),
						  slice._dscp,
						  quantum,
						  amsdu_aggregation ? "yes" : "no",
						  scheduler);
		}

		uint32_t tr_quantum = (quantum == 0) ? _quantum : quantum;
		SliceQueue *queue = new SliceQueue(this, slice, _capacity, tr_quantum, amsdu_aggregation, scheduler);
		_slices.set(slice, queue);
		_head_table.set(slice, 0);
	} else {
		if (_debug) {
			click_chatter("%{element} :: %s :: Updating slice queue for ssid %s dscp %u quantum %u A-MSDU %s scheduler %u",
						  this,
						  __func__,
						  slice._ssid.c_str(),
						  slice._dscp,
						  quantum,
						  amsdu_aggregation ? "yes" : "no",
						  scheduler);
		}

		SliceQueue* queue = itr.value();
		queue->_quantum = quantum;
		queue->_amsdu_aggregation = amsdu_aggregation;
		queue->_scheduler = scheduler;
	}

	_el->send_status_slice(_iface_id, ssid, dscp);

	_lock.release_write();
}

void EmpowerQOSManager::del_slice(String ssid, int dscp) {

	_lock.acquire_write();

	if (_debug) {
		click_chatter("%{element} :: %s :: Deleting slice queue for ssid %s dscp %u",
					  this,
					  __func__,
					  ssid.c_str(),
					  dscp);
	}

	// remove from active list
	Vector<Slice>::iterator it = _active_list.begin();
	while (it != _active_list.end()) {
		if (it->_ssid == ssid && it->_dscp == dscp) {
			it = _active_list.erase(it);
			break;
		}
		it++;
	}

	Slice slice = Slice(ssid, dscp);

	// remove slice
	SIter itr = _slices.find(slice);
	if (itr == _slices.end()) {
		return;
	}
	SliceQueue *sliceq = itr.value();
	delete sliceq;
	_slices.erase(itr);

	// remove from head table
	HItr itr2 = _head_table.find(slice);
	Packet *p = itr2.value();
	if (p){
		p->kill();
	}
	_head_table.erase(itr2);

	_lock.release_write();

}

String EmpowerQOSManager::list_slices() {
	StringAccum result;
	SIter itr = _slices.begin();
	while (itr != _slices.end()) {
		SliceQueue *sliceq = itr.value();
		result << sliceq->unparse();
		itr++;
	} // end while
	return result.take_string();
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
