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
#include <elements/wifi/minstrel.hh>
#include <elements/wifi/bitrate.hh>
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

	// If traffic is unicast we need to check if the lvap is active and if it is on the same interface of
	// this element, then we lookup the traffic rule queue and enqueue the Ethernet packet with all the info
	// necessary later to build the wifi frame.
	if (!dst.is_broadcast() && !dst.is_group()) {
		EmpowerStationState *ess = _el->get_ess(dst);
		if (!ess) {
			p->kill();
			return;
		}
		if (ess->_iface_id != iface_id) {
			p->kill();
			return;
		}
		if (!ess->_set_mask) {
			p->kill();
			return;
		}
        if (!ess->_authentication_status) {
			click_chatter("%{element} :: %s :: station %s not authenticated",
						  this,
						  __func__,
						  dst.unparse().c_str());
			p->kill();
			return;
		}
        if (!ess->_association_status) {
			click_chatter("%{element} :: %s :: station %s not associated",
						  this,
						  __func__,
						  dst.unparse().c_str());
			p->kill();
			return;
		}
        TxPolicyInfo * txp = _el->get_txp(ess->_sta);
        txp->update_tx(p->length());
        store(ess->_ssid, dscp, p, dst, ess->_lvap_bssid);
		return;
	}

	if (_rc->tx_policies()->lookup(dst)->_tx_mcast == TX_MCAST_DMS) {

		// DMS mcast policy, duplicate the frame for each station in
		// each bssid and use unicast destination addresses. note that
		// a given station cannot be in more than one bssid, so just
		// track if the frame has already been delivered to a given
		// station.

		// It delivers the frame just to the stations connected to
		// this multicast group.

		Vector<EmpowerMulticastTable :: EmpowerMulticastReceiver> *mcast_receivers = _el->get_mcast_receivers(dst);
		Vector<EmpowerMulticastTable :: EmpowerMulticastReceiver>::iterator a;

		if (!mcast_receivers) {
			p->kill();
			return;
		}

		for (a = mcast_receivers->begin() ; a != mcast_receivers->end(); a++) {
			EtherAddress sta = a->sta;
			EmpowerStationState * ess = _el->lvaps()->get_pointer(sta);

			if (ess->_iface_id != iface_id) {
				continue;
			}
			if (!ess->_set_mask) {
				continue;
			}
			if (!ess->_authentication_status) {
				continue;
			}
			if (!ess->_association_status) {
				continue;
			}

			Packet *q = p->clone();
			if (!q) {
				continue;
			}

			store(ess->_ssid, dscp, q, sta, ess->_lvap_bssid);
		}

	} else {

		// Legacy mcast policy, we have two cases. Frames must still be duplicated for unique tenants
		// since the lvap model does not support broadcasting. then the frame must be send also to the
		// shared tenants

		// broadcast and multicast traffic, we need to transmit one frame for each unique
		// bssid. this is due to the fact that we can have the same bssid for multiple LVAPs.
		TxPolicyInfo *mcast_tx_policy = _rc->tx_policies()->supported(dst);

		// If this multicast address is not being currently handled, it should be
		// notified to the controller.
		if (dst.is_group() && !mcast_tx_policy) {
			click_chatter("%{element} :: %s :: Missing transmission policy for multicast address %s in interface %d. Sending request to the controller.",
															 this,
															 __func__,
															 dst.unparse().c_str(), iface_id);

			_el->send_incoming_mcast_address(dst, iface_id);
		}

		if (mcast_tx_policy) {

			// If there is a transmission policy, it means that destination is a multicast address
			// and the policy is set to legacy, so frames must be sent just to the bssids of the
			// multicast receptors that subscribed that multicast stream

			Vector<EmpowerMulticastTable :: EmpowerMulticastReceiver> *mcast_receivers = _el->get_mcast_receivers(dst);

			if (!mcast_receivers) {
				p->kill();
				return;
			}

			EtherAddress sta = mcast_receivers->begin()->sta;
			EmpowerStationState *first = _el->lvaps()->get_pointer(sta);

			store(first->_ssid, dscp, p, dst, first->_lvap_bssid);

		} else {

			// If there is no transmission policy for the multicast address or it is a broadcast
			// destination, all of the lvaps and vaps have to receive it.

			// handle unique LVAPs
			for (LVAPIter it = _el->lvaps()->begin(); it.live(); it++) {
				if (it.value()._iface_id != iface_id) {
					continue;
				}
				if (!it.value()._set_mask) {
					continue;
				}
				if (!it.value()._authentication_status) {
					continue;
				}
				if (!it.value()._association_status) {
					continue;
				}
				if (it.value()._lvap_bssid != it.value()._net_bssid) {
					continue;
				}
				Packet *q = p->clone();
				store(it.value()._ssid, dscp, q, it.value()._sta, it.value()._lvap_bssid);
			}

			// handle VAPs
			for (VAPIter it = _el->vaps()->begin(); it.live(); it++) {
				if (it.value()._iface_id != iface_id) {
					continue;
				}
				Packet *q = p->clone();
				store(it.value()._ssid, dscp, q, dst, it.value()._net_bssid);
			}
		}
	}

	p->kill();
	return;

}

