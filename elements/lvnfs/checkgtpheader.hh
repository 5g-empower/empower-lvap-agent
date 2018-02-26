/*
 * =====================================================================================
 *
 *       Filename:  checkgtpheader.hh
 *
 *    Description:  Element checks GTPv1 header format for correctness.
 *                  
 *
 *        Created:  11/16/2017 10:33:22 AM
 *       Compiler:  gcc
 *
 *         Author:  Tejas Subramanya
 *        Company:  FBK CREATE-NET
 *
 * =====================================================================================
 */

#ifndef CLICK_CHECKGTPHEADER_HH
#define CLICK_CHECKGTPHEADER_HH
#include <click/element.hh>
#include <click/atomic.hh>
CLICK_DECLS

class CheckGTPHeader : public Element { public:

  CheckGTPHeader();
  ~CheckGTPHeader();

  const char *class_name() const              { return "CheckGTPHeader"; }
  const char *port_count() const              { return "1/1-2"; }
  const char *processing() const              { return "PROCESSING_A_AH"; }
  const char *flags() const                   { return "0"; }

  int configure(Vector<String> &, ErrorHandler *);
  void add_handlers();

  Packet *simple_action(Packet *);

private:

  bool _verbose;
  
  uatomic32_t _drops;                                                                                                                                                                                            
  uatomic32_t *_reason_drops;
  
  enum Reason {
    MINISCULE_PACKET,
    BAD_VERSION,
    NO_SUPPORT_PROTOCOLTYPE,
    NO_SUPPORT_MESSAGETYPE,
    BAD_HLEN,
    NREASONS
  };

  static const char * const reason_texts[NREASONS];
  Packet *drop(Reason, Packet *); 
  static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif  

