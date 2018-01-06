/*
 * =====================================================================================
 *
 *       Filename:  gtpencap.cc
 *
 *    Description:  Element encaps GTP header to IP packet.
 *
 *        Created:  11/17/2017 11:33:41 AM
 *       Compiler:  gcc
 *
 *         Author:  Tejas SUbramanya
 *        Company:  FBK CREATE-NET
 *
 * =====================================================================================
 */

#include <click/config.h>
#include "gtpencap.hh"
#include "gtp.h"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/standard/alignmentinfo.hh>
#include <click/string.hh>
#include <click/ipaddress.hh>
#include <arpa/inet.h>
#include "scyllas1monitor.hh"
CLICK_DECLS

GTPEncap::GTPEncap()
{
}

GTPEncap::~GTPEncap()
{
}

int
GTPEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
  memset(&_gtph, 0, sizeof(_gtph));

  _gtph.message_type = GPDU_TYPE;
  _gtph.flags |= GTP_VERSION;
  _gtph.flags |= GTP_PROTOCOL_TYPE;  

  return 0;
}

Packet *
GTPEncap::simple_action(Packet *p_in)
{
  click_chatter("Number of UEs monitored: %d",S1APMonElelist.size());
  struct click_ip *ip = (struct click_ip *) (p_in->data());
  for (int i = 0; i != S1APMonElelist.size() ; i++) {
    unsigned int temp;
    if (!strcmp(inet_ntoa(ip->ip_src),S1APMonElelist[i].UE_IP.c_str())) {
      sscanf(S1APMonElelist[i].UE2EPC_teid.c_str(),"%X",&temp);
      _gtph.teid = ntohl(temp);
    }
    else if (!strcmp(inet_ntoa(ip->ip_dst),S1APMonElelist[i].UE_IP.c_str())) { 
      sscanf(S1APMonElelist[i].EPC2UE_teid.c_str(),"%X",&temp); 
      _gtph.teid = ntohl(temp);
    }
  }

  WritablePacket *p = p_in->push(GTP_HEADER_LEN);
  if (!p) 
    return 0;

  _gtph.message_length = htons(p->length() - GTP_HEADER_LEN); 
  click_gtp *gtph = reinterpret_cast<click_gtp *>(p->data());  
  memcpy(gtph, &_gtph,GTP_HEADER_LEN);
  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(GTPEncap)
ELEMENT_MT_SAFE(GTPEncap)

