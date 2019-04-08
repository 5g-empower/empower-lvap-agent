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
#include <elements/wifi/minstrel.hh>
#include <elements/wifi/bitrate.hh>
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

class SlicePairQueue {

public:

	ReadWriteLock _queue_lock;
	Packet** _q;

	uint32_t _capacity;
	uint32_t _deficit;
	uint32_t _deficit_used;
	uint32_t _tx_packets;
	uint32_t _tx_bytes;
	uint32_t _nb_pkts;
	uint32_t _max_queue_length;
	uint32_t _drops;
	uint32_t _head;
	uint32_t _tail;

public:

	SlicePairQueue(uint32_t capacity) {
		_q = new Packet*[capacity];
		_deficit = 0;
		_deficit_used = 0;
		_tx_bytes = 0;
		_tx_packets = 0;
		_capacity = capacity;
		_nb_pkts = 0;
		_max_queue_length = 0;
		_drops = 0;
		_head = 0;
		_tail = 0;
		for (unsigned i = 0; i < _capacity; i++) {
			_q[i] = 0;
		}
	}

	String unparse() {
		StringAccum result;
		_queue_lock.acquire_read();
		result << "    status: " << _nb_pkts << "/" << _capacity << "\n";
		result << "    deficit: " << _deficit << "\n";
		result << "    deficit_used: " << _deficit_used << "\n";
		result << "    tx_packets: " << _tx_packets << "\n";
		result << "    tx_bytes: " << _tx_bytes << "\n";
		_queue_lock.release_read();
		return result.take_string();
	}

	~SlicePairQueue() {
		_queue_lock.acquire_write();
		for (uint32_t i = 0; i < _capacity; i++) {
			if (_q[i]) {
				_q[i]->kill();
			}
		}
		delete[] _q;
		_queue_lock.release_write();
	}

	Packet* pull() {
		Packet* p = 0;
		_queue_lock.acquire_write();
		if (_nb_pkts > 0) {
			p = _q[_head];
			_q[_head] = 0;
			_head++;
			_head %= _capacity;
			_nb_pkts--;
		}
		_queue_lock.release_write();
		return p;
	}

	bool push(Packet* p) {
		bool result = false;
		_queue_lock.acquire_write();
		if (_nb_pkts == _capacity) {
			_drops++;
			result = false;
		} else {
			_q[_tail] = p;
			_tail++;
			_tail %= _capacity;
			_nb_pkts++;
			if (_nb_pkts > _max_queue_length)
				_max_queue_length = _nb_pkts;
			result = true;
		}
		_queue_lock.release_write();
		return result;
	}

};

class SlicePairKey {

public:

    EtherAddress _ra;
    EtherAddress _ta;

    SlicePairKey(EtherAddress ra, EtherAddress ta) :
        _ra(ra), _ta(ta) {
    }

    inline hashcode_t hashcode() const {
        return CLICK_NAME(hashcode)(_ra) + CLICK_NAME(hashcode)(_ta);
    }

    inline bool operator==(SlicePairKey other) const {
        return (other._ra == _ra && other._ta == _ta);
    }

    inline bool operator!=(SlicePairKey other) const {
        return (other._ra != _ra || other._ta != _ta);
    }

    String unparse() {
        StringAccum result;
        result << "(" << _ra.unparse() << ", " << _ta.unparse() << ")";
        return result.take_string();
    }

};

class SlicePair {

public:

    SlicePairKey _key;
    uint32_t _quantum;
    SlicePairQueue _queue;

    SlicePair(EtherAddress ra, EtherAddress ta, uint32_t quantum) :
        _key(ra, ta), _quantum(quantum), _queue(500) {
    }

    inline bool operator==(SlicePairKey other) const {
        return (other == _key);
    }

    inline bool operator!=(SlicePairKey other) const {
        return (other != _key);
    }

    inline bool operator==(SlicePair other) const {
        return (other._key == _key && other._key == _key);
    }

    inline bool operator!=(SlicePair other) const {
        return (other._key != _key || other._key != _key);
    }

    String unparse() {
        return _key.unparse();
    }

};

typedef HashTable<SlicePairKey, SlicePair*> SlicePairs;
typedef SlicePairs::iterator SlicePairsIter;

class SliceKey {

public:

    String _ssid;
    int _dscp;

    SliceKey(String ssid, int dscp) :
        _ssid(ssid),_dscp(dscp) {
    }

