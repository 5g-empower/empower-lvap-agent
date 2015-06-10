#ifndef CLICK_MINSTREL_HH
#define CLICK_MINSTREL_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <elements/wifi/bitrate.hh>
#include <elements/wifi/availablerates.hh>
CLICK_DECLS

/*
 * =c
 * Minstrel([, I<KEYWORDS>])
 * =s Wifi
 * Minstrel wireless bit-rate selection algorithm
 * =a SetTXRate, FilterTX
 */


struct MinstrelDstInfo {
public:
	EtherAddress eth;
	Vector<int> rates;
	Vector<int> successes;
	Vector<int> attempts;
	Vector<int> last_successes;
	Vector<int> last_attempts;
	Vector<int> hist_successes;
	Vector<int> hist_attempts;
	Vector<int> cur_prob;
	Vector<int> cur_tp;
	Vector<int> probability;
	Vector<int> sample_limit;
	int packet_count;
	int sample_count;
	int max_tp_rate;
	int max_tp_rate2;
	int max_prob_rate;
	MinstrelDstInfo() {}
	MinstrelDstInfo(EtherAddress neighbor, Vector<int> supported) {
		eth = neighbor;
		int i;
		for (i = 0; i < supported.size(); i++) {
			rates.push_back(supported[i]);
		}
		successes = Vector<int>(supported.size(), 0);
		attempts = Vector<int>(supported.size(), 0);
		last_successes = Vector<int>(supported.size(), 0);
		last_attempts = Vector<int>(supported.size(), 0);
		hist_successes = Vector<int>(supported.size(), 0);
		hist_attempts = Vector<int>(supported.size(), 0);
		cur_prob = Vector<int>(supported.size(), 0);
		cur_tp = Vector<int>(supported.size(), 0);
		probability = Vector<int>(supported.size(), 0);
		sample_limit = Vector<int>(supported.size(), -1);
		packet_count = 0;
		sample_count = 0;
		max_tp_rate = 0;
		max_tp_rate2 = 0;
		max_prob_rate = 0;
	}
	int rate_index(int rate) {
		int ndx = -1;
		for (int x = 0; x < rates.size(); x++) {
			if (rate == rates[x]) {
				ndx = x;
				break;
			}
		}
		return (ndx == rates.size()) ? -1 : ndx;
	}
	void add_result(int rate, int tries, int success) {
		int ndx = rate_index(rate);
		if (ndx >= 0) {
			successes[ndx] += success;
			attempts[ndx] += tries;
		}
	}
};

typedef HashMap<EtherAddress, MinstrelDstInfo> MinstrelNeighborTable;
typedef MinstrelNeighborTable::iterator MinstrelIter;

class Minstrel : public Element { public:

	Minstrel();
	~Minstrel();

	const char *class_name() const		{ return "Minstrel"; }
	const char *port_count() const		{ return "2/0-2"; }
	const char *processing() const		{ return "ah/a"; }
	const char *flow_code() const		{ return "#/#"; }

	int initialize(ErrorHandler *);
	int configure(Vector<String> &, ErrorHandler *);
	void run_timer(Timer *);

	void push (int, Packet *);
	Packet *pull(int);

	/* handler stuff */
	void add_handlers();
	String print_rates();

	void assign_rate(Packet *);
	void process_feedback(Packet *);

	MinstrelNeighborTable * neighbors() { return &_neighbors; }
	AvailableRates * rtable() { return _rtable; }
	AvailableRates * rtable_ht() { return _rtable_ht; }

private:

	MinstrelNeighborTable _neighbors;
	AvailableRates *_rtable;
	AvailableRates *_rtable_ht;
	Timer _timer;

	unsigned _lookaround_rate;
	unsigned _offset;
	bool _active;
	unsigned _period;
	unsigned _ewma_level;
	bool _debug;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif

