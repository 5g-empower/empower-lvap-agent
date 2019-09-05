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

	/* Core messages 0x01 - 0x3F */

    // base messages
    EMPOWER_PT_HELLO_REQUEST = 0x01,                // ac -> wtp
    EMPOWER_PT_HELLO_RESPONSE = 0x02,               // wtp -> ac

    EMPOWER_PT_CAPS_REQUEST = 0x03,                 // ac -> wtp
    EMPOWER_PT_CAPS_RESPONSE = 0x04,                // wtp -> ac

    EMPOWER_PT_PROBE_REQUEST = 0x05,                // wtp -> ac
    EMPOWER_PT_PROBE_RESPONSE = 0x06,               // ac -> wtp

	EMPOWER_PT_AUTH_REQUEST = 0x07,                 // wtp -> ac
    EMPOWER_PT_AUTH_RESPONSE = 0x08,                // ac -> wtp

	EMPOWER_PT_ASSOC_REQUEST = 0x09,                // wtp -> ac
    EMPOWER_PT_ASSOC_RESPONSE = 0x0A,               // ac -> wtp

    // lvap messages
    EMPOWER_PT_ADD_LVAP = 0x0B,                     // ac -> wtp
    EMPOWER_PT_ADD_LVAP_RESPONSE = 0x0C,            // ac -> wtp
	EMPOWER_PT_DEL_LVAP = 0x0D,                     // ac -> wtp
	EMPOWER_PT_DEL_LVAP_RESPONSE = 0x0E,            // ac -> wtp
	EMPOWER_PT_STATUS_LVAP = 0x0F,                  // wtp -> ac
    EMPOWER_PT_LVAP_STATUS_REQ = 0x10,              // ac -> wtp

    // vap
    EMPOWER_PT_ADD_VAP = 0x11,                      // ac -> wtp
    EMPOWER_PT_DEL_VAP = 0x12,                      // ac -> wtp
    EMPOWER_PT_STATUS_VAP = 0x13,                   // wtp -> ac
    EMPOWER_PT_VAP_STATUS_REQ = 0x14,               // ac -> wtp

    // TX Policies
    EMPOWER_PT_SET_PORT= 0x15,                      // ac -> wtp
    EMPOWER_PT_DEL_PORT= 0x16,                      // ac -> wtp
    EMPOWER_PT_STATUS_PORT = 0x17,                  // wtp -> ac
    EMPOWER_PT_PORT_STATUS_REQ = 0x18,              // ac -> wtp

    // Slice messages
    EMPOWER_PT_SET_SLICE = 0x19,             		// ac -> wtp
    EMPOWER_PT_DEL_SLICE = 0x1A,             		// ac -> wtp
    EMPOWER_PT_STATUS_SLICE = 0x1B,          		// wtp -> ac
    EMPOWER_PT_SLICE_STATUS_REQ = 0x1C,      		// ac -> wtp

	/* Workers 0x40 - 0x7F */

    // Channel Quality Maps
    EMPOWER_PT_UCQM_REQUEST = 0x40,                 // ac -> wtp
    EMPOWER_PT_UCQM_RESPONSE = 0x41,                // wtp -> ac
    EMPOWER_PT_NCQM_REQUEST = 0x42,                 // ac -> wtp
    EMPOWER_PT_NCQM_RESPONSE = 0x43,                // wtp -> ac

    // wifi stats
    EMPOWER_PT_WIFI_STATS_REQUEST = 0x4A,           // ac -> wtp
    EMPOWER_PT_WIFI_STATS_RESPONSE = 0x4B,          // wtp -> ac

    // Slice Stats
    EMPOWER_PT_SLICE_STATS_REQUEST = 0x4C,   		// ac -> wtp
    EMPOWER_PT_SLICE_STATS_RESPONSE = 0x4D,  		// wtp -> ac

	/* Primitives 0x80 - 0xCF*/

    // Link Stats
    EMPOWER_PT_LVAP_STATS_REQUEST = 0x80,           // ac -> wtp
    EMPOWER_PT_LVAP_STATS_RESPONSE = 0x81,          // wtp -> ac

    // Packet/Bytes counters
    EMPOWER_PT_COUNTERS_REQUEST = 0x82,             // ac -> wtp
    EMPOWER_PT_COUNTERS_RESPONSE = 0x83,            // wtp -> ac

	// TXP Packet/Bytes counters
    EMPOWER_PT_TXP_COUNTERS_REQUEST = 0x84,         // ac -> wtp
    EMPOWER_PT_TXP_COUNTERS_RESPONSE = 0x85,        // wtp -> ac

    // Triggers
    EMPOWER_PT_ADD_RSSI_TRIGGER = 0x86,             // ac -> wtp
    EMPOWER_PT_RSSI_TRIGGER = 0x87,                 // ac -> wtp
    EMPOWER_PT_DEL_RSSI_TRIGGER = 0x89,             // ac -> wtp

	// Packet Summaries
    EMPOWER_PT_ADD_SUMMARY_TRIGGER = 0x8A,          // ac -> wtp
    EMPOWER_PT_SUMMARY_TRIGGER = 0x8B,              // ac -> wtp
    EMPOWER_PT_DEL_SUMMARY_TRIGGER = 0x8C,          // ac -> wtp

	/* Extra 0xE0 - FF */

    // IGMP messages
    EMPOWER_PT_IGMP_REPORT = 0xE0,                  // wtp -> ac
    EMPOWER_PT_INCOMING_MCAST_ADDRESS = 0xE1,       // wtp -> ac
};

