#ifndef CLICK_SCYLLAWIFIDUPEFILTER_HH
#define CLICK_SCYLLAWIFIDUPEFILTER_HH
#include <click/element.hh>
#include <click/string.hh>
#include <click/hashmap.hh>
CLICK_DECLS

class ScyllaDupeFilterDstInfo {
public:
	EtherAddress _eth;
	int _dupes;
	int _packets;
	uint16_t _seq;
	uint16_t _frag;
	ScyllaDupeFilterDstInfo() :
			_dupes(0), _packets(0), _seq(0), _frag(0) {
	}
	ScyllaDupeFilterDstInfo(EtherAddress eth) :
			_dupes(0), _packets(0), _seq(0), _frag(0) {
		_eth = eth;
	}
	ScyllaDupeFilterDstInfo(EtherAddress eth, int dupes, int packets,
			uint16_t seq, uint16_t frag) {
		_eth = eth;
		_dupes = dupes;
		_packets = packets;
		_seq = seq;
		_frag = frag;
	}
	void clear() {
	}
};

typedef HashMap <EtherAddress, ScyllaDupeFilterDstInfo> DupeTable;
typedef DupeTable::const_iterator DupeIter;

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

  DupeTable* table() { return &_table; }

private:

  bool _debug;

  DupeTable _table;

  static int write_handler(const String &, Element *, void *, ErrorHandler *);
  static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
