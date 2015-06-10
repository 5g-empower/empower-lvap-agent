#ifndef CLICK_EMPOWER_DSTINFO_HH
#define CLICK_EMPOWER_DSTINFO_HH
#include <click/straccum.hh>
#include <click/etheraddress.hh>
#include <click/hashcode.hh>
#include <click/timer.hh>
#include <click/vector.hh>
CLICK_DECLS

class EWMA {
public:

	EWMA() : internal(0), weight(75) {
	}

	EWMA(unsigned long weight) : internal(0), weight(weight) {
	}

	~EWMA() {
	}

	void add(int val) {
		internal = (internal != 0) ? (val * (100 - weight) + internal * weight) / 100 : val;
	}

	double avg() const {
		return internal;
	}

private:

	double internal;
	long weight;

};

class SMA {
public:
	SMA(unsigned int period) :
		period(period), window(new int[period]), head(NULL), tail(NULL), total(0) {
		assert(period >= 1);
	}
	~SMA() {
		delete[] window;
	}
	// Adds a value to the average, pushing one out if necessary
	void add(int val) {
		// Special case: Initialisation
		if (head == NULL) {
			head = window;
			*head = val;
			tail = head;
			inc(tail);
			total = val;
			return;
		}
		// Were we already full?
		if (head == tail) {
			// Fix total-cache
			total -= *head;
			// Make room
			inc(head);
		}
		// Write the value in the next spot.
		*tail = val;
		inc(tail);
		// Update our total-cache
		total += val;
	}

	// Returns the average of the last P elements added to this SMA.
	// If no elements have been added yet, returns 0.0
	double avg() const {
		ptrdiff_t size = this->size();
		if (size == 0) {
			return 0; // No entries => 0 average
		}
		return total / (double) size;
	}

private:
	unsigned int period;
	int * window; // Holds the values to calculate the average of.

	// Logically, head is before tail
	int * head; // Points at the oldest element we've stored.
	int * tail; // Points at the newest element we've stored.

	int total; // Cache the total so we don't sum everything each time.

	// Bumps the given pointer up by one.
	// Wraps to the start of the array if needed.
	void inc(int * & p) {
		if (++p >= window + period) {
			p = window;
		}
	}

	// Returns how many numbers we have stored.
	ptrdiff_t size() const {
		if (head == NULL)
			return 0;
		if (head == tail)
			return period;
		return (period + tail - head) % period;
	}
};

class DstInfo {
public:
	EtherAddress _eth;
	int _accum_rssi;
	int _squares_rssi;
	int _packets;
	double _last_rssi;
	double _last_std;
	int _last_packets;
	SMA *_sma_rssi;
	int _sma_period;
	int _aging;
	EWMA * _ewma_rssi;
	int _ewma_level;
	int _hist_rssi;
	int _hist_packets;
	int _iface_id;
	Timestamp _last_received;

	DstInfo() {
		_eth = EtherAddress();
		_sma_period = 101;
		_sma_rssi = new SMA(_sma_period);
		_ewma_level = 80;
		_accum_rssi = 0;
		_squares_rssi = 0;
		_aging = -95;
		_packets = 0;
		_ewma_rssi = new EWMA(_ewma_level);
		_last_rssi = 0;
		_last_std= 0;
		_last_packets= 0;
		_hist_rssi = 0;
		_hist_packets = 0;
		_iface_id = -1;
	}

	DstInfo(EtherAddress eth, int ewma_level, int sma_period, int aging) {
		_eth = eth;
		_sma_period = sma_period;
		_sma_rssi = new SMA(_sma_period);
		_ewma_level = ewma_level;
		_aging = aging;
		_accum_rssi = 0;
		_squares_rssi = 0;
		_packets = 0;
		_ewma_rssi = new EWMA(_ewma_level);
		_last_rssi = 0;
		_last_std = 0;
		_last_packets= 0;
		_hist_rssi = 0;
		_hist_packets = 0;
		_iface_id = -1;
	}

	void update() {
		// Implement simple aging mechanism
		if (_packets == 0) {
			_ewma_rssi->add(_aging);
			_sma_rssi->add(_aging);
		}
		// Update stats
		_hist_rssi += _accum_rssi;
		_hist_packets += _packets;
		_last_rssi = (_packets > 0) ? _accum_rssi / (double) _packets : 0;
		_last_std = (_packets > 0) ? sqrt( (_squares_rssi / (double) _packets) - (_last_rssi * _last_rssi) ) : 0;
		_last_packets = _packets;
		_packets = 0;
		_accum_rssi = 0;
		_squares_rssi = 0;
	}

	void add_rssi_sample(int rssi) {
		_packets++;
		_accum_rssi += rssi;
		_squares_rssi += rssi * rssi;
		_ewma_rssi->add(rssi);
		_sma_rssi->add(rssi);
		_last_received.assign_now();
	}

	String unparse(bool station) {
		Timestamp now = Timestamp::now();
		StringAccum sa;
		Timestamp age = now - _last_received;
		sa << _eth.unparse();
		sa << (station ? " STA" : " AP");
		sa << " sma_rssi " << _sma_rssi->avg();
		sa << " ewma_rssi " << _ewma_rssi->avg();
		sa << " last_rssi_avg " << _last_rssi;
		sa << " last_rssi_std " << _last_std;
		sa << " last_packets " << _last_packets;
		sa << " hist_packets " << _hist_packets;
		sa << " last_received " << age;
		sa << " iface_id " << _iface_id << "\n";
		return sa.take_string();
	}
};

CLICK_ENDDECLS
#endif /* CLICK_EMPOWER_DSTINFO_HH */
