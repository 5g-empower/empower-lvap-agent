#ifndef CLICK_EMPOWERCQM_HH
#define CLICK_EMPOWERCQM_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/hashtable.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/straccum.hh>
#include <clicknet/wifi.h>
#include <click/sync.hh>
#include "cqmlink.hh"
#include "busynessinfo.hh"
#include "empowerpacket.hh"
CLICK_DECLS

/*
 =c

 EmpowerCQM(EL[, I<KEYWORDS>])

 =s EmPOWER

 Tracks RX statistics for neighbouring STAs

 =d

 Keyword arguments are:

 =over 8

 =item EL
 An EmpowerLVAPManager element

 =item DEBUG
 Turn debug on/off

 =back 8

 =a EmpowerLVAPManager
 */

typedef HashTable<EtherAddress, CqmLink> CqmLinkTable;
typedef CqmLinkTable::iterator CLTIter;

class EmpowerLVAPManager;

class EmpowerCQM: public Element {
public:

	EmpowerCQM();
	~EmpowerCQM();

	const char *class_name() const { return "EmpowerCQM"; }
	const char *port_count() const { return PORTS_1_1; }
	const char *processing() const { return AGNOSTIC; }

	int initialize(ErrorHandler *);
	int configure(Vector<String> &, ErrorHandler *);
	void run_timer(Timer *);

	Packet *simple_action(Packet *);

	void add_handlers();

	ReadWriteLock lock;
	CqmLinkTable links;

private:

	EmpowerLVAPManager *_el;
	Timer _timer;

	unsigned _period; // in ms
	unsigned _samples; // in #
	unsigned _max_silent_window_count; // in number of windows
    double _rssi_threshold; // threshold for rssi cdf

	bool _debug;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

	void update_link_table(EtherAddress, uint8_t, uint16_t, uint32_t, uint8_t);
	void update_channel_busy_time(uint8_t, EtherAddress, uint32_t, uint8_t);

};

CLICK_ENDDECLS
#endif
