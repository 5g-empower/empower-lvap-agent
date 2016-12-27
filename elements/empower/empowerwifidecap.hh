// -*- mode: c++; c-basic-offset: 2 -*-
#ifndef CLICK_EMPOWERWIFIDECAP_HH
#define CLICK_EMPOWERWIFIDECAP_HH
#include <click/config.h>
#include <click/element.hh>
CLICK_DECLS

/*
=c

EmpowerWifiDecap(EL)

=s EmPOWER

Turns 802.11 packets into Ethernet packets. Packets coming from stations
that do not an LVAP are silently dropped.

=d

Strips the 802.11 frame header and LLC header off the packet and pushes
an Ethernet header onto the packet. Discards packets that are shorter
than the length of the 802.11 frame header and LLC header and packets
that do not have a LVAP or packets coming from station that have not
completed the authentication and the association procedure.

=over 8

=item EL
An EmpowerLVAPManager element

=item DEBUG
Turn debug on/off

=back

=a EmpowerWifiEncap
*/

class EmpowerWifiDecap: public Element {
public:

	EmpowerWifiDecap();
	~EmpowerWifiDecap();

	const char *class_name() const { return "EmpowerWifiDecap"; }
	const char *port_count() const { return "1/2"; }
	const char *processing() const { return AGNOSTIC; }

	int configure(Vector<String> &, ErrorHandler *);

	void push(int, Packet *p);

	void add_handlers();

private:

	class EmpowerLVAPManager *_el;

	bool _debug;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
