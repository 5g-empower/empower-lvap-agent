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

uint8_t sctp_pt = 0x84;

struct click_sctp {
private:
    uint16_t _src_port;
    uint16_t _dst_port;
    uint32_t _tag;
    uint32_t _checksum;
public:
    uint16_t src_port()                 { return ntohs(_src_port); }
    uint16_t dst_port()                 { return ntohs(_dst_port); }
    uint32_t tag()                      { return ntohl(_tag); }
    uint32_t checksum()                 { return ntohl(_checksum); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

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

	if (ip->ip_p != sctp_pt) {
		return p;
	}

	struct click_sctp *sctp = (struct click_sctp *) (p->data() + _offset + ip->ip_hl * 4);

	click_chatter("src port %u", sctp->src_port());
	click_chatter("dst port %u", sctp->dst_port());

	return p;

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
