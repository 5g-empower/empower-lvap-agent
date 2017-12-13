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

	RegmonRegister(String name, int iface_id, uint32_t size) {
		_name = name;
		_iface_id = iface_id;
		_size = size;
		_samples = new uint8_t[size]();
		_index = 0;
		_last_value = 0;
		_skipped = 0;
		_min_value = 0xffffffff;
		_max_value = 0;
		_first_run = true;
		memset(_samples, 0, _size);
	}

	void add_sample(uint32_t value, uint32_t mac_ticks_delta) {

		if (_first_run) {
			_first_run = false;
			_samples[_index] = 0;
		}
		else {
			uint32_t perc = ((value - _last_value) * 100) / mac_ticks_delta;
			 _samples[_index] = (uint8_t)perc;
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

		sa << "Register=" << _name << "\t\t";
		sa << "Id=" << _iface_id << "\t\t";
		sa << "Size=" << _size << "\t\t";
		sa << "Index=" << _index << "\t\t";
		sa << "Skipped=" << _skipped << "\t\t";
		sa << "MinValue=" << _min_value << "\t\t";
		sa << "MaxValue=" << _max_value << "\t\t";

		return sa.take_string();

	}

private:

	String _name;
	int _iface_id;
	int _size;
	uint8_t *_samples;
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

private:

	class EmpowerLVAPManager *_el;
    int _iface_id;

	uint32_t _elem_period; // msecs
	uint32_t _reg_period; // msecs
	Timer _timer;

	bool _debug;

	String _debugfs;

	FILE *_register_log_file;
	Timestamp _read_start, _read_end;
	uint32_t _last_mac_ticks;

	Registers _registers;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
