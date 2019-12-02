// -*- mode: c++; c-basic-offset: 2 -*-
#ifndef CLICK_EMPOWEROPENAUTHRESPONDER_HH
#define CLICK_EMPOWEROPENAUTHRESPONDER_HH
#include <click/element.hh>
#include <click/config.h>
CLICK_DECLS

/*
=c

EmpowerOpenAuthResponder(RT, EL[, I<KEYWORDS>])

=s EmPOWER

Respond to 802.11 open authentication requests.

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

class EmpowerOpenAuthResponder: public Element {
public:

	EmpowerOpenAuthResponder();
	~EmpowerOpenAuthResponder();

	const char *class_name() const { return "EmpowerOpenAuthResponder"; }
	const char *port_count() const { return "1/1"; }
	const char *processing() const { return PUSH; }

	int configure(Vector<String> &, ErrorHandler *);
	void add_handlers();
	void send_auth_response(EtherAddress);
	void push(int, Packet *);

private:

	class EmpowerLVAPManager *_el;

	bool _debug;

	static String read_handler(Element *e, void *user_data);
	static int write_handler(const String &, Element *, void *, ErrorHandler *);

};

CLICK_ENDDECLS
#endif
