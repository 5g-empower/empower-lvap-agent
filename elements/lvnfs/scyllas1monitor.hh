#ifndef CLICK_SCYLLAS1MONITOR_HH
#define CLICK_SCYLLAS1MONITOR_HH
#include <click/element.hh>
#include <click/string.hh>
#include <click/hashmap.hh>
CLICK_DECLS

class ScyllaS1Monitor : public Element {

 public:

  ScyllaS1Monitor();
  ~ScyllaS1Monitor();

  const char *class_name() const		{ return "ScyllaS1Monitor"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);

  void add_handlers();

private:

  bool _debug;
  unsigned _offset;

  static int write_handler(const String &, Element *, void *, ErrorHandler *);
  static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
