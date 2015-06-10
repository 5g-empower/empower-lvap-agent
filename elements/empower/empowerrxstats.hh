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
#include "summary_trigger.hh"
#include "rssi_trigger.hh"
#include "dstinfo.hh"
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

	void add_rssi_trigger(EtherAddress, uint32_t, relation_t, int);
	void del_rssi_trigger(EtherAddress, uint32_t, relation_t, int);

	void add_summary_trigger(EtherAddress, uint32_t, int16_t, uint16_t);
	void del_summary_trigger(EtherAddress, uint32_t, int16_t, uint16_t);

	void clear_triggers();

	NeighborTable * aps() { return &_aps; }
	NeighborTable * stas() { return &_stas; }

	void clear_stale_neighbors(Timer *timer);

private:

	EmpowerLVAPManager *_el;
	Timer _timer;

	RssiTriggersList _rssi_triggers;
	SummaryTriggersList _summary_triggers;
	NeighborTable _aps;
	NeighborTable _stas;

	int _signal_offset;
	int _aging;
	int _aging_th;
	unsigned _period;
	unsigned _ewma_level;
	unsigned _sma_period;
	bool _debug;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
