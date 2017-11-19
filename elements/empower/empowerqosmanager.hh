// -*- mode: c++; c-basic-offset: 2 -*-
#ifndef CLICK_EMPOWERQOSMANAGER_HH
#define CLICK_EMPOWERQOSMANAGER_HH
#include <click/config.h>
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/notifier.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/hashmap.hh>
#include <click/hashtable.hh>
#include <click/straccum.hh>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>
#include <elements/standard/simplequeue.hh>
CLICK_DECLS

/*
=c

EmpowerQOSManager(EL, DEBUG)

=s EmPOWER

Converts Ethernet packets to 802.11 packets with a LLC header. Setting
the appropriate BSSID given the destination address. An EmPOWER Access
Point generates one virtual BSSID called LVAP for each active station.
Also maintains a dedicated queue for each pair tenant/dscp.

=d

Strips the Ethernet header off the front of the packet and pushes
an 802.11 frame header and LLC header onto the packet.

Arguments are:

=item EL
An EmpowerLVAPManager element

=item DEBUG
Turn debug on/off

=back 8

=a EmpowerWifiDecap
*/

class AggregationQueue {

public:

	ReadWriteLock _queue_lock;
	Packet** _q;

	uint32_t _capacity;
	EtherAddress _ra;
	EtherAddress _ta;
	EtherAddress _sa;
	uint32_t _size;
	uint32_t _bsize;
	uint32_t _drops;
	uint32_t _head;
	uint32_t _tail;

	AggregationQueue(uint32_t capacity, EtherAddress ra, EtherAddress sa, EtherAddress ta) {
		_q = new Packet*[_capacity];
		_capacity = capacity;
		_ra = ra;
		_ta = ta;
		_sa = sa;
		_size = 0;
		_bsize = 0;
		_drops = 0;
		_head = 0;
		_tail = 0;
	}

	String unparse() {
		StringAccum result;
		_queue_lock.acquire_read();
		result << "RA: " << _ra << ", TA: " << _ta << " SA: " << _sa << " status: " << _size << "/" << _capacity << "\n";
		_queue_lock.release_read();
		return result.take_string();
	}

	~AggregationQueue() {
		_queue_lock.acquire_write();
		for (uint32_t i = 0; i < _capacity; i++) {
			if (_q[i]) {
				click_chatter("nothing to seeeee....");
				_q[i]->kill();
			}
		}
		delete[] _q;
		_queue_lock.release_write();
	}

	Packet* pull() {
		Packet* p = 0;
		_queue_lock.acquire_write();
		if (_size > 0) {
			p = _q[_head];
			_q[_head] = 0;
			_head++;
			_head %= _capacity;
			_size--;
		}
		_queue_lock.release_write();
		return (p);
	}

	bool push(Packet* p) {
		if (p->shared()) {
			click_chatter("is shared");
		}
		return false;
		bool result = false;
		_queue_lock.acquire_write();
		if (_size == _capacity) {
			_drops++;
			result = false;
		} else {
			_q[_tail] = 0;
			_tail++;
			_tail %= _capacity;
			_size++;
			result = true;
		}
		_queue_lock.release_write();
		return result;
	}

	const Packet* top() {
		Packet* p = 0;
		_queue_lock.acquire_write();
		if (_head != _tail) {
			p = _q[(_head + 1) % _capacity];
		}
		_queue_lock.release_write();
		return p;
	}