void EmpowerQOSManager::store(String ssid, int dscp, Packet *q, EtherAddress ra, EtherAddress ta) {

	_lock.acquire_write();

	TrafficRule tr = TrafficRule(ssid, dscp);
	TrafficRuleQueue *trq = 0;

	if (_rules.find(tr) == _rules.end()) {
		tr = TrafficRule(ssid, 0);
		trq = (_rules.get(tr));
	} else {
		trq = _rules.get(tr);
	}

	if (trq->enqueue(q, ra, ta)) {
		// check if tr is in active list
		if (trq->size() == 0) {
			trq->_deficit = 0;
			_active_list.push_back(tr);
		}
		trq->update_size();
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

	TrafficRule tr = _active_list[0];
	_active_list.pop_front();

	TRIter active = _rules.find(tr);
	HItr head = _head_table.find(tr);
	TrafficRuleQueue* queue = active.value();

	Packet *p = 0;
	if (head.value()) {
		p = head.value();
		_head_table.set(tr, 0);
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
		if (queue->size() > 0) {
			_active_list.push_front(tr);
		}
		_lock.release_write();
		return p;
	} else {
		_head_table.set(tr, p);
		_active_list.push_back(tr);
		queue->_deficit += queue->_quantum;
	}

	_lock.release_write();

	return 0;
}

void EmpowerQOSManager::set_traffic_rule(String ssid, int dscp, uint32_t quantum, bool amsdu_aggregation) {
	_lock.acquire_write();
	TrafficRule tr = TrafficRule(ssid, dscp);
	if (_rules.find(tr) == _rules.end()) {
		click_chatter("%{element} :: %s :: creating new traffic rule queue for ssid %s dscp %u quantum %u A-MSDU %s",
					  this,
					  __func__,
					  tr._ssid.c_str(),
					  tr._dscp,
					  quantum,
					  amsdu_aggregation ? "yes." : "no");
		uint32_t tr_quantum = (quantum == 0) ? _quantum : quantum;
		TrafficRuleQueue *queue = new TrafficRuleQueue(tr, _capacity, tr_quantum, amsdu_aggregation);
		_rules.set(tr, queue);
		_head_table.set(tr, 0);
		_active_list.push_back(tr);
		_el->send_status_traffic_rule(ssid, dscp, _iface_id);
	}
	_lock.release_write();
}

void EmpowerQOSManager::del_traffic_rule(String ssid, int dscp) {

	_lock.acquire_write();

	// remove from active list
	Vector<TrafficRule>::iterator it = _active_list.begin();
	while (it != _active_list.end()) {
		if (it->_ssid == ssid && it->_dscp == dscp) {
			it = _active_list.erase(it);
			break;
		}
		it++;
	}

	TrafficRule tr = TrafficRule(ssid, dscp);

	// remove traffic rule
	TRIter itr = _rules.find(tr);
	if (itr == _rules.end()) {
		click_chatter("%{element} :: %s :: unable to find traffic rule queue for ssid %s dscp %u",
					  this,
					  __func__,
					  tr._ssid.c_str(),
					  tr._dscp);
		return;
	}
	TrafficRuleQueue *trq = itr.value();
	delete trq;
	_rules.erase(itr);

	// remove from head table
	HItr itr2 = _head_table.find(tr);
	Packet *p = itr2.value();
	if (p){
		p->kill();
	}
	_head_table.erase(itr2);

	_lock.release_write();

}

String EmpowerQOSManager::list_queues() {
	StringAccum result;
	TRIter itr = _rules.begin();
	while (itr != _rules.end()) {
		TrafficRuleQueue *trq = itr.value();
		result << trq->unparse();
		itr++;
	} // end while
	return result.take_string();
}

enum {
	H_DEBUG, H_QUEUES
};

String EmpowerQOSManager::read_handler(Element *e, void *thunk) {
	EmpowerQOSManager *td = (EmpowerQOSManager *) e;
	switch ((uintptr_t) thunk) {
	case H_QUEUES:
		return (td->list_queues());
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
	add_read_handler("queues", read_handler, (void *) H_QUEUES);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EmpowerQOSManager)
ELEMENT_REQUIRES(userlevel)