    inline hashcode_t hashcode() const {
        return CLICK_NAME(hashcode)(_ssid) + CLICK_NAME(hashcode)(_dscp);
    }

    inline bool operator==(SliceKey other) const {
        return (other._ssid == _ssid && other._dscp == _dscp);
    }

    inline bool operator!=(SliceKey other) const {
        return (other._ssid != _ssid || other._dscp != _dscp);
    }

    String unparse() {
        StringAccum result;
        result << _ssid << ":" << _dscp;
        return result.take_string();
    }

};

class Slice {

public:

    SliceKey _key;

    uint32_t _quantum;
    bool _amsdu_aggregation;
    uint32_t _scheduler;

    SlicePairs _slice_pairs;

    Slice(String ssid, int dscp, uint32_t quantum, bool amsdu_aggregation, uint32_t scheduler) :
        _key(ssid, dscp), _quantum(quantum), _amsdu_aggregation(amsdu_aggregation), _scheduler(scheduler) {
    }

    inline bool operator==(SliceKey other) const {
        return (other == _key);
    }

    inline bool operator!=(SliceKey other) const {
        return (other != _key);
    }

    inline bool operator==(Slice other) const {
        return (other._key == _key && other._key == _key);
    }

    inline bool operator!=(Slice other) const {
        return (other._key != _key || other._key != _key);
    }

    String unparse() {
        StringAccum result;
        result << _key.unparse() << "  - ";
        result << " quantum: " << _quantum;
        result << " scheduler: " << _scheduler << " \n";

        for (SlicePairsIter it = _slice_pairs.begin(); it.live(); it++) {
            result << "  Pair: " << it.value()->_key.unparse() << " \n";
            result << "    quantum: " << it.value()->_quantum << "\n";
            result << it.value()->_queue.unparse();
        }

        return result.take_string();
    }

    void set_aggr_queues_quantum(class Minstrel *rc) {

        // Each time the slice is renewed quantum, the time is divided
        // according to the scheduler policy.

        switch (_scheduler) {
        // Airtime fairness
        case 1: {
            Vector<uint32_t> transm_times;
            uint32_t total_time = 0;

            // Estimation of the time needed by each stations depending on the
            // most probable MCS and a standard 1500 bytes packet.
            if (_slice_pairs.size() > 0) {

                uint32_t estimated_time = 0;
                for (SlicePairsIter it = _slice_pairs.begin(); it.live();
                        it++) {
                    estimated_time = rc->estimate_usecs_default_packet(1500,
                            it.value()->_key._ra);
                    transm_times.push_back(estimated_time);
                    total_time += estimated_time;
                }

                if (total_time == 0) {
                    break;
                }

                int station = 0;
                for (SlicePairsIter it = _slice_pairs.begin(); it.live();
                        it++) {
                    it.value()->_quantum = (_quantum * (transm_times[station]))
                            / total_time;
                    station++;
                }
            }
            break;
        }
            // Round robin
        case 0:
        default: {
            if (_slice_pairs.size() > 0) {
                uint32_t new_quantum = floor(_quantum / _slice_pairs.size());

                for (SlicePairsIter it = _slice_pairs.begin(); it.live();
                        it++) {
                    it.value()->_quantum = new_quantum;
                }
            }
            break;
        }
        }
    }
};

typedef HashTable<SliceKey, Slice*> Slices;
typedef Slices::iterator SlicesIter;

typedef Pair<SliceKey, SlicePairKey> SlicePairQueueKey;

typedef HashTable<SlicePairQueueKey, SlicePairQueue*> SlicePairQueues;

typedef HashTable<SlicePairQueueKey, Packet*> HeadTable;

class EmpowerQOSManager: public Element {

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
	void set_default_slice(String);
	void set_slice(String, int, uint32_t, bool, uint32_t);
	void del_slice(String, int);

	Slices *slices() { return &_slices; }

private:

    ReadWriteLock _lock;

    enum { SLEEPINESS_TRIGGER = 9 };

    ActiveNotifier _empty_note;
	class EmpowerLVAPManager *_el;
	class Minstrel * _rc;

	Slices _slices;

    Vector<SlicePairQueueKey> _active_list;
    HeadTable _head_table;

    int _sleepiness;

    int _iface_id;

    bool _debug;

	void store(String, int, Packet *, EtherAddress, EtherAddress);
	String list_slices();

	Packet * wifi_encap(Packet *, EtherAddress, EtherAddress, EtherAddress);

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
