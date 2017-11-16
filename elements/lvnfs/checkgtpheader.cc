/*
 * =====================================================================================
 *
 *       Filename:  checkgtpheader.cc
 *
 *    Description: Element checks GTPv1 header format for correctness. 
 *
 *        Created:  11/16/2017 10:55:59 AM
 *       Compiler:  gcc
 *
 *         Author:  Tejas Subramanya
 *        Company:  FBK CREATE-NET
 *
 * =====================================================================================
 */


#include <click/config.h>
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/standard/alignmentinfo.hh>
#include "checkgtpheader.hh"
#include "gtp.h"
CLICK_DECLS

const char * const CheckGTPHeader::reason_texts[NREASONS] = {
  "tiny packet", "bad GTP version", "protocol type not supported", 
  "message type not supported", "bad GTP header length"
};

CheckGTPHeader::CheckGTPHeader()
  :  _reason_drops(0)
{
  _drops = 0;
}

CheckGTPHeader::~CheckGTPHeader()
{
  delete[] _reason_drops;
}

int CheckGTPHeader::configure(Vector<String> &conf, ErrorHandler *errh)
{
  bool details = false;
  
  if (cp_va_kparse_remove_keywords(conf, this, errh,
                "VERBOSE", 0, cpBool, &_verbose,
                "DETAILS", 0, cpBool, &details,
                cpEnd) < 0)
    return -1;

  if (details)
    _reason_drops = new uatomic32_t[NREASONS];

  return 0;
}


Packet * 
CheckGTPHeader::drop(Reason reason, Packet *p)
{
  if (_drops == 0 || _verbose)
    click_chatter("GTP header check failed: %s", reason_texts[reason]);
  _drops++;

  if (_reason_drops)
    _reason_drops[reason]++;
  
  if (noutputs() == 2)
    output(1).push(p);
  else
    p->kill();

  return 0;
}

Packet *
CheckGTPHeader::simple_action(Packet *p)
{
  const click_gtp *gtph = reinterpret_cast<const click_gtp *>(p->data());
  uint8_t flags;
  unsigned plen = p->length();
  unsigned hlen = 8;

  if ((int)plen < (int)hlen)
    return drop(MINISCULE_PACKET, p);

  memcpy(&flags, &gtph->flags, sizeof(gtph->flags));

  if ((flags & GTP_VERSION) != GTP_VERSION)
    return drop(BAD_VERSION, p);

  if ((flags & GTP_PROTOCOL_TYPE) != GTP_PROTOCOL_TYPE) /* GTPv1 support only */
    return drop(NO_SUPPORT_PROTOCOLTYPE, p);

  if (gtph->message_type != GPDU_TYPE) /* GPDU support only */
    return drop(NO_SUPPORT_MESSAGETYPE, p);

  return(p); 
}

String
CheckGTPHeader::read_handler(Element *e, void *thunk)
{
  CheckGTPHeader *c = reinterpret_cast<CheckGTPHeader *>(e);
  switch ((intptr_t)thunk) {

   case 0:      // drops
    return String(c->_drops) + "\n";

   case 1: {      // drop_details
     StringAccum sa;
     for (int i = 0; i < NREASONS; i++)
       sa << c->_reason_drops[i] << '\t' << reason_texts[i] << '\n';
     return sa.take_string();
   }

   default:
    return String("<error>\n");

  }
}

void 
CheckGTPHeader::add_handlers()
{
  add_read_handler("drops", read_handler, (void *)0);
  if (_reason_drops)
    add_read_handler("drop_details", read_handler, (void *)1);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(CheckGTPHeader)







