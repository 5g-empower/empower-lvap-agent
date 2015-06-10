#ifndef CLICK_EMPOWERDUMP_HH
#define CLICK_EMPOWERDUMP_HH
#include <click/straccum.hh>
#include <click/hashcode.hh>
#include <click/timer.hh>
#include <click/vector.hh>
#include "dstinfo.hh"
#include "trigger.hh"
CLICK_DECLS

enum relation_t {
	EQ = 0,
	GT = 1,
	LT = 2,
	GE = 3,
	LE = 4,
};

class RssiTrigger: public Trigger {
public:

	relation_t _rel;
	int _val;
	bool _dispatched;

	RssiTrigger(EtherAddress, uint32_t, relation_t, int, bool, uint16_t, EmpowerLVAPManager *, EmpowerRXStats *);
	~RssiTrigger();

	String unparse();

	bool matches(const DstInfo* nfo);

	inline bool operator==(const RssiTrigger &b) {
		return (_eth== b._eth) && (_rel == b._rel) && (_val == b._val);
	}

};

CLICK_ENDDECLS
#endif /* CLICK_EMPOWER_DUMP_HH */
