// -*- mode: c++; c-basic-offset: 2 -*-
#ifndef CLICK_EMPOWERPOWERSAVEFILTER_HH
#define CLICK_EMPOWERPOWERSAVEFILTER_HH
#include <click/config.h>
#include <click/element.hh>
CLICK_DECLS

/*
=c

EmpowerPowerSaveFilter(EL, EPSB)

=s EmPOWER

Track power save mode changes in associated Stations

=d

Tracks power save mode changes in associated Stations. When a station
goes to sleep the element set the power save bit in the EmpowerStationStatus
structure. From this moment on, outgoing frame addressed to that station
are buffered. When the station wakes up the buffered frame are transmitted.

=over 8

=item EL
An EmpowerLVAPManager element

=item EPSB
An EmpowerPowerSaveBuffer element

=item DEBUG
Turn debug on/off

=back

=a EmpowerWifiEncap
*/


class EmpowerPowerSaveFilter: public Element {
public:

	EmpowerPowerSaveFilter();
	~EmpowerPowerSaveFilter();

	const char *class_name() const { return "EmpowerPowerSaveFilter"; }
	const char *port_count() const { return PORTS_1_1; }
	const char *processing() const { return AGNOSTIC; }

	int configure(Vector<String> &, ErrorHandler *);

	Packet *simple_action(Packet *);

	void add_handlers();

private:

	class EmpowerLVAPManager *_el;
	class EmpowerPowerSaveBuffer *_epsb;

	bool _debug;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
