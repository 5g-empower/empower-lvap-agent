/*
 * transmissionpolicies.{cc,hh} -- Transmission Policies
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
#include "transmissionpolicies.hh"
CLICK_DECLS

TransmissionPolicies::TransmissionPolicies() : _default_tx_policy(0) {
}

TransmissionPolicies::~TransmissionPolicies() {
}

int TransmissionPolicies::configure(Vector<String> &conf, ErrorHandler *errh) {

	int res = 0;

	for (int x = 0; x < conf.size(); x++) {

		Vector<String> args;
		cp_spacevec(conf[x], args);

		if (args.size() != 2) {
			return errh->error("error param %s must have 2 args", conf[x].c_str());
		}

		if (args[0] == "DEFAULT") {

			TransmissionPolicy * tx_policy;

			if (!ElementCastArg("TransmissionPolicy").parse(args[1], tx_policy, Args(conf, this, errh))) {
				return errh->error("error param %s: must be a TransmissionPolicy element", conf[x].c_str());
			}

			_default_tx_policy = tx_policy->tx_policy();

		} else {

			EtherAddress eth;

			if (!EtherAddressArg().parse(args[0], eth)) {
				return errh->error("error param %s: must start with ethernet address", conf[x].c_str());
			}

			TransmissionPolicy * tx_policy;

			if (!ElementCastArg("TransmissionPolicy").parse(args[1], tx_policy, Args(conf, this, errh))) {
				return errh->error("error param %s: must be a TransmissionPolicy element", conf[x].c_str());
			}

			_tx_table.insert(eth, tx_policy->tx_policy());

		}

	}

	if (!_default_tx_policy) {
		_default_tx_policy = new TxPolicyInfo();
		_default_tx_policy->_mcs.push_back(2);
	}

	return res;

}

TxPolicyInfo *
TransmissionPolicies::lookup(EtherAddress eth) {

	if (!eth) {
		click_chatter("%s: lookup called with NULL eth!\n", name().c_str());
		return new TxPolicyInfo();
	}

	TxPolicyInfo * dst = _tx_table.find(eth);

	if (dst) {
		return dst;
	}

	return _default_tx_policy;

}

TxPolicyInfo *
TransmissionPolicies::supported(EtherAddress eth) {

	if (!eth) {
		click_chatter("%s: lookup called with NULL eth!\n", name().c_str());
		return new TxPolicyInfo();
	}

	TxPolicyInfo *dst = _tx_table.find(eth);

	if (dst) {
		return dst;
	}

	return 0;

}

int TransmissionPolicies::insert(EtherAddress eth, Vector<int> mcs, Vector<int> ht_mcs, bool no_ack, empower_tx_mcast_type tx_mcast, int ur_mcast_count, int rts_cts, int max_amsdu_len) {

	if (!(eth)) {
		click_chatter("TransmissionPolicies %s: You fool, you tried to insert %s\n",
					  name().c_str(),
					  eth.unparse().c_str());
		return -1;
	}

	TxPolicyInfo *dst = _tx_table.find(eth);

	if (!dst) {
		_tx_table.insert(eth, new TxPolicyInfo());
		dst = _tx_table.find(eth);
	}

	dst->_mcs.clear();
	dst->_ht_mcs.clear();
	dst->_no_ack = no_ack;
	dst->_tx_mcast = tx_mcast;
	dst->_ur_mcast_count = ur_mcast_count;
	dst->_rts_cts = rts_cts;
	dst->_max_amsdu_len = max_amsdu_len;

	if (_default_tx_policy->_mcs.size()) {
		/* only add rates that are in the default rates */
		for (int x = 0; x < mcs.size(); x++) {
			for (int y = 0; y < _default_tx_policy->_mcs.size(); y++) {
				if (mcs[x] == _default_tx_policy->_mcs[y]) {
					dst->_mcs.push_back(mcs[x]);
				}
			}
		}
	} else {
		dst->_mcs = mcs;
	}

	if (_default_tx_policy->_ht_mcs.size()) {
		/* only add rates that are in the default rates */
		for (int x = 0; x < ht_mcs.size(); x++) {
			for (int y = 0; y < _default_tx_policy->_ht_mcs.size(); y++) {
				if (ht_mcs[x] == _default_tx_policy->_ht_mcs[y]) {
					dst->_ht_mcs.push_back(ht_mcs[x]);
				}
			}
		}
	} else {
		dst->_ht_mcs = ht_mcs;
	}

	return 0;

}

int TransmissionPolicies::remove(EtherAddress eth) {

	if (!(eth)) {
		click_chatter("TransmissionPolicies %s: You fool, you tried to insert %s\n",
					  name().c_str(),
					  eth.unparse().c_str());
		return -1;
	}

	TxPolicyInfo *dst = _tx_table.find(eth);

	if (!dst) {
		return -1;
	}

	_tx_table.remove(eth);

	return 0;

}

void TransmissionPolicies::clear() {
	for (TxTableIter it = _tx_table.begin(); it.live(); it++) {
		remove(it.key());
	}
}

enum {
	H_POLICIES
};

String TransmissionPolicies::read_handler(Element *e, void *thunk) {
	TransmissionPolicies *td = (TransmissionPolicies *) e;
	switch ((uintptr_t) thunk) {
	case H_POLICIES: {
	    StringAccum sa;
	    sa << "DEFAULT " << td->_default_tx_policy->unparse() << "\n";
		for (TxTableIter it = td->_tx_table.begin(); it.live(); it++) {
		    sa << it.key().unparse() << " " << it.value()->unparse() << "\n";
		}
		return sa.take_string();
	}
	default:
		return String();
	}
}

void TransmissionPolicies::add_handlers() {
	add_read_handler("policies", read_handler, (void *) H_POLICIES);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TransmissionPolicies)

