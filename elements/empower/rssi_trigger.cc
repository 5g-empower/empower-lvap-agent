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
#include "trigger.hh"
CLICK_DECLS

RssiTrigger::RssiTrigger(EtherAddress eth, uint32_t trigger_id, relation_t rel, int val, bool dispatched, uint16_t period, EmpowerLVAPManager * el, EmpowerRXStats * ers) :
		Trigger(eth, trigger_id, period, el, ers), _rel(rel), _val(val), _dispatched(dispatched) {

}

RssiTrigger::~RssiTrigger() {
}

String RssiTrigger::unparse() {
	StringAccum sa;
	sa << _eth.unparse();
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

bool RssiTrigger::matches(const DstInfo* nfo) {
	if (_eth != nfo->_eth)
		return false;
	bool match = false;
	switch (_rel) {
	case EQ:
		match = (nfo->_ewma_rssi->avg() == _val);
		break;
	case GT:
		match = (nfo->_ewma_rssi->avg() > _val);
		break;
	case LT:
		match = (nfo->_ewma_rssi->avg() < _val);
		break;
	case GE:
		match = (nfo->_ewma_rssi->avg() >= _val);
		break;
	case LE:
		match = (nfo->_ewma_rssi->avg() <= _val);
		break;
	}
	return match;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(RssiTrigger)

