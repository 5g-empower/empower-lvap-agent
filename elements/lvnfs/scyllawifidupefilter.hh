#ifndef CLICK_SCYLLAWIFIDUPEFILTER_HH
#define CLICK_SCYLLAWIFIDUPEFILTER_HH
#include <click/element.hh>
#include <click/string.hh>
#include <click/hashmap.hh>
CLICK_DECLS

class SeqBuffer {
public:
	SeqBuffer(unsigned int period) :
		period(period), window(new int[period]), head(NULL), tail(NULL) {
		assert(period >= 1);
	}
	~SeqBuffer() {
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
			return;
		}
		// Were we already full?
		if (head == tail) {
			// Make room
			inc(head);
		}
		// Write the value in the next spot.
		*tail = val;
		inc(tail);
	}

	String unparse() {
		StringAccum sa;
		if (!head) {
			return sa.take_string();
		}
		int * ptr = head;
		do {
			sa << ' ' << *ptr;
			inc(ptr);
		} while (ptr != head);
		return sa.take_string();
	}

	bool contains(int val) {
		if (!head) {
			return false;
		}
		int * ptr = head;
		do {
			if (val == *ptr) {
				return true;
			}
			inc(ptr);
		} while (ptr != head);
		return false;
	}

private:

	unsigned int period;
	int * window; // Holds the sequence numbers

	// Logically, head is before tail
	int * head; // Points at the oldest element we've stored.
	int * tail; // Points at the newest element we've stored.

	// Bumps the given pointer up by one.
	// Wraps to the start of the array if needed.
	void inc(int * & p) {
		if (++p >= window + period) {
			p = window;
		}
	}

	// Returns how many sequence numbers
	ptrdiff_t size() const {
		if (head == NULL)
			return 0;
		if (head == tail)
			return period;
		return (period + tail - head) % period;
	}

};

class DupeFilterDstInfo {
public:
	EtherAddress _eth;
	int _dupes;
	int _buffer_size;
	SeqBuffer * _buffer;
	DupeFilterDstInfo() {
		_eth = EtherAddress();
		_dupes = 0;
		_buffer_size = 5;
		_buffer = new SeqBuffer(_buffer_size);
	}
	DupeFilterDstInfo(EtherAddress eth, int buffer_size) {
		_eth = eth;
		_dupes = 0;
		_buffer_size = buffer_size;
		_buffer = new SeqBuffer(_buffer_size);
	}
	DupeFilterDstInfo(EtherAddress eth, int dupes, int buffer_size, SeqBuffer * buffer) {
		_eth = eth;
		_dupes = dupes;
		_buffer_size = buffer_size;
		_buffer = buffer;
	}
};

typedef HashMap <EtherAddress, DupeFilterDstInfo> DupesTable;
typedef DupesTable::const_iterator DupesIter;

class ScyllaWifiDupeFilter : public Element {

 public:

  ScyllaWifiDupeFilter() CLICK_COLD;
  ~ScyllaWifiDupeFilter() CLICK_COLD;

  const char *class_name() const		{ return "ScyllaWifiDupeFilter"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);

  void add_handlers() CLICK_COLD;

  DupesTable* dupes_table() { return &_dupes_table; }

private:

  bool _debug;
  int _buffer_size;

  DupesTable _dupes_table;

  static int write_handler(const String &, Element *, void *, ErrorHandler *);
  static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
