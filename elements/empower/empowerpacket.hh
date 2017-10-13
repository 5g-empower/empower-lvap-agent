#ifndef CLICK_EMPOWERPACKET_HH
#define CLICK_EMPOWERPACKET_HH
#include <clicknet/wifi.h>
#include <elements/wifi/transmissionpolicies.hh>
#include "empowerlvapmanager.hh"
CLICK_DECLS

/* protocol version */
static const uint8_t _empower_version = 0x00;

/* protocol type */
enum empower_packet_types {

    // Internal messages
    EMPOWER_PT_WTP_BYE = 0x00,          // ac -> ac
    EMPOWER_PT_WTP_REGISTER = 0x01,     // ac -> ac
    EMPOWER_PT_LVAP_JOIN = 0x02,        // ac -> ac
    EMPOWER_PT_LVAP_LEAVE = 0x03,       // ac -> ac

    // LVAP messages
    EMPOWER_PT_HELLO = 0x04,            // wtp -> ac
    EMPOWER_PT_PROBE_REQUEST = 0x05,    // wtp -> ac
    EMPOWER_PT_PROBE_RESPONSE = 0x06,   // ac -> wtp
    EMPOWER_PT_AUTH_REQUEST = 0x07,     // wtp -> ac
    EMPOWER_PT_AUTH_RESPONSE = 0x08,    // ac -> wtp
    EMPOWER_PT_ASSOC_REQUEST = 0x09,    // wtp -> ac
    EMPOWER_PT_ASSOC_RESPONSE = 0x10,   // ac -> wtp
    EMPOWER_PT_ADD_LVAP = 0x11,         // ac -> wtp
    EMPOWER_PT_DEL_LVAP = 0x12,         // ac -> wtp
    EMPOWER_PT_STATUS_LVAP = 0x13,      // wtp -> ac
    EMPOWER_PT_SET_PORT= 0x14,          // ac -> wtp
    EMPOWER_PT_STATUS_PORT = 0x15,      // wtp -> ac
	EMPOWER_PT_CAPS_REQUEST = 0x16,     // ac -> wtp
    EMPOWER_PT_CAPS_RESPONSE = 0x17,    // wtp -> ac

    // Packet/Bytes counters
    EMPOWER_PT_COUNTERS_REQUEST = 0x18,         // ac -> wtp
    EMPOWER_PT_COUNTERS_RESPONSE = 0x19,        // wtp -> ac

    // Triggers
    EMPOWER_PT_ADD_RSSI_TRIGGER = 0x20, 		// ac -> wtp
    EMPOWER_PT_RSSI_TRIGGER = 0x21,     		// ac -> wtp
    EMPOWER_PT_DEL_RSSI_TRIGGER = 0x22, 		// ac -> wtp

    EMPOWER_PT_ADD_SUMMARY_TRIGGER = 0x23, 		// ac -> wtp
    EMPOWER_PT_SUMMARY_TRIGGER = 0x24,     		// ac -> wtp
    EMPOWER_PT_DEL_SUMMARY_TRIGGER = 0x25, 		// ac -> wtp

    EMPOWER_PT_ADD_BUSYNESS_TRIGGER = 0x39, 	// ac -> wtp
    EMPOWER_PT_BUSYNESS_TRIGGER = 0x40,     	// ac -> wtp
    EMPOWER_PT_DEL_BUSYNESS_TRIGGER = 0x41, 	// ac -> wtp

    // Channel Quality Maps
    EMPOWER_PT_UCQM_REQUEST = 0x26,     		// ac -> wtp
    EMPOWER_PT_UCQM_RESPONSE = 0x27,    		// wtp -> ac

    EMPOWER_PT_NCQM_REQUEST = 0x28,     		// ac -> wtp
    EMPOWER_PT_NCQM_RESPONSE = 0x29,    		// wtp -> ac

    // Link Stats
    EMPOWER_PT_LVAP_STATS_REQUEST = 0x30,   	// ac -> wtp
    EMPOWER_PT_LVAP_STATS_RESPONSE = 0x31,  	// wtp -> ac

    // VAPs
    EMPOWER_PT_ADD_VAP = 0x32,         			// ac -> wtp
    EMPOWER_PT_DEL_VAP = 0x33,         			// ac -> wtp
    EMPOWER_PT_STATUS_VAP = 0x34,      			// wtp -> ac

    // MCAST Packet/Bytes counters
    EMPOWER_PT_TXP_COUNTERS_REQUEST = 0x35,		// ac -> wtp
    EMPOWER_PT_TXP_COUNTERS_RESPONSE = 0x36,	// wtp -> ac

    // Busyness
    EMPOWER_PT_BUSYNESS_REQUEST = 0x37,			// ac -> wtp
    EMPOWER_PT_BUSYNESS_RESPONSE = 0x38,		// wtp -> ac

    // WTP Packet/Bytes counters
    EMPOWER_PT_WTP_COUNTERS_REQUEST = 0x42,     // ac -> wtp
    EMPOWER_PT_WTP_COUNTERS_RESPONSE = 0x43,    // wtp -> ac

	// P_CQM
	EMPOWER_PT_CQM_LINKS_REQUEST = 0x44,        // ac -> wtp
	EMPOWER_PT_CQM_LINKS_RESPONSE = 0x45,       // ac -> wtp

	// Multicast address transmission policies
	EMPOWER_PT_INCOM_MCAST_REQUEST = 0x46,		// wtp -> ac
	EMPOWER_PT_INCOM_MCAST_RESPONSE = 0x47,		// ac -> wtp

	// IGMP messages
	EMPOWER_PT_IGMP_REQUEST = 0x48,				// wtp -> ac
	EMPOWER_PT_DEL_MCAST_ADDR = 0x49,			// ac -> wtp
	EMPOWER_PT_DEL_MCAST_RECEIVER = 0x50,		// ac -> wtp

