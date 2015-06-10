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
	EtherAddress _eth;
	uint64_t _tsft;
	uint16_t _seq;
	int8_t _rssi;
	uint8_t _rate;
	uint8_t _type;
	uint8_t _subtype;
	uint32_t _length;
	uint32_t _dur;
	Frame() :
			_eth(EtherAddress()), _tsft(0), _seq(0), _rssi(0), _rate(0), _type(
					0), _subtype(0), _length(0), _dur(0) {
	}
	Frame(EtherAddress eth, uint64_t tsft, uint16_t seq, int8_t rssi,
			uint8_t rate, uint8_t type, uint8_t subtype, uint32_t length,
			uint32_t dur) :
			_eth(eth), _tsft(tsft), _seq(seq), _rssi(rssi), _rate(rate),
			_type(type), _subtype(subtype), _length(length), _dur(dur) {
	}
	String unparse() {
		StringAccum sa;
		sa << _eth.unparse() << ' ' << ' ' << _tsft << ' ' << _seq << ' '
				<< _rssi << ' ' << _rate << ' ' << _type << ' ' << _subtype
				<< ' ' << _length << ' ' << _dur;
		return sa.take_string();
	}
};

typedef Vector<Frame> FramesList;
typedef FramesList::iterator FIter;

class SummaryTrigger: public Trigger {

public:

	uint32_t _sent;
	int16_t _limit;
	FramesList _frames;

	SummaryTrigger(EtherAddress, uint32_t, int16_t, uint16_t, EmpowerLVAPManager *, EmpowerRXStats *);
	~SummaryTrigger();

	String unparse();

	inline bool operator==(const SummaryTrigger &b) {
		return _eth== b._eth && _limit == b._limit && _period == b._period;
	}

};

CLICK_ENDDECLS
#endif /* CLICK_EMPOWER_SUMMARY_HH */
