// -*- mode: c++; c-basic-offset: 2 -*-
#ifndef CLICK_EMPOWERMULTICASTTABLE_HH
#define CLICK_EMPOWERMULTICASTTABLE_HH
#include <click/element.hh>
#include <click/config.h>
#include <click/etheraddress.hh>
CLICK_DECLS

/*
=c

EmpowerMulticastTable(RL, EL, [, I<KEYWORDS>])

=s EmPOWER

Handle IGMP packets

=d

Keyword arguments are:

=over 8

=item RT
An AvailableRates element

=item EL
An EmpowerLVAPManager element

=item DEBUG
Turn debug on/off

=back 8

=a EmpowerLVAPManager
*/

struct EmpowerMulticastGroup {
	IPAddress group; // group address
	EtherAddress mac_group;
	Vector<EtherAddress> receivers;
};

typedef Vector<EmpowerMulticastGroup> MulticastGroups;
typedef MulticastGroups::iterator MGIter;


class EmpowerMulticastTable: public Element {
public:

	EmpowerMulticastTable();
	~EmpowerMulticastTable();

	const char *class_name() const { return "EmpowerMulticastTable"; }
	const char *port_count() const { return PORTS_0_0; }

	int configure(Vector<String> &, ErrorHandler *);
	void add_handlers();

	MulticastGroups multicastgroups;

	EtherAddress ip_mcast_addr_to_mac(IPAddress ip) {

		unsigned long ip_addr = ntohl(ip.addr());
		unsigned char mac_addr[6] = {0x01, 0x0, 0x5e, 0x0, 0x0, 0x0};

		mac_addr[5] = ip_addr & 0xff;
		mac_addr[4] = (ip_addr & 0xff00)>>8;
		mac_addr[3] = (ip_addr & 0x7f0000)>>16;

		EtherAddress mac = EtherAddress (mac_addr);

		return mac;

	}

	bool add_group(IPAddress);
	bool join_group(EtherAddress, IPAddress);
	bool leave_group(EtherAddress, IPAddress);
	bool leave_all_groups(EtherAddress);
	Vector<EtherAddress> *get_receivers(EtherAddress);

private:

	bool _debug;

	// Read/Write handlers
	static String read_handler(Element *e, void *user_data);
	static int write_handler(const String &, Element *, void *, ErrorHandler *);

};

CLICK_ENDDECLS
#endif
