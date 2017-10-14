#ifndef CLICK_EMPOWERLVAPMANAGER_HH
#define CLICK_EMPOWERLVAPMANAGER_HH
#include <click/config.h>
#include <click/element.hh>
#include <click/timer.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/hashtable.hh>
#include <clicknet/wifi.h>
#include <click/sync.hh>
#include <elements/wifi/minstrel.hh>
#include "empowerrxstats.hh"
#include "empowerpacket.hh"
#include "igmppacket.hh"
#include "empowermulticasttable.hh"
CLICK_DECLS

/*
=c

EmpowerLVAPManager(HWADDR, EBS, DEBUGFS[, I<KEYWORDS>])

=s EmPOWER

Handles LVAPs and communication with the Access Controller.

=d

Keyword arguments are:

=over 8

=item HWADDR
The raw wireless interface hardware address

=item EBS
An EmpowerBeaconSource element

=item EAUTHR
An EmpowerOpenAuthResponder element

=item EASSOR
An EmpowerAssociationResponder element

=item DEBUGFS
The path to the bssid_extra file

=item EPSB
An EmpowerPowerSaveBuffer element

=item PERIOD
Interval between hello messages to the Access Controller (in msec), default is 5000

=item EDEAUTHR
An EmpowerDeAuthResponder element

=item EDISASSOR
An EmpowerDisassocResponder element

=item DEBUG
Turn debug on/off

=back 8

=a EmpowerLVAPManager
*/

enum empower_port_flags {
    EMPOWER_STATUS_PORT_NOACK = (1<<0),
};

enum empower_lvap_flags {
    EMPOWER_STATUS_LVAP_AUTHENTICATED = (1<<0),
    EMPOWER_STATUS_LVAP_ASSOCIATED = (1<<1),
    EMPOWER_STATUS_LVAP_SET_MASK = (1<<2),
};

enum empower_bands_types {
    EMPOWER_BT_L20 = 0x0,
    EMPOWER_BT_HT20 = 0x1,
};

typedef HashTable<uint16_t, uint32_t> CBytes;
typedef CBytes::iterator CBytesIter;

class Minstrel;

class NetworkPort {
public:
	EtherAddress _hwaddr;
	String _iface;
	uint16_t _port_id;
	NetworkPort() :
			_hwaddr(EtherAddress()), _port_id(0) {
	}
	NetworkPort(EtherAddress hwaddr, String iface, uint16_t port_id) :
			_hwaddr(hwaddr), _iface(iface), _port_id(port_id) {
	}
	String unparse() {
		StringAccum sa;
		sa << _hwaddr.unparse() << ' ' << _iface << ' ' << _port_id;
		return sa.take_string();
	}
};

// An EmPOWER Virtual Access Point or VAP. This is an AP than
// can be used by multiple clients (unlike the LVAP that is
// specific to each client.
// The _net_bssid is write only and is used to set the bssid
// mask, meaning that the monitor interface must ACK frames
// addressed to this VAP
class EmpowerVAPState {
public:
	EtherAddress _net_bssid;
	String _ssid;
	EtherAddress _hwaddr;
	int _channel;
	int _band;
	int _iface_id;
};

// An EmPOWER Light Virtual Access Point or LVAP. This is an
// AP that is defined for a specific client. The net_bssid is
// generated from a controller-specific prefix plus the last
// three bytes of the station address. The net_bssid is write
// only and is used to set the bssid mask. Meaning that frame
// addressed to net_bssid must be acked by this WTP.
// The lvap_bssid is the one to which the client is currently
// attached. The lvap_bssid can be modified only as the result
// of auth request and assoc request messages. Probe request
// cannot modify the lvap_bssid.
class EmpowerStationState {
public:
	EtherAddress _sta;
	EtherAddress _net_bssid;
	EtherAddress _lvap_bssid;
	EtherAddress _encap;
	Vector<String> _ssids;
	String _ssid;
	int _assoc_id;
	EtherAddress _hwaddr;
	int _channel;
	empower_bands_types _band;
	empower_bands_types _supported_band;
	empower_bands_types _target_band;
	int _iface_id;
	bool _set_mask;
	bool _authentication_status;
	bool _association_status;
	// CSA entries
	bool _csa_active;
	int _csa_switch_mode;
	int _csa_switch_count;
	EtherAddress _target_hwaddr;
	int _target_channel;
	// ADD/DEL LVAP response entries
	uint32_t _add_lvap_module_id;
	uint32_t _del_lvap_module_id;
};

