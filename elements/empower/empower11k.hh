// -*- mode: c++; c-basic-offset: 2 -*-
#ifndef CLICK_EMPOWER11K_HH
#define CLICK_EMPOWER11k_HH
#include <click/element.hh>
#include <click/config.h>
CLICK_DECLS

/*
=c

Empower11k(EL[, I<KEYWORDS>])

=s EmPOWER

Send and parse 802.11k messages.

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

class Empower11k: public Element {
public:

	Empower11k();
	~Empower11k();

	const char *class_name() const { return "Empower11k"; }
	const char *port_count() const { return "1/1"; }
	const char *processing() const { return PUSH; }

	int configure(Vector<String> &, ErrorHandler *);
	void add_handlers();
	void send_neighbor_report_request(EtherAddress, uint8_t);
	void send_link_measurement_request(EtherAddress, uint8_t);
	void push(int, Packet *);

private:

	class EmpowerLVAPManager *_el;

	bool _debug;

	static String read_handler(Element *e, void *user_data);
	static int write_handler(const String &, Element *, void *, ErrorHandler *);

};

CLICK_ENDDECLS
#endif
