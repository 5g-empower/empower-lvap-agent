/*
 * resourceelements.{cc,hh} -- List of supported resource elements
 * Roberto Riggio
 *
 * Copyright (c) 2014 CREATE-NET
 *
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
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "empowerresourceelements.hh"
CLICK_DECLS

void
cp_slashvec(const String &str, Vector<String> &conf)
{
	int start = 0;
	int pos = str.find_left('/');
	int len = str.length();
	do {
	    conf.push_back(str.substring(start, pos - start));
		start = pos + 1;
	} while ((pos = str.find_left('/', start)) != -1);
	conf.push_back(str.substring(start, len - start));
}

EmpowerResourceElements::EmpowerResourceElements() : _debug(0) {
}

EmpowerResourceElements::~EmpowerResourceElements() {
}

void *
EmpowerResourceElements::cast(const char *n) {
	if (strcmp(n, "ResourceElements") == 0)
		return (EmpowerResourceElements *) this;
	else
		return 0;
}

int EmpowerResourceElements::parse_and_insert(String s, ErrorHandler *errh) {

	EtherAddress eth;

	Vector<ResourceElement> elements;
	Vector<String> args;

	cp_spacevec(s, args);

	for (int x = 0; x < args.size(); x++) {

		Vector<String> tokens;

		cp_slashvec(args[x], tokens);

		if (tokens.size() < 2 || tokens.size() > 3) {
			return errh->error(
					"error param %s: must be in the form <channel/band/iface> or <channel/band>",
					args[x].c_str());
		}

		int channel = -1;
		empower_bands_types band = EMPOWER_BT_L20;
		int iface_id = -1;

		IntArg().parse(tokens[0], channel);

		if (tokens[1] == "L20") {
			band = EMPOWER_BT_L20;
		} else if (tokens[1] == "HT20") {
			band = EMPOWER_BT_HT20;
		} else if (tokens[1] == "HT40") {
			band = EMPOWER_BT_HT40;
		}

		if (tokens.size() > 2) {
			IntArg().parse(tokens[2], iface_id);
		}

		ResourceElement elm = ResourceElement(channel, band);

		if (iface_id > -1) {
			_ifaces_to_elements.set(iface_id, elm);
		}

		_elements_to_ifaces.set(elm, iface_id);

	}

	return 0;

}

int EmpowerResourceElements::configure(Vector<String> &conf, ErrorHandler *errh) {
	int res = 0;
	_debug = false;
	for (int x = 0; x < conf.size(); x++) {
		res = parse_and_insert(conf[x], errh);
		if (res != 0) {
			return res;
		}
	}

	return res;
}

enum {
	H_DEBUG, H_ELEMENTS, H_INTERFACES
};

String EmpowerResourceElements::read_handler(Element *e, void *thunk) {
	EmpowerResourceElements *td = (EmpowerResourceElements *) e;
	switch ((uintptr_t) thunk) {
	case H_DEBUG:
		return String(td->_debug) + "\n";
	case H_ELEMENTS: {
		StringAccum sa;
		for (IfIter iter = td->_elements_to_ifaces.begin(); iter.live(); iter++) {
			sa << iter.key().unparse() << " -> " << iter.value()  << "\n";
		}
		return sa.take_string();
	}
	case H_INTERFACES: {
		StringAccum sa;
		for (REIter iter = td->_ifaces_to_elements.begin(); iter.live(); iter++) {
			sa << iter.key() << " -> " << iter.value().unparse()  << "\n";
		}
		return sa.take_string();
	}
	default:
		return String();
	}
}

int EmpowerResourceElements::write_handler(const String &in_s, Element *e,
		void *vparam, ErrorHandler *errh) {

	EmpowerResourceElements *f = (EmpowerResourceElements *) e;
	String s = cp_uncomment(in_s);

	switch ((intptr_t) vparam) {
	case H_DEBUG: {
		bool debug;
		if (!BoolArg().parse(s, debug))
			return errh->error("debug parameter must be boolean");
		f->_debug = debug;
		break;
	}
	}
	return 0;
}

void EmpowerResourceElements::add_handlers() {
	add_read_handler("debug", read_handler, (void *) H_DEBUG);
	add_read_handler("elements", read_handler, (void *) H_ELEMENTS);
	add_read_handler("interfaces", read_handler, (void *) H_INTERFACES);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EmpowerResourceElements)
