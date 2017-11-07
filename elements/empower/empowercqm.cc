/*
 * Copyright 2017 RISE SICS AB
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>
#include <elements/wifi/bitrate.hh>
#include "empowerlvapmanager.hh"
#include "empowercqm.hh"
#include "frame.hh"
#include "sma.hh"
CLICK_DECLS

EmpowerCQM::EmpowerCQM() :
		_el(0), _timer(this), _period(50), _samples(40), _max_silent_window_count(10),
		_rssi_threshold(-70), _debug(false) {

}

EmpowerCQM::~EmpowerCQM() {
}

int EmpowerCQM::initialize(ErrorHandler *) {
	_timer.initialize(this);
	_timer.schedule_now();
	return 0;
}

int EmpowerCQM::configure(Vector<String> &conf, ErrorHandler *errh) {

	int ret = Args(conf, this, errh)
			.read_m("EL", ElementCastArg("EmpowerLVAPManager"), _el)
			.read("PERIOD", _period)
			.read("SAMPLES", _samples)
			.read("DEBUG", _debug)
			.complete();

	return ret;

}

void EmpowerCQM::run_timer(Timer *) {
	lock.acquire_write();
	// process links
	for (CLTIter iter = links.begin(); iter.live();) {
		// Update estimator
		CqmLink *nfo = &iter.value();
		nfo->estimator(_period, _debug);
		// Delete stale entries
		if (nfo->silentWindowCount> _max_silent_window_count) {
			iter = links.erase(iter);
		} else {
			++iter;
		}
	}
	lock.release_write();
	// rescheduler
	_timer.schedule_after_msec(_period);
}

Packet *
EmpowerCQM::simple_action(Packet *p) {

	struct click_wifi *w = (struct click_wifi *) p->data();

	unsigned wifi_header_size = sizeof(struct click_wifi);

	if ((w->i_fc[1] & WIFI_FC1_DIR_MASK) == WIFI_FC1_DIR_DSTODS)
		wifi_header_size += WIFI_ADDR_LEN;

	if (WIFI_QOS_HAS_SEQ(w))
		wifi_header_size += sizeof(uint16_t);

	struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

	if ((ceh->magic == WIFI_EXTRA_MAGIC) && ceh->pad && (wifi_header_size & 3))
		wifi_header_size += 4 - (wifi_header_size & 3);

	if (p->length() < wifi_header_size) {
		return p;
	}

	int type = w->i_fc[0] & WIFI_FC0_TYPE_MASK;
	int retry = w->i_fc[1] & WIFI_FC1_RETRY;

	// Discard frames that do not have sequence numbers
	if (type == WIFI_FC0_TYPE_CTL) {
		return p;
	}

	EtherAddress ta = EtherAddress(w->i_addr2);

	int8_t rssi;
	memcpy(&rssi, &ceh->rssi, 1);

	uint8_t iface_id = PAINT_ANNO(p);

	lock.acquire_write();

	if (retry != 1) {
		update_link_table(ta, iface_id, w->i_seq, p->length(), rssi);
	}

	update_channel_busy_time(iface_id, ta, p->length(), ceh->rate);

	lock.release_write();

	return p;

}

void EmpowerCQM::update_channel_busy_time(uint8_t iface_id, EtherAddress ta, uint32_t len, uint8_t rate) {
	for (CLTIter iter = links.begin(); iter.live(); iter++) {
		CqmLink *nfo = &iter.value();
		if (nfo && nfo->iface_id == iface_id) {
			if(ta != nfo->sourceAddr) {
				unsigned usec = calc_usecs_wifi_packet(len, rate, 0);
				nfo->add_cbt_sample(usec);
			}
		}
	}
}

void EmpowerCQM::update_link_table(EtherAddress ta, uint8_t iface_id, uint16_t seq, uint32_t len, uint8_t rssi) {

	// Update channel quality map
	CqmLink *nfo;
	nfo = links.get_pointer(ta);
	if (!nfo) {
		links[ta] = CqmLink();
		nfo = links.get_pointer(ta);
		nfo->cqm = this;
		nfo->sourceAddr = ta;
		nfo->iface_id = iface_id;
		nfo->samples = _samples;
		nfo->lastEstimateTime = Timestamp::now();
		nfo->currentTime = Timestamp::now();
		nfo->lastSeqNum = seq - 1;
		nfo->currentSeqNum = seq;
		nfo->xi = 0;
		nfo->rssi_threshold = _rssi_threshold;
	}

	// Add sample
	nfo->add_sample(len, rssi, seq);

}

enum {
	H_DEBUG,
	H_LINKS,
};

String EmpowerCQM::read_handler(Element *e, void *thunk) {

	EmpowerCQM *td = (EmpowerCQM *) e;

	switch ((uintptr_t) thunk) {
	case H_LINKS: {
		StringAccum sa;
		for (CLTIter iter = td->links.begin(); iter.live(); iter++) {
			CqmLink *nfo = &iter.value();
			sa << nfo->unparse();
		}
		return sa.take_string();
	}
	case H_DEBUG:
		return String(td->_debug) + "\n";
	default:
		return String();
	}
}

int EmpowerCQM::write_handler(const String &in_s, Element *e, void *vparam,
		ErrorHandler *errh) {

	EmpowerCQM *f = (EmpowerCQM *) e;
	String s = cp_uncomment(in_s);

	switch ((intptr_t) vparam) {
	case H_DEBUG: {
		bool debug;
		if (!BoolArg().parse(s, debug))
			return errh->error("debug parameter must be boolean");
		f->_debug = debug;
		break;
	}
	}
	return 0;
}

void EmpowerCQM::add_handlers() {
	add_read_handler("links", read_handler, (void *) H_LINKS);
	add_read_handler("debug", read_handler, (void *) H_DEBUG);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
}

EXPORT_ELEMENT(EmpowerCQM)
ELEMENT_REQUIRES(bitrate Frame CqmLink)
CLICK_ENDDECLS