// Cross structure mapping bssids to list of associated
// station and to the interface id
class InfoBssid {
public:
	EtherAddress _bssid;
	Vector<EtherAddress> _stas;
	int _iface_id;
};

typedef HashTable<EtherAddress, InfoBssid> InfoBssids;
typedef InfoBssids::iterator IBIter;

typedef HashTable<EtherAddress, EmpowerVAPState> VAP;
typedef VAP::iterator VAPIter;

typedef HashTable<EtherAddress, EmpowerStationState> LVAP;
typedef LVAP::iterator LVAPIter;

typedef HashTable<int, NetworkPort> Ports;
typedef Ports::iterator PortsIter;

class ResourceElement {
public:

	EtherAddress _hwaddr;
	int _channel;
	empower_bands_types _band;

	ResourceElement() :
			_hwaddr(EtherAddress()), _channel(1), _band(EMPOWER_BT_L20) {
	}

	ResourceElement(EtherAddress hwaddr, int channel, empower_bands_types band) :
			_hwaddr(hwaddr), _channel(channel), _band(band) {
	}

	inline size_t hashcode() const {
		return _hwaddr.hashcode();
	}

	inline String unparse() const {
		StringAccum sa;
		sa << "(";
		sa << _hwaddr.unparse();
		sa << ", ";
		sa << _channel;
		sa << ", ";
		switch (_band) {
		case EMPOWER_BT_L20:
			sa << "L20";
			break;
		case EMPOWER_BT_HT20:
			sa << "HT20";
			break;
		}
		sa << ")";
		return sa.take_string();
	}

};

inline bool operator==(const ResourceElement &a, const ResourceElement &b) {
	return a._hwaddr == b._hwaddr && a._channel == b._channel && a._band == b._band;
}

typedef HashTable<int, ResourceElement *> RETable;
typedef RETable::const_iterator REIter;

class EmpowerLVAPManager: public Element {
public:

	EmpowerLVAPManager();
	~EmpowerLVAPManager();

	const char *class_name() const { return "EmpowerLVAPManager"; }
	const char *port_count() const { return "1/1"; }
	const char *processing() const { return PUSH; }

	int initialize(ErrorHandler *);
	int configure(Vector<String> &, ErrorHandler *);
	void add_handlers();
	void run_timer(Timer *);
	void reset();

	void push(int, Packet *);

	int handle_add_lvap(Packet *, uint32_t);
	int handle_del_lvap(Packet *, uint32_t);
	int handle_add_vap(Packet *, uint32_t);
	int handle_del_vap(Packet *, uint32_t);
	int handle_probe_response(Packet *, uint32_t);
	int handle_auth_response(Packet *, uint32_t);
	int handle_assoc_response(Packet *, uint32_t);
	int handle_counters_request(Packet *, uint32_t);
	int handle_txp_counters_request(Packet *, uint32_t);
	int handle_add_rssi_trigger(Packet *, uint32_t);
	int handle_del_rssi_trigger(Packet *, uint32_t);
	int handle_del_summary_trigger(Packet *, uint32_t);
	int handle_add_summary_trigger(Packet *, uint32_t);
	int handle_uimg_request(Packet *, uint32_t);
	int handle_nimg_request(Packet *, uint32_t);
	int handle_set_port(Packet *, uint32_t);
	int handle_frames_request(Packet *, uint32_t);
	int handle_lvap_stats_request(Packet *, uint32_t);
	int handle_add_busyness_trigger(Packet *, uint32_t);
	int handle_del_busyness_trigger(Packet *, uint32_t);
	int handle_busyness_request(Packet *, uint32_t);
	int handle_incom_mcast_addr_response(Packet *, uint32_t);
	int handle_wtp_counters_request(Packet *, uint32_t);
	int handle_del_mcast_addr(Packet *, uint32_t);
	int handle_del_mcast_receiver(Packet *, uint32_t);
	int handle_cqm_links_request(Packet *, uint32_t);
	int handle_caps_request(Packet *, uint32_t);

