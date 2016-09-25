#ifndef CLICK_EMPOWER_FRAME_HH
#define CLICK_EMPOWER_FRAME_HH
#include <click/straccum.hh>
#include <click/etheraddress.hh>
#include <click/hashcode.hh>
#include <click/timer.hh>
#include <click/vector.hh>
CLICK_DECLS

class Frame {
public:

	EtherAddress _ra;
	EtherAddress _ta;
	uint64_t _tsft;
	uint16_t _flags;
	uint16_t _seq;
	int8_t _rssi;
	uint8_t _rate;
	uint8_t _type;
	uint8_t _subtype;
	uint32_t _length;
	uint8_t _retry;
	bool _station;
	uint8_t _iface_id;

	Frame() :
			_ra(EtherAddress()), _ta(EtherAddress()), _tsft(0), _flags(0),
			_seq(0), _rssi(0), _rate(0), _type(0), _subtype(0), _length(0),
			_retry(0), _station(false), _iface_id(0) {
	}

	Frame(EtherAddress ra, EtherAddress ta, uint64_t tsft, uint16_t flags, uint16_t seq, int8_t rssi,
			uint8_t rate, uint8_t type, uint8_t subtype, uint32_t length, uint8_t retry, bool station, int iface_id) :
			_ra(ra), _ta(ta), _tsft(tsft), _flags(flags), _seq(seq), _rssi(rssi),
			_rate(rate), _type(type), _subtype(subtype), _length(length), _retry(retry), _station(station), _iface_id(iface_id) {
	}

	String unparse() {
		StringAccum sa;
		sa << _ta.unparse() << " -> " << _ra.unparse();
		sa << " rssi " << _rssi;
		sa << " length " << _length;
		return sa.take_string();
	}
};

CLICK_ENDDECLS
#endif /* CLICK_EMPOWER_FRAME_HH */
