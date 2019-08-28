#ifndef CLICK_TRANSMISSIONPOLICY_HH
#define CLICK_TRANSMISSIONPOLICY_HH
#include <click/config.h>
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/hashtable.hh>
#include <click/bighashmap.hh>
#include <click/straccum.hh>
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

typedef HashTable<uint16_t, uint32_t> CBytes;
typedef CBytes::iterator CBytesIter;


enum empower_tx_mcast_type {
	TX_MCAST_LEGACY = 0x0,
	TX_MCAST_DMS = 0x1,
	TX_MCAST_UR = 0x2,
};

class TxPolicyInfo {
public:

	Vector<int> _mcs;
	Vector<int> _ht_mcs;
	bool _no_ack;
	empower_tx_mcast_type _tx_mcast;
	int _ur_mcast_count;
	int _rts_cts;
	int _max_amsdu_len;
	CBytes _tx;
	CBytes _rx;

	TxPolicyInfo() {
		_mcs = Vector<int>();
		_ht_mcs = Vector<int>();
		_no_ack = false;
		_tx_mcast = TX_MCAST_DMS;
		_rts_cts = 2436;
		_max_amsdu_len = 3839;
		_ur_mcast_count = 3;
	}

	TxPolicyInfo(Vector<int> mcs, Vector<int> ht_mcs, bool no_ack, empower_tx_mcast_type tx_mcast,
			int ur_mcast_count, int rts_cts, int max_amsdu_len) {

		_mcs = mcs;
		_ht_mcs = ht_mcs;
		_no_ack = no_ack;
		_tx_mcast = tx_mcast;
		_rts_cts = rts_cts;
		_max_amsdu_len = max_amsdu_len;
		_ur_mcast_count = ur_mcast_count;
	}

	void update_tx(uint16_t len) {
		if (_tx.find(len) == _tx.end()) {
			_tx.set(len, 0);
		}
		(*_tx.get_pointer(len))++;
	}

	void update_rx(uint16_t len) {
		if (_rx.find(len) == _rx.end()) {
			_rx.set(len, 0);
		}
		(*_rx.get_pointer(len))++;
	}

	String unparse() {
		StringAccum sa;
		sa << "mcs [";
		for (int i = 0; i < _mcs.size(); i++) {
			sa << " " << _mcs[i] << " ";
		}
		sa << "]";
		sa << " ht mcs [";
		for (int i = 0; i < _ht_mcs.size(); i++) {
			sa << " " << _ht_mcs[i] << " ";
		}
		sa << "]";
		sa << " no_ack " << _no_ack << " mcast ";
		if (_tx_mcast == TX_MCAST_LEGACY) {
			sa << "legacy";
		} else if (_tx_mcast == TX_MCAST_DMS) {
			sa << "dms";
		} else if (_tx_mcast == TX_MCAST_UR) {
			sa << "ur";
		} else {
			sa << "unknown";
		}
		sa << " ur_mcast_count " << _ur_mcast_count;
		sa << " rts_cts " << _rts_cts;
		sa << " max_amsdu_len " << _max_amsdu_len;
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
