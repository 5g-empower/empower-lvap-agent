// -*- mode: c++; c-basic-offset: 2 -*-
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
typedef CqmLinkTable::iterator CIter;

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

	//void set_rssi_threshold(double threshold) { _rssi_threshold = threshold; };
	//double get_rssi_threshold(void) { return _rssi_threshold; };

	//void set_max_silent_window_count(uint64_t count) { _max_silent_window_count = count; };
	//uint64_t get_max_silent_window_count(void) { return _max_silent_window_count; };

	ReadWriteLock lock;
	CqmLinkTable links;

private:

	EmpowerLVAPManager *_el;
	Timer _timer;

	int _signal_offset;
	unsigned _period; // in ms
	unsigned _max_silent_window_count; // in number of windows
    double _rssi_threshold; // threshold for rssi cdf

	bool _debug;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
