/*
 * =====================================================================================
 *
 *       Filename:  gtpencap.hh
 *
 *    Description:  Element encapsulates the GTP header.
 *
 *        Created:  11/16/2017 05:19:08 PM
 *       Compiler:  gcc
 *
 *         Author:  Tejas Subramanya
 *        Company:  FBK CREATE-NET
 *
 * =====================================================================================
 */

#ifndef CLICK_GTPENCAP_HH
#define CLICK_GTPENCAP_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include "gtp.h"
CLICK_DECLS

class GTPEncap : public Element { public:
  
  GTPEncap();
  ~GTPEncap();
  
  const char *class_name() const    { return "GTPEncap"; }
  const char *port_count() const    { return PORTS_1_1; }
  const char *processing() const    { return AGNOSTIC; }
  const char *flags() const      { return "0"; }
  
  int configure(Vector<String> &, ErrorHandler *);
  
  private:
  
  click_gtp _gtph; // GTP header to append to each packet
  Packet *simple_action(Packet *);
                 
};

CLICK_ENDDECLS
#endif
