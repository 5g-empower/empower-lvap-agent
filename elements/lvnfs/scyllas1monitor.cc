#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <click/args.hh>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>
#include "scyllas1monitor.hh"
CLICK_DECLS

ScyllaS1Monitor::ScyllaS1Monitor() :
		_debug(false), _offset(12) {
}

ScyllaS1Monitor::~ScyllaS1Monitor() {
}

int ScyllaS1Monitor::configure(Vector<String> &conf, ErrorHandler *errh) {
	return Args(conf, this, errh).read("DEBUG", _debug)
                                 .read("OFFSET", _offset)
						         .complete();
}

Packet *
ScyllaS1Monitor::simple_action(Packet *p) {

	struct click_ip *ip = (struct click_ip *) (p->data() + _offset);

	if (ip->ip_p != 132) {
		return p;
	}

	struct click_sctp *sctp = (struct click_sctp *) (p->data() + _offset + ip->ip_hl * 4);

	uint8_t *ptr =  (uint8_t *) sctp;
	uint8_t *end = ptr + (p->length() - _offset - ip->ip_hl * 4);

	ptr += sizeof(struct click_sctp);

	while (ptr < end) {
		struct click_sctp_chunk *chunk = (struct click_sctp_chunk *) ptr;
		if (chunk->type() == 0) {
			struct click_sctp_data_chunk *data = (struct click_sctp_data_chunk *) ptr;
			if (data->ppi() == 18) {
				parse_s1ap(data);
			}
		}
		if (chunk->length() % 4 == 0) {
			ptr += chunk->length();
		} else {
			ptr += (chunk->length() + 4 - chunk->length() % 4);
		}
	}

	return p;

}

void ScyllaS1Monitor::parse_s1ap(click_sctp_data_chunk *data) {

	click_chatter("s1ap");
	click_chatter("ppi %u", data->ppi());
	click_chatter("len %u", data->length());

}

enum {
	H_DEBUG,
};

String ScyllaS1Monitor::read_handler(Element *e, void *thunk) {
	ScyllaS1Monitor *td = (ScyllaS1Monitor *) e;
	switch ((uintptr_t) thunk) {
	case H_DEBUG:
		return String(td->_debug) + "\n";
	default:
		return String();
	}
}

int ScyllaS1Monitor::write_handler(const String &in_s, Element *e,
		void *vparam, ErrorHandler *errh) {
	ScyllaS1Monitor *f = (ScyllaS1Monitor *) e;
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

void ScyllaS1Monitor::add_handlers() {
	add_read_handler("debug", read_handler, (void *) H_DEBUG);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ScyllaS1Monitor)