    /*Packet * wifi_encap(Packet *p, EtherAddress ra, EtherAddress sa, EtherAddress ta) {

        WritablePacket *q = p->uniqueify();

		if (!q) {
			return 0;
		}

		uint8_t mode = WIFI_FC1_DIR_FROMDS;
		uint16_t ethtype;

		memcpy(&ethtype, q->data() + 12, 2);

		q->pull(sizeof(struct click_ether));
		q = q->push(sizeof(struct click_llc));

		if (!q) {
			q->kill();
			return 0;
		}

		memcpy(q->data(), WIFI_LLC_HEADER, WIFI_LLC_HEADER_LEN);
		memcpy(q->data() + 6, &ethtype, 2);

		q = q->push(sizeof(struct click_wifi));

		if (!q) {
			q->kill();
			return 0;
		}

		struct click_wifi *w = (struct click_wifi *) q->data();

		memset(q->data(), 0, sizeof(click_wifi));

		w->i_fc[0] = (uint8_t) (WIFI_FC0_VERSION_0 | WIFI_FC0_TYPE_DATA);
		w->i_fc[1] = 0;
		w->i_fc[1] |= (uint8_t) (WIFI_FC1_DIR_MASK & mode);

		memcpy(w->i_addr1, ra.data(), 6);
		memcpy(w->i_addr2, ta.data(), 6);
		memcpy(w->i_addr3, sa.data(), 6);

		return q;

    }*/
};

typedef HashTable<EtherAddress, AggregationQueue*> AggregationQueues;
typedef AggregationQueues::iterator AQIter;

class EmpowerQOSManager;

class TrafficRuleQueue {

  public:

    TrafficRuleQueue(String ssid, int dscp, uint32_t capacity) : _ssid(ssid), _dscp(dscp), _capacity(capacity) {}
    ~TrafficRuleQueue() {}

    bool enqueue(Packet *p, EtherAddress ra, EtherAddress sa, EtherAddress ta) {

    		bool result = false;

		_queue_lock.acquire_write();

		if (_queues.find(ra) == _queues.end()) {

			click_chatter("TrafficRuleQueue :: enqueue :: creating new aggregation queue for ra %s sa %s ta %s",
						  ra.unparse().c_str(),
						  sa.unparse().c_str(),
						  ta.unparse().c_str());

			AggregationQueue *queue = new AggregationQueue(_capacity, ra, sa, ta);
			_queues.set(ra, queue);

		}

		AggregationQueue *queue = _queues.get(ra);
		result = queue->push(p);

		_queue_lock.release_write();

		return result;

    }

	String unparse() {
		StringAccum result;
		_queue_lock.acquire_read();
		result << _ssid << "/" << _dscp << " -> capacity: " << _capacity
				<< "\n";
		AQIter itr = _queues.begin();
		while (itr != _queues.end()) {
			AggregationQueue *aq = itr.value();
			result << "  " << aq->unparse();
			itr++;
		} // end while
		_queue_lock.release_read();
		return result.take_string();
	}

  private:

    ReadWriteLock _queue_lock;
	AggregationQueues _queues;

	String _ssid;
	int _dscp;
    uint32_t _capacity;

};

class TrafficRule {
  public:

    String _ssid;
    int _dscp;

    TrafficRule() : _ssid(""), _dscp(0) {
    }

    TrafficRule(String ssid, int dscp) : _ssid(ssid), _dscp(dscp) {
    }

    inline hashcode_t hashcode() const {
    		return CLICK_NAME(hashcode)(_ssid) + CLICK_NAME(hashcode)(_dscp);
    }

    inline bool operator==(TrafficRule other) const {
    		return (other._ssid == _ssid && other._dscp == _dscp);
    }

};

typedef HashTable<TrafficRule, TrafficRuleQueue*> TrafficRules;
typedef TrafficRules::iterator TRIter;

class EmpowerQOSManager: public SimpleQueue {
public:

	EmpowerQOSManager();
	~EmpowerQOSManager();

	const char *class_name() const { return "EmpowerQOSManager"; }
	const char *port_count() const { return PORTS_1_1; }
	const char *processing() const { return PUSH_TO_PULL; }
    void *cast(const char *);

	int configure(Vector<String> &, ErrorHandler *);

	void push(int, Packet *);
	Packet *pull(int);

	void add_handlers();
	void create_traffic_rule(String, int);

private:

    enum { SLEEPINESS_TRIGGER = 9 };

    ActiveNotifier _empty_note;
	class EmpowerLVAPManager *_el;
	class Minstrel * _rc;

	TrafficRules _rules;

    int _sleepiness;
    uint32_t _capacity;

    bool _debug;

	void enqueue(String, int, Packet *, EtherAddress, EtherAddress, EtherAddress);

	String list_queues();

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
