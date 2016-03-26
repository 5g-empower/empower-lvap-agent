// -*- mode: c++; c-basic-offset: 2 -*-
#ifndef CLICK_EMPOWERWIFIENCAP_HH
#define CLICK_EMPOWERWIFIENCAP_HH
#include <click/config.h>
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
CLICK_DECLS

/*
=c

EmpowerWifiEncap(EL)

=s EmPOWER

Converts Ethernet packets to 802.11 packets with a LLC header. Setting
the appropriate BSSID given the destination address. An EmPOWER Access
Point generates one virtual BSSID called LVAP for each active station.

=d

Strips the Ethernet header off the front of the packet and pushes
an 802.11 frame header and LLC header onto the packet.

Arguments are:

=item EL
An EmpowerLVAPManager element

=item DEBUG
Turn debug on/off

=back 8

=a EmpowerWifiDecap
*/

class EmpowerWifiEncap: public Element {
public:

	EmpowerWifiEncap();
	~EmpowerWifiEncap();

	const char *class_name() const { return "EmpowerWifiEncap"; }
	const char *port_count() const { return PORTS_1_1; }
	const char *processing() const { return PUSH; }

	int configure(Vector<String> &, ErrorHandler *);
	void push(int, Packet *);
	void add_handlers();

private:

	class EmpowerLVAPManager *_el;

	bool _debug;

	Packet *wifi_encap(Packet *, EtherAddress, EtherAddress, EtherAddress);

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
