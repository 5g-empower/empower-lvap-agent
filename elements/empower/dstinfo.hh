#ifndef CLICK_EMPOWER_DSTINFO_HH
#define CLICK_EMPOWER_DSTINFO_HH
#include <click/straccum.hh>
#include <click/etheraddress.hh>
#include <click/hashcode.hh>
#include <click/timer.hh>
#include <click/vector.hh>
#include "frame.hh"
#include "sma.hh"
CLICK_DECLS

class DstInfo {
public:
	EtherAddress _eth;
    int _sender_type;
	int _accum_rssi;
	int _squares_rssi;
	int _packets;
	int _last_rssi;
	int _last_std;
	int _last_packets;
	SMA *_sma_rssi;
	unsigned _silent_window_count;
	int _hist_packets;
	int _iface_id;
	Timestamp _last_received;

	DstInfo() {
		_eth = EtherAddress();
		_sender_type = 0;
		_sma_rssi = 0;
		_accum_rssi = 0;
		_squares_rssi = 0;
		_silent_window_count = 0;
		_packets = 0;
		_last_rssi = 0;
		_last_std= 0;
		_last_packets= 0;
		_hist_packets = 0;
		_iface_id = -1;
	}

	~DstInfo() {
		delete _sma_rssi;
	}

	void update() {
		_hist_packets += _packets;
		_last_rssi = (_packets > 0) ? _accum_rssi / (double) _packets : 0;
		_last_std = (_packets > 0) ? sqrt( (_squares_rssi / (double) _packets) - (_last_rssi * _last_rssi) ) : 0;
		_last_packets = _packets;
		if (_packets == 0) {
			_silent_window_count++;
		} else {
			_silent_window_count = 0;
			_sma_rssi->add(_last_rssi);
		}
		_packets = 0;
		_accum_rssi = 0;
		_squares_rssi = 0;
	}

	void add_sample(uint8_t rssi) {
		_packets++;
		_accum_rssi += rssi;
		_squares_rssi += rssi * rssi;
		_last_received.assign_now();
	}

	String unparse() {
		Timestamp now = Timestamp::now();
		StringAccum sa;
		Timestamp age = now - _last_received;
		sa << _eth.unparse();
		sa << (_sender_type == 0 ? " STA" : " AP");
		sa << " sma_rssi " << _sma_rssi->avg();
		sa << " last_rssi_avg " << _last_rssi;
		sa << " last_rssi_std " << _last_std;
		sa << " last_packets " << _last_packets;
		sa << " hist_packets " << _hist_packets;
		sa << " last_received " << age;
		sa << " silent_window_count " << _silent_window_count;
		sa << " iface_id " << _iface_id << "\n";
		return sa.take_string();
	}
};

CLICK_ENDDECLS
#endif /* CLICK_EMPOWER_DSTINFO_HH */
