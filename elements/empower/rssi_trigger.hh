#ifndef CLICK_EMPOWERRSSI_HH
#define CLICK_EMPOWERRSSI_HH
#include <click/straccum.hh>
#include <click/hashcode.hh>
#include <click/timer.hh>
#include <click/vector.hh>
#include "empowerpacket.hh"
#include "dstinfo.hh"
#include "trigger.hh"
CLICK_DECLS

class RssiTrigger: public Trigger {
public:

	EtherAddress _eth;
	empower_trigger_relation _rel;
	int _val;
	bool _dispatched;

	RssiTrigger(EtherAddress, uint32_t, empower_trigger_relation, int, bool, uint16_t, EmpowerLVAPManager *, EmpowerRXStats *);
	~RssiTrigger();

	String unparse();

	bool matches(const DstInfo* nfo);

	inline bool operator==(const RssiTrigger &b) {
		return (_eth == b._eth) && (_rel == b._rel) && (_val == b._val);
	}

};

CLICK_ENDDECLS
#endif /* CLICK_EMPOWER_RSSI_HH */