	void send_hello();
	void send_probe_request(EtherAddress, String, EtherAddress, int, empower_bands_types, empower_bands_types);
	void send_auth_request(EtherAddress, EtherAddress);
	void send_association_request(EtherAddress, EtherAddress, String, EtherAddress, int, empower_bands_types, empower_bands_types);
	void send_status_lvap(EtherAddress);
	void send_status_vap(EtherAddress);
	void send_status_port(EtherAddress, int);
	void send_status_port(EtherAddress, EtherAddress, int, empower_bands_types);
	void send_status_port(EtherAddress, int, EtherAddress, int, empower_bands_types);
	void send_counters_response(EtherAddress, uint32_t);
	void send_txp_counters_response(uint32_t, EtherAddress, uint8_t, empower_bands_types, EtherAddress);
	void send_img_response(int, uint32_t, EtherAddress, uint8_t, empower_bands_types);
	void send_busyness_response(uint32_t, EtherAddress, uint8_t, empower_bands_types);
	void send_caps();
	void send_rssi_trigger(uint32_t, uint32_t, uint8_t);
	void send_summary_trigger(SummaryTrigger *);
	void send_busyness_trigger(uint32_t, uint32_t, uint32_t);
	void send_lvap_stats_response(EtherAddress, uint32_t);
	void send_incomming_mcast_address (EtherAddress, int);
	void send_wtp_counters_response(uint32_t);
	void send_igmp_report(EtherAddress, Vector<IPAddress>*, Vector<enum empower_igmp_record_type>*);
	void send_cqm_links_response(uint32_t);
	void send_add_del_lvap_response(uint8_t, EtherAddress, uint32_t, uint32_t);

	int remove_lvap(EmpowerStationState *);
	LVAP* lvaps() { return &_lvaps; }
	VAP* vaps() { return &_vaps; }
	EtherAddress wtp() { return _wtp; }

	uint32_t get_next_seq() { return ++_seq; }

	int element_to_iface(EtherAddress hwaddr, uint8_t channel, empower_bands_types band) {
		for (REIter iter = _ifaces_to_elements.begin(); iter.live(); iter++) {
			if (iter.value()->_hwaddr == hwaddr && iter.value()->_channel == channel && iter.value()->_band == band) {
				return iter.key();
			}
		}
		return -1;
	}

	ResourceElement* iface_to_element(int iface) {
		return _ifaces_to_elements.get(iface);
	}

	int num_ifaces() {
		return _rcs.size();
	}

	EmpowerStationState * get_ess(EtherAddress sta) {
		EmpowerStationState *ess = _lvaps.get_pointer(sta);
		return ess;
	}

	TxPolicyInfo * get_txp(EtherAddress sta) {
		EmpowerStationState *ess = _lvaps.get_pointer(sta);
		if (!ess) {
			return 0;
		}
		Minstrel * rc = _rcs[ess->_iface_id];
		TxPolicyInfo * txp = rc->tx_policies()->lookup(ess->_sta);
		return txp;
	}

	TransmissionPolicies * get_tx_policies(int iface_id) {
		Minstrel * rc = _rcs[iface_id];
		return rc->tx_policies();
	}

private:

	ReadWriteLock _ports_lock;
	RETable _ifaces_to_elements;

	void compute_bssid_mask();

	void send_message(Packet *);

	class Empower11k *_e11k;
	class EmpowerBeaconSource *_ebs;
	class EmpowerOpenAuthResponder *_eauthr;
	class EmpowerAssociationResponder *_eassor;
	class EmpowerDeAuthResponder *_edeauthr;
	class EmpowerRXStats *_ers;
	class EmpowerCQM *_cqm;
	class EmpowerMulticastTable * _mtbl;

	LVAP _lvaps;
	Ports _ports;
	VAP _vaps;
	Vector<EtherAddress> _masks;
	Vector<Minstrel *> _rcs;
	Vector<String> _debugfs_strings;
	Timer _timer;
	uint32_t _seq;
	EtherAddress _wtp;
	unsigned int _period; // msecs
	bool _debug;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
