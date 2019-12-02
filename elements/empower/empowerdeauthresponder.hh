// -*- mode: c++; c-basic-offset: 2 -*-
#ifndef CLICK_EMPOWERDEAUTHRESPONDER_HH
#define CLICK_EMPOWERDEAUTHRESPONDER_HH
#include <click/element.hh>
#include <click/config.h>
CLICK_DECLS

/*
=c

EmpowerDeAuthResponder(EL[, I<KEYWORDS>])

=s EmPOWER

Respond to 802.11 de authentication requests.

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

class EmpowerDeAuthResponder: public Element {
public:

	EmpowerDeAuthResponder();
	~EmpowerDeAuthResponder();

	const char *class_name() const { return "EmpowerDeAuthResponder"; }
	const char *port_count() const { return "1/1"; }
	const char *processing() const { return PUSH; }

	int configure(Vector<String> &, ErrorHandler *);
	void add_handlers();
	void push(int, Packet *);
	void send_deauth_request (EtherAddress, uint16_t, int);

private:

	class EmpowerLVAPManager *_el;

	bool _debug;

	static String read_handler(Element *e, void *user_data);
	static int write_handler(const String &, Element *, void *, ErrorHandler *);

};

CLICK_ENDDECLS
#endif
