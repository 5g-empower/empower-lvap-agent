/*
 * summary_trigger.{cc,hh}
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
#include "summary_trigger.hh"
CLICK_DECLS

SummaryTrigger::SummaryTrigger(int iface_id, EtherAddress eth, uint32_t trigger_id, int16_t limit,
		uint16_t period, EmpowerLVAPManager * el, EmpowerRXStats * ers) :
		Trigger(trigger_id, period, el, ers), _eth(eth), _iface_id(iface_id), _sent(0), _limit(limit) {

}

SummaryTrigger::~SummaryTrigger() {
	_frames.clear();
}

String SummaryTrigger::unparse() {
	StringAccum sa;
	sa << Trigger::unparse();
	sa << " iface_id ";
	sa << _iface_id;
	sa << " limit ";
	sa << _limit;
	sa << " period ";
	sa << _period;
	sa << " frames ";
	sa << _frames.size();
	sa << " sent ";
	sa << _sent;
	return sa.take_string();
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(SummaryTrigger)

