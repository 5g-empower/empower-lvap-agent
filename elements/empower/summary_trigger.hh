#ifndef CLICK_EMPOWER_SUMMARY_HH
#define CLICK_EMPOWER_SUMMARY_HH
#include <click/straccum.hh>
#include <click/hashcode.hh>
#include <click/timer.hh>
#include <click/vector.hh>
#include "trigger.hh"
#include "frame.hh"
CLICK_DECLS

typedef Vector<Frame> FramesList;
typedef FramesList::iterator FIter;

class SummaryTrigger: public Trigger {

public:

	EtherAddress _eth;
	int _iface_id;
	uint32_t _sent;
	int16_t _limit;
	FramesList _frames;

	SummaryTrigger(int, EtherAddress, uint32_t, int16_t, uint16_t, EmpowerLVAPManager *, EmpowerRXStats *);
	~SummaryTrigger();

	String unparse();

	inline bool operator==(const SummaryTrigger &b) {
		return (_iface_id == b._iface_id) && (_eth == b._eth);
	}

};

CLICK_ENDDECLS
#endif /* CLICK_EMPOWER_SUMMARY_HH */
