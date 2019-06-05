#ifndef CLICK_EMPOWEREGMON_HH
#define CLICK_EMPOWEREGMON_HH
#include <click/element.hh>
#include <click/config.h>
#include <click/timer.hh>
#include <click/vector.hh>
#include <click/straccum.hh>
#include <unistd.h>
#include "empowerlvapmanager.hh"
CLICK_DECLS


class RegmonRegister {
public:

	RegmonRegister(empower_regmon_types type, int iface_id, uint32_t size) {
		_type = type;
		_iface_id = iface_id;
		_size = size;
		_samples = new uint32_t[size]();
		_timestamps = new uint64_t[size]();
		_index = 0;
		_last_value = 0;
		_skipped = 0;
		_min_value = 0xffffffff;
		_max_value = 0;
		_first_run = true;
		memset(_samples, 0, _size);
	}

	void add_sample(uint64_t timestamp, uint32_t value, uint32_t mac_ticks_delta, bool valid) {

		if (_first_run) {
			_first_run = false;
			_samples[_index] = 0;
			_timestamps[_index] = timestamp;
		} else {

			if (!valid)

				_samples[_index] = 36000;
			else {

				uint64_t value_delta = value - _last_value;
				_samples[_index] = (uint32_t)((value_delta * 18000) / mac_ticks_delta);
			}

			_timestamps[_index] = timestamp;
		}

		_last_value = value;

		if (value > _max_value)
			_max_value = value;

		if (value < _min_value)
			_min_value = value;

		_index++;
		_index %= _size;

	}

	String unparse() {

		StringAccum sa;

		if (_type == EMPOWER_REGMON_TX) {
			sa << "Register=tx\t";
		} else if (_type == EMPOWER_REGMON_RX) {
			sa << "Register=rx\t";
		} else {
			sa << "Register=ed\t";
		}

		sa << "Id=" << _iface_id << "\t";
		sa << "Size=" << _size << "\t";
		sa << "Index=" << _index << "\t";
		sa << "Skipped=" << _skipped << "\t";
		sa << "MinValue=" << _min_value << "\t\t";
		sa << "MaxValue=" << _max_value;

		return sa.take_string();

	}

	empower_regmon_types _type;
	int _iface_id;
	int _size;
	uint32_t *_samples;
	uint64_t *_timestamps;
	int _index;
	uint32_t _last_value;
	int _skipped;
	uint32_t _min_value;
	uint32_t _max_value;
	bool _first_run;

};

typedef Vector<RegmonRegister> Registers;
typedef Registers::iterator RegistersIter;

class EmpowerRegmon: public Element {
public:

	EmpowerRegmon();
	~EmpowerRegmon();

	const char *class_name() const { return "EmpowerRegmon"; }

	int configure(Vector<String> &, ErrorHandler *);
	void add_handlers();
	int initialize(ErrorHandler *);
	void run_timer(Timer *);
	RegmonRegister * registers(int i) { return &_registers.at(i); }

private:

	class EmpowerLVAPManager *_el;
    int _iface_id;

	uint32_t _elem_period; // msecs
	uint32_t _reg_period; // msecs
	Timer _timer;

	bool _debug;

	String _debugfs;

	FILE *_register_log_file;
	uint32_t _last_mac_ticks;

	Registers _registers;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
