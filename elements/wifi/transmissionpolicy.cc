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
	int max_amsdu_len = 3839;
	String mcs_string;
	String ht_mcs_string;
	String tx_mcast_string = "LEGACY";
	empower_tx_mcast_type tx_mcast;
	int ur_mcast_count = 3;
	Vector<int> mcs;
	Vector<int> ht_mcs;

	res = Args(conf, this, errh).read_m("MCS", mcs_string)
								.read_m("HT_MCS", ht_mcs_string)
								.read("NO_ACK", no_ack)
								.read("TX_MCAST", tx_mcast_string)
								.read("UR_MCAST_COUNT", ur_mcast_count)
								.read("RTS_CTS", rts_cts)
								.read("MAX_AMSDU_LEN", max_amsdu_len)
			                    .complete();

	if (tx_mcast_string == "LEGACY") {
		tx_mcast = TX_MCAST_LEGACY;
	} else if (tx_mcast_string == "DMS") {
		tx_mcast = TX_MCAST_DMS;
	} else if (tx_mcast_string == "UR") {
		tx_mcast = TX_MCAST_UR;
	} else {
		return errh->error("error param TX_MCAST: must be in the form LEGACY/DMS/UR");
	}

	Vector<String> args;
	cp_spacevec(mcs_string, args);

	for (int x = 0; x < args.size(); x++) {
		int r = 0;
		IntArg().parse(args[x], r);
		mcs.push_back(r);
	}

	Vector<String> ht_args;
	cp_spacevec(ht_mcs_string, ht_args);

	for (int x = 0; x < ht_args.size(); x++) {
		int r = 0;
		IntArg().parse(ht_args[x], r);
		ht_mcs.push_back(r);
	}

	_tx_policy = TxPolicyInfo(mcs, ht_mcs, no_ack, tx_mcast, ur_mcast_count, rts_cts, max_amsdu_len);
	return res;

}

CLICK_ENDDECLS
EXPORT_ELEMENT(TransmissionPolicy)