/* header format, common to all messages */
struct empower_header {
  private:
    uint8_t  _version;  /* see protocol version */
    uint8_t  _type;     /* see protocol type */
    uint32_t _length;   /* including this header */
    uint32_t _seq;      /* sequence number */
    uint32_t _xid;      /* sequence number */
    uint8_t  _wtp[6];   /* EtherAddress */

  public:
    uint8_t      version()                   { return _version; }
    uint8_t      type()                      { return _type; }
    uint32_t     length()                    { return ntohl(_length); }
    uint32_t     seq()                       { return ntohl(_seq); }
    uint32_t     xid()                       { return ntohl(_xid); }
    EtherAddress wtp()                       { return EtherAddress(_wtp); }
    void     set_seq(uint32_t seq)           { _seq = htonl(seq); }
    void     set_version(uint8_t version)    { _version = version; }
    void     set_type(uint8_t type)          { _type = type; }
    void     set_length(uint32_t length)     { _length = htonl(length); }
    void     set_xid(uint32_t xid)           { _xid = htonl(xid); }
    void     set_wtp(EtherAddress wtp)       { memcpy(_wtp, wtp.data(), 6); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* hello request packet format */
typedef struct empower_header empower_hello_request;

/* hello response packet format */
typedef struct empower_header empower_hello_response;

/* probe request packet format */
struct empower_probe_request : public empower_header {
  private:
    uint32_t _iface_id; 				/* Interface id (int) */
    uint8_t _sta[6];                    /* EtherAddress */
    uint8_t _flags;            			/* Flags (empower_probe_assoc_flags) */
    uint16_t _ht_caps_info;				/* HT capabilities */
    char    _ssid[WIFI_NWID_MAXSIZE+1]; /* Null terminated SSID */
  public:
    void set_sta(EtherAddress sta) 					{ memcpy(_sta, sta.data(), 6); }
    void set_flag(uint16_t f)            			{ _flags = _flags | f; }
    void set_ht_caps_info(uint16_t ht_caps_info) 	{ _ht_caps_info = htons(ht_caps_info); }
    void set_ssid(String ssid) 						{ memset(_ssid, 0, WIFI_NWID_MAXSIZE+1); memcpy(_ssid, ssid.data(), ssid.length()); }
    void set_iface_id(uint32_t iface_id) 			{ _iface_id = htonl(iface_id); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* probe response packet format */
struct empower_probe_response : public empower_header {
  private:
    uint8_t _sta[6];                 /* EtherAddress */
    char _ssid[WIFI_NWID_MAXSIZE+1]; /* Null terminated SSID */
  public:
    EtherAddress sta()  { return EtherAddress(_sta); }
    String ssid()       { return String((char *) _ssid); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* auth request packet format */
struct empower_auth_request : public empower_header {
  private:
    uint8_t _sta[6];    /* EtherAddress */
    uint8_t _bssid[6];  /* EtherAddress */
  public:
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
    uint8_t _sta[6];    				/* EtherAddress */
    uint8_t _flags;     				/* Flags (empower_probe_assoc_flags) */
    uint16_t _ht_caps_info;				/* HT capabilities */
    uint8_t _bssid[6];  				/* EtherAddress */
    char _ssid[WIFI_NWID_MAXSIZE+1];    /* Null terminated SSID */
  public:
    void set_sta(EtherAddress sta)    	 			{ memcpy(_sta, sta.data(), 6); }
    void set_ht_caps_info(uint16_t ht_caps_info) 	{ _ht_caps_info = htons(ht_caps_info); }
    void set_flag(uint16_t f)            			{ _flags = _flags | f; }
    void set_bssid(EtherAddress bssid) 				{ memcpy(_bssid, bssid.data(), 6); }
    void set_ssid(String ssid)         				{ memset(_ssid, 0, WIFI_NWID_MAXSIZE+1); memcpy(_ssid, ssid.data(), ssid.length()); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* association response packet format */
struct empower_assoc_response : public empower_header {
  private:
    uint8_t _sta[6]; /* EtherAddress */
  public:
    EtherAddress sta() { return EtherAddress(_sta); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* caps request packet format */
typedef empower_header empower_caps_request;

/* caps response packet format */
struct empower_caps_response : public empower_header {
  private:
    uint8_t _nb_resources_elements;   /* Int */
  public:
    void set_nb_resources_elements(uint8_t nb_resources_elements) { _nb_resources_elements = nb_resources_elements; }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* resource element entry format */
struct resource_elements_entry {
  private:
    uint8_t  _iface_id;     /* The interface id (Int) */
    uint8_t  _hwaddr[6];    /* EtherAddress */
    uint8_t  _channel;      /* WiFi Channel (Int) */
    uint8_t  _band;         /* WiFi band (empower_bands_types) */
  public:
    void set_hwaddr(EtherAddress hwaddr) { memcpy(_hwaddr, hwaddr.data(), 6); }
    void set_band(uint8_t band)          { _band = band; }
    void set_channel(uint8_t channel)    { _channel = channel; }
    void set_iface_id(uint8_t iface_id)  { _iface_id = iface_id; }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* link stats request packet format */
struct empower_lvap_stats_request : public empower_header {
  private:
    uint8_t  _sta[6];     /* EtherAddress */
  public:
    EtherAddress sta()           { return EtherAddress(_sta); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* link stats response packet format */
struct empower_lvap_stats_response : public empower_header {
  private:
    uint32_t _iface_id; 	/* sequence number */
    uint8_t  _sta[6];     	/* EtherAddress */
    uint16_t _nb_entries; 	/* Int */
  public:
    void set_iface_id(uint32_t iface_id) { _iface_id = htonl(iface_id); }
    void set_sta(EtherAddress sta) { memcpy(_sta, sta.data(), 6); }
    void set_nb_entries(uint16_t nb_entries)       { _nb_entries = htons(nb_entries); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* lvap_stats entry format */
struct lvap_stats_entry {
  private:
    uint8_t  _rate;     /* Rate in units of 500kbps or MCS index */
    uint32_t _prob;     /* Probability [0-18000] */
    uint32_t _cur_prob; /* Probability [0-18000] */
  public:
    void set_rate(uint8_t rate)          { _rate = rate; }
    void set_prob(uint32_t prob)         { _prob = htonl(prob); }
    void set_cur_prob(uint32_t cur_prob) { _cur_prob = htonl(cur_prob); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* wifi stats request packet format */
struct empower_wifi_stats_request : public empower_header {
  private:
    uint32_t _iface_id; /* sequence number */
  public:
    uint32_t iface_id() { return ntohl(_iface_id); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* wifi stats entry format */
struct wifi_stats_entry {
  private:
      uint8_t  _type;       /* EtherAddress */
      uint64_t _timestamp;  /* Timestamp in microseconds (int) */
      uint32_t _sample;     /* Std RSSI during last window in dBm (int) */
  public:
    void set_type(uint8_t type)                         { _type = type; }
    void set_sample(uint32_t sample)                    { _sample = htonl(sample); }
    void set_timestamp(uint64_t timestamp)              { _timestamp = htobe64(timestamp); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* wifi stats response packet format */
struct empower_wifi_stats_response : public empower_header {
  private:
    uint32_t _iface_id; /* sequence number */
    uint16_t _nb_entries;     /* Int */
  public:
    void set_iface_id(uint32_t iface_id)     { _iface_id = htonl(iface_id); }
    void set_nb_entries(uint16_t nb_entries) { _nb_entries = htons(nb_entries); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* channel quality map request packet format */
struct empower_cqm_request : public empower_header {
  private:
    uint32_t _iface_id; /* sequence number */
  public:
    uint32_t iface_id() { return ntohl(_iface_id); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* channel quality map entry format */
struct cqm_entry {
  private:
    uint8_t  _addr[6];       /* EtherAddress */
    uint8_t  _last_rssi_std; /* Std RSSI during last window in dBm (int) */
    int8_t   _last_rssi_avg; /* Std RSSI during last window in dBm (int) */
    uint32_t _last_packets;  /* Number of frames in last window (int) */
    uint32_t _hist_packets;  /* Total number of frames (int) */
    int8_t   _mov_rssi;      /* Moving RSSI in dBm (int) */
  public:
    void set_addr(EtherAddress addr)                { memcpy(_addr, addr.data(), 6); }
    void set_last_rssi_std(uint8_t last_rssi_std)   { _last_rssi_std = last_rssi_std; }
    void set_last_rssi_avg(int8_t last_rssi_avg)    { _last_rssi_avg = last_rssi_avg; }
    void set_last_packets(uint32_t last_packets)    { _last_packets = htonl(last_packets); }
    void set_hist_packets(uint32_t hist_packets)    { _hist_packets = htonl(hist_packets); }
    void set_mov_rssi(int8_t mov_rssi)              { _mov_rssi = mov_rssi; }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* channel quality map response packet format */
struct empower_cqm_response : public empower_header {
  private:
    uint32_t _iface_id; /* sequence number */
    uint16_t _nb_entries;     /* Int */
  public:
    void set_iface_id(uint32_t iface_id) { _iface_id = htonl(iface_id); }
    void set_nb_entries(uint16_t nb_entries) { _nb_entries = htons(nb_entries); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* counters request packet format */
  struct empower_counters_request : public empower_header {
  private:
    uint8_t  _sta[6];        /* EtherAddress */
  public:
    EtherAddress sta()     { return EtherAddress(_sta); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* counters response packet format */
struct empower_counters_response : public empower_header {
  private:
    uint8_t  _sta[6];         /* EtherAddress */
    uint16_t _nb_tx;          /* Int */
    uint16_t _nb_rx;          /* Int */
  public:
    void set_sta(EtherAddress sta) { memcpy(_sta, sta.data(), 6); }
    void set_nb_tx(uint16_t nb_tx) { _nb_tx = htons(nb_tx); }
    void set_nb_rx(uint16_t nb_rx) { _nb_rx = htons(nb_rx); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* txp counters request packet format */
struct empower_txp_counters_request : public empower_header {
  private:
    uint32_t _iface_id; /* sequence number */
    uint8_t  _mcast[6];       /* EtherAddress */
  public:
    uint32_t iface_id()   { return ntohl(_iface_id); }
    EtherAddress mcast()  { return EtherAddress(_mcast); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* txp counters response packet format */
struct empower_txp_counters_response : public empower_header {
  private:
    uint32_t _iface_id;       /* sequence number */
    uint8_t  _mcast[6];       /* EtherAddress */
    uint16_t _nb_tx;          /* Int */
  public:
    void set_iface_id(uint32_t iface_id) { _iface_id = htonl(iface_id); }
    void set_mcast(EtherAddress mcast) { memcpy(_mcast, mcast.data(), 6); }
    void set_nb_tx(uint16_t nb_tx) { _nb_tx = htons(nb_tx); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* counters entry format */
struct counters_entry {
  private:
    uint16_t _size;     /* Frame size in bytes (int) */
    uint32_t _count;    /* Number of frames (int) */
  public:
    void set_size(uint16_t size)   { _size = htons(size); }
    void set_count(uint32_t count) { _count = htonl(count); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* SSID entry */
struct ssid_entry {
  private:
    uint8_t      _bssid[6];                     /* EtherAddress */
    char         _ssid[WIFI_NWID_MAXSIZE+1];    /* Null terminated SSID */
  public:
    EtherAddress bssid()                { return EtherAddress(_bssid); }
    String ssid()                       { return String((char *) _ssid); }
    void set_bssid(EtherAddress bssid)  { memcpy(_bssid, bssid.data(), 6); }
    void set_ssid(String ssid)          { memset(_ssid, 0, WIFI_NWID_MAXSIZE+1); memcpy(_ssid, ssid.data(), ssid.length()); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* add lvap packet format */
struct empower_add_lvap : public empower_header {
  private:
    uint32_t     _iface_id; /* sequence number */
    uint8_t      _flags;            /* Flags (empower_lvap_flags) */
    uint16_t     _assoc_id;         /* Association id */
    uint16_t 	 _ht_caps_info;		/* HT capabilities */
    uint8_t      _sta[6];           /* EtherAddress */
    uint8_t      _encap[6];         /* EtherAddress */
    uint8_t      _bssid[6];         /* EtherAddress */
    char         _ssid[WIFI_NWID_MAXSIZE+1]; /* Null terminated SSID */
  public:
    uint32_t     iface_id()         { return ntohl(_iface_id); }
    bool         flag(int f)        { return _flags & f; }
    uint16_t     assoc_id()         { return ntohs(_assoc_id); }
    uint16_t     ht_caps_info()     { return ntohs(_ht_caps_info); }
    EtherAddress sta()              { return EtherAddress(_sta); }
    EtherAddress encap()            { return EtherAddress(_encap); }
    EtherAddress bssid()            { return EtherAddress(_bssid); }
    String       ssid()             { return String((char *) _ssid); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* del lvap packet format */
struct empower_del_lvap : public empower_header {
  private:
    uint8_t _sta[6];              /* EtherAddress */
    uint8_t _csa_switch_mode;
    uint8_t _csa_switch_count;
    uint8_t _csa_switch_channel;  /* WiFi channel (int) */
  public:
    EtherAddress sta()              { return EtherAddress(_sta); }
    uint8_t csa_switch_mode()       { return _csa_switch_mode; }
    uint8_t csa_switch_count()      { return _csa_switch_count; }
    uint8_t csa_switch_channel()    { return _csa_switch_channel; }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* lvap add/del response packet format */
struct empower_add_del_lvap_response : public empower_header {
  private:
    uint8_t  _sta[6];           /* EtherAddress */
    uint32_t _status;           /* Status code */
  public:
    void set_sta(EtherAddress sta)          { memcpy(_sta, sta.data(), 6); }
    void set_status(uint32_t status)        { _status = htonl(status); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* lvap status packet format */
struct empower_status_lvap : public empower_header {
  private:
    uint32_t     _iface_id;         /* sequence number */
    uint8_t      _flags;            /* Flags (empower_lvap_flags) */
    uint16_t     _assoc_id;         /* Association id */
    uint16_t 	 _ht_caps_info;	    /* HT capabilities */
    uint8_t      _sta[6];           /* EtherAddress */
    uint8_t      _encap[6];         /* EtherAddress */
    uint8_t      _bssid[6];         /* EtherAddress */
    char         _ssid[WIFI_NWID_MAXSIZE+1]; /* Null terminated SSID */
  public:
    void set_flag(uint16_t f)            		 { _flags = _flags | f; }
    void set_assoc_id(uint16_t assoc_id) 		 { _assoc_id = htons(assoc_id); }
    void set_ht_caps_info(uint16_t ht_caps_info) { _ht_caps_info = htons(ht_caps_info); }
    void set_sta(EtherAddress sta)       		 { memcpy(_sta, sta.data(), 6); }
    void set_encap(EtherAddress encap)   		 { memcpy(_encap, encap.data(), 6); }
    void set_bssid(EtherAddress bssid)   		 { memcpy(_bssid, bssid.data(), 6); }
    void set_ssid(String ssid)           		 { memcpy(&_ssid, ssid.data(), ssid.length()); }
    void set_iface_id(uint32_t iface_id) 		 { _iface_id = htonl(iface_id); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* set port packet format */
struct empower_set_port : public empower_header {
  private:
    uint32_t _iface_id;       /* sequence number */
    uint8_t  _flags;          /* Flags (empower_port_flags) */
    uint8_t  _sta[6];         /* EtherAddress */
    uint16_t _rts_cts;        /* RTS/CTS threshold */
    uint16_t _max_amsdu_len;  /* RTS/CTS threshold */
    uint8_t  _tx_mcast;       /* multicast mode (tx_mcast_type) */
    uint8_t  _ur_mcast_count; /* Number of unsolicited replies (int) */
    uint8_t  _nb_mcs;         /* Number of rate entries (int) */
    uint8_t  _nb_ht_mcs;      /* Number of HT rate entries (int) */
    uint8_t  *mcs[];          /* Rates in units of 500kbps or MCS index */
    uint8_t  *ht_mcs[];       /* HT Rate entries as MCS index */
  public:
    uint32_t iface_id()                 { return ntohl(_iface_id); }
    bool flag(int f)                    { return _flags & f;  }
    EtherAddress addr()                 { return EtherAddress(_sta); }
    uint16_t rts_cts()                  { return ntohs(_rts_cts); }
    uint16_t max_amsdu_len()            { return ntohs(_max_amsdu_len); }
    empower_tx_mcast_type tx_mcast()    { return empower_tx_mcast_type(_tx_mcast); }
    uint8_t ur_mcast_count()            { return _ur_mcast_count; }
    uint8_t nb_mcs()                    { return _nb_mcs; }
    uint8_t nb_ht_mcs()                 { return _nb_ht_mcs; }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* set port packet format */
struct empower_del_port : public empower_header {
  private:
    uint32_t _iface_id; /* sequence number */
    uint8_t  _addr[6];           /* EtherAddress */
  public:
    uint32_t     iface_id()             { return ntohl(_iface_id); }
    EtherAddress addr()                 { return EtherAddress(_addr); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* lvap status packet format */
struct empower_status_port : public empower_header {
  private:
    uint32_t _iface_id; /* sequence number */
    uint8_t  _flags;            /* Flags (empower_port_flags) */
    uint8_t  _sta[6];           /* EtherAddress */
    uint16_t _rts_cts;          /* RTS/CTS threshold */
    uint16_t _max_amsdu_len;    /* RTS/CTS threshold */
    uint8_t  _tx_mcast;         /* multicast mode (tx_mcast_type) */
    uint8_t  _ur_mcast_count;   /* Number of unsolicited replies (int) */
    uint8_t  _nb_mcs;           /* Number of rate entries (int) */
    uint8_t  _nb_ht_mcs;        /* Number of HT rate entries (int) */
    uint8_t  *mcs[];            /* Rate entries in units of 500kbps or MCS index */
    uint8_t  *ht_mcs[];         /* HT Rate entries as MCS index */
  public:
    void set_flag(uint16_t f)                           { _flags = _flags | f; }
    void set_sta(EtherAddress sta)                      { memcpy(_sta, sta.data(), 6); }
    void set_rts_cts(uint16_t rts_cts)                  { _rts_cts = htons(rts_cts); }
    void set_max_amsdu_len(uint16_t max_amsdu_len)      { _max_amsdu_len = htons(max_amsdu_len); }
    void set_tx_mcast(empower_tx_mcast_type tx_mcast)   { _tx_mcast = uint8_t(tx_mcast); }
    void set_nb_mcs(uint8_t nb_mcs)                     { _nb_mcs = nb_mcs; }
    void set_nb_ht_mcs(uint8_t nb_ht_mcs)               { _nb_ht_mcs = nb_ht_mcs; }
    void set_ur_mcast_count(uint8_t ur_mcast_count)     { _ur_mcast_count = ur_mcast_count; }
    void set_iface_id(uint32_t iface_id) { _iface_id = htonl(iface_id); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* add rssi trigger packet format */
struct empower_add_rssi_trigger: public empower_header {
  private:
    uint8_t  _sta[6];       /* EtherAddress */
    uint8_t  _relation; /* Relation (relation_t) */
    int8_t   _value;        /* RSSI value in dBm (int) */
    uint16_t _period;       /* Reporting period in ms (int) */
  public:
    EtherAddress sta()    { return EtherAddress(_sta); }
    uint8_t relation()    { return _relation; }
    int8_t value()        { return _value; }
    uint16_t period()     { return ntohs(_period); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* del rssi trigger packet format */
typedef empower_header empower_del_rssi_trigger;

/* rssi trigger packet format */
struct empower_rssi_trigger: public empower_header {
  private:
    uint32_t _iface_id; /* sequence number */
    int8_t  _current;       /* RSSI value in dBm (int) */
  public:
    void set_current(int8_t current)     { _current = current; }
    void set_iface_id(uint32_t iface_id) { _iface_id = htonl(iface_id); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* add summary packet format */
struct empower_add_summary_trigger: public empower_header {
  private:
    uint32_t _iface_id; /* sequence number */
    uint8_t  _addr[6];  /* EtherAddress */
    int16_t  _limit;    /* Number of reports to be sent, -1 forever (int) */
    uint16_t _period;   /* Reporting period in ms (int) */
  public:
    uint32_t iface_id()   { return ntohl(_iface_id); }
    EtherAddress addr()   { return EtherAddress(_addr); }
    int16_t limit()       { return ntohs(_limit); }
    uint16_t period()     { return ntohs(_period); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* summary packet format */
struct empower_summary_trigger: public empower_header {
  private:
    uint32_t _iface_id; /* sequence number */
    uint8_t  _addr[6]; /* EtherAddress */
    int16_t  _limit;   /* Number of reports to be sent, -1 forever (int) */
    uint16_t _period;  /* Reporting period in ms (int) */
    uint16_t _nb_entries;       /* Number of frames (int) */
  public:
    void set_iface_id(uint32_t iface_id) { _iface_id = htonl(iface_id); }
    void set_period(uint16_t period)  { _period = htons(period); }
    void set_limit(uint16_t limit)  { _limit = htons(limit); }
    void set_addr(EtherAddress addr) { memcpy(_addr, addr.data(), 6); }
    void set_nb_frames(uint16_t nb_entries)  { _nb_entries = htons(nb_entries); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* summary entry format */
struct summary_entry {
  private:
    uint8_t  _ra[6];    /* Receiver address (EtherAddress) */
    uint8_t  _ta[6];    /* Transmitter address (EtherAddress) */
    uint64_t _tsft;     /* Timestamp in microseconds (int) */
    uint16_t _seq;      /* Sequence number */
    int8_t   _rssi;     /* RSSI in dBm (int) */
    uint8_t  _rate;     /* Rate in units of 500kbps or MCS index */
    uint8_t  _type;     /* WiFi frame type */
    uint8_t  _subtype;  /* WiFi frame sub-type */
    uint32_t _length;   /* Frame length in bytes (int) */
  public:
    void set_tsft(uint64_t tsft)        { _tsft = htobe64(tsft); }
    void set_seq(int16_t seq)           { _seq = htons(seq); }
    void set_rssi(int8_t rssi)          { _rssi = rssi; }
    void set_rate(uint8_t rate)         { _rate = rate; }
    void set_type(uint8_t type)         { _type = type; }
    void set_subtype(uint8_t subtype)   { _subtype = subtype; }
    void set_length(uint32_t length)    { _length= htonl(length); }
    void set_ra(EtherAddress ra)        { memcpy(_ra, ra.data(), 6); }
    void set_ta(EtherAddress ta)        { memcpy(_ta, ta.data(), 6); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* del summary packet format */
typedef empower_header empower_del_summary_trigger;

/* add vap packet format */
struct empower_add_vap : public empower_header {
  private:
    uint32_t _iface_id; /* sequence number */
    uint8_t _bssid[6];      /* EtherAddress */
    char _ssid[WIFI_NWID_MAXSIZE+1];    /* Null terminated SSID */
  public:
    uint32_t     iface_id()                  { return ntohl(_iface_id); }
    EtherAddress bssid()     { return EtherAddress(_bssid); }
    String       ssid()      { return String((char *) _ssid); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* del vap packet format */
struct empower_del_vap : public empower_header {
  private:
    uint8_t _bssid[6]; /* EtherAddress */
  public:
    EtherAddress bssid()   { return EtherAddress(_bssid); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* lvap status packet format */
struct empower_status_vap : public empower_header {
  private:
    uint32_t _iface_id; /* sequence number */
    uint8_t _bssid[6];                   /* EtherAddress */
    char    _ssid[WIFI_NWID_MAXSIZE+1];  /* Null terminated SSID */
  public:
    void set_bssid(EtherAddress bssid)   { memcpy(_bssid, bssid.data(), 6); }
    void set_ssid(String ssid)           { memset(_ssid, 0, WIFI_NWID_MAXSIZE+1); memcpy(_ssid, ssid.data(), ssid.length()); }
    void set_iface_id(uint32_t iface_id) { _iface_id = htonl(iface_id); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* incomming multicast address request format */
struct empower_incoming_mcast_address : public empower_header {
  private:
    uint32_t _iface_id; /* sequence number */
    uint8_t _mcast_addr[6];    /* EtherAddress */
  public:
    void set_iface_id(uint32_t iface_id) { _iface_id = htonl(iface_id); }
    void set_mcast_addr(EtherAddress mcast_addr) { memcpy(_mcast_addr, mcast_addr.data(), 6); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

struct empower_igmp_report : public empower_header {
  private:
    uint8_t  _sta [6];          /* EtherAddress */
    uint8_t  _mcast_addr[4];    /* IPAddress */
    uint8_t  _igmp_type;        /* IGMP record type */
  public:
    void set_sta(EtherAddress sta)            { memcpy(_sta, sta.data(), 6); }
    void set_mcast_addr(IPAddress mcast_addr) { memcpy(_mcast_addr, mcast_addr.data(), 4); }
    void set_igmp_type(int igmp_type)         { _igmp_type = igmp_type; }
} CLICK_SIZE_PACKED_ATTRIBUTE;

struct empower_set_slice : public empower_header {
  private:
    uint32_t    _iface_id; 					/* sequence number */
    uint8_t     _dscp;          			/* Traffic DSCP (int) */
    uint8_t    	_scheduler;     			/* The type of scheduler (int) */
    uint8_t     _flags;         			/* Flags (empower_slice_flags) */
    uint32_t    _quantum;       			/* Priority of the slice (int) */
    char        _ssid[WIFI_NWID_MAXSIZE+1];	/* Null terminated SSID */
  public:
    uint32_t     iface_id()             { return ntohl(_iface_id); }
    uint8_t      dscp()          		{ return _dscp; }
    uint8_t      scheduler()     		{ return _scheduler; }
    bool         flags(int f)    		{ return _flags & f; }
    uint32_t     quantum()       		{ return ntohl(_quantum); }
    String       ssid()          		{ return String((char *) _ssid); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

struct empower_del_slice : public empower_header {
  private:
    uint32_t 	_iface_id; 					/* sequence number */
    uint8_t     _dscp;                      /* Traffic DSCP (int) */
    char        _ssid[WIFI_NWID_MAXSIZE+1]; /* Null terminated SSID */
  public:
    uint32_t     iface_id()                 { return ntohl(_iface_id); }
    uint8_t      dscp()          			{ return _dscp; }
    String       ssid()          			{ return String((char *) _ssid); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* slice status packet format */
struct empower_status_slice : public empower_header {
  private:
    uint32_t    _iface_id; 					/* sequence number */
    uint8_t     _dscp;          			/* Traffic DSCP (int) */
    uint8_t    	_scheduler;     			/* The type of scheduler (int) */
    uint8_t     _flags;         			/* Flags (empower_slice_flags) */
    uint32_t    _quantum;       			/* Priority of the slice (int) */
    char        _ssid[WIFI_NWID_MAXSIZE+1];	/* Null terminated SSID */
  public:
    void set_iface_id(uint32_t iface_id)        				{ _iface_id = htonl(iface_id); }
    void set_dscp(uint8_t dscp)                 				{ _dscp = dscp; }
    void set_scheduler(uint8_t scheduler)      					{ _scheduler = scheduler; }
    void set_flag(uint16_t f)                  					{ _flags = _flags | f; }
    void set_quantum(uint32_t quantum)          				{ _quantum = htonl(quantum); }
    void set_ssid(String ssid)                  				{ memset(_ssid, 0, WIFI_NWID_MAXSIZE+1); memcpy(_ssid, ssid.data(), ssid.length()); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* slice stats request packet format */
struct empower_slice_stats_request : public empower_header {
  private:
    char    _ssid[WIFI_NWID_MAXSIZE+1]; /* Null terminated SSID */
	uint8_t _dscp;  					/* Traffic DSCP (int) */
  public:
    String   ssid()          		{ return String((char *) _ssid); }
    uint32_t dscp() 				{ return _dscp; }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* slice queue status packet format */
struct empower_slice_stats_entry {
  private:
    uint32_t 	_iface_id; 						/* Interface id (int) */
    uint32_t    _deficit_used;      			/* Total deficit used by this queue */
    uint32_t    _max_queue_length;  			/* Maximum queue length reached */
    uint32_t    _tx_packets;        			/* Int */
    uint32_t    _tx_bytes;          			/* Int */
  public:
    void set_iface_id(uint32_t iface_id)            		{ _iface_id = htonl(iface_id); }
    void set_deficit_used(uint32_t deficit_used)            { _deficit_used = htonl(deficit_used); }
    void set_max_queue_length(uint32_t max_queue_length)    { _max_queue_length = htonl(max_queue_length); }
    void set_tx_packets(uint32_t tx_packets)                { _tx_packets = htonl(tx_packets); }
    void set_tx_bytes(uint32_t tx_bytes)                    { _tx_bytes = htonl(tx_bytes); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* slice stats response packet format */
struct empower_slice_stats_response : public empower_header {
  private:
    char    	_ssid[WIFI_NWID_MAXSIZE+1];		/* Null terminated SSID */
    uint8_t  	_dscp;                      	/* Traffic DSCP (int) */
    uint16_t 	_nb_entries; 					/* Int */
  public:
    void set_dscp(uint8_t dscp) 					{ _dscp = dscp; }
    void set_ssid(String ssid) 						{ memset(_ssid, 0, WIFI_NWID_MAXSIZE+1); memcpy(_ssid, ssid.data(), ssid.length()); }
    void set_nb_entries(uint16_t nb_entries)       	{ _nb_entries = htons(nb_entries); }
} CLICK_SIZE_PACKED_ATTRIBUTE;

CLICK_ENDDECLS
#endif /* CLICK_EMPOWERPACKET_HH */
