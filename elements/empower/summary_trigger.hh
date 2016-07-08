#ifndef CLICK_EMPOWER_SUMMARY_HH
#define CLICK_EMPOWER_SUMMARY_HH
#include <click/straccum.hh>
#include <click/hashcode.hh>
#include <click/timer.hh>
#include <click/vector.hh>
#include "trigger.hh"
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
	Frame() :
			_ra(EtherAddress()), _ta(EtherAddress()), _tsft(0), _flags(0),
			_seq(0), _rssi(0), _rate(0), _type(0), _subtype(0), _length(0) {
	}
	Frame(EtherAddress ra, EtherAddress ta, uint64_t tsft, uint16_t flags,
			uint16_t seq, int8_t rssi, uint8_t rate, uint8_t type,
			uint8_t subtype, uint32_t length) :
			_ra(ra), _ta(ta), _tsft(tsft), _flags(flags), _seq(seq),
			_rssi(rssi), _rate(rate), _type(type), _subtype(subtype), _length(length) {
	}
};

typedef Vector<Frame> FramesList;
typedef FramesList::iterator FIter;

class SummaryTrigger: public Trigger {

public:

	int _iface;
	uint32_t _sent;
	int16_t _limit;
	FramesList _frames;

	SummaryTrigger(int, EtherAddress, uint32_t, int16_t, uint16_t, EmpowerLVAPManager *, EmpowerRXStats *);
	~SummaryTrigger();

	String unparse();

	inline bool operator==(const SummaryTrigger &b) {
		return (_iface == b._iface) && (_eth == b._eth);
	}

};

CLICK_ENDDECLS
#endif /* CLICK_EMPOWER_SUMMARY_HH */
