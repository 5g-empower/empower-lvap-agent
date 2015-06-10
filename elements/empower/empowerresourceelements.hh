#ifndef CLICK_EMPOWERRESOURCEELEMENTS_HH
#define CLICK_EMPOWERRESOURCEELEMENTS_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/glue.hh>
#include <click/hashtable.hh>
#include "empowerpacket.hh"
CLICK_DECLS

/*
 =c

 AvailableRates()

 =s Wifi, Wireless Station, Wireless AccessPoint

 Tracks bit-rate capabilities of other stations.

 =d

 Tracks a list of bitrates other stations are capable of.

 =h insert write-only
 Inserts an ethernet address and a list of bitrates to the database.

 =h remove write-only
 Removes an ethernet address from the database.

 =h rates read-only
 Shows the entries in the database.

 =a BeaconScanner
 */

class ResourceElement {
public:

	int _channel;
	empower_bands_types _band;

	ResourceElement() : _channel(1), _band(EMPOWER_BT_L20) {
	}

	ResourceElement(int channel, empower_bands_types band) : _channel(channel), _band(band) {
	}

	inline size_t hashcode() const {
		return _channel | _band;
	}

	inline String unparse() const {
		StringAccum sa;
		sa << "(";
		sa << _channel;
		sa << ", ";
		switch (_band) {
		case EMPOWER_BT_L20:
			sa << "L20";
			break;
		case EMPOWER_BT_HT20:
			sa << "HT20";
			break;
		case EMPOWER_BT_HT40:
			sa << "HT40";
			break;
		}
		sa << ")";
		return sa.take_string();
	}

};

inline bool operator==(const ResourceElement &a, const ResourceElement &b) {
	return a._channel == b._channel && a._band == b._band;
}

typedef HashTable<ResourceElement, int> IfTable;
typedef IfTable::const_iterator IfIter;

typedef HashTable<int, ResourceElement> RETable;
typedef RETable::const_iterator REIter;

class EmpowerResourceElements: public Element {
public:

	EmpowerResourceElements() CLICK_COLD;
	~EmpowerResourceElements() CLICK_COLD;

	const char *class_name() const { return "EmpowerResourceElements"; }
	const char *port_count() const { return PORTS_0_0; }

	int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
	void *cast(const char *n);

	void add_handlers() CLICK_COLD;
	int parse_and_insert(String s, ErrorHandler *errh);

	const IfTable & elements() { return _elements_to_ifaces; }

	int element_to_iface(uint8_t channel, empower_bands_types band) {
		return _elements_to_ifaces.find(ResourceElement(channel, band)).value();
	}

	ResourceElement* iface_to_element(int iface) { return _ifaces_to_elements.get_pointer(iface); }

private:

	IfTable _elements_to_ifaces;
	RETable _ifaces_to_elements;

	bool _debug;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
