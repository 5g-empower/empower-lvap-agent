// -*- mode: c++; c-basic-offset: 2 -*-
#ifndef CLICK_EMPOWERPOWERSAVEBUFFER_HH
#define CLICK_EMPOWERPOWERSAVEBUFFER_HH
#include <click/config.h>
#include <click/element.hh>
#include <click/notifier.hh>
#include <click/task.hh>
#include <click/timer.hh>
#include <click/hashtable.hh>
#include <click/etheraddress.hh>
#include <elements/standard/simplequeue.hh>
#include "empowerlvapmanager.hh"
CLICK_DECLS

/*
=c

EmpowerPowerSaveBuffer(EL, EPSB)

=s EmPOWER

Buffers frame for stations in power save mode

=d

Buffers frame for stations in power save mode. Buffers are created by the
EmPOWERLVAPManager element upon spawning a new LVAPS. Buffers are freed
when the LVAP is moved to another WTP. If the CAPACITY is exceed new frames
will be dropped.

=over 8

=item EL
An EmpowerLVAPManager element

=item CAPACITY
Buffer capacity, default is 50 frames

=item DEBUG
Turn debug on/off

=back

=a EmpowerWifiEncap
*/

class PowerSaveQueue {

public:

	ReadWriteLock _queue_lock;
	Packet** _q;

	uint32_t _capacity;
	uint32_t _size;
	uint32_t _bsize;
	uint32_t _drops;
	uint32_t _head;
	uint32_t _tail;

	PowerSaveQueue(int capacity) :
			_capacity(capacity) {
		_q = new Packet*[_capacity];
		reset();
	}

	~PowerSaveQueue() {
		_queue_lock.acquire_write();
		for (uint32_t i = 0; i < _capacity; i++) {
			if (_q[i]) {
				_q[i]->kill();
			}
		}
		delete[] _q;
		_queue_lock.release_write();
	}

	void reset() {
		_size = 0;
		_bsize = 0;
		_drops = 0;
		_head = 0;
		_tail = 0;
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
		return (p);
	}

};

class EmpowerPowerSaveBuffer: public SimpleQueue {
public:

	EmpowerPowerSaveBuffer();
	~EmpowerPowerSaveBuffer();

	const char *class_name() const { return "EmpowerPowerSaveBuffer"; }
    const char *port_count() const { return PORTS_1_1; }
    const char* processing() const { return PUSH_TO_PULL; }
    void *cast(const char *);

	int configure(Vector<String> &, ErrorHandler *);

	void push(int, Packet *);
    Packet *pull(int);

    String list_queues();

    void request_queue(EtherAddress);
    void release_queue(EtherAddress);
    void touch_queue(EtherAddress);

    void add_handlers();

private:

    enum { SLEEPINESS_TRIGGER = 9 };

    int _sleepiness;
    ActiveNotifier _empty_note;

    typedef HashTable<EtherAddress, PowerSaveQueue*> PowerSaveTable;
    typedef PowerSaveTable::iterator PSTIter;

    typedef Vector<PowerSaveQueue*> QueuePool;
    typedef QueuePool::iterator PoolIter;

    PowerSaveTable* _ps_table;
    QueuePool* _queue_pool;

    EtherAddress _next;

    uint32_t _size;
    uint32_t _drops; // packets dropped because of full queue
    uint32_t _bdrops; // bytes dropped
    uint32_t _capacity;

	class EmpowerLVAPManager *_el;

    bool _debug;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
