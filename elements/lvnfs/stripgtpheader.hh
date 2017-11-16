/*
 * =====================================================================================
 *
 *       Filename:  stripgtpheader.hh
 *
 *    Description:  Element strips GTP header.
 *
 *        Created:  11/16/2017 04:47:32 PM
 *       Compiler:  gcc
 *
 *         Author:  Tejas Subramanya
 *        Company:  FBK CREATE-NET
 *
 * =====================================================================================
 */

#ifndef CLICK_STRIPGTPHEADER_HH
#define CLICK_STRIPGTPHEADER_HH
#include <click/element.hh>
CLICK_DECLS

class StripGTPHeader : public Element {

  public:

  StripGTPHeader();
  ~StripGTPHeader();
  
  const char *class_name() const           { return "StripGTPHeader"; }
  const char *port_count() const           { return PORTS_1_1; }

  Packet *simple_action(Packet *);  
};

CLICK_ENDDECLS
#endif

