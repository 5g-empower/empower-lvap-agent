#ifndef CLICK_EMPOWER_BUSYNESSINFO_HH
#define CLICK_EMPOWER_BUSYNESSINFO_HH
#include <click/straccum.hh>
#include <click/etheraddress.hh>
#include <click/hashcode.hh>
#include <click/timer.hh>
#include <click/vector.hh>
#include <elements/wifi/bitrate.hh>
#include "frame.hh"
#include "sma.hh"
CLICK_DECLS

class BusynessInfo {
public:

	uint32_t _last_busyness; // usec
	uint32_t _accum_busyness; // usec
    int _last_packets;
	SMA *_sma_busyness;
    int _packets;
    unsigned _silent_window_count;
	int _iface_id;
	Timestamp _last_updated;

	BusynessInfo() {
		_last_packets = 0;
		_sma_busyness = 0;
		_last_busyness = 0;
		_accum_busyness = 0;
		_packets = 0;
		_silent_window_count = 0;
		_iface_id = -1;
	}

	~BusynessInfo() {
		delete _sma_busyness;
	}

	void update() {
		Timestamp delta = Timestamp::now() - _last_updated;
		_last_busyness = (_accum_busyness > 0) ? (_accum_busyness * 18000) / delta.usec() : 0;
		_silent_window_count = (_packets == 0) ? _silent_window_count + 1 : 0;
		_last_packets = _packets;
		_sma_busyness->add(_last_busyness);
		_packets = 0;
		_accum_busyness = 0;
		_last_updated.assign_now();
	}

	void add_sample(uint32_t len, uint8_t rate) {
		_packets++;
		_accum_busyness += calc_usecs_wifi_packet(len, rate, 0);
	}

	String unparse() {
		Timestamp now = Timestamp::now();
		Timestamp age = now - _last_updated;
		StringAccum sa;
		sa << "iface_id " << _iface_id;
		sa << " last_busyness " << ((double) _last_busyness / 18000);
		sa << " sma_busyness " << ((double) _sma_busyness->avg() / 18000);
		sa << " last_packets " << _last_packets;
		sa << " last_received " << age;
		sa << " silent_window_count " << _silent_window_count << "\n";
		return sa.take_string();
	}
};

CLICK_ENDDECLS
#endif /* CLICK_EMPOWER_BUSYNESSINFO_HH */
