// -*- mode: c++; c-basic-offset: 2 -*-
#ifndef CLICK_EMPOWERPACKET_HH
#define CLICK_EMPOWERPACKET_HH
#include <click/config.h>
#include <clicknet/wifi.h>
CLICK_DECLS

/* protocol version */
static const uint8_t _empower_version = 0x00;

/* protocol type */
enum empower_packet_types {

	// Internal messages
	EMPOWER_PT_WTP_BYE = 0x00,          // ac -> ac
	EMPOWER_PT_LVAP_JOIN = 0x01,        // ac -> ac
	EMPOWER_PT_LVAP_LEAVE = 0x02,       // ac -> ac
	EMPOWER_PT_WTP_REGISTER = 0xFF,     // ac -> ac

	// LVAP messages
	EMPOWER_PT_HELLO = 0x03,            // wtp -> ac
	EMPOWER_PT_PROBE_REQUEST = 0x04,    // wtp -> ac
	EMPOWER_PT_PROBE_RESPONSE = 0x05,   // ac -> wtp
	EMPOWER_PT_AUTH_REQUEST = 0x06,     // wtp -> ac
	EMPOWER_PT_AUTH_RESPONSE = 0x07,    // ac -> wtp
	EMPOWER_PT_ASSOC_REQUEST = 0x08,    // wtp -> ac
	EMPOWER_PT_ASSOC_RESPONSE = 0x09,   // ac -> wtp
	EMPOWER_PT_ADD_LVAP = 0x10,         // ac -> wtp
	EMPOWER_PT_DEL_LVAP = 0x11,         // ac -> wtp
	EMPOWER_PT_STATUS_LVAP = 0x12,      // wtp -> ac
	EMPOWER_PT_SET_PORT= 0x13,          // ac -> wtp
	EMPOWER_PT_STATUS_PORT = 0x14,      // wtp -> ac
	EMPOWER_PT_CAPS_REQUEST = 0x15,     // ac -> wtp
	EMPOWER_PT_CAPS_RESPONSE = 0x16,    // wtp -> ac

	// Packet/Bytes counters
	EMPOWER_PT_COUNTERS_REQUEST = 0x17, // ac -> wtp
	EMPOWER_PT_COUNTERS_RESPONSE = 0x18,// wtp -> ac

	// Triggers
	EMPOWER_PT_ADD_RSSI_TRIGGER = 0x19, // ac -> wtp
	EMPOWER_PT_RSSI_TRIGGER = 0x20,     // ac -> wtp
	EMPOWER_PT_DEL_RSSI_TRIGGER = 0x21, // ac -> wtp

	EMPOWER_PT_ADD_SUMMARY_TRIGGER = 0x22, // ac -> wtp
	EMPOWER_PT_SUMMARY_TRIGGER = 0x23,     // ac -> wtp
	EMPOWER_PT_DEL_SUMMARY_TRIGGER = 0x24, // ac -> wtp

	// Channel Quality Maps
	EMPOWER_PT_UCQM_REQUEST = 0x25,     // ac -> wtp
	EMPOWER_PT_UCQM_RESPONSE = 0x26,    // wtp -> ac

	EMPOWER_PT_NCQM_REQUEST = 0x27,     // ac -> wtp
	EMPOWER_PT_NCQM_RESPONSE = 0x28,    // wtp -> ac

	// Link Stats
	EMPOWER_PT_LINK_STATS_REQUEST = 0x29,    // ac -> wtp
	EMPOWER_PT_LINK_STATS_RESPONSE = 0x30,   // wtp -> ac

	// VAPs
	EMPOWER_PT_ADD_VAP = 0x31,         // ac -> wtp
	EMPOWER_PT_DEL_VAP = 0x32,         // ac -> wtp
	EMPOWER_PT_STATUS_VAP = 0x33,      // wtp -> ac

};

enum empower_port_flags {
	EMPOWER_STATUS_PORT_NOACK = (1<<0),
};

enum empower_packet_flags {
	EMPOWER_STATUS_LVAP_AUTHENTICATED = (1<<0),
	EMPOWER_STATUS_LVAP_ASSOCIATED = (1<<1),
	EMPOWER_STATUS_SET_MASK = (1<<2),
};

