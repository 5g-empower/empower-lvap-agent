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
	uint16_t seq;
	uint16_t frag;
	ScyllaDupeFilterDstInfo() :
			_dupes(0), _packets(0), seq(0), frag(0) {
	}
	ScyllaDupeFilterDstInfo(EtherAddress eth) :
			_dupes(0), _packets(0), seq(0), frag(0) {
		_eth = eth;
	}
	void clear() {
		_dupes = 0;
		_packets = 0;
		seq = 0;
		frag = 0;

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

private:

  bool _debug;

  DupeTable _table;

  static int write_handler(const String &, Element *, void *, ErrorHandler *);
  static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
