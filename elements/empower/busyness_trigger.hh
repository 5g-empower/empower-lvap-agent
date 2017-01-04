#ifndef CLICK_EMPOWERBUSYNESS_HH
#define CLICK_EMPOWERBUSYNESS_HH
#include <click/straccum.hh>
#include <click/hashcode.hh>
#include <click/timer.hh>
#include <click/vector.hh>
#include "empowerpacket.hh"
#include "busynessinfo.hh"
#include "trigger.hh"
CLICK_DECLS

class BusynessTrigger: public Trigger {
public:

	int _iface_id;
	empower_trigger_relation _rel;
	int _val;
	bool _dispatched;

	BusynessTrigger(int, uint32_t, empower_trigger_relation, int, bool, uint16_t, EmpowerLVAPManager *, EmpowerRXStats *);
	~BusynessTrigger();

	String unparse();

	bool matches(const BusynessInfo * nfo);

	inline bool operator==(const BusynessTrigger &b) {
		return (_iface_id == b._iface_id) && (_rel == b._rel) && (_val == b._val);
	}

};

CLICK_ENDDECLS
#endif /* CLICK_EMPOWER_BUSYNESS_HH */
