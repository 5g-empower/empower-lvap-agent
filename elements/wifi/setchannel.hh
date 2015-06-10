#ifndef CLICK_SETCHANNEL_HH
#define CLICK_SETCHANNEL_HH
#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
=c

SetChannel([I<KEYWORDS>])

=s Wifi

Sets the channel annotation for a packet.

=d

Sets the Wifi Channel Annotation on a packet. Notice that this
does not affect the channel on which the packet will be sent,
it is there to driver the wireless driver in selecting the right
band.

Regular Arguments:

=over 8
=item CHANNEL

The channel frequency in MHz
=back 8

=h channel
Same as CHANNEL argument.

=a  SetTXRate
*/

class SetChannel : public Element { public:

  SetChannel() CLICK_COLD;
  ~SetChannel() CLICK_COLD;

  const char *class_name() const		{ return "SetChannel"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const		{ return true; }

  Packet *simple_action(Packet *);

  void add_handlers() CLICK_COLD;

private:

  unsigned _channel;

  // Read/Write handlers
  static String read_handler(Element *e, void *user_data);
  static int write_handler(const String &, Element *, void *, ErrorHandler *);

};

CLICK_ENDDECLS
#endif
