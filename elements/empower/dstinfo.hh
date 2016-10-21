#ifndef CLICK_EMPOWER_DSTINFO_HH
#define CLICK_EMPOWER_DSTINFO_HH
#include <click/straccum.hh>
#include <click/etheraddress.hh>
#include <click/hashcode.hh>
#include <click/timer.hh>
#include <click/vector.hh>
#include "frame.hh"
CLICK_DECLS

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
	int avg() const {
		ptrdiff_t size = this->size();
		if (size == 0) {
			return 0; // No entries => 0 average
		}
		return (total / (double) size);
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
    int _sender_type;
	int _accum_rssi;
	int _squares_rssi;
	int _packets;
	int _last_rssi;
	int _last_std;
	int _last_packets;
	SMA *_sma_rssi;
	int _sma_period;
	unsigned _silent_window_count;
	int _hist_packets;
	int _iface_id;
	Timestamp _last_received;

	DstInfo() {
		_eth = EtherAddress();
		_sender_type = 0;
		_sma_period = 101;
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

	void add_sample(Frame *frame) {
		_packets++;
		_accum_rssi += frame->_rssi;
		_squares_rssi += frame->_rssi * frame->_rssi;
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
