/*
 * empowerpowersavefilter.{cc,hh} -- handles 802.11 power save mode (EmPOWER Access Point)
 * Roberto Riggio
 *
 * Copyright (c) 2013 CREATE-NET
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
#include "empowerpowersavefilter.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <clicknet/wifi.h>
#include <clicknet/ether.h>
#include <click/packet_anno.hh>
#include <click/etheraddress.hh>
#include "empowerlvapmanager.hh"
#include "empowerpowersavebuffer.hh"
CLICK_DECLS

EmpowerPowerSaveFilter::EmpowerPowerSaveFilter() :
		_el(0), _epsb(0), _debug(false) {
}

EmpowerPowerSaveFilter::~EmpowerPowerSaveFilter() {
}

int EmpowerPowerSaveFilter::configure(Vector<String> &conf,
		ErrorHandler *errh) {

	return Args(conf, this, errh)
			.read_m("EL", ElementCastArg("EmpowerLVAPManager"), _el)
            .read("EPSB", ElementCastArg("EmpowerPowerSaveBuffer"), _epsb)
			.read("DEBUG", _debug).complete();

}

Packet *
EmpowerPowerSaveFilter::simple_action(Packet *p) {

    if (p->length() < sizeof(struct click_wifi)) {
        click_chatter("%{element} :: %s :: packet too small: %d vs %d",
                      this,
                      __func__,
                      p->length(),
                      sizeof(struct click_ether));
        p->kill();
        return 0;
    }

    struct click_wifi *w = (struct click_wifi *) p->data();
    uint8_t dir = w->i_fc[1] & WIFI_FC1_DIR_MASK;
    EtherAddress src;

    switch (dir) {
    case WIFI_FC1_DIR_NODS:
    case WIFI_FC1_DIR_TODS:
    case WIFI_FC1_DIR_DSTODS:
        src = EtherAddress(w->i_addr2);
        break;
    case WIFI_FC1_DIR_FROMDS:
        src = EtherAddress(w->i_addr3);
        break;
    default:
        click_chatter("%{element} :: %s :: invalid dir %d",
                      this,
                      __func__,
                      dir);
        return p;
    }

	EmpowerStationState *ess = _el->lvaps()->get_pointer(src);

	if (!ess) {
		return p;
	}

	if (ess->_power_save && (!(w->i_fc[1] & WIFI_FC1_PWR_MGT))) {
		ess->_power_save = (w->i_fc[1] & WIFI_FC1_PWR_MGT);
		_epsb->touch_queue(ess->_bssid);
	} else {
		ess->_power_save = (w->i_fc[1] & WIFI_FC1_PWR_MGT);
	}

	return p;

}

enum {
	H_DEBUG
};

String EmpowerPowerSaveFilter::read_handler(Element *e, void *thunk) {
	EmpowerPowerSaveFilter *td = (EmpowerPowerSaveFilter *) e;
	switch ((uintptr_t) thunk) {
	case H_DEBUG:
		return String(td->_debug) + "\n";
	default:
		return String();
	}
}

int EmpowerPowerSaveFilter::write_handler(const String &in_s, Element *e,
		void *vparam, ErrorHandler *errh) {

	EmpowerPowerSaveFilter *f = (EmpowerPowerSaveFilter *) e;
	String s = cp_uncomment(in_s);

	switch ((intptr_t) vparam) {
	case H_DEBUG: {    //debug
		bool debug;
		if (!BoolArg().parse(s, debug))
			return errh->error("debug parameter must be boolean");
		f->_debug = debug;
		break;
	}
	}
	return 0;
}

void EmpowerPowerSaveFilter::add_handlers() {
	add_read_handler("debug", read_handler, (void *) H_DEBUG);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EmpowerPowerSaveFilter)
ELEMENT_REQUIRES(userlevel)
