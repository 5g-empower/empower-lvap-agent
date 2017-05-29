/*
 * empowerhypervisor.{cc,hh}
 *
 * Katerina Koutlia
 * Copyright (c) 2017, UPC
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   - Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   - Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   - Neither the name of the CREATE-NET nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <click/config.h>
#include "empowerhypervisor.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/standard/scheduleinfo.hh>
#include <clicknet/ether.h>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>
#include <elements/wifi/bitrate.hh>
CLICK_DECLS

enum {
	H_DROPS, H_BYTEDROPS, H_CAPACITY, H_LIST_QUEUES, H_DEBUG
};

EmpowerHypervisor::EmpowerHypervisor() {
	_drops = 0;
	_bdrops = 0;
	_sleepiness = 0;
	_quantum = 1470;
	_capacity = 50;
	_debug = false;
}

EmpowerHypervisor::~EmpowerHypervisor() {
	// de-allocate hypervisor table
	HTIter itr = _hyper_table.begin();
	while (itr != _hyper_table.end()) {
		release_queue(itr.key());
		itr++;
	}
	_hyper_table.clear();
}

uint32_t EmpowerHypervisor::compute_deficit(const Packet* p) {
	return p ? p->length() : 0;
}

void * EmpowerHypervisor::cast(const char *n) {
	if (strcmp(n, "EmpowerHypervisor") == 0)
		return (EmpowerHypervisor *) this;
	else if (strcmp(n, Notifier::EMPTY_NOTIFIER) == 0)
		return static_cast<Notifier *>(&_empty_note);
	else
		return SimpleQueue::cast(n);
}

int EmpowerHypervisor::configure(Vector<String>& conf, ErrorHandler* errh)
{

  int res = cp_va_kparse(conf, this, errh,
			"CAPACITY", 0, cpUnsigned, &_capacity,
			"QUANTUM", 0, cpUnsigned, &_quantum,
			"DEBUG", 0, cpBool, &_debug,
			cpEnd);

  _empty_note.initialize(Notifier::EMPTY_NOTIFIER, router());
  return res;

}

int EmpowerHypervisor::initialize(ErrorHandler *) {
	return 0;
}

void EmpowerHypervisor::push(int, Packet* p) {

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
	EtherAddress sta = EtherAddress(w->i_addr1);
	EtherAddress bssid = EtherAddress(w->i_addr2);

	// get queue for bssid
	EmpowerPacketBuffer *q = _hyper_table.get(bssid);

	// if queue does not exist the create it (just for debugging without lvapmanager
	if (!q) {
		request_queue(bssid, sta);
		q = _hyper_table.get(bssid);
	}

	// push packet on queue or fail
	if (q->push(p)) {
		// if station is not in the active list then add it at the end
		if (find(_active_list.begin(), _active_list.end(), bssid) == _active_list.end()) {
			if (_debug) {
				click_chatter("%{element} :: %s :: pushing back on active list %s",
						      this,
							  __func__,
							  bssid.unparse().c_str());
			}
			_active_list.push_back(bssid);
			q->_deficit = _quantum;
		}
		_empty_note.wake();
		_sleepiness = 0;
	} else {
		// queue overflow, destroy the packet
		if (_drops == 0) {
			click_chatter("%{element} :: %s :: overflow", this, __func__);
		}
		_bdrops += p->length();
		_drops++;
		p->kill();
	}

}

Packet * EmpowerHypervisor::pull(int) {

	if (_hyper_table.empty()) {
		_empty_note.sleep();
		_sleepiness = 0;
		return 0;
	}

	if (_active_list.empty()) {
		if (++_sleepiness == SLEEPINESS_TRIGGER) {
			if (_debug) {
				click_chatter("%{element} :: %s :: active queue is empty, going to sleep",
						      this,
							  __func__);
			}
			_empty_note.sleep();
		}
		return 0;
	}

	HTIter active = _hyper_table.find(_active_list[0]);
	EmpowerPacketBuffer* queue = active.value();
	const Packet *p = queue->top();

	if (_debug) {
		click_chatter("%{element} :: %s :: active queue %s, deficit %u",
				      this,
					  __func__,
					  queue->_bssid.unparse().c_str(),
					  queue->_deficit);
	}

	// no packet at the front of the buffer, reset deficit and remove queue from active list
	if (!p) {
		queue->_deficit = 0;
		_active_list.pop_front();
		return 0;
	}

	uint32_t deficit = compute_deficit(p);

	// queue deficit is enough, decrease deficit and leave queue at the front of the active list
	if (deficit <= queue->_deficit) {
		queue->_deficit -= compute_deficit(p);
		return queue->pull();
	}

	// queue deficit is not enough, increase deficit for next round and put queue at the end of the active list
	if (_debug) {
		click_chatter("%{element} :: %s :: active queue %s, insufficient deficit %u/%u",
				      this,
					  __func__,
					  queue->_bssid.unparse().c_str(),
					  queue->_deficit,
					  deficit);
	}
	EtherAddress next = _active_list.front();
	_active_list.push_back(next);
	_active_list.pop_front();
	queue->_deficit += _quantum;

	return 0;

}

String EmpowerHypervisor::list_queues() {
	StringAccum result;
	HTIter itr = _hyper_table.begin();
	result << "Key,Capacity,Packets,Bytes\n";
	while (itr != _hyper_table.end()) {
		result << (itr.key()).unparse() << "," << itr.value()->_capacity << "," << itr.value()->_size << "," << itr.value()->_bsize << "\n";
		itr++;
	}
	return (result.take_string());
}

void EmpowerHypervisor::request_queue(EtherAddress bssid, EtherAddress dst) {
	if (_debug) {
		click_chatter("%{element} :: %s :: request new queue for bssid %s dst %s",
				      this,
					  __func__,
					  bssid.unparse().c_str(),
					  dst.unparse().c_str());
	}
	EmpowerPacketBuffer* q = new EmpowerPacketBuffer(_capacity, bssid, dst);
	if (_hyper_table.empty()) {
		_empty_note.wake();
		_sleepiness = 0;
	}
	_hyper_table.set(bssid, q);
}

void EmpowerHypervisor::release_queue(EtherAddress bssid) {
	if (_debug) {
		click_chatter("%{element} :: %s :: releasing queue %s",
				      this,
					  __func__,
					  bssid.unparse().c_str());
	}
	HTIter itr = _hyper_table.find(bssid);
	// destroy queued packets
	Packet* p = 0;
	do {
		p = itr.value()->pull();
		// flush
		if (p) {
			p->kill();
		}
	} while (p);
	// erase queue
	_hyper_table.erase(itr);
	// set new next
	if (!_hyper_table.empty()) {
		if (itr == _hyper_table.end()) {
			itr = _hyper_table.begin();
		}
	} else {
		_empty_note.sleep();
		_sleepiness = 0;
	}
}

void EmpowerHypervisor::add_handlers() {
	add_read_handler("debug", read_handler, (void *) H_DEBUG);
	add_read_handler("drops", read_handler, (void*) H_DROPS);
	add_read_handler("byte_drops", read_handler, (void*) H_BYTEDROPS);
	add_read_handler("capacity", read_handler, (void*) H_CAPACITY);
	add_read_handler("list_queues", read_handler, (void*) H_LIST_QUEUES);
	add_write_handler("capacity", write_handler, (void*) H_CAPACITY);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
}

String EmpowerHypervisor::read_handler(Element *e, void *thunk) {
	EmpowerHypervisor *c = (EmpowerHypervisor *) e;
	switch ((intptr_t) thunk) {
	case H_DROPS:
		return (String(c->drops()) + "\n");
	case H_DEBUG:
		return String(c->_debug) + "\n";
	case H_BYTEDROPS:
		return (String(c->bdrops()) + "\n");
	case H_CAPACITY:
		return (String(c->capacity()) + "\n");
	case H_LIST_QUEUES:
		return (c->list_queues());
	default:
		return "<error>\n";
	}
}

int EmpowerHypervisor::write_handler(const String &in_s, Element *e, void *vparam, ErrorHandler *errh) {
	EmpowerHypervisor *d = (EmpowerHypervisor *) e;
	String s = cp_uncomment(in_s);
	switch ((intptr_t) vparam) {
	case H_DEBUG: {
		bool _debug;
		if (!cp_bool(s, &_debug))
			return errh->error("debug parameter must be boolean");
		d->_debug = _debug;
		break;
	}
	case H_CAPACITY: {
		uint32_t capacity;
		if (!cp_unsigned(s, &capacity))
			return errh->error("parameter must be a positive integer");
		d->_capacity = capacity;
		break;
	}
	}
	return 0;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(NotifierQueue)
EXPORT_ELEMENT(EmpowerHypervisor)