	// ADD/DEL response messages
    EMPOWER_PT_ADD_LVAP_RESPONSE = 0x51,        // ac -> wtp
    EMPOWER_PT_DEL_LVAP_RESPONSE = 0x52         // ac -> wtp

};

/* header format, common to all messages */
struct empower_header {
  private:
    uint8_t  _version; /* see protocol version */
    uint8_t  _type;    /* see protocol type */
    uint32_t _length;  /* including this header */
    uint32_t _seq;     /* sequence number */
  public:
    uint8_t  version()                    { return _version; }
    uint8_t  type()                       { return _type; }
    uint32_t length()                     { return ntohl(_length); }
    uint32_t seq()                        { return ntohl(_seq); }
    void     set_seq(uint32_t seq)        { _seq = htonl(seq); }
    void     set_version(uint8_t version) { _version = version; }
    void     set_type(uint8_t type)       { _type = type; }
    void     set_length(uint32_t length)  { _length = htonl(length); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* hello packet format */
struct empower_hello : public empower_header {
  private:
    uint8_t  _wtp[6]; /* EtherAddress */
    uint32_t _period; /* Hello period (ms) */
  public:
    void set_period(uint32_t period) { _period = htonl(period); }
    void set_wtp(EtherAddress wtp)   { memcpy(_wtp, wtp.data(), 6); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* probe request packet format */
struct empower_probe_request : public empower_header {
  private:
    uint8_t _wtp[6];			/* EtherAddress */
    uint8_t _sta[6];			/* EtherAddress */
    uint8_t _hwaddr[6];			/* EtherAddress */
    uint8_t _channel;			/* WiFi channel (int) */
    uint8_t _band;				/* WiFi band (empower_bands_types) */
    uint8_t _supported_band;	/* WiFi band supported by client (empower_bands_types) */
    char    _ssid[];    		/* SSID (string) */
  public:
    void set_hwaddr(EtherAddress hwaddr) 			{ memcpy(_hwaddr, hwaddr.data(), 6); }
    void set_band(uint8_t band)          			{ _band = band; }
    void set_supported_band(uint8_t supported_band)	{ _supported_band = supported_band; }
    void set_channel(uint8_t channel)    			{ _channel = channel; }
    void set_wtp(EtherAddress wtp)       			{ memcpy(_wtp, wtp.data(), 6); }
    void set_sta(EtherAddress sta)       			{ memcpy(_sta, sta.data(), 6); }
    void set_ssid(String ssid)           			{ memcpy(&_ssid, ssid.data(), ssid.length()); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* probe response packet format */
struct empower_probe_response : public empower_header {
  private:
    uint8_t _sta[6]; /* EtherAddress */
    char    _ssid[];    /* SSID (string) */
  public:
    EtherAddress sta()	{ return EtherAddress(_sta); }
    String ssid()		{ int len = length() - 16; return String((char *) _ssid, WIFI_MIN(len, WIFI_NWID_MAXSIZE)); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* auth request packet format */
struct empower_auth_request : public empower_header {
  private:
    uint8_t _wtp[6]; 	/* EtherAddress */
    uint8_t _sta[6]; 	/* EtherAddress */
    uint8_t _bssid[6];	/* EtherAddress */
  public:
    void set_wtp(EtherAddress wtp)     { memcpy(_wtp, wtp.data(), 6); }
    void set_sta(EtherAddress sta)     { memcpy(_sta, sta.data(), 6); }
    void set_bssid(EtherAddress bssid) { memcpy(_bssid, bssid.data(), 6); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* auth response packet format */
struct empower_auth_response : public empower_header {
private:
  uint8_t _sta[6]; /* EtherAddress */
public:
    EtherAddress sta() { return EtherAddress(_sta); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* association request packet format */
struct empower_assoc_request : public empower_header {
  private:
    uint8_t _wtp[6];	/* EtherAddress */
    uint8_t _sta[6];	/* EtherAddress */
    uint8_t _bssid[6];	/* EtherAddress */
    uint8_t _hwaddr[6]; /* EtherAddress */
    uint8_t _channel;	/* WiFi channel (int) */
    uint8_t _band;		/* WiFi band (empower_bands_types) */
    uint8_t _supported_band;	/* WiFi band supported by client (empower_bands_types) */
    char    _ssid[];	/* SSID (String) */
  public:
    void set_wtp(EtherAddress wtp)     { memcpy(_wtp, wtp.data(), 6); }
    void set_sta(EtherAddress sta)     { memcpy(_sta, sta.data(), 6); }
    void set_bssid(EtherAddress bssid) { memcpy(_bssid, bssid.data(), 6); }
    void set_ssid(String ssid)         { memcpy(&_ssid, ssid.data(), ssid.length()); }
    void set_hwaddr(EtherAddress hwaddr) { memcpy(_hwaddr, hwaddr.data(), 6); }
    void set_band(uint8_t band)          { _band = band; }
    void set_channel(uint8_t channel)    { _channel = channel; }
    void set_supported_band(uint8_t supported_band)	{ _supported_band = supported_band; }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* association response packet format */
struct empower_assoc_response : public empower_header {
  private:
    uint8_t _sta[6]; /* EtherAddress */
  public:
    EtherAddress sta() { return EtherAddress(_sta); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* caps packet format */
struct empower_caps : public empower_header {
private:
  uint8_t _wtp[6];					/* EtherAddress */
  uint8_t _nb_resources_elements;	/* Int */
  uint8_t _nb_ports_elements;		/* Int */
public:
    void set_wtp(EtherAddress wtp)                                { memcpy(_wtp, wtp.data(), 6); }
    void set_nb_resources_elements(uint8_t nb_resources_elements) { _nb_resources_elements = nb_resources_elements; }
    void set_nb_ports_elements(uint8_t nb_ports_elements)         { _nb_ports_elements = nb_ports_elements; }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* resource element entry format */
struct resource_elements_entry {
  private:
    uint8_t  _hwaddr[6]; 	/* EtherAddress */
    uint8_t  _channel;		/* WiFi Channel (Int) */
    uint8_t  _band;			/* WiFi band (empower_bands_types) */
  public:
    void set_hwaddr(EtherAddress hwaddr) { memcpy(_hwaddr, hwaddr.data(), 6); }
    void set_band(uint8_t band)          { _band = band; }
    void set_channel(uint8_t channel)    { _channel = channel; }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* port element entry format */
struct port_elements_entry {
  private:
    uint8_t  _hwaddr[6];	/* EtherAddress */
    uint16_t _port_id;		/* OVS port id (Int) */
    char    _iface[10];		/* OVS interface name (String) */
  public:
    void set_hwaddr(EtherAddress hwaddr) { memcpy(_hwaddr, hwaddr.data(), 6); }
    void set_port_id(uint16_t port_id)   { _port_id = htons(port_id); }
    void set_iface(String iface)         { memset(_iface, 0, 10); memcpy(&_iface, iface.data(), iface.length()); }
} CLICK_SIZE_PACKED_ATTRIBUTE;


/* link stats request packet format */
struct empower_lvap_stats_request : public empower_header {
private:
  uint32_t _lvap_stats_id;	/* Module id (int) */
  uint8_t  _sta[6];		/* EtherAddress */

public:
    uint32_t     lvap_stats_id() { return ntohl(_lvap_stats_id); }
    EtherAddress sta()           { return EtherAddress(_sta); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* link stats response packet format */
struct empower_lvap_stats_response : public empower_header {
private:
  uint32_t _lvap_stats_id;	/* Module id (int) */
  uint8_t  _wtp[6];			/* EtherAddress */
  uint16_t _nb_entries;	    /* Int */
public:
  void set_lvap_stats_id(uint32_t lvap_stats_id) { _lvap_stats_id = htonl(lvap_stats_id); }
  void set_wtp(EtherAddress wtp)                 { memcpy(_wtp, wtp.data(), 6); }
  void set_nb_entries(uint16_t nb_entries) 		 { _nb_entries = htons(nb_entries); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* lvap_stats entry format */
struct lvap_stats_entry {
  private:
    uint8_t  _rate; 	/* Rate in units of 500kbps or MCS index */
    uint16_t _flags;	/* Flags (empower_rate_flags) */
    uint32_t _prob; 	/* Probability [0-18000] */
    uint32_t _cur_prob; 	/* Probability [0-18000] */

  public:
    void set_flags(uint16_t flags)    		{ _flags = htons(flags); }
    void set_flag(uint16_t f)        		{ _flags = htons(ntohs(_flags) | f); }
    void set_rate(uint8_t rate)  			{ _rate = rate; }
    void set_prob(uint32_t prob) 			{ _prob = htonl(prob); }
    void set_cur_prob(uint32_t cur_prob) 	{ _cur_prob = htonl(cur_prob); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* busyness request packet format */
struct empower_busyness_request : public empower_header {
private:
  uint32_t _busyness_id; /* Module id (int) */
  uint8_t  _hwaddr[6];	 /* EtherAddress */
  uint8_t  _channel;	 /* WiFi Channel (int) */
  uint8_t  _band;		 /* WiFi band (empower_band_types) */
public:
    uint32_t busyness_id()	{ return ntohl(_busyness_id); }
    uint8_t channel()     	{ return _channel; }
    uint8_t band()        	{ return _band; }
    EtherAddress hwaddr() 	{ return EtherAddress(_hwaddr); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* link stats response packet format */
struct empower_busyness_response : public empower_header {
private:
  uint32_t _busyness_id;	/* Module id (int) */
  uint8_t  _wtp[6];			/* EtherAddress */
  uint32_t _prob; 			/* Probability [0-18000] */
public:
  void set_busyness_id(uint32_t busyness_id) 	{ _busyness_id = htonl(busyness_id); }
  void set_wtp(EtherAddress wtp)				{ memcpy(_wtp, wtp.data(), 6); }
  void set_prob(uint32_t prob) 					{ _prob = htonl(prob); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* channel quality map request packet format */
struct empower_cqm_request : public empower_header {
private:
  uint32_t _graph_id;	/* Module id (int) */
  uint8_t  _hwaddr[6];	/* EtherAddress */
  uint8_t  _channel;	/* WiFi Channel (int) */
  uint8_t  _band;		/* WiFi band (empower_band_types) */
public:
    uint32_t graph_id()   { return ntohl(_graph_id); }
    uint8_t channel()     { return _channel; }
    uint8_t band()        { return _band; }
    EtherAddress hwaddr() { return EtherAddress(_hwaddr); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* channel quality map entry format */
struct cqm_entry {
  private:
      uint8_t  _addr[6];		/* EtherAddress */
      uint8_t  _last_rssi_std;	/* Std RSSI during last window in dBm (int) */
      int8_t   _last_rssi_avg;	/* Std RSSI during last window in dBm (int) */
      uint32_t _last_packets;	/* Number of frames during last window in dBm (int) */
      uint32_t _hist_packets;   /* Total number of frames (int) */
      int8_t   _mov_rssi;       /* Moving RSSI computed across windows in dBm (int) */
  public:
    void set_sta(EtherAddress addr)                 { memcpy(_addr, addr.data(), 6); }
    void set_last_rssi_std(uint8_t last_rssi_std)   { _last_rssi_std = last_rssi_std; }
    void set_last_rssi_avg(int8_t last_rssi_avg)    { _last_rssi_avg = last_rssi_avg; }
    void set_last_packets(uint32_t last_packets)    { _last_packets = htonl(last_packets); }
    void set_hist_packets(uint32_t hist_packets)    { _hist_packets = htonl(hist_packets); }
    void set_mov_rssi(int8_t mov_rssi)              { _mov_rssi = mov_rssi; }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* channel quality map response packet format */
struct empower_cqm_response : public empower_header {
private:
  uint32_t _graph_id;		/* Module id (int) */
  uint8_t  _wtp[6];			/* EtherAddress */
  uint16_t _nb_entries;	    /* Int */
public:
    void set_graph_id(uint32_t graph_id)          { _graph_id = htonl(graph_id); }
    void set_wtp(EtherAddress wtp)                { memcpy(_wtp, wtp.data(), 6); }
    void set_nb_entries(uint16_t nb_entries)  	  { _nb_entries = htons(nb_entries); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* counters request packet format */
struct empower_counters_request : public empower_header {
private:
  uint32_t  _counters_id;	/* Module id (int) */
  uint8_t   _sta[6];		/* EtherAddress */
public:
    uint32_t counters_id() { return ntohl(_counters_id); }
    EtherAddress sta()     { return EtherAddress(_sta); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* counters response packet format */
struct empower_counters_response : public empower_header {
private:
  uint32_t _counters_id;	/* Module id (int) */
  uint8_t  _wtp[6];			/* EtherAddress */
  uint8_t  _sta[6];			/* EtherAddress */
  uint16_t _nb_tx;			/* Int */
  uint16_t _nb_rx;			/* Int */
public:
    void set_wtp(EtherAddress wtp)             { memcpy(_wtp, wtp.data(), 6); }
    void set_sta(EtherAddress sta)             { memcpy(_sta, sta.data(), 6); }
    void set_nb_tx(uint16_t nb_tx)             { _nb_tx = htons(nb_tx); }
    void set_nb_rx(uint16_t nb_rx)             { _nb_rx = htons(nb_rx); }
    void set_counters_id(uint32_t counters_id) { _counters_id = htonl(counters_id); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* txp counters request packet format */
struct empower_txp_counters_request : public empower_header {
private:
  uint32_t _counters_id;	/* Module id (int) */
  uint8_t  _hwaddr[6];		/* EtherAddress */
  uint8_t  _channel;		/* WiFi Channel (int) */
  uint8_t  _band;			/* WiFi band (empower_band_types) */
  uint8_t  _mcast[6];		/* EtherAddress */
public:
    uint32_t counters_id() { return ntohl(_counters_id); }
    EtherAddress mcast()   { return EtherAddress(_mcast); }
    uint8_t channel()      { return _channel; }
    uint8_t band()         { return _band; }
    EtherAddress hwaddr()  { return EtherAddress(_hwaddr); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* txp counters response packet format */
struct empower_txp_counters_response : public empower_header {
private:
  uint32_t _counters_id;	/* Module id (int) */
  uint8_t  _wtp[6];			/* EtherAddress */
  uint16_t _nb_tx;			/* Int */
public:
    void set_wtp(EtherAddress wtp)             { memcpy(_wtp, wtp.data(), 6); }
    void set_nb_tx(uint16_t nb_tx)             { _nb_tx = htons(nb_tx); }
    void set_counters_id(uint32_t counters_id) { _counters_id = htonl(counters_id); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* counters entry format */
struct counters_entry {
  private:
    uint16_t _size;		/* Frame size in bytes (int) */
    uint32_t _count; 	/* Number of frames (int) */
  public:
    void set_size(uint16_t size)   { _size = htons(size); }
    void set_count(uint32_t count) { _count = htonl(count); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* counters request packet format */
struct empower_wtp_counters_request : public empower_header {
private:
  uint32_t  _counters_id;	/* Module id (int) */
public:
    uint32_t counters_id() { return ntohl(_counters_id); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* counters response packet format */
struct empower_wtp_counters_response : public empower_header {
private:
  uint32_t _counters_id;	/* Module id (int) */
  uint8_t  _wtp[6];			/* EtherAddress */
  uint16_t _nb_tx;			/* Int */
  uint16_t _nb_rx;			/* Int */
public:
    void set_wtp(EtherAddress wtp)             { memcpy(_wtp, wtp.data(), 6); }
    void set_nb_tx(uint16_t nb_tx)             { _nb_tx = htons(nb_tx); }
    void set_nb_rx(uint16_t nb_rx)             { _nb_rx = htons(nb_rx); }
    void set_counters_id(uint32_t counters_id) { _counters_id = htonl(counters_id); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* counters entry format */
struct wtp_counters_entry {
  private:
    uint8_t  _sta[6];	/* EtherAddress */
    uint16_t _size;		/* Frame size in bytes (int) */
    uint32_t _count; 	/* Number of frames (int) */
  public:
    void set_sta(EtherAddress sta) { memcpy(_sta, sta.data(), 6); }
    void set_size(uint16_t size)   { _size = htons(size); }
    void set_count(uint32_t count) { _count = htonl(count); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* SSID entry */
struct ssid_entry {
  private:
    uint8_t _length;	/* Length of the ssid in bytes (int) */
    char *  _ssid[];    /* SSID (String) */
  public:
    uint8_t length()                   { return _length; }
    String  ssid()                     { return String((char *) _ssid, WIFI_MIN(_length, WIFI_NWID_MAXSIZE)); }
    void    set_length(uint8_t lenght) { _length = lenght; }
    void    set_ssid(String ssid)      { memset(_ssid, 0, _length); memcpy(_ssid, ssid.data(), ssid.length()); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* add lvap packet format */
struct empower_add_lvap : public empower_header {
private:
	uint32_t	 _module_id;		/* Transaction id */
	uint16_t     _flags;			/* Flags (empower_packet_flags) */
    uint16_t     _assoc_id;			/* Association id */
    uint8_t      _hwaddr[6];		/* EtherAddress */
    uint8_t      _channel;			/* WiFi channel (int) */
    uint8_t      _band;				/* WiFi band (empower_band_types) */
    uint8_t      _supported_band;	/* WiFi band supported by client(empower_band_types) */
    uint8_t      _sta[6];			/* EtherAddress */
    uint8_t      _encap[6];			/* EtherAddress */
    uint8_t      _net_bssid[6];		/* EtherAddress */
    uint8_t      _lvap_bssid[6];	/* EtherAddress */
    ssid_entry * _ssids[];			/* SSIDs (ssid_entry) */
public:
    uint32_t     module_id()   		{ return ntohl(_module_id); }
    uint8_t      band()       		{ return _band; }
    uint8_t      supported_band()	{ return _supported_band; }
    uint8_t      channel()    		{ return _channel; }
    bool         flag(int f)  		{ return ntohs(_flags) & f;  }
    uint16_t     assoc_id()  		{ return ntohs(_assoc_id); }
    EtherAddress sta()        		{ return EtherAddress(_sta); }
    EtherAddress hwaddr()     		{ return EtherAddress(_hwaddr); }
    EtherAddress encap()      		{ return EtherAddress(_encap); }
    EtherAddress net_bssid()  		{ return EtherAddress(_net_bssid); }
    EtherAddress lvap_bssid() 		{ return EtherAddress(_lvap_bssid); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* del lvap packet format */
struct empower_del_lvap : public empower_header {
  private:
	uint32_t _module_id;		/* Transaction id */
    uint8_t _sta[6]; 			/* EtherAddress */
    uint8_t _target_hwaddr[6];	/* EtherAddress */
    uint8_t _target_channel;	/* WiFi channel (int) */
    uint8_t _target_band;		/* WiFi band (empower_band_types) */
    uint8_t _csa_switch_mode;
    uint8_t _csa_switch_count;
  public:
    uint32_t module_id()   			{ return ntohl(_module_id); }
    EtherAddress sta() 				{ return EtherAddress(_sta); }
    uint8_t csa_switch_mode()		{ return _csa_switch_mode; }
    uint8_t csa_switch_count()		{ return _csa_switch_count; }
    uint8_t target_band()       	{ return _target_band; }
    uint8_t target_channel()    	{ return _target_channel; }
    EtherAddress target_hwaddr()	{ return EtherAddress(_target_hwaddr); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* lvap add/del response packet format */
struct empower_add_del_lvap_response : public empower_header {
private:
    uint8_t  _wtp[6];			/* EtherAddress */
    uint8_t  _sta[6];			/* EtherAddress */
	uint32_t _module_id;		/* Transaction id */
	uint32_t _status;			/* Status code */
public:
    void set_sta(EtherAddress sta)          { memcpy(_sta, sta.data(), 6); }
    void set_module_id(uint32_t module_id)	{ _module_id = htonl(module_id); }
    void set_status(uint32_t status)		{ _status = htonl(status); }
    void set_wtp(EtherAddress wtp)          { memcpy(_wtp, wtp.data(), 6); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* lvap status packet format */
struct empower_status_lvap : public empower_header {
private:
    uint16_t     _flags;			/* Flags (empower_packet_flags) */
    uint16_t     _assoc_id;			/* Association id */
    uint8_t      _wtp[6];			/* EtherAddress */
    uint8_t      _sta[6];			/* EtherAddress */
    uint8_t      _encap[6];			/* EtherAddress */
    uint8_t      _hwaddr[6];		/* EtherAddress */
    uint8_t      _channel;			/* WiFi channel (int) */
    uint8_t      _band;				/* WiFi band (empower_band_types) */
    uint8_t      _supported_band;	/* WiFi band supported by client (empower_band_types) */
    uint8_t      _net_bssid[6];		/* EtherAddress */
    uint8_t      _lvap_bssid[6];	/* EtherAddress */
    ssid_entry  *_ssids[];			/* SSIDs (ssid_entry) */
public:
    void set_band(uint8_t band)             { _band = band; }
    void set_channel(uint8_t channel)       { _channel = channel; }
    void set_flag(uint16_t f)               { _flags = htons(ntohs(_flags) | f); }
    void set_assoc_id(uint16_t assoc_id)    { _assoc_id = htons(assoc_id); }
    void set_hwaddr(EtherAddress hwaddr)    { memcpy(_hwaddr, hwaddr.data(), 6); }
    void set_wtp(EtherAddress wtp)          { memcpy(_wtp, wtp.data(), 6); }
    void set_sta(EtherAddress sta)          { memcpy(_sta, sta.data(), 6); }
    void set_encap(EtherAddress encap)      { memcpy(_encap, encap.data(), 6); }
    void set_net_bssid(EtherAddress bssid)  { memcpy(_net_bssid, bssid.data(), 6); }
    void set_lvap_bssid(EtherAddress bssid) { memcpy(_lvap_bssid, bssid.data(), 6); }
    void set_supported_band(uint8_t supported_band)	{ _supported_band = supported_band; }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* set port packet format */
struct empower_set_port : public empower_header {
private:
    uint16_t _flags;			/* Flags (empower_port_flags) */
    uint8_t  _hwaddr[6];		/* EtherAddress */
    uint8_t  _channel;			/* WiFi channel (int) */
    uint8_t  _band;				/* WiFi band (empower_band_types) */
    uint8_t  _sta[6];			/* EtherAddress */
    uint16_t _rts_cts;			/* RTS/CTS threshold */
    uint8_t  _tx_mcast;			/* multicast mode (tx_mcast_type) */
    uint8_t  _ur_mcast_count;	/* Number of unsolicited replies (int) */
    uint8_t  _nb_mcs;			/* Number of rate entries (int) */
    uint8_t  _nb_ht_mcs;		/* Number of HT rate entries (int) */
    uint8_t  *mcs[];			/* Rate entries in units of 500kbps or MCS index */
    uint8_t  *ht_mcs[];			/* HT Rate entries as MCS index */
public:
    bool flag(int f)         			{ return ntohs(_flags) & f;  }
    uint8_t band()           			{ return _band; }
    uint8_t channel()        			{ return _channel; }
    EtherAddress hwaddr()    			{ return EtherAddress(_hwaddr); }
    EtherAddress addr()      			{ return EtherAddress(_sta); }
    uint16_t rts_cts()       			{ return ntohs(_rts_cts); }
    empower_tx_mcast_type tx_mcast()	{ return empower_tx_mcast_type(_tx_mcast); }
    uint8_t ur_mcast_count() 			{ return _ur_mcast_count; }
    uint8_t nb_mcs()         			{ return _nb_mcs; }
    uint8_t nb_ht_mcs()         		{ return _nb_ht_mcs; }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* lvap status packet format */
struct empower_status_port : public empower_header {
private:
    uint16_t _flags;			/* Flags (empower_port_flags) */
    uint8_t  _wtp[6];			/* EtherAddress */
    uint8_t  _sta[6];			/* EtherAddress */
    uint8_t  _hwaddr[6];		/* EtherAddress */
    uint8_t  _channel;			/* WiFi channel (int) */
    uint8_t  _band;				/* WiFi band (empower_band_types) */
    uint16_t _rts_cts;			/* RTS/CTS threshold */
    uint8_t  _tx_mcast;			/* multicast mode (tx_mcast_type) */
    uint8_t  _ur_mcast_count;	/* Number of unsolicited replies (int) */
    uint8_t  _nb_mcs;			/* Number of rate entries (int) */
    uint8_t  _nb_ht_mcs;		/* Number of HT rate entries (int) */
    uint8_t  *mcs[];			/* Rate entries in units of 500kbps or MCS index */
    uint8_t  *ht_mcs[];			/* HT Rate entries as MCS index */
public:
    void set_band(uint8_t band)                     	{ _band = band; }
    void set_channel(uint8_t channel)               	{ _channel = channel; }
    void set_flag(uint16_t f)                       	{ _flags = htons(ntohs(_flags) | f); }
    void set_hwaddr(EtherAddress hwaddr)            	{ memcpy(_hwaddr, hwaddr.data(), 6); }
    void set_wtp(EtherAddress wtp)                  	{ memcpy(_wtp, wtp.data(), 6); }
    void set_sta(EtherAddress sta)                  	{ memcpy(_sta, sta.data(), 6); }
    void set_rts_cts(uint16_t rts_cts)              	{ _rts_cts = htons(rts_cts); }
    void set_tx_mcast(empower_tx_mcast_type tx_mcast)	{ _tx_mcast = uint8_t(tx_mcast); }
    void set_nb_mcs(uint8_t nb_mcs)                 	{ _nb_mcs = nb_mcs; }
    void set_nb_ht_mcs(uint8_t nb_ht_mcs)               { _nb_ht_mcs = nb_ht_mcs; }
    void set_ur_mcast_count(uint8_t ur_mcast_count) 	{ _ur_mcast_count = ur_mcast_count; }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* add busyness packet format */
struct empower_add_busyness_trigger: public empower_header {
private:
    uint32_t _trigger_id;	/* Module id (int) */
    uint8_t  _hwaddr[6];	/* EtherAddress */
    uint8_t  _channel;		/* WiFi channel (int) */
    uint8_t  _band;			/* WiFi band (empower_band_types) */
    uint8_t  _relation;   	/* Relation (relation_t) */
    uint32_t _value;		/* Busyness value between 0 and 18000 */
    uint16_t _period;		/* Reporting period in ms (int) */
public:
    EtherAddress hwaddr() { return EtherAddress(_hwaddr); }
    uint8_t channel()     { return _channel; }
    uint8_t band()        { return _band; }
    uint32_t trigger_id() { return ntohl(_trigger_id); }
    uint8_t relation()    { return _relation; }
    uint32_t value()      { return ntohl(_value); }
    uint16_t period()     { return ntohs(_period); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* del busyness trigger packet format */
struct empower_del_busyness_trigger: public empower_header {
private:
    uint32_t _trigger_id; /* Module id (int) */
public:
    uint32_t trigger_id() { return ntohl(_trigger_id); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* busyness trigger packet format */
struct empower_busyness_trigger: public empower_header {
private:
    uint32_t _trigger_id; 	/* Module id (int) */
    uint8_t  _wtp[6];		/* EtherAddress */
    uint8_t  _hwaddr[6];	/* EtherAddress */
    uint8_t  _channel;		/* WiFi channel (int) */
    uint8_t  _band;			/* WiFi band (empower_band_types) */
    uint32_t  _current;     /* Busyness value between 0 and 18000 */
public:
    void set_wtp(EtherAddress wtp)          { memcpy(_wtp, wtp.data(), 6); }
    void set_band(uint8_t band)             { _band = band; }
    void set_channel(uint8_t channel)       { _channel = channel; }
    void set_hwaddr(EtherAddress hwaddr)    { memcpy(_hwaddr, hwaddr.data(), 6); }
    void set_current(uint32_t current)      { _current = htonl(current); }
    void set_trigger_id(int32_t trigger_id) { _trigger_id = htonl(trigger_id); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* add rssi trigger packet format */
struct empower_add_rssi_trigger: public empower_header {
private:
    uint32_t _trigger_id;	/* Module id (int) */
    uint8_t  _sta[6];		/* EtherAddress */
    uint8_t  _relation;	/* Relation (relation_t) */
    int8_t   _value;		/* RSSI value in dBm (int) */
    uint16_t _period;		/* Reporting period in ms (int) */
public:
    uint32_t trigger_id() { return ntohl(_trigger_id); }
    EtherAddress sta()    { return EtherAddress(_sta); }
    uint8_t relation()    { return _relation; }
    int8_t value()        { return _value; }
    uint16_t period()     { return ntohs(_period); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* del rssi trigger packet format */
struct empower_del_rssi_trigger: public empower_header {
private:
    uint32_t _trigger_id; /* Module id (int) */
public:
    uint32_t trigger_id() { return ntohl(_trigger_id); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* rssi trigger packet format */
struct empower_rssi_trigger: public empower_header {
private:
    uint32_t _trigger_id; 	/* Module id (int) */
    uint8_t  _wtp[6];		/* EtherAddress */
    uint8_t  _hwaddr[6];	/* EtherAddress */
    uint8_t  _channel;		/* WiFi channel (int) */
    uint8_t  _band;			/* WiFi band (empower_band_types) */
    int8_t  _current;		/* RSSI value in dBm (int) */
public:
    void set_wtp(EtherAddress wtp)          { memcpy(_wtp, wtp.data(), 6); }
    void set_band(uint8_t band)             { _band = band; }
    void set_channel(uint8_t channel)       { _channel = channel; }
    void set_hwaddr(EtherAddress hwaddr)    { memcpy(_hwaddr, hwaddr.data(), 6); }
    void set_current(int8_t current)        { _current = current; }
    void set_trigger_id(int32_t trigger_id) { _trigger_id = htonl(trigger_id); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* add summary packet format */
struct empower_add_summary_trigger: public empower_header {
private:
    uint32_t _trigger_id;	/* Module id (int) */
    uint8_t  _addr[6];		/* EtherAddress */
    uint8_t  _hwaddr[6];	/* EtherAddress */
    uint8_t  _channel;		/* WiFi channel (int) */
    uint8_t  _band;			/* WiFi band (empower_band_types) */
    int16_t  _limit;		/* Number of reports to be sent, -1 send forever (int) */
    uint16_t _period;		/* Reporting period in ms (int) */
public:
    EtherAddress addr()   { return EtherAddress(_addr); }
    EtherAddress hwaddr() { return EtherAddress(_hwaddr); }
    uint8_t channel()     { return _channel; }
    uint8_t band()        { return _band; }
    uint32_t trigger_id() { return ntohl(_trigger_id); }
    int16_t limit()       { return ntohs(_limit); }
    uint16_t period()     { return ntohs(_period); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* summary packet format */
struct empower_summary_trigger: public empower_header {
private:
    uint32_t _trigger_id; 	/* Module id (int) */
    uint8_t _wtp[6];			/* EtherAddress */
    uint16_t _nb_entries;		/* Number of frames (int) */
public:
    void set_trigger_id(uint32_t trigger_id) { _trigger_id = htonl(trigger_id); }
    void set_wtp(EtherAddress wtp)           { memcpy(_wtp, wtp.data(), 6); }
    void set_nb_frames(uint16_t nb_entries)  { _nb_entries = htons(nb_entries); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* summary entry format */
struct summary_entry {
  private:
    uint8_t  _ra[6];	/* Receiver address (EtherAddress) */
    uint8_t  _ta[6];	/* Transmitter address (EtherAddress) */
    uint64_t _tsft;		/* Timestamp in microseconds (int) */
    uint16_t _flags;	/* Flags (empower_rate_flags) */
    uint16_t _seq;		/* Sequence number */
    int8_t   _rssi;		/* RSSI in dBm (int) */
    uint8_t  _rate;		/* Rate in units of 500kbps or MCS index */
    uint8_t  _type;		/* WiFi frame type */
    uint8_t  _subtype;	/* WiFi frame sub-type */
    uint32_t _length;	/* Frame length in bytes (int) */
  public:
    void set_tsft(uint64_t tsft)      	{ _tsft = htobe64(tsft); }
    void set_flags(uint16_t flags)    	{ _flags = htons(flags); }
    void set_flag(uint16_t f)        	{ _flags = htons(ntohs(_flags) | f); }
    void set_seq(int16_t seq)         	{ _seq = htons(seq); }
    void set_rssi(int8_t rssi)        	{ _rssi = rssi; }
    void set_rate(uint8_t rate)       	{ _rate = rate; }
    void set_type(uint8_t type)      	{ _type = type; }
    void set_subtype(uint8_t subtype) 	{ _subtype = subtype; }
    void set_length(uint32_t length)  	{ _length= htonl(length); }
    void set_ra(EtherAddress ra)  	  	{ memcpy(_ra, ra.data(), 6); }
    void set_ta(EtherAddress ta)		{ memcpy(_ta, ta.data(), 6); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* del summary packet format */
struct empower_del_summary_trigger: public empower_header {
private:
    uint32_t _trigger_id; /* Module id (int) */
public:
    uint32_t trigger_id() { return ntohl(_trigger_id); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* add vap packet format */
struct empower_add_vap : public empower_header {
private:
    uint8_t _hwaddr[6];		/* EtherAddress */
    uint8_t _channel;		/* WiFi channel (int) */
    uint8_t _band;			/* WiFi band (empower_band_types) */
    uint8_t _net_bssid[6];	/* EtherAddress */
    char    _ssid[];		/* SSID (String) */
public:
    uint8_t      band()      { return _band; }
    uint8_t      channel()   { return _channel; }
    EtherAddress hwaddr()    { return EtherAddress(_hwaddr); }
    EtherAddress net_bssid() { return EtherAddress(_net_bssid); }
    String       ssid()      { int len = length() - 24; return String((char *) _ssid, WIFI_MIN(len, WIFI_NWID_MAXSIZE)); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* del vap packet format */
struct empower_del_vap : public empower_header {
  private:
    uint8_t _net_bssid[6]; /* EtherAddress */
  public:
    EtherAddress net_bssid()   { return EtherAddress(_net_bssid); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* lvap status packet format */
struct empower_status_vap : public empower_header {
private:
    uint8_t _wtp[6];			/* EtherAddress */
    uint8_t _hwaddr[6];		/* EtherAddress */
    uint8_t _channel;			/* WiFi channel (int) */
    uint8_t _band;				/* WiFi band (empower_band_types) */
    uint8_t _net_bssid[6];	/* EtherAddress */
    char    _ssid[];			/* SSID (String) */
public:
    void set_band(uint8_t band)            { _band = band; }
    void set_hwaddr(EtherAddress hwaddr)   { memcpy(_hwaddr, hwaddr.data(), 6); }
    void set_channel(uint8_t channel)      { _channel = channel; }
    void set_wtp(EtherAddress wtp)         { memcpy(_wtp, wtp.data(), 6); }
    void set_net_bssid(EtherAddress bssid) { memcpy(_net_bssid, bssid.data(), 6); }
    void set_ssid(String ssid)             { memcpy(&_ssid, ssid.data(), ssid.length()); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* cqm links request packet format */
struct empower_cqm_links_request : public empower_header {
private:
  uint32_t  _cqm_links_id;	/* Module id (int) */
public:
    uint32_t cqm_links_id() { return ntohl(_cqm_links_id); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* counters response packet format */
struct empower_cqm_links_response : public empower_header {
private:
  uint32_t _cqm_links_id;	/* Module id (int) */
  uint8_t  _wtp[6];			/* EtherAddress */
  uint16_t _nb_links;		/* Int */
public:
    void set_wtp(EtherAddress wtp)               { memcpy(_wtp, wtp.data(), 6); }
    void set_nb_links(uint16_t nb_links)         { _nb_links = htons(nb_links); }
    void set_cqm_links_id(uint32_t cqm_links_id) { _cqm_links_id = htonl(cqm_links_id); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* cqm link entry */
struct empower_cqm_link {
  private:
    uint8_t  _ta[6]; 					/* EtherAddress */
    uint32_t _p_channel_busy_fraction;	/* p_channel_busy_fraction [0-18000] */
    uint32_t _p_throughput;				/* p_throughput [0-18000] */
    uint32_t _p_available_bw;			/* p_available_bw [0-18000] */
    uint32_t _p_pdr;  					/* p_pdr [0-18000] */
    uint32_t _p_attainable_throughput;	/* p_available_bw [0-18000] */
  public:
    void set_p_pdr(uint32_t p_pdr) 										{ _p_pdr = htonl(p_pdr); }
    void set_p_available_bw(uint32_t p_available_bw) 					{ _p_available_bw = htonl(p_available_bw); }
    void set_p_throughput(uint32_t p_throughput) 						{ _p_throughput = htonl(p_throughput); }
    void set_p_attainable_throughput(uint32_t p_attainable_throughput) 	{ _p_attainable_throughput = htonl(p_attainable_throughput); }
    void set_p_channel_busy_fraction(uint32_t p_channel_busy_fraction) 	{ _p_channel_busy_fraction = htonl(p_channel_busy_fraction); }
    void set_ta(EtherAddress ta)   					   			 		{ memcpy(_ta, ta.data(), 6); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* incomming multicast address request format */
struct empower_incom_mcast_addr : public empower_header {
  private:
    uint8_t  _wtp[6]; 			/* EtherAddress */
    uint8_t  _mcast_addr[6]; 	/* EtherAddress */
    uint8_t  _iface;			/* OVS interface name (String) */
  public:
    void set_mcast_addr(EtherAddress mcast_addr) 	{ memcpy(_mcast_addr, mcast_addr.data(), 6); }
    void set_wtp(EtherAddress wtp)   				{ memcpy(_wtp, wtp.data(), 6); }
    void set_iface(int iface)         				{ _iface = iface; }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* incomming multicast address response format */
struct empower_incom_mcast_addr_response : public empower_header {
  private:
    uint8_t  _mcast_addr[6]; 	/* EtherAddress */
    uint8_t  _iface;			/* OVS interface name (String) */
  public:
    EtherAddress mcast_addr() 	{ return EtherAddress(_mcast_addr); }
    uint8_t iface()     		{ return _iface; }
} CLICK_SIZE_PACKED_ATTRIBUTE;

struct empower_igmp_request : public empower_header {
  private:
    uint8_t  _wtp[6]; 			/* EtherAddress */
    uint8_t  _sta [6];			/* EtherAddress */
    uint8_t  _mcast_addr[4]; 	/* IPAddress */
    uint8_t  _igmp_type;		/* IGMP record type */
  public:
    void set_wtp(EtherAddress wtp)   				{ memcpy(_wtp, wtp.data(), 6); }
    void set_sta(EtherAddress sta)                  { memcpy(_sta, sta.data(), 6); }
    void set_mcast_addr(IPAddress mcast_addr) 		{ memcpy(_mcast_addr, mcast_addr.data(), 4); }
    void set_igmp_type(int igmp_type)         		{ _igmp_type = igmp_type; }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* delete multicast address format */
struct empower_del_mcast_addr : public empower_header {
  private:
    uint8_t  _mcast_addr[6]; 	/* EtherAddress */
    uint8_t  _hwaddr[6];		/* EtherAddress */
	uint8_t  _channel;			/* WiFi channel (int) */
	uint8_t  _band;				/* WiFi band (empower_band_types) */
  public:
    EtherAddress mcast_addr() 	{ return EtherAddress(_mcast_addr); }
    uint8_t      band()      	{ return _band; }
	uint8_t      channel()   	{ return _channel; }
	EtherAddress hwaddr()    	{ return EtherAddress(_hwaddr); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* delete multicast receiver format */
struct empower_del_mcast_receiver : public empower_header {
  private:
	uint8_t  _sta[6]; 			/* EtherAddress */
	uint8_t  _hwaddr[6];		/* EtherAddress */
	uint8_t  _channel;			/* WiFi channel (int) */
	uint8_t  _band;				/* WiFi band (empower_band_types) */
  public:
	EtherAddress sta()			{ return EtherAddress(_sta); }
	uint8_t      band()			{ return _band; }
	uint8_t      channel()		{ return _channel; }
	EtherAddress hwaddr()		{ return EtherAddress(_hwaddr); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

CLICK_ENDDECLS
#endif /* CLICK_EMPOWERPACKET_HH */
