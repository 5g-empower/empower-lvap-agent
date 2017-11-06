#ifndef CLICK_SCYLLAS1MONITOR_HH
#define CLICK_SCYLLAS1MONITOR_HH
#include <click/element.hh>
#include <click/string.hh>
#include <click/hashmap.hh>
CLICK_DECLS

struct click_sctp {
private:
    uint16_t _src_port;
    uint16_t _dst_port;
    uint32_t _tag;
    uint32_t _checksum;
public:
    uint16_t src_port()                 { return ntohs(_src_port); }
    uint16_t dst_port()                 { return ntohs(_dst_port); }
    uint32_t tag()                      { return ntohl(_tag); }
    uint32_t checksum()                 { return ntohl(_checksum); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

struct click_sctp_chunk {
private:
    uint8_t _type;
    uint8_t _flags;
    uint16_t _length;
public:
    uint8_t type()                 { return _type; }
    uint8_t flags()                { return _flags; }
    uint16_t length()              { return ntohs(_length); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

struct click_sctp_data_chunk {
private:
    uint8_t _type;
    uint8_t _flags;
    uint16_t _length;
    uint32_t _tsn;
    uint16_t _sid;
    uint16_t _ssn;
    uint32_t _ppi;
public:
    uint8_t type()                 { return _type; }
    uint8_t flags()                { return _flags; }
    uint16_t length()              { return ntohs(_length); }
    uint32_t ppi()                 { return ntohl(_ppi); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

struct S1APMonitorElement {
    /* eNB UE S1AP Id. */
    uint32_t eNB_UE_S1AP_ID;
    /* MME UE S1AP Id. */
    uint32_t MME_UE_S1AP_ID;
    /* eRAB Id. */
    uint8_t e_RAB_ID;
    /* EPC IP address. */
    String EPC_IP;
    /* eNB IP address. */
    String eNB_IP;
    /* UE IP address. */
    String UE_IP;
    /* Tunnel End Point Id used for GTP traffic from UE to EPC. */
    String UE2EPC_teid;
    /* Tunnel End Point Id used for GTP traffic from EPC to UE. */
    String EPC2UE_teid;
};

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
  void parse_s1ap(click_sctp_data_chunk *);

  void add_handlers();

private:

  bool _debug;
  unsigned _offset;

  static int write_handler(const String &, Element *, void *, ErrorHandler *);
  static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
