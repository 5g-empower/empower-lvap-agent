#ifndef CLICK_EMPOWER_TRIGGER_HH
#define CLICK_EMPOWER_TRIGGER_HH
#include <click/straccum.hh>
#include <click/hashcode.hh>
#include <click/timer.hh>
#include <click/vector.hh>
#include <click/etheraddress.hh>
#include "dstinfo.hh"
CLICK_DECLS

enum empower_trigger_relation {
	EQ = 0x0,
	GT = 0x1,
	LT = 0x2,
	GE = 0x3,
	LE = 0x4,
};

class EmpowerLVAPManager;
class EmpowerRXStats;

class Trigger {

public:

	uint32_t _trigger_id;
	Timer * _trigger_timer;
	uint16_t _period;
	EmpowerLVAPManager * _el;
	EmpowerRXStats * _ers;

	Trigger(uint32_t trigger_id, uint16_t period, EmpowerLVAPManager * el, EmpowerRXStats * ers) :
			_trigger_id(trigger_id), _period(period), _el(el), _ers(ers) {

		_trigger_timer = new Timer();

	}
	~Trigger() {
		_trigger_timer->clear();
	}

	String unparse() {
		StringAccum sa;
		sa << _trigger_id;
		return sa.take_string();
	}

	inline bool operator==(const Trigger &b) {
		return _trigger_id == b._trigger_id;
	}

};

CLICK_ENDDECLS
#endif /* CLICK_EMPOWER_TRIGGER_HH */
