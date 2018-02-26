#ifndef CLICK_EMPOWERTEE_HH
#define CLICK_EMPOWERTEE_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * EmpowerTee([N])
 * =s basictransfer
 * duplicates packets
 * =d
 * EmpowerTee sends a copy of each incoming packet out each output.N.
 */

class EmpowerTee : public Element {

public:

	EmpowerTee() CLICK_COLD;

	const char *class_name() const		{ return "EmpowerTee"; }
	const char *port_count() const		{ return "1/1-"; }
	const char *processing() const		{ return PUSH; }

	int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

	void push(int, Packet *);

private:

	class EmpowerLVAPManager *_el;

};

CLICK_ENDDECLS
#endif
