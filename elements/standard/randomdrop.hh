#ifndef CLICK_RANDOMDROP_HH
#define CLICK_RANDOMDROP_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

RandomDrop(PROB)

=s classification

drops packets with probability PROB

=d

RandomDrop drops packets with probability PROB.
*/

class RandomDrop : public Element { public:

  RandomDrop() CLICK_COLD;

  const char *class_name() const		{ return "RandomDrop"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return PUSH; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  void push(int, Packet *);

 protected:

  unsigned _prob;

};

CLICK_ENDDECLS
#endif
