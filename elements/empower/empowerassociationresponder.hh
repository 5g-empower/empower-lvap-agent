// -*- mode: c++; c-basic-offset: 2 -*-
#ifndef CLICK_EMPOWERASSOCIATIONRESPONDER_HH
#define CLICK_EMPOWERASSOCIATIONRESPONDER_HH
#include <click/element.hh>
#include <click/config.h>
CLICK_DECLS

/*
=c

EmpowerAssociationResponder(RL, EL, [, I<KEYWORDS>])

=s EmPOWER

Respond to 802.11 association requests.

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

class EmpowerAssociationResponder: public Element {
public:

	EmpowerAssociationResponder();
	~EmpowerAssociationResponder();

	const char *class_name() const { return "EmpowerAssociationResponder"; }
	const char *port_count() const { return "1/1"; }
	const char *processing() const { return PUSH; }

	int configure(Vector<String> &, ErrorHandler *);
	void add_handlers();
	void send_association_response(EtherAddress);
	void push(int, Packet *);

private:

	class EmpowerLVAPManager *_el;

	bool _debug;

	// Read/Write handlers
	static String read_handler(Element *e, void *user_data);
	static int write_handler(const String &, Element *, void *, ErrorHandler *);

};

CLICK_ENDDECLS
#endif
