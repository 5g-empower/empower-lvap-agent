#ifndef EMPOWERHYPERVISOR_HH
#define EMPOWERHYPERVISOR_HH
#include <click/element.hh>
#include <click/notifier.hh>
#include <click/hashtable.hh>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <elements/standard/simplequeue.hh>
CLICK_DECLS

/*
 * =c
 * EmpowerHypervisor([CAPACITY, QUANTUM, DEBUG])
 * =s wifi
 * Implements the ADWRR scheduling policy
 * =io
 * one output, one input
 * =d
 * This element expects WiFi frames. Incoming MAC frames are classified according to the
 * destination address (the LVAP). Each flows is fed to a different queue holding up to
 * CAPACITY frames. The default for CAPACITY is 1000. Buffers are polled according with
 * a Airtime Deficit Weighted Round Robin (DRR) policy. Queues must be created by the
 * EmpowerLVAPManager Element
 */

class EmpowerPacketBuffer {

public:

	uint32_t _capacity;

	EtherAddress _bssid;
	EtherAddress _dst;
	ReadWriteLock _queue_lock;
	Packet** _q;

	uint32_t _deficit;
	uint32_t _size;
	uint32_t _bsize;
	uint32_t _drops;
	uint32_t _head;
	uint32_t _tail;

	EmpowerPacketBuffer(uint32_t capacity, EtherAddress bssid, EtherAddress dst) :
			_capacity(capacity), _bssid(bssid), _dst(dst) {
		_q = new Packet*[_capacity];
		_size = 0;
		_bsize = 0;
		_drops = 0;
		_head = 0;
		_tail = 0;
		_deficit = 0;
	}

	~EmpowerPacketBuffer() {
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
		if (_head != _tail) {
			_head = (_head + 1) % _capacity;
			p = _q[_head];
			_q[_head] = 0;
			_size--;
			_bsize -= p->length();
		}
		_queue_lock.release_write();
		return (p);
	}

	bool push(Packet* p) {
		bool result = false;
		_queue_lock.acquire_write();
		if ((_tail + 1) % _capacity != _head) {
			_tail++;
			_tail %= _capacity;
			_q[_tail] = p;
			_size++;
			_bsize += p->length();
			result = true;
		} else {
			_drops++;
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

};

class EmpowerHypervisor : public SimpleQueue { public:

	EmpowerHypervisor();
    ~EmpowerHypervisor();

    const char* class_name() const		{ return "EmpowerHypervisor"; }
    const char *port_count() const		{ return PORTS_1_1; }
    const char* processing() const		{ return PUSH_TO_PULL; }
    void *cast(const char *);

    int configure(Vector<String> &conf, ErrorHandler *);
    int initialize(ErrorHandler *);

    void push(int port, Packet *);
    Packet *pull(int port);

    void add_handlers();

    uint32_t drops() { return(_drops); } // dropped packets
    uint32_t bdrops() { return(_bdrops); } // bytes dropped

    uint32_t quantum() { return(_quantum); }
    uint32_t capacity() { return(_capacity); }

    String list_queues();
    void request_queue(EtherAddress, EtherAddress);
    void release_queue(EtherAddress);

  protected:

    enum { SLEEPINESS_TRIGGER = 9 };

    int _sleepiness;
    ActiveNotifier _empty_note;

    typedef HashTable<EtherAddress, EmpowerPacketBuffer*> HypervisorTable;
    typedef HypervisorTable::iterator HTIter;

    Vector<EtherAddress> _active_list;
    HypervisorTable _hyper_table;

    uint32_t _quantum;
    uint32_t _capacity;

    uint32_t _drops; // packets dropped because of full queue
    uint32_t _bdrops; // bytes dropped

	bool _debug;

    uint32_t compute_deficit(const Packet *);

    static int write_handler(const String &, Element *, void *, ErrorHandler *);
    static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
