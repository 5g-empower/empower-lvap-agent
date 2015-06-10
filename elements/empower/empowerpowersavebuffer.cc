/*
 * empowerpowersavebuffer.{cc,hh} -- buffers frame for stations in power save mode (EmPOWER Access Point)
 * Roberto Riggio
 *
 * Copyright (c) 2013 CREATE-NET
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
#include "empowerpowersavebuffer.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>
#include <clicknet/ether.h>
#include <click/packet_anno.hh>
#include <click/etheraddress.hh>
#include <click/straccum.hh>
CLICK_DECLS

EmpowerPowerSaveBuffer::EmpowerPowerSaveBuffer() :
		_sleepiness(0), _ps_table(0), _queue_pool(0), _next(EtherAddress()),
		_size(0), _drops(0), _bdrops(0), _capacity(1000), _el(0), _debug(false) {

	_ps_table = new PowerSaveTable();
	_queue_pool = new QueuePool();
}

EmpowerPowerSaveBuffer::~EmpowerPowerSaveBuffer() {
	// de-allocate ps-table
	PSTIter itr = _ps_table->begin();
	while (itr != _ps_table->end()) {
		release_queue(itr.key());
		itr++;
	} // end while
	_ps_table->clear();
	delete _ps_table;
	// de-allocate pool
	PoolIter itr_pool = _queue_pool->begin();
	while (itr_pool != _queue_pool->end()) {
		Packet* p = 0;
		// destroy queued packets
		do {
			p = (*itr_pool)->pull();
			// flush
			if (p) {
				p->kill();
			} // end if
		} while (p);
		itr_pool++;
	} // end while
	_queue_pool->clear();
	delete _queue_pool;
}

void *
EmpowerPowerSaveBuffer::cast(const char *n) {
	if (strcmp(n, "EmpowerPowerSaveBuffer") == 0)
		return (EmpowerPowerSaveBuffer *) this;
	else if (strcmp(n, Notifier::EMPTY_NOTIFIER) == 0)
		return static_cast<Notifier *>(&_empty_note);
	else
		return SimpleQueue::cast(n);
}

int EmpowerPowerSaveBuffer::configure(Vector<String> &conf,
		ErrorHandler *errh) {

	return Args(conf, this, errh)
   				.read_m("EL", ElementCastArg("EmpowerLVAPManager"), _el)
				.read("CAPACITY", _capacity)
				.read("DEBUG", _debug).complete();

}

void EmpowerPowerSaveBuffer::push(int, Packet *p) {

	if (p->length() < sizeof(struct click_wifi)) {
		click_chatter("%{element} :: %s :: packet too small: %d vs %d",
				      this,
				      __func__,
				      p->length(),
				      sizeof(struct click_ether));
		p->kill();
		return;
	}

	struct click_wifi *w = (struct click_wifi *) p->data();
	uint8_t dir = w->i_fc[1] & WIFI_FC1_DIR_MASK;
	EtherAddress bssid;

	switch (dir) {
	case WIFI_FC1_DIR_TODS:
		bssid = EtherAddress(w->i_addr1);
		break;
	case WIFI_FC1_DIR_FROMDS:
		bssid = EtherAddress(w->i_addr2);
		break;
	case WIFI_FC1_DIR_NODS:
	case WIFI_FC1_DIR_DSTODS:
		bssid = EtherAddress(w->i_addr3);
		break;
	default:
		click_chatter("%{element} :: %s :: invalid dir %d",
				      this,
				      __func__,
				      dir);
		p->kill();
		return;
	}

	// get queue for bssid (virtual)
	PowerSaveQueue* q = _ps_table->get(bssid);

	// no queue, this should never happen
	if (!q) {
		click_chatter("%{element} :: %s :: no buffer for bssid %s",
				      this,
				      __func__,
				      bssid.unparse().c_str());
		p->kill();
		return;
	} // end if

	if (!p->timestamp_anno()) {
		p->timestamp_anno().assign_now();
	}

	// push packet on queue or fail
	if (q->push(p)) {
		if (_size == 0) {
			_next = bssid;
			_empty_note.wake();
			_sleepiness = 0;
		}
		_size++;
	} else {
		// queue overflow, destroy the packet
		if (_drops == 0) {
			click_chatter("%{element} :: %s :: overflow", this, __func__);
		}
		_bdrops += p->length();
		_drops++;
		p->kill();
	} // end if

}

Packet *
EmpowerPowerSaveBuffer::pull(int) {

	if (_ps_table->empty() || _size == 0) {
		if (++_sleepiness == SLEEPINESS_TRIGGER) {
			_empty_note.sleep();
		}
		return 0;
	}

	PSTIter active = _ps_table->find(_next);
	PowerSaveQueue* q = active.value();
	EmpowerStationState* ess = _el->reverse_lvaps()->get_pointer(_next);
	Packet* p = 0;

	if (!ess->_power_save && q->top()) {
		p = q-> pull();
		_size--;
		_sleepiness = 0;
	} else if (++_sleepiness == SLEEPINESS_TRIGGER) {
		_empty_note.sleep();
	}

	active++;

	if (active == _ps_table->end()) {
		active = _ps_table->begin();
	}

	_next = active.key();

	return p;

}

void
EmpowerPowerSaveBuffer::request_queue(EtherAddress dst) {
	PowerSaveQueue* q = 0;
	if (_queue_pool->size() == 0) {
		_queue_pool->push_back(new PowerSaveQueue(_capacity));
	}
	// check out queue and remove from queue pool
	q = _queue_pool->back();
	_queue_pool->pop_back();
	q->reset();
	if (_ps_table->empty()) {
		_next = dst;
		_empty_note.wake();
		_sleepiness = 0;
	}
	_ps_table->set(dst, q);
}

void EmpowerPowerSaveBuffer::release_queue(EtherAddress dst) {
	PSTIter iter = _ps_table->find(dst);
	_queue_pool->push_back(iter.value());
	iter = _ps_table->erase(iter);
	if (!_ps_table->empty()) {
		if (iter == _ps_table->end()) {
			iter = _ps_table->begin();
		}
		_next = iter.key();
	} else {
		_next = EtherAddress();
	}
}

void EmpowerPowerSaveBuffer::touch_queue(EtherAddress dst) {
	if (!_ps_table->get(dst)->top()) {
		return;
	}
	_next = dst;
	_empty_note.wake();
	_sleepiness = 0;
}

String EmpowerPowerSaveBuffer::list_queues() {
	StringAccum result;
	PSTIter itr = _ps_table->begin();
	while (itr != _ps_table->end()) {
		result << "sta " << (itr.key()).unparse()
			   << " capacity " << itr.value()->_capacity
			   << " size " << itr.value()->_size
			   << " bytes " << itr.value()->_bsize
			   << "\n";
		itr++;
	} // end while
	return (result.take_string());
}

enum {
	H_DEBUG,
	H_LIST_QUEUES
};

String EmpowerPowerSaveBuffer::read_handler(Element *e, void *thunk) {
	EmpowerPowerSaveBuffer *td = (EmpowerPowerSaveBuffer *) e;
	switch ((uintptr_t) thunk) {
	case H_LIST_QUEUES:
		return td->list_queues();
	case H_DEBUG:
		return String(td->_debug) + "\n";
	default:
		return String();
	}
}

int EmpowerPowerSaveBuffer::write_handler(const String &in_s, Element *e,
		void *vparam, ErrorHandler *errh) {

	EmpowerPowerSaveBuffer *f = (EmpowerPowerSaveBuffer *) e;
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

void EmpowerPowerSaveBuffer::add_handlers() {
	add_read_handler("list_queues", read_handler, (void *) H_LIST_QUEUES);
	add_read_handler("debug", read_handler, (void *) H_DEBUG);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EmpowerPowerSaveBuffer)
ELEMENT_REQUIRES(userlevel)
