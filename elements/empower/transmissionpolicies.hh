#ifndef CLICK_TRANSMISSIONPOLICIES_HH
#define CLICK_TRANSMISSIONPOLICIES_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
#include "transmissionpolicy.hh"
CLICK_DECLS

/*
=c

TransmissionPolicies()

=s Wifi, Wireless Station, Wireless AccessPoint

Tracks bit-rate capabilities of other stations.

=d

Tracks a list of bitrates other stations are capable of.

=h insert write-only
Inserts an ethernet address and a list of bitrates to the database.

=h remove write-only
Removes an ethernet address from the database.

=h rates read-only
Shows the entries in the database.

=a BeaconScanner
 */

typedef HashMap<EtherAddress, TxPolicyInfo *> TxTable;
typedef TxTable::const_iterator TxTableIter;

class TransmissionPolicies : public Element { public:

  TransmissionPolicies() CLICK_COLD;
  ~TransmissionPolicies() CLICK_COLD;

  const char *class_name() const		{ return "TransmissionPolicies"; }
  const char *port_count() const		{ return PORTS_0_0; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  void add_handlers() CLICK_COLD;

  TxTable * tx_table() { return &_tx_table; }
  TxPolicyInfo * default_tx_policy() { return _default_tx_policy; }
  void clear();

  TxPolicyInfo * lookup(EtherAddress eth);
  TxPolicyInfo * supported(EtherAddress eth);

  int insert(EtherAddress, Vector<int>, Vector<int>, bool, empower_tx_mcast_type, int, int, int);
  int remove(EtherAddress);

private:

  TxTable _tx_table;
  TxPolicyInfo * _default_tx_policy;

  static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
