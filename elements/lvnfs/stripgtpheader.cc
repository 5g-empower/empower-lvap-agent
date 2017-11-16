/*
 * =====================================================================================
 *
 *       Filename:  stripgtpheader.cc
 *
 *    Description:  Element strips GTP header.
 *
 *        Created:  11/16/2017 04:49:15 PM
 *       Compiler:  gcc
 *
 *         Author:  Tejas Subramanya
 *        Company:  FBK CREATE-NET
 *
 * =====================================================================================
 */

#include <click/config.h>
#include "stripgtpheader.hh"
#include "gtp.h"
CLICK_DECLS

StripGTPHeader::StripGTPHeader()
{
}

StripGTPHeader::~StripGTPHeader()
{
}

Packet *
StripGTPHeader::simple_action(Packet *p)
{
  p->pull(GTP_HEADER_LEN);
  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(StripGTPHeader)
ELEMENT_MT_SAFE(StripGTPHeader)

