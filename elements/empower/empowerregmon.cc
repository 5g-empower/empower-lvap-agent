/*
 * empowerregmon.{cc,hh} -- Regmon Element (EmPOWER Access Point)
 * Giovanni Baggio
 *
 * Copyright (c) 2017 FBK CREATE-NET
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "empowerregmon.hh"
#include <click/args.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
#include "empowerlvapmanager.hh"
CLICK_DECLS

EmpowerRegmon::EmpowerRegmon() :
		_el(0), _iface_id(0), _elem_period(4000), _reg_period(1000),
		_timer(this), _debug(false), _register_log_file(0), _last_mac_ticks(0) {
}

EmpowerRegmon::~EmpowerRegmon() {
}


int EmpowerRegmon::initialize(ErrorHandler *) {

	String registers_name[3] = {"tx_busy", "rx_busy", "ed"};

	for (int i = 0; i < 3; i++) {
		RegmonRegister reg = RegmonRegister(registers_name[i], i, 100);
		_registers.push_back(reg);
	}

	_last_mac_ticks = 0;

	// set sampling interval
	String period_file_path = _debugfs + "/sampling_interval";
	FILE *period_file = fopen(period_file_path.c_str(), "w");

	if (period_file != NULL) {
		fprintf(period_file, "%d", _reg_period * 1000000);
		fclose(period_file);
	} else {
		click_chatter("%{element} :: %s :: unable to open sampling period file %s",
					  this,
					  __func__,
					  period_file_path.c_str());
	}

	// flush measurements register and keep the file open
	// due to bugs in the driver patch, the fread can return more data than the one available in the buffer
	// it is also likely that the read handler reports lines instead of bytes
	String register_log_file_path = _debugfs + "/register_log";
	_register_log_file = fopen(register_log_file_path.c_str(), "r");
	char* buffer[8192];
	int nread;

	do {
		nread = fread(&buffer, 1, sizeof buffer, _register_log_file);
	} while (nread > 0); // fixme, read operation could be slower than kernel measurements writing

	_timer.initialize(this);
	_timer.schedule_now();

	if (_debug) {
		click_chatter("%{element} :: %s :: iface_id %d initialised",
					  this,
					  __func__,
					  _iface_id);
	}

	return 0;
}

int EmpowerRegmon::configure(Vector<String> &conf, ErrorHandler *errh) {

	int ret = Args(conf, this, errh)
              .read_m("EL", ElementCastArg("EmpowerLVAPManager"), _el)
			  .read_m("IFACE_ID", _iface_id)
			  .read("ELEM_PERIOD", _elem_period)
			  .read("REG_PERIOD", _reg_period)
			  .read_m("DEBUGFS", _debugfs)
			  .read("DEBUG", _debug).complete();

	return ret;

}

void EmpowerRegmon::run_timer(Timer *) {

	_read_start.assign_now();

	char buffer[1024];
	int nread;

	// due to bugs in the driver patch, it is necessary to open the file each time
	fclose(_register_log_file);
	String register_log_file_path = _debugfs + "/register_log";
	_register_log_file = fopen(register_log_file_path.c_str(), "r");

	char *block_state = NULL;

	nread = fread(&buffer, 1, sizeof buffer, _register_log_file);

	while (nread > 0) {

		char * line = strtok_r(strdup(buffer), "\n", &block_state);

		while (line != NULL && strlen(line) > 20) {

			char *line_state = NULL;

			int i = 0;
			int mac_ticks_delta = 0;
			bool skip_line = false;

			char * reg_col = strtok_r(strdup(line), ",", &line_state);
			while (reg_col != NULL && i < 7 && !skip_line) {
				uint32_t value = strtoul(reg_col, NULL, 0);
				switch(i) {
					case 0:
					case 1:
					case 2:
						break;
					case 3:
						mac_ticks_delta = value - _last_mac_ticks;
						_last_mac_ticks = value;
						if (mac_ticks_delta < 0)
							skip_line = true;
						break;
					default:
						_registers[i - 4].add_sample(value, mac_ticks_delta);
				}
				reg_col = strtok_r(NULL, ",", &line_state);
				i++;
			}

			line = strtok_r(NULL, "\n", &block_state);
			nread = fread(&buffer, 1, sizeof buffer, _register_log_file);

		}

	}

	// re-schedule the timer considering the time spent for computation
	_read_end.assign_now();

	// fixme, computation might take seconds
	unsigned delta = (_read_end - _read_start).msec();

	if (delta > _elem_period)
		click_chatter("%{element} :: %s :: processing samples took too much time (%d msec)",
				      this,
					  __func__,
					  delta);

	_timer.schedule_after_msec(_elem_period - delta);

}

enum {
	H_STATUS,
};

String EmpowerRegmon::read_handler(Element *e, void *thunk) {
	StringAccum sa;
	EmpowerRegmon *eg = (EmpowerRegmon *) e;
	switch ((uintptr_t) thunk) {
	case H_STATUS: {
		sa << "Registers:" << "\n";
		for (RegistersIter iter = eg->_registers.begin(); iter != eg->_registers.end(); iter++) {
			sa << iter->unparse() << "\n";
		}
		return sa.take_string();
	}
	default:
		return String();
	}
}

void EmpowerRegmon::add_handlers() {
	add_read_handler("status", read_handler, (void *) H_STATUS);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EmpowerRegmon)
