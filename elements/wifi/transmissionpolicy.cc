/*
 * transmissionpolicy.{cc,hh} -- Transmission Policy
 * Roberto Riggio
 *
 * Copyright (c) 2016 CREATE-NET
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
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "transmissionpolicy.hh"
CLICK_DECLS

TransmissionPolicy::TransmissionPolicy() {
}

TransmissionPolicy::~TransmissionPolicy() {
}

int TransmissionPolicy::configure(Vector<String> &conf, ErrorHandler *errh) {

	int res = 0;
	bool no_ack = false;
	int rts_cts = 2436;
	String mcs_string;
	String mcs_select_string = "BR";
	tp_mcs_selec_types mcs_select;
	Vector<int> mcs;

	res = Args(conf, this, errh).read_m("MCS", mcs_string)
								.read("NO_ACK", no_ack)
								.read("MCS_SELECT", mcs_select_string)
								.read("RTS_CTS", rts_cts)
			                    .complete();

	if (mcs_select_string == "BR") {
		mcs_select = TP_BR;
	} else if (mcs_select_string == "BP") {
		mcs_select = TP_BP;
	} else if (mcs_select_string == "LR") {
		mcs_select = TP_LR;
	} else {
		return errh->error("error param MCS_SELECT: must be in the form BR/BP/LR");
	}

	Vector<String> args;
	cp_spacevec(mcs_string, args);

	for (int x = 0; x < args.size(); x++) {
		int r = 0;
		IntArg().parse(args[x], r);
		mcs.push_back(r);
	}

	_tx_policy = TxPolicyInfo(mcs, no_ack, mcs_select, rts_cts);
	return res;

}

CLICK_ENDDECLS
EXPORT_ELEMENT(TransmissionPolicy)
