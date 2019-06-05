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

#include <stdio.h>
#include <inttypes.h>
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

	RegmonRegister reg_tx = RegmonRegister(EMPOWER_REGMON_TX, _iface_id, 100);
	_registers.push_back(reg_tx);

	RegmonRegister reg_rx = RegmonRegister(EMPOWER_REGMON_RX, _iface_id, 100);
	_registers.push_back(reg_rx);

	RegmonRegister reg_ed = RegmonRegister(EMPOWER_REGMON_ED, _iface_id, 100);
	_registers.push_back(reg_ed);

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

	Timestamp start = Timestamp::now();

	FILE * fp;
	char * line = NULL;
	size_t len = 0;
	ssize_t read;

	String register_log_file_path = _debugfs + "/register_log";
	fp = fopen(register_log_file_path.c_str(), "r");

	if (fp == NULL) {
		click_chatter("%{element} :: %s :: unable to open file %s",
					  this,
					  __func__,
					  register_log_file_path.c_str());
		_timer.schedule_after_msec(_elem_period);
		return;
	}

	while ((read = getline(&line, &len, fp)) != -1) {
		Vector<String> values;
		char* token = strtok(line, ",");
		while (token != NULL) {
			values.push_back(String(token));
			token = strtok(NULL, ",");
		}
		uint32_t sec = strtoul(values[0].c_str(), NULL, 10);
		uint32_t nsec = strtoul(values[1].c_str(), NULL, 10);
		uint32_t mac_ticks = strtoul(values[3].c_str(), NULL, 16);
		uint32_t tx = strtoul(values[4].c_str(), NULL, 16);
		uint32_t rx = strtoul(values[5].c_str(), NULL, 16);
		uint32_t ed = strtoul(values[6].c_str(), NULL, 16);

		bool valid;
		uint32_t mac_ticks_delta = 0;

		if (mac_ticks < _last_mac_ticks) {

			_last_mac_ticks = mac_ticks;
			valid = false;
		}
		else {

			mac_ticks_delta = mac_ticks - _last_mac_ticks;
			_last_mac_ticks = mac_ticks;
			valid = true;
		}

		uint64_t ts_int = sec * 1000000LL + nsec / 1000;
		_registers[EMPOWER_REGMON_TX].add_sample(ts_int, tx, mac_ticks_delta, valid);
		_registers[EMPOWER_REGMON_RX].add_sample(ts_int, rx, mac_ticks_delta, valid);
		_registers[EMPOWER_REGMON_ED].add_sample(ts_int, ed, mac_ticks_delta, valid);
	}

	fclose(fp);

	Timestamp delta = Timestamp::now() -start;

	if (delta.msec() > _elem_period) {
		click_chatter("%{element} :: %s :: processing samples took too much time %s",
				      this,
					  __func__,
					  delta.unparse().c_str());
	}

	_timer.schedule_after_msec(_elem_period - delta.msec());
	return;

}

enum {
	H_STATUS,
	H_FULL,
};

String EmpowerRegmon::read_handler(Element *e, void *thunk) {
	StringAccum sa;
	EmpowerRegmon *eg = (EmpowerRegmon *) e;
	switch ((uintptr_t) thunk) {
	case H_STATUS: {
		for (RegistersIter iter = eg->_registers.begin(); iter != eg->_registers.end(); iter++) {
			sa << iter->unparse() << "\n";
		}
		return sa.take_string();
	}
	case H_FULL: {
		for (RegistersIter iter = eg->_registers.begin(); iter != eg->_registers.end(); iter++) {
			sa << iter->unparse() << "\n";
			for (int i = 0; i < 100; i++) {
				sa << iter->_timestamps[i] << " " << iter->_samples[i] << '\n';
			}
		}
		return sa.take_string();
	}
	default:
		return String();
	}
}

void EmpowerRegmon::add_handlers() {
	add_read_handler("status", read_handler, (void *) H_STATUS);
	add_read_handler("full", read_handler, (void *) H_FULL);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EmpowerRegmon)
