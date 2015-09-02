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
		_debug(false), _buffer_size(10) {
}

ScyllaWifiDupeFilter::~ScyllaWifiDupeFilter() {
}

int ScyllaWifiDupeFilter::configure(Vector<String> &conf, ErrorHandler *errh) {
	return Args(conf, this, errh).read("BUFFER_SIZE", _buffer_size)
			                     .read("DEBUG", _debug)
						         .complete();
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
	uint8_t more_frag = w->i_fc[1] & WIFI_FC1_MORE_FRAG;

	bool is_frag = frag || more_frag;

	DupeFilterDstInfo *nfo = _dupes_table.findp(src);

	if ((w->i_fc[0] & WIFI_FC0_TYPE_CTL) || dst.is_group() || is_frag) {
		return p_in;
	}

	if (!nfo) {
		_dupes_table.insert(src, DupeFilterDstInfo(src, _buffer_size));
		nfo = _dupes_table.findp(src);
	}

	if (nfo->_buffer->contains(seq)) {
		nfo->_dupes++;
		p_in->kill();
		return 0;
	}

	nfo->_buffer->add(seq);
	return p_in;
}

enum {
	H_DEBUG,
	H_DUPES_TABLE
};

String ScyllaWifiDupeFilter::read_handler(Element *e, void *thunk) {
	ScyllaWifiDupeFilter *td = (ScyllaWifiDupeFilter *) e;
	switch ((uintptr_t) thunk) {
	case H_DEBUG:
		return String(td->_debug) + "\n";
	case H_DUPES_TABLE: {
		StringAccum sa;
		for (DupesIter it = td->dupes_table()->begin(); it.live(); it++) {
			sa << it.value()._eth.unparse() << ' ' << it.value()._dupes << ' '
					<< it.value()._buffer_size << it.value()._buffer->unparse()
					<< ' ' << "\n";
		}
		return sa.take_string();
	}
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
	case H_DUPES_TABLE: {
		Vector<String> tokens;
		cp_spacevec(s, tokens);
		if (tokens.size() < 4)
			return errh->error("state_table requires at least 4 parameters <eth> <dupes> <N> <seq1> ... <seqN>");
		EtherAddress eth;
		int dupes;
		int buffer_size;
		if (!EtherAddressArg().parse(tokens[0], eth)) {
			return errh->error("error param %s: must start with Ethernet address", tokens[0].c_str());
		}
		if (!IntArg().parse(tokens[1], dupes)) {
			return errh->error("error param %s: must start with int", tokens[1].c_str());
		}
		if (!IntArg().parse(tokens[2], buffer_size)) {
			return errh->error("error param %s: must start with int", tokens[2].c_str());
		}
		SeqBuffer * buffer = new SeqBuffer(buffer_size);
		for (int i = 3; i < tokens.size(); i++) {
			int seq;
			if (!IntArg().parse(tokens[i], seq)) {
				return errh->error("error param %s: must start with int", tokens[i].c_str());
			}
			buffer->add(seq);
		}
		f->dupes_table()->insert(eth, DupeFilterDstInfo(eth, dupes, buffer_size, buffer));
		break;
	}
	}
	return 0;

}

void ScyllaWifiDupeFilter::add_handlers() {
	add_read_handler("debug", read_handler, (void *) H_DEBUG);
	add_read_handler("dupes_table", read_handler, (void *) H_DUPES_TABLE);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
	add_write_handler("dupes_table", write_handler, (void *) H_DUPES_TABLE);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ScyllaWifiDupeFilter)
