// -*- mode: c++; c-basic-offset: 2 -*-
#ifndef CLICK_EMPOWERRXSTATS_HH
#define CLICK_EMPOWERRXSTATS_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/hashtable.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/straccum.hh>
#include <clicknet/wifi.h>
#include <click/sync.hh>
#include "summary_trigger.hh"
#include "rssi_trigger.hh"
#include "dstinfo.hh"
#include "empowerpacket.hh"
CLICK_DECLS

/*
 =c

 EmpowerRXStats(EL[, I<KEYWORDS>])

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

typedef HashTable<EtherAddress, DstInfo> NeighborTable;
typedef NeighborTable::iterator NTIter;

typedef Vector<RssiTrigger *> RssiTriggersList;
typedef RssiTriggersList::iterator RTIter;

typedef Vector<SummaryTrigger *> SummaryTriggersList;
typedef SummaryTriggersList::iterator DTIter;


class EmpowerLVAPManager;

class EmpowerRXStats: public Element {
public:

	EmpowerRXStats();
	~EmpowerRXStats();

	const char *class_name() const { return "EmpowerRXStats"; }
	const char *port_count() const { return PORTS_1_1; }
	const char *processing() const { return AGNOSTIC; }

	int initialize(ErrorHandler *);
	int configure(Vector<String> &, ErrorHandler *);
	void run_timer(Timer *);

	Packet *simple_action(Packet *);

	void add_handlers();

	void add_rssi_trigger(EtherAddress, uint32_t, empower_trigger_relation, int, uint16_t);
	void del_rssi_trigger(uint32_t);

	void add_summary_trigger(int, EtherAddress, uint32_t, int16_t, uint16_t);
	void del_summary_trigger(uint32_t);

	void clear_triggers();

	ReadWriteLock lock;

	NeighborTable aps;
	NeighborTable stas;

private:

	EmpowerLVAPManager *_el;
	Timer _timer;

	RssiTriggersList _rssi_triggers;
	SummaryTriggersList _summary_triggers;

	int _signal_offset;
	unsigned _period; // in ms
	unsigned _sma_period;
	unsigned _max_silent_window_count; // in number of windows

	bool _debug;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

	void update_neighbor(EtherAddress, bool, uint8_t, uint8_t);

};

CLICK_ENDDECLS
#endif
