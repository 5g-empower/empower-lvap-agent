#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <click/args.hh>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>
#include "scyllawifidupefilter.hh"

CLICK_DECLS

ScyllaWifiDupeFilter::ScyllaWifiDupeFilter() :
		_debug(false) {
}

ScyllaWifiDupeFilter::~ScyllaWifiDupeFilter() {
}

int ScyllaWifiDupeFilter::configure(Vector<String> &conf, ErrorHandler *errh) {
	return Args(conf, this, errh).read("DEBUG", _debug).complete();
}

Packet *
ScyllaWifiDupeFilter::simple_action(Packet *p_in) {

	click_wifi *w = (click_wifi *) p_in->data();

	if (p_in->length() < sizeof(click_wifi)) {
		return p_in;
	}

	EtherAddress src = EtherAddress(w->i_addr2);
	EtherAddress dst = EtherAddress(w->i_addr1);
	uint16_t seq = le16_to_cpu(w->i_seq) >> WIFI_SEQ_SEQ_SHIFT;
	uint8_t frag = le16_to_cpu(w->i_seq) & WIFI_SEQ_FRAG_MASK;
	u_int8_t more_frag = w->i_fc[1] & WIFI_FC1_MORE_FRAG;

	bool is_frag = frag || more_frag;

	ScyllaDupeFilterDstInfo *nfo = _table.findp(src);

	if ((w->i_fc[0] & WIFI_FC0_TYPE_CTL) || dst.is_group()) {
		return p_in;
	}

	if (!nfo) {
		_table.insert(src, ScyllaDupeFilterDstInfo(src));
		nfo = _table.findp(src);
		nfo->clear();
	}

	if (seq == nfo->seq && (!is_frag || frag <= nfo->frag)) {
		nfo->_dupes++;
		p_in->kill();
		return 0;
	}

	nfo->seq = seq;
	nfo->frag = frag;

	return p_in;
}

enum {
	H_DEBUG
};

String ScyllaWifiDupeFilter::read_handler(Element *e, void *thunk) {
	ScyllaWifiDupeFilter *td = (ScyllaWifiDupeFilter *) e;
	switch ((uintptr_t) thunk) {
	case H_DEBUG:
		return String(td->_debug) + "\n";
	default:
		return String();
	}
}

int ScyllaWifiDupeFilter::write_handler(const String &in_s, Element *e,
		void *vparam, ErrorHandler *errh) {

	ScyllaWifiDupeFilter *f = (ScyllaWifiDupeFilter *) e;
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

void ScyllaWifiDupeFilter::add_handlers() {
	add_read_handler("debug", read_handler, (void *) H_DEBUG);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ScyllaWifiDupeFilter)
