/*
 * rssi_trigger.{cc,hh}
 * Roberto Riggio
 *
 * Copyright (c) 2009 CREATE-NET
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "empowerrxstats.hh"
#include "empowerlvapmanager.hh"
#include "busyness_trigger.hh"
CLICK_DECLS

BusynessTrigger::BusynessTrigger(int iface_id, uint32_t trigger_id,
		empower_trigger_relation rel, int val, bool dispatched, uint16_t period,
		EmpowerLVAPManager * el, EmpowerRXStats * ers) :
		Trigger(trigger_id, period, el, ers), _iface_id(iface_id), _rel(rel), _val(val),
		_dispatched(dispatched) {

}

BusynessTrigger::~BusynessTrigger() {
}

String BusynessTrigger::unparse() {
	StringAccum sa;
	sa << Trigger::unparse();
	sa << "iface_id ";
	sa << _iface_id;
	sa << " rel ";
	switch (_rel) {
	case EQ:
		sa << "EQ";
		break;
	case GT:
		sa << "GT";
		break;
	case LT:
		sa << "LT";
		break;
	case GE:
		sa << "GE";
		break;
	case LE:
		sa << "LE";
		break;
	}
	sa << " val " << _val;
	if (_dispatched) {
		sa << " dispatched";
	}
	return sa.take_string();
}

bool BusynessTrigger::matches(const BusynessInfo* nfo) {
	bool match = false;
	switch (_rel) {
	case EQ:
		match = (nfo->_sma_busyness->avg() == _val);
		break;
	case GT:
		match = (nfo->_sma_busyness->avg() > _val);
		break;
	case LT:
		match = (nfo->_sma_busyness->avg() < _val);
		break;
	case GE:
		match = (nfo->_sma_busyness->avg() >= _val);
		break;
	case LE:
		match = (nfo->_sma_busyness->avg() <= _val);
		break;
	}
	return match;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(BusynessTrigger)
