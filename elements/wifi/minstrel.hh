#ifndef CLICK_MINSTREL_HH
#define CLICK_MINSTREL_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/hashtable.hh>
#include <elements/wifi/bitrate.hh>
#include "transmissionpolicies.hh"
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
	Vector<uint32_t> successes_bytes;
	Vector<uint32_t> attempts_bytes;
	Vector<uint32_t> last_successes_bytes;
	Vector<uint32_t> last_attempts_bytes;
	Vector<uint32_t> hist_successes_bytes;
	Vector<uint32_t> hist_attempts_bytes;
	Vector<int> cur_prob;
	Vector<int> cur_tp;
	Vector<int> probability;
	Vector<int> sample_limit;
	int packet_count;
	int sample_count;
	int max_tp_rate;
	int max_tp_rate2;
	int max_prob_rate;
	bool ht;
	MinstrelDstInfo() {
		eth = EtherAddress();
		rates = Vector<int>();
		successes = Vector<int>();
		attempts = Vector<int>();
		last_successes = Vector<int>();
		last_attempts = Vector<int>();
		hist_successes = Vector<int>();
		hist_attempts = Vector<int>();
		successes_bytes = Vector<uint32_t>();
		attempts_bytes = Vector<uint32_t>();
		last_successes_bytes = Vector<uint32_t>();
		last_attempts_bytes = Vector<uint32_t>();
		hist_successes_bytes = Vector<uint32_t>();
		hist_attempts_bytes = Vector<uint32_t>();
		cur_prob = Vector<int>();
		cur_tp = Vector<int>();
		probability = Vector<int>();
		sample_limit = Vector<int>();
		packet_count = 0;
		sample_count = 0;
		max_tp_rate = 0;
		max_tp_rate2 = 0;
		max_prob_rate = 0;
		ht = false;
	}
	MinstrelDstInfo(EtherAddress neighbor, Vector<int> supported, bool ht_rates) {
		eth = neighbor;
		for (int i = 0; i < supported.size(); i++) {
			rates.push_back(supported[i]);
		}
		successes = Vector<int>(supported.size(), 0);
		attempts = Vector<int>(supported.size(), 0);
		last_successes = Vector<int>(supported.size(), 0);
		last_attempts = Vector<int>(supported.size(), 0);
		hist_successes = Vector<int>(supported.size(), 0);
		hist_attempts = Vector<int>(supported.size(), 0);
		successes_bytes = Vector<uint32_t>(supported.size(), 0);
		attempts_bytes = Vector<uint32_t>(supported.size(), 0);
		last_successes_bytes = Vector<uint32_t>(supported.size(), 0);
		last_attempts_bytes = Vector<uint32_t>(supported.size(), 0);
		hist_successes_bytes = Vector<uint32_t>(supported.size(), 0);
		hist_attempts_bytes = Vector<uint32_t>(supported.size(), 0);
		cur_prob = Vector<int>(supported.size(), 0);
		cur_tp = Vector<int>(supported.size(), 0);
		probability = Vector<int>(supported.size(), 0);
		sample_limit = Vector<int>(supported.size(), -1);
		packet_count = 0;
		sample_count = 0;
		max_tp_rate = 0;
		max_tp_rate2 = 0;
		max_prob_rate = 0;
		ht = ht_rates;
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
	void add_result(int rate, int tries, int success, uint32_t pkt_length) {
		int ndx = rate_index(rate);
		if (ndx >= 0) {
			successes[ndx] += success;
			attempts[ndx] += tries;
			successes_bytes[ndx] += uint32_t (success * pkt_length);
			attempts_bytes[ndx] += uint32_t (tries * pkt_length);
		}
	}
	String unparse() {
		StringAccum sa;
		int tp, prob, eprob, rate;
		char buffer[4096];
		sa << eth << "\n";
		sa << "rate    throughput    ewma prob    this prob    this success (attempts)    success    attempts    this success_bytes    this attempts_bytes        success_bytes    attempts_bytes\n";
		for (int i = 0; i < rates.size(); i++) {
			tp = cur_tp[i] / ((18000 << 10) / 96);
			prob = cur_prob[i] / 18;
			eprob = probability[i] / 18;
			if (ht) {
				rate = rates[i];
			} else {
				rate = rates[i] / 2;
			}
			sprintf(buffer, "%2d%s    %2u.%1u    %3u.%1u    %3u.%1u    %3u (%3u)    %8llu    %8llu    %8llu    %8llu    %8llu    %8llu\n",
					rate,
					(rates[i] % 1 && !ht) ? ".5" : "  ",
					tp / 10, tp % 10,
					eprob / 10, eprob % 10,
					prob / 10, prob % 10,
					last_successes[i],
					last_attempts[i],
					(unsigned long long) hist_successes[i],
					(unsigned long long) hist_attempts[i],
                    (unsigned long long) last_successes_bytes[i],
                    (unsigned long long) last_attempts_bytes[i],
                    (unsigned long long) hist_successes_bytes[i],
                    (unsigned long long) hist_attempts_bytes[i]);
			if (i == max_tp_rate)
				sa << 'T';
			else if (i == max_tp_rate2)
				sa << 't';
			else
				sa << ' ';
			if (i == max_prob_rate)
				sa << 'P';
			else
				sa << ' ';
			sa << buffer;
		}
		sa << "\n"
		   << "Total packet count: ideal "
		   << (packet_count - sample_count)
		   << " lookaround "
		   << sample_count
		   << " total "
		   << packet_count
		   << "\n\n";
		return sa.take_string();
	}
};

typedef HashMap<EtherAddress, MinstrelDstInfo> MinstrelNeighborTable;
typedef MinstrelNeighborTable::iterator MinstrelIter;

typedef HashTable<uint8_t, uint32_t> TTime;
typedef TTime::iterator TTimeIter;

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

	inline uint32_t estimate_usecs_wifi_packet(Packet *p) {
		struct click_wifi *w = (struct click_wifi *) p->data();
		EtherAddress dst = EtherAddress(w->i_addr1);
		if (!dst.is_broadcast() && !dst.is_group()) {
			uint32_t rate = 1;
			MinstrelDstInfo *nfo = _neighbors.findp(dst);
			if (nfo) {
				rate = nfo->rates[nfo->max_tp_rate];
			}
			return calc_usecs_wifi_packet(p->length(), rate, 0);
		} else {
			return calc_usecs_wifi_packet(p->length(), 1, 0);
		}
	}

	MinstrelNeighborTable * neighbors() { return &_neighbors; }
	TransmissionPolicies * tx_policies() { return _tx_policies; }
	bool forget_station(EtherAddress addr) { return _neighbors.erase(addr); }

	MinstrelDstInfo * insert_neighbor(EtherAddress dst, TxPolicyInfo * txp) {
		MinstrelDstInfo *nfo;
		if (txp->_ht_mcs.size()) {
			_neighbors.insert(dst, MinstrelDstInfo(dst, txp->_ht_mcs, true));
			nfo = _neighbors.findp(dst);
		} else {
			_neighbors.insert(dst, MinstrelDstInfo(dst, txp->_mcs, false));
			nfo = _neighbors.findp(dst);
		}
		return nfo;
	}

private:

	MinstrelNeighborTable _neighbors;
	TransmissionPolicies * _tx_policies;
	Timer _timer;
	TTime _transm_time;

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

