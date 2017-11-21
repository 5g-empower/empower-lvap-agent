/*
 * empowertee.{cc,hh} -- element duplicates packets according to the destination address
 * Roberto Riggio
 *
 * Copyright (c) 2017 Roberto Riggio
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
#include "empowertee.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include "empowerlvapmanager.hh"
CLICK_DECLS

EmpowerTee::EmpowerTee() : _el(0) {
}

int EmpowerTee::configure(Vector<String> &conf, ErrorHandler *errh) {

	unsigned n = noutputs();

	int res = Args(conf, this, errh)
					.read_p("N", n)
					.read_m("EL", ElementCastArg("EmpowerLVAPManager"), _el)
					.complete();

	if (res < 0)
		return -1;

	if (n != (unsigned) noutputs())
		return errh->error("%d outputs implies %d arms", noutputs(), noutputs());

	return 0;

}

void EmpowerTee::push(int, Packet *p) {

	if (p->length() < sizeof(struct click_ether)) {
		click_chatter("%{element} :: %s :: packet too small: %d vs %d",
					  this,
					  __func__,
					  p->length(),
					  sizeof(struct click_ether));
		p->kill();
		return;
	}

	click_ether *eh = (click_ether *) p->data();
	EtherAddress dst = EtherAddress(eh->ether_dhost);

	// frame is unicast then send only to the correct interface
	if (!dst.is_broadcast() && !dst.is_group()) {
		EmpowerStationState *ess = _el->get_ess(dst);
		if (!ess) {
			p->kill();
			return;
		}
		output(ess->_iface_id).push(p);
		return;
	}

	// if multicast or broadcast then send to all ports
	int n = noutputs();
	for (int i = 0; i < n - 1; i++) {
		if (Packet *q = p->clone()) {
			output(i).push(q);
		}
	}
	output(n - 1).push(p);

}

CLICK_ENDDECLS
EXPORT_ELEMENT(EmpowerTee)
ELEMENT_REQUIRES(userlevel)