enum empower_bands_types {
	EMPOWER_BT_L20 = 0x0,
	EMPOWER_BT_HT20 = 0x1,
	EMPOWER_BT_HT40 = 0x2,
};

/* header format, common to all messages */
struct empower_header {
  private:
	uint8_t _version; /* see protocol version */
	uint8_t _type;    /* see protocol type */
	uint16_t _length; /* including this header */
	uint32_t _seq;    /* sequence number */
  public:
	uint8_t      version()                        { return _version; }
	uint8_t      type()                           { return _type; }
	uint16_t     length()                         { return ntohs(_length); }
	uint32_t     seq()                            { return ntohl(_seq); }
	void         set_seq(uint32_t seq)            { _seq = htonl(seq); }
	void         set_version(uint8_t version)     { _version = version; }
	void         set_type(uint8_t type)           { _type = type; }
	void         set_length(uint16_t length)      { _length = htons(length); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* hello packet format */
struct empower_hello : public empower_header {
  private:
	uint32_t _period;           /* Hello period */
	uint8_t	_wtp[6];            /* EtherAddress */
	uint32_t _uplink_bytes;     /* number bytes (uplink) */
	uint32_t _downlink_bytes;   /* number bytes (downlink) */
  public:
	void set_period(uint32_t period)        				{ _period = htonl(period); }
	void set_wtp(EtherAddress wtp)	  	  					{ memcpy(_wtp, wtp.data(), 6); }
	void set_uplink_bytes(uint32_t uplink_bytes)          	{ _uplink_bytes = htonl(uplink_bytes); }
	void set_downlink_bytes(uint32_t downlink_bytes)      	{ _downlink_bytes = htonl(downlink_bytes); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* probe request packet format */
struct empower_probe_request : public empower_header {
  private:
    uint8_t	_wtp[6];
    uint8_t	_sta[6];
    uint8_t _channel;
    uint8_t _band;
    char _ssid[];
  public:
	void set_band(uint8_t band)			{ _band = band; }
	void set_channel(uint8_t channel)	{ _channel = channel; }
	void set_wtp(EtherAddress wtp)		{ memcpy(_wtp, wtp.data(), 6); }
	void set_sta(EtherAddress sta)		{ memcpy(_sta, sta.data(), 6); }
	void set_ssid(String ssid)		    { memcpy(&_ssid, ssid.data(), ssid.length()); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* probe response packet format */
struct empower_probe_response : public empower_header {
  private:
    uint8_t	_sta[6];
  public:
	EtherAddress sta() { return EtherAddress(_sta); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* auth request packet format */
struct empower_auth_request : public empower_header {
  private:
    uint8_t	_wtp[6];
    uint8_t	_sta[6];
    uint8_t	_bssid[6];
  public:
	void set_wtp(EtherAddress wtp) { memcpy(_wtp, wtp.data(), 6); }
	void set_sta(EtherAddress sta) { memcpy(_sta, sta.data(), 6); }
	void set_bssid(EtherAddress bssid) { memcpy(_bssid, bssid.data(), 6); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* auth response packet format */
struct empower_auth_response : public empower_header {
private:
  uint8_t	_sta[6];
public:
	EtherAddress sta()							  { return EtherAddress(_sta); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* association request packet format */
struct empower_assoc_request : public empower_header {
  private:
    uint8_t	_wtp[6];
    uint8_t	_sta[6];
    uint8_t	_bssid[6];
    char _ssid[];
  public:
	void         set_wtp(EtherAddress wtp)		  { memcpy(_wtp, wtp.data(), 6); }
	void         set_sta(EtherAddress sta)		  { memcpy(_sta, sta.data(), 6); }
	void         set_bssid(EtherAddress bssid)	  { memcpy(_bssid, bssid.data(), 6); }
	void         set_ssid(String ssid)		      { memcpy(&_ssid, ssid.data(), ssid.length()); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* association response packet format */
struct empower_assoc_response : public empower_header {
  private:
    uint8_t	_sta[6];
  public:
	EtherAddress sta()							  { return EtherAddress(_sta); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* caps request packet format */
struct empower_caps_request : public empower_header {
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* link stats request packet format */
struct empower_link_stats_request : public empower_header {
private:
  uint32_t _link_stats_id;
  uint8_t _sta[6];

public:
	uint32_t     link_stats_id() { return ntohl(_link_stats_id); }
	EtherAddress sta()	{ return EtherAddress(_sta); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* link stats response packet format */
struct empower_link_stats_response : public empower_header {
private:
  uint32_t _link_stats_id;
  uint8_t _wtp[6];
  uint8_t _sta[6];
  uint16_t _nb_link_stats;
public:
  void set_link_stats_id(uint32_t link_stats_id)   { _link_stats_id = htonl(link_stats_id); }
  void set_wtp(EtherAddress wtp)         { memcpy(_wtp, wtp.data(), 6); }
  void set_sta(EtherAddress sta)         { memcpy(_sta, sta.data(), 6); }
  void set_nb_link_stats(uint16_t nb_link_stats)   { _nb_link_stats = htons(nb_link_stats); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* link_stats entry format */
struct link_stats_entry {
  private:
	uint8_t _rate;
	uint8_t _prob;
  public:
	void set_rate(uint8_t rate)    { _rate = rate; }
	void set_prob(uint8_t prob)    { _prob = prob; }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* channel quality map request packet format */
CLICK_PACKED_STRUCTURE(
struct empower_cqm_request : public empower_header {,
private:
  uint32_t _graph_id;
  uint8_t _addr[6];
  uint8_t _channel;
  uint8_t _band;
public:
	uint32_t graph_id() 		        { return ntohl(_graph_id); }
	uint8_t channel() 		            { return _channel; }
	uint8_t band() 		                { return _band; }
	EtherAddress addr()				    { return EtherAddress(_addr); }
});

/* channel quality map entry format */
struct cqm_entry {
  private:
	  uint8_t _sta[6];
	  uint32_t _last_std;
	  int32_t _last_rssi;
	  uint32_t _last_packets;
	  uint32_t _hist_packets;
	  int32_t _ewma_rssi;
	  int32_t _sma_rssi;
  public:
	void set_sta(EtherAddress sta)		  			{ memcpy(_sta, sta.data(), 6); }
	void set_last_std(double last_std) 				{ _last_std = htonl(static_cast<uint32_t> (last_std * 1000 - 0.5)); }
	void set_last_rssi(double last_rssi) 			{ _last_rssi = htonl(static_cast<int32_t> (last_rssi * 1000 - 0.5)); }
	void set_last_packets(uint32_t last_packets) 	{ _last_packets = htonl(last_packets); }
	void set_hist_packets(uint32_t hist_packets) 	{ _hist_packets = htonl(hist_packets); }
	void set_ewma_rssi(double ewma_rssi) 			{ _ewma_rssi = htonl(static_cast<int32_t> (ewma_rssi * 1000 - 0.5)); }
	void set_sma_rssi(double sma_rssi) 				{ _sma_rssi = htonl(static_cast<int32_t> (sma_rssi * 1000 - 0.5)); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* channel quality map response packet format */
struct empower_cqm_response : public empower_header {
private:
  uint32_t _graph_id;
  uint8_t _wtp[6];
  uint8_t _channel;
  uint8_t _band;
  uint16_t _nb_neighbors;
public:
	void set_graph_id(uint32_t graph_id)          { _graph_id = htonl(graph_id); }
	void set_wtp(EtherAddress wtp)		  		  { memcpy(_wtp, wtp.data(), 6); }
	void set_band(uint8_t band)	        		  { _band = band; }
	void set_channel(uint8_t channel)			  { _channel = channel; }
	void set_nb_neighbors(uint16_t nb_neighbors)  { _nb_neighbors = htons(nb_neighbors); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* resource element entry format */
struct resource_elements_entry {
  private:
	uint8_t _channel;
	uint8_t _band;
	uint16_t _flags;
  public:
	void set_flag(uint16_t f)           { _flags = htons(ntohs(_flags) | f); }
	void set_band(uint8_t band)	        { _band = band; }
	void set_channel(uint8_t channel)	{ _channel = channel; }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* port element entry format */
struct port_elements_entry {
  private:
    uint8_t	_hwaddr[6];
	uint16_t _port_id;
	char _iface[10];
  public:
	void set_hwaddr(EtherAddress hwaddr)	{ memcpy(_hwaddr, hwaddr.data(), 6); }
	void set_port_id(uint16_t port_id)	    { _port_id = htons(port_id); }
	void set_iface(String iface)			{ memset(_iface, 0, 10); memcpy(&_iface, iface.data(), iface.length()); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* caps response packet format */
struct empower_caps_response : public empower_header {
private:
  uint8_t	_wtp[6];
  uint8_t	_nb_resources_elements;
  uint8_t	_nb_ports_elements;
public:
	void set_wtp(EtherAddress wtp)		          						{ memcpy(_wtp, wtp.data(), 6); }
	void set_nb_resources_elements(uint8_t nb_resources_elements)	  	{ _nb_resources_elements = nb_resources_elements; }
	void set_nb_ports_elements(uint8_t nb_ports_elements)				{ _nb_ports_elements = nb_ports_elements; }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* counters entryformat */
struct counters_entry {
  private:
	uint16_t _size;
	uint32_t _count;
  public:
	void set_size(uint16_t size)		{ _size = htons(size); }
	void set_count(uint32_t count) 		{ _count = htonl(count); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* counters request packet format */
struct empower_counters_request : public empower_header {
private:
  uint32_t	_counters_id;
  uint8_t	_lvap[6];
public:
	uint32_t counters_id()							  { return ntohl(_counters_id); }
	EtherAddress lvap()							  { return EtherAddress(_lvap); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* counters response packet format */
struct empower_counters_response : public empower_header {
private:
  uint32_t	_counters_id;
  uint8_t	_wtp[6];
  uint8_t	_sta[6];
  uint16_t _nb_tx;
  uint16_t _nb_rx;
public:
	void         set_wtp(EtherAddress wtp)		  { memcpy(_wtp, wtp.data(), 6); }
	void         set_sta(EtherAddress sta)		  { memcpy(_sta, sta.data(), 6); }
	void         set_nb_tx(uint16_t nb_tx)		  { _nb_tx = htons(nb_tx); }
	void         set_nb_rx(uint16_t nb_rx)		  { _nb_rx = htons(nb_rx); }
	void set_counters_id(uint32_t counters_id)		  { _counters_id = htonl(counters_id); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* SSID entry */
struct ssid_entry {
  private:
	uint8_t _length;
	char *_ssid[];    /* see protocol type */
  public:
	uint8_t      length()                        { return _length; }
	String       ssid()                          { return String((char *) _ssid, WIFI_MIN(_length, WIFI_NWID_MAXSIZE)); }
	void         set_length(uint8_t lenght)      { _length = lenght; }
	void         set_ssid(String ssid)           { memset(_ssid, 0, _length); memcpy(_ssid, ssid.data(), ssid.length()); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* add vap packet format */
struct empower_add_lvap : public empower_header {
private:
	uint16_t _flags;
	uint16_t _assoc_id;
    uint8_t _channel;
    uint8_t _band;
	uint8_t	_sta[6];
	uint8_t	_encap[6];
	uint8_t	_net_bssid[6];
	uint8_t	_lvap_bssid[6];
	ssid_entry *_ssids[];
public:
	uint8_t      band()	        { return _band; }
	uint8_t      channel()	    { return _channel; }
	bool         flag(int f)	{ return ntohs(_flags) & f;  }
	uint16_t     assoc_id()     { return ntohs(_assoc_id); }
	EtherAddress sta()			{ return EtherAddress(_sta); }
	EtherAddress encap()		{ return EtherAddress(_encap); }
	EtherAddress net_bssid()		{ return EtherAddress(_net_bssid); }
	EtherAddress lvap_bssid()		{ return EtherAddress(_lvap_bssid); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* del vap packet format */
struct empower_del_lvap : public empower_header {
  private:
    uint8_t	_sta[6];
  public:
	EtherAddress sta()							  { return EtherAddress(_sta); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* lvap status packet format */
struct empower_status_lvap : public empower_header {
private:
	uint16_t _flags;
	uint16_t _assoc_id;
	uint8_t	_wtp[6];
	uint8_t	_sta[6];
	uint8_t	_encap[6];
    uint8_t _channel;
    uint8_t _band;
	uint8_t	_net_bssid[6];
	uint8_t	_lvap_bssid[6];
	ssid_entry *_ssids[];
public:
	void set_band(uint8_t band)	        { _band = band; }
	void set_channel(uint8_t channel)	{ _channel = channel; }
	void set_flag(uint16_t f)           { _flags = htons(ntohs(_flags) | f); }
	void set_assoc_id(uint16_t assoc_id){ _assoc_id = htons(assoc_id); }
	void set_wtp(EtherAddress wtp)		{ memcpy(_wtp, wtp.data(), 6); }
	void set_sta(EtherAddress sta)		{ memcpy(_sta, sta.data(), 6); }
	void set_encap(EtherAddress encap)	{ memcpy(_encap, encap.data(), 6); }
	void set_net_bssid(EtherAddress bssid)	{ memcpy(_net_bssid, bssid.data(), 6); }
	void set_lvap_bssid(EtherAddress bssid)	{ memcpy(_lvap_bssid, bssid.data(), 6); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* set port packet format */
struct empower_set_port : public empower_header {
private:
	uint16_t _flags;
	uint8_t	_sta[6];
	uint16_t _rts_cts;
	uint8_t _nb_mcs;
	uint8_t *mcs[];
public:
	bool flag(int f)                          { return ntohs(_flags) & f;  }
	EtherAddress sta()					      { return EtherAddress(_sta); }
	uint16_t rts_cts()					      { return ntohs(_rts_cts); }
	uint8_t nb_mcs()					      { return _nb_mcs; }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* lvap status packet format */
struct empower_status_port : public empower_header {
private:
	uint16_t _flags;
    uint8_t	_wtp[6];
	uint8_t	_sta[6];
    uint8_t _channel;
    uint8_t _band;
	uint16_t _rts_cts;
	uint8_t _nb_mcs;
	uint8_t *mcs[];
public:
	void set_band(uint8_t band)	        { _band = band; }
	void set_channel(uint8_t channel)	{ _channel = channel; }
	void      set_flag(uint16_t f)          { _flags = htons(ntohs(_flags) | f); }
	void      set_wtp(EtherAddress wtp)		{ memcpy(_wtp, wtp.data(), 6); }
	void      set_sta(EtherAddress sta)		{ memcpy(_sta, sta.data(), 6); }
	void      set_rts_cts(uint16_t rts_cts) { _rts_cts = htons(rts_cts); }
	void      set_nb_mcs(uint8_t nb_mcs)    { _nb_mcs = nb_mcs; }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* add rssi trigger packet format */
struct empower_add_rssi_trigger: public empower_header {
private:
	uint32_t _trigger_id;
	uint8_t	_sta[6];
	uint8_t _relation;
	int8_t _value;
public:
    uint32_t trigger_id()                    { return ntohl(_trigger_id); }
	EtherAddress sta()					     { return EtherAddress(_sta); }
    uint8_t relation()                       { return _relation; }
    int8_t value()                           { return _value; }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* del rssi trigger packet format */
struct empower_del_rssi_trigger: public empower_header {
private:
	uint32_t _trigger_id;
	uint8_t	_sta[6];
	uint8_t _relation;
	int8_t _value;
public:
    uint32_t trigger_id()                    { return ntohl(_trigger_id); }
	EtherAddress sta()					     { return EtherAddress(_sta); }
    uint8_t relation()                       { return _relation; }
    int8_t value()                           { return _value; }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* rssi trigger packet format */
struct empower_rssi_trigger: public empower_header {
private:
	uint32_t _trigger_id;
	uint8_t	_wtp[6];
	uint8_t	_sta[6];
	uint8_t _relation;
	int8_t _value;
	int8_t _current;
public:
	void      set_wtp(EtherAddress wtp)	     { memcpy(_wtp, wtp.data(), 6); }
	void      set_sta(EtherAddress sta)	     { memcpy(_sta, sta.data(), 6); }
	void      set_relation(uint8_t relation) { _relation = relation; }
	void      set_value(int8_t value)        { _value = value; }
	void      set_current(int8_t current)    { _current = current; }
	void      set_trigger_id(int32_t trigger_id) { _trigger_id = htonl(trigger_id); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* add summary packet format */
struct empower_add_summary_trigger: public empower_header {
private:
	uint32_t _trigger_id;
	int16_t _limit;
	uint16_t _period;
	uint8_t	_sta[6];
public:
    uint32_t trigger_id()                    { return ntohl(_trigger_id); }
    int16_t limit()                          { return ntohs(_limit); }
    uint16_t period()                        { return ntohs(_period); }
	EtherAddress sta()					     { return EtherAddress(_sta); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* summary packet format */
struct empower_summary_trigger: public empower_header {
private:
	uint32_t _trigger_id;
	uint8_t	_wtp[6];
	uint8_t	_sta[6];
    uint16_t _nb_frames;
public:
	void set_trigger_id(uint32_t trigger_id) { _trigger_id = htonl(trigger_id); }
	void set_wtp(EtherAddress wtp)	       { memcpy(_wtp, wtp.data(), 6); }
	void set_sta(EtherAddress sta)	       { memcpy(_sta, sta.data(), 6); }
	void set_nb_frames(uint16_t nb_frames) { _nb_frames = htons(nb_frames); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* summary entry format */
struct summary_entry {
  private:
    uint8_t _sta[6];
	uint64_t _tsft;
	uint16_t _seq;
	int8_t _rssi;
	uint8_t _rate;
	uint8_t _type;
	uint8_t _subtype;
	uint32_t _length;
	uint32_t _dur;
  public:
	void set_tsft(uint64_t tsft) 		              { _tsft = htobe64(tsft); }
	void set_seq(int16_t seq) 		                  { _seq = htons(seq); }
	void set_rssi(int8_t rssi) 		                  { _rssi = rssi; }
	void set_rate(uint8_t rate) 		              { _rate = rate; }
	void set_type(uint8_t type) 		              { _type = type; }
	void set_subtype(uint8_t subtype) 		          { _subtype = subtype; }
	void set_length(uint32_t length) 		          { _length= htonl(length); }
	void set_dur(uint32_t dur) 		                  { _dur = htonl(dur); }
	void set_sta(EtherAddress sta)		              { memcpy(_sta, sta.data(), 6); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* del summary packet format */
struct empower_del_summary_trigger: public empower_header {
private:
	uint32_t _trigger_id;
	int16_t _limit;
	uint16_t _period;
	uint8_t	_sta[6];
public:
    uint32_t trigger_id()                    { return ntohl(_trigger_id); }
    int16_t limit()                          { return ntohs(_limit); }
    uint16_t period()                        { return ntohs(_period); }
	EtherAddress sta()					     { return EtherAddress(_sta); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* add vap packet format */
struct empower_add_vap : public empower_header {
private:
    uint8_t _channel;
    uint8_t _band;
	uint8_t	_net_bssid[6];
	char _ssid[];
public:
	uint8_t      band()	        { return _band; }
	uint8_t      channel()	    { return _channel; }
	EtherAddress net_bssid()		{ return EtherAddress(_net_bssid); }
	String       ssid()         { int len = length() - 16; return String((char *) _ssid, WIFI_MIN(len, WIFI_NWID_MAXSIZE)); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* del vap packet format */
struct empower_del_vap : public empower_header {
  private:
    uint8_t	_net_bssid[6];
  public:
	EtherAddress net_bssid()							  { return EtherAddress(_net_bssid); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* lvap status packet format */
struct empower_status_vap : public empower_header {
private:
	uint8_t	_wtp[6];
    uint8_t _channel;
    uint8_t _band;
	uint8_t	_net_bssid[6];
	char _ssid[];
public:
	void set_band(uint8_t band)	        { _band = band; }

	void set_channel(uint8_t channel)	{ _channel = channel; }
	void set_wtp(EtherAddress wtp)		{ memcpy(_wtp, wtp.data(), 6); }
	void set_net_bssid(EtherAddress bssid)	{ memcpy(_net_bssid, bssid.data(), 6); }
	void set_ssid(String ssid)		    { memcpy(&_ssid, ssid.data(), ssid.length()); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

CLICK_ENDDECLS
#endif /* CLICK_EMPOWERPACKET_HH */
