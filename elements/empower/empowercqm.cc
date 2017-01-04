/*
 * empowercqm.{cc,hh}
 * Akila Rao, Roberto Riggio
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
		_el(0), _timer(this), _signal_offset(0), _period(500),
		_max_silent_window_count(10), _rssi_threshold(-70),
		_debug(false) {

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
			.read("EL", ElementCastArg("EmpowerLVAPManager"), _el)
			.read("SIGNAL_OFFSET", _signal_offset)
			.read("PERIOD", _period)
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

	int dir = w->i_fc[1] & WIFI_FC1_DIR_MASK;
	int type = w->i_fc[0] & WIFI_FC0_TYPE_MASK;
	int subtype = w->i_fc[0] & WIFI_FC0_SUBTYPE_MASK;
	int retry = w->i_fc[1] & WIFI_FC1_RETRY;
	bool station = false;

	// Ignore frames that do not have sequence numbers or that are retries
	if (retry == 1 || type == WIFI_FC0_TYPE_CTL) {
		return p;
	}

	switch (dir) {
	case WIFI_FC1_DIR_TODS:
		// TODS bit not set when TA is an access point, but only when TA is a station
		station = true;
		break;
	case WIFI_FC1_DIR_NODS:
		if (type == WIFI_FC0_TYPE_DATA) {
			// NODS never set for data frames unless in ad-hoc mode
			station = true;
			break;
		} else if (type == WIFI_FC0_TYPE_MGT) {
			if (subtype == WIFI_FC0_SUBTYPE_BEACON
					|| subtype == WIFI_FC0_SUBTYPE_PROBE_RESP) {
				// NODS set for beacon frames and probe response from access points
				station = false;
				break;
			} else if (subtype == WIFI_FC0_SUBTYPE_PROBE_REQ
					|| subtype == WIFI_FC0_SUBTYPE_REASSOC_REQ
					|| subtype == WIFI_FC0_SUBTYPE_ASSOC_REQ
					|| subtype == WIFI_FC0_SUBTYPE_AUTH
					|| subtype == WIFI_FC0_SUBTYPE_DISASSOC
					|| subtype == WIFI_FC0_SUBTYPE_DEAUTH) {
				// NODS set for beacon frames and probe response from access points
				station = true;
				break;
			}
		}
		// no idea, ignore packet
		return p;
	case WIFI_FC1_DIR_FROMDS:
		// FROMDS bit not set when TA is an station, but only when TA is an access point
		station = false;
		break;
	case WIFI_FC1_DIR_DSTODS:
		// DSTODS bit never set
		station = false;
		break;
	}

	EtherAddress ra = EtherAddress(w->i_addr1);
	EtherAddress ta = EtherAddress(w->i_addr2);

	int8_t rssi;
	memcpy(&rssi, &ceh->rssi, 1);

	uint8_t iface_id = PAINT_ANNO(p);

	// create frame meta-data
	Frame *frame = new Frame(ra, ta, ceh->tsft, ceh->flags, w->i_seq, rssi, ceh->rate, type, subtype, p->length(), retry, station, iface_id);

	if (false) {
		click_chatter("%{element} :: %s :: %s",
				      this,
					  __func__,
					  frame->unparse().c_str());
	}

	lock.acquire_write();

	//update_link_table(frame);


	lock.release_write();

	return p;

}

void EmpowerCQM::update_link_table(Frame *frame) {

	// Update channel quality map
	CqmLink *nfo;
	nfo = links.get_pointer(frame->_ta);
	if (!nfo) {
		links[frame->_ta] = CqmLink();
		nfo = links.get_pointer(frame->_ta);
		nfo->sourceAddr = frame->_ta;
		nfo->lastEstimateTime = Timestamp::now();
		nfo->currentTime = Timestamp::now();
		nfo->lastSeqNum = frame->_seq - 1;
		nfo->currentSeqNum = frame->_seq;
		nfo->xi = 0;
		nfo->rssiThreshold = _rssi_threshold;
	}

	// Add sample
	nfo->add_sample(frame);

}

enum {
	H_DEBUG,
	H_LINKS,
	H_RESET,
	H_SIGNAL_OFFSET,
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
	case H_SIGNAL_OFFSET: {
		int signal_offset;
		if (!IntArg().parse(s, signal_offset))
			return errh->error("signal _offset parameter must be integer");
		f->_signal_offset = signal_offset;
		break;
	}
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
	add_read_handler("signal_offset", read_handler, (void *) H_SIGNAL_OFFSET);
	add_write_handler("signal_offset", write_handler, (void *) H_SIGNAL_OFFSET);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
}

EXPORT_ELEMENT(EmpowerCQM)
ELEMENT_REQUIRES(bitrate Frame)
CLICK_ENDDECLS
