#ifndef CLICK_TRANSMISSIONPOLICY_HH
#define CLICK_TRANSMISSIONPOLICY_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
=c

TransmissionPolicy()

=s Wifi, Wireless Station, Wireless AccessPoint

Tracks bit-rate capabilities of other stations.

=d

Tracks a list of bitrates other stations are capable of.

=a BeaconScanner
 */


enum tp_mcs_selec_types {
	TP_BR = 0x0,
	TP_BP = 0x1,
	TP_LR = 0x2,
};


class TxPolicyInfo {
public:

	Vector<int> _mcs;
	bool _no_ack;
	tp_mcs_selec_types _mcs_select;
	int _rts_cts;

	TxPolicyInfo() {
		_mcs = Vector<int>();
		_no_ack = false;
		_mcs_select = TP_BR;
		_rts_cts = 2436;
	}

	TxPolicyInfo(Vector<int> mcs, bool no_ack, tp_mcs_selec_types mcs_select, int rts_cts) {
		_mcs = mcs;
		_no_ack = no_ack;
		_mcs_select = mcs_select;
		_rts_cts = rts_cts;
	}

	String unparse() {
		StringAccum sa;
		sa << "mcs [";
		for (int i = 0; i < _mcs.size(); i++) {
			sa << " " << _mcs[i] << " ";
		}
		sa << "]";
		sa << " no_ack " << _no_ack << " mcs_select ";
		if (_mcs_select == TP_BR) {
			sa << "best rate";
		} else if (_mcs_select == TP_BR) {
			sa << "best rate";
		} else if (_mcs_select == TP_BP) {
			sa << "best prob.";
		} else if (_mcs_select == TP_LR) {
			sa << "lowest rate";
		} else {
			sa << "unknown";
		}
		sa << " rts_cts " << _rts_cts;
		return sa.take_string();
	}

};

class TransmissionPolicy : public Element { public:

  TransmissionPolicy() CLICK_COLD;
  ~TransmissionPolicy() CLICK_COLD;

  const char *class_name() const		{ return "TransmissionPolicy"; }
  const char *port_count() const		{ return PORTS_0_0; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  TxPolicyInfo * tx_policy() { return &_tx_policy; }

private:

  TxPolicyInfo _tx_policy;

};

CLICK_ENDDECLS
#endif
