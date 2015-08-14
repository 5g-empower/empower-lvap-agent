/*
 * empowerrxstats.{cc,hh} -- tracks rx stats
 * John Bicket, Roberto Riggio
 *
 * Copyright (c) 2003 Massachusetts Institute of Technology
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
#include "empowerrxstats.hh"
CLICK_DECLS

void send_summary_trigger_callback(Timer *timer, void *data) {
	// send summary
	SummaryTrigger *summary = (SummaryTrigger *) data;
	summary->_el->send_summary_trigger(summary);
	summary->_sent++;
	if (summary->_sent >= (uint32_t) summary->_limit) {
		summary->_ers->del_summary_trigger(summary->_eth, summary->_trigger_id, summary->_limit, summary->_period);
		return;
	}
	// re-schedule the timer
	timer->schedule_after_msec(summary->_period);
}

void send_rssi_trigger_callback(Timer *timer, void *data) {
	// process triggers
	RssiTrigger *rssi = (RssiTrigger *) data;
	for (NTIter iter = rssi->_ers->stas()->begin(); iter.live(); iter++) {
		DstInfo *nfo = &iter.value();
		if ( rssi->_eth != nfo->_eth ) {
			continue;
		}
		if (rssi->matches(nfo)) {
			if (!rssi->_dispatched) {
				if (rssi->_el) {
					rssi->_el->send_rssi_trigger(rssi->_eth, rssi->_trigger_id, rssi->_rel, rssi->_val, nfo->_ewma_rssi->avg());
				}
				rssi->_dispatched = true;
			}
		} else if (!rssi->matches(nfo) && rssi->_dispatched) {
			rssi->_dispatched = false;
		}
	}
	// re-schedule the timer
	timer->schedule_after_msec(rssi->_period);
}

EmpowerRXStats::EmpowerRXStats() :
		_el(0), _timer(this), _signal_offset(0), _aging(-95), _aging_th(-94),
		_period(1000), _ewma_level(80), _sma_period(13), _debug(false) {

}

EmpowerRXStats::~EmpowerRXStats() {
}

int EmpowerRXStats::initialize(ErrorHandler *) {
	_timer.initialize(this);
	_timer.schedule_now();
	return 0;
}

int EmpowerRXStats::configure(Vector<String> &conf, ErrorHandler *errh) {

	int ret = Args(conf, this, errh)
			.read("EL", ElementCastArg("EmpowerLVAPManager"), _el)
			.read("AGING", _aging)
			.read("AGING_TH", _aging_th)
			.read("EWMA_LEVEL", _ewma_level)
			.read("SMA_PERIOD", _sma_period)
			.read("SIGNAL_OFFSET", _signal_offset)
			.read("PERIOD", _period)
			.read("DEBUG", _debug)
			.complete();

	return ret;

}

void EmpowerRXStats::run_timer(Timer *)
{
	// process stations
	for (NTIter iter = _stas.begin(); iter.live();) {
		// Update stats
		DstInfo *nfo = &iter.value();
		nfo->update();
		// Delete entries with RSSI lower than -90
		if (nfo->_last_packets == 0 && nfo->_ewma_rssi->avg() <= _aging_th && nfo->_sma_rssi->avg() <= _aging_th) {
			iter = _stas.erase(iter);
		} else {
			++iter;
		}
	}
	// process access points
	for (NTIter iter = _aps.begin(); iter.live();) {
		// Update aps
		DstInfo *nfo = &iter.value();
		nfo->update();
		// Delete entries with RSSI lower than -90
		if (nfo->_last_packets == 0 && nfo->_ewma_rssi->avg() <= _aging_th && nfo->_sma_rssi->avg() <= _aging_th) {
			iter = _aps.erase(iter);
		} else {
			++iter;
		}
	}
	// rescheduler
	_timer.schedule_after_msec(_period);
}

Packet *
EmpowerRXStats::simple_action(Packet *p) {

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
	bool station = false;

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

	EtherAddress ta = EtherAddress(w->i_addr2);;
	DstInfo *nfo;

	if (station) {
		nfo = _stas.get_pointer(ta);
	} else {
		nfo = _aps.get_pointer(ta);
	}

	if (!nfo) {
		if (station) {
			_stas[ta] = DstInfo(ta, _ewma_level, _sma_period, _aging);
			nfo = _stas.get_pointer(ta);
		} else {
			_aps[ta] = DstInfo(ta, _ewma_level, _sma_period, _aging);
			nfo = _aps.get_pointer(ta);
		}
	}

	// Update iface id
	int iface_id = PAINT_ANNO(p);
	nfo->_iface_id = iface_id;

	int8_t rssi;
	memcpy(&rssi, &ceh->rssi, 1);

	uint32_t dur;
	if (ceh->flags & WIFI_EXTRA_MCS) {
		dur = calc_transmit_time_ht(ceh->rate, p->length());
	} else {
		dur = calc_transmit_time(ceh->rate, p->length());
	}

	nfo->add_rssi_sample(rssi);

	if (station) {
		for (DTIter qi = _summary_triggers.begin(); qi != _summary_triggers.end(); qi++) {
			if ((*qi)->_eth == nfo->_eth) {
				Frame frame = Frame(ta, ceh->tsft, w->i_seq, rssi, ceh->rate, type, subtype, p->length(), dur);
				(*qi)->_frames.push_back(frame);
			}
		}
	}

	if (_debug) {
		click_chatter("%{element} :: %s :: src %s dir %u rssi %d length %d rate %u",
				      this,
				      __func__,
				      ta.unparse().c_str(),
				      dir,
				      rssi,
				      p->length(),
					  ceh->rate);
	}

	return p;

}

void EmpowerRXStats::add_rssi_trigger(EtherAddress eth, uint32_t trigger_id, relation_t rel, int val) {
	RssiTrigger * rssi = new RssiTrigger(eth, trigger_id, rel, val, false, _period, _el, this);
	for (RTIter qi = _rssi_triggers.begin(); qi != _rssi_triggers.end(); qi++) {
		if (*rssi== **qi) {
			click_chatter("%{element} :: %s :: trigger already defined (%s), setting sent to false",
						  this,
						  __func__,
						  rssi->unparse().c_str());
			(*qi)->_dispatched = false;
			return;
		}
	}
	rssi->_trigger_timer->assign(&send_rssi_trigger_callback, (void *) rssi);
	rssi->_trigger_timer->initialize(this);
	rssi->_trigger_timer->schedule_now();
	_rssi_triggers.push_back(rssi);
}

void EmpowerRXStats::del_rssi_trigger(EtherAddress eth, uint32_t trigger_id, relation_t rel, int val) {
	RssiTrigger * rssi = new RssiTrigger(eth, trigger_id, rel, val, false, _period, _el, this);
	for (RTIter qi = _rssi_triggers.begin(); qi != _rssi_triggers.end(); qi++) {
		if (*rssi== **qi) {
			(*qi)->_trigger_timer->clear();
			_rssi_triggers.erase(qi);
			break;
		}
	}
}

void EmpowerRXStats::clear_triggers() {
	// clear rssi triggers
	for (RTIter qi = _rssi_triggers.begin(); qi != _rssi_triggers.end(); qi++) {
		(*qi)->_trigger_timer->clear();
	}
	_rssi_triggers.clear();
	// clear summary triggers
	for (DTIter qi = _summary_triggers.begin(); qi != _summary_triggers.end(); qi++) {
		(*qi)->_trigger_timer->clear();
		delete *qi;
	}
	_summary_triggers.clear();
}

void EmpowerRXStats::add_summary_trigger(EtherAddress eth, uint32_t summary_id, int16_t limit, uint16_t period) {
	SummaryTrigger * summary = new SummaryTrigger(eth, summary_id, limit, period, _el, this);
	for (DTIter qi = _summary_triggers.begin(); qi != _summary_triggers.end(); qi++) {
		if (*summary == **qi) {
			click_chatter("%{element} :: %s :: summary already defined (%s), ignoring",
						  this,
						  __func__,
						  summary->unparse().c_str());
			return;
		}
	}
	summary->_trigger_timer->assign(&send_summary_trigger_callback, (void *) summary);
	summary->_trigger_timer->initialize(this);
	summary->_trigger_timer->schedule_now();
	_summary_triggers.push_back(summary);
}

void EmpowerRXStats::del_summary_trigger(EtherAddress eth, uint32_t summary_id, int16_t limit, uint16_t period) {
	SummaryTrigger summary = SummaryTrigger(eth, summary_id, limit, period, _el, this);
	for (DTIter qi = _summary_triggers.begin(); qi != _summary_triggers.end(); qi++) {
		if (summary == **qi) {
			(*qi)->_trigger_timer->clear();
			_summary_triggers.erase(qi);
			delete *qi;
			break;
		}
	}
}

enum {
	H_DEBUG,
	H_STATS,
	H_RESET,
	H_SIGNAL_OFFSET,
	H_MATCHES,
	H_RSSI_TRIGGERS,
	H_SUMMARY_TRIGGERS
};

String EmpowerRXStats::read_handler(Element *e, void *thunk) {

	EmpowerRXStats *td = (EmpowerRXStats *) e;

	switch ((uintptr_t) thunk) {
	case H_MATCHES: {
		StringAccum sa;
		for (RTIter qi = td->_rssi_triggers.begin(); qi != td->_rssi_triggers.end(); qi++) {
			for (NTIter iter = td->_stas.begin(); iter.live(); iter++) {
				DstInfo *nfo = &iter.value();
				if (!nfo)
					continue;
				if ((*qi)->matches(nfo)) {
					sa << (*qi)->unparse();
					sa << " current " << nfo->_ewma_rssi->avg();
					sa << "\n";
				}
			}
		}
		return sa.take_string();
	}
	case H_RSSI_TRIGGERS: {
		StringAccum sa;
		for (RTIter qi = td->_rssi_triggers.begin(); qi != td->_rssi_triggers.end(); qi++) {
			sa << (*qi)->unparse() << "\n";
		}
		return sa.take_string();
	}
	case H_SUMMARY_TRIGGERS: {
		StringAccum sa;
		for (DTIter qi = td->_summary_triggers.begin(); qi != td->_summary_triggers.end(); qi++) {
			sa << (*qi)->unparse() << "\n";
		}
		return sa.take_string();
	}
	case H_STATS: {
		StringAccum sa;
		for (NTIter iter = td->_stas.begin(); iter.live(); iter++) {
			DstInfo *nfo = &iter.value();
			sa << nfo->unparse(true);
		}
		for (NTIter iter = td->_aps.begin(); iter.live(); iter++) {
			DstInfo *nfo = &iter.value();
			sa << nfo->unparse(false);
		}
		return sa.take_string();
	}
	case H_SIGNAL_OFFSET:
		return String(td->_signal_offset) + "\n";
	case H_DEBUG:
		return String(td->_debug) + "\n";
	default:
		return String();
	}
}

int EmpowerRXStats::write_handler(const String &in_s, Element *e, void *vparam,
		ErrorHandler *errh) {

	EmpowerRXStats *f = (EmpowerRXStats *) e;
	String s = cp_uncomment(in_s);

	switch ((intptr_t) vparam) {
	case H_RESET: {
		f->_stas.clear();
		f->_aps.clear();
		break;
	}
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

void EmpowerRXStats::add_handlers() {
	add_read_handler("stats", read_handler, (void *) H_STATS);
	add_read_handler("summary_triggers", read_handler, (void *) H_SUMMARY_TRIGGERS);
	add_read_handler("matches", read_handler, (void *) H_MATCHES);
	add_read_handler("rssi_triggers", read_handler, (void *) H_RSSI_TRIGGERS);
	add_read_handler("debug", read_handler, (void *) H_DEBUG);
	add_read_handler("signal_offset", read_handler, (void *) H_SIGNAL_OFFSET);
	add_write_handler("signal_offset", write_handler, (void *) H_SIGNAL_OFFSET);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
}

EXPORT_ELEMENT(EmpowerRXStats)
ELEMENT_REQUIRES(bitrate DstInfo Trigger SummaryTrigger RssiTrigger)
CLICK_ENDDECLS

