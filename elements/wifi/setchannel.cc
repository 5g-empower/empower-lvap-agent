/*
 * setchannel.{cc,hh} -- sets wifi channel annotation on a packet
 * John Bicket
 *
 * Copyright (c) 2003 Massachusetts Institute of Technology
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
#include <click/packet_anno.hh>
#include "setchannel.hh"
#include <clicknet/ether.h>
#include <clicknet/wifi.h>
#include <click/etheraddress.hh>
CLICK_DECLS

SetChannel::SetChannel() : _channel(0)
{
}

SetChannel::~SetChannel()
{
}

int
SetChannel::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh).read_p("CHANNEL", _channel).complete();
}

Packet *
SetChannel::simple_action(Packet *p_in)
{
  if (p_in) {
    struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p_in);
    ceh->magic = WIFI_EXTRA_MAGIC;
    ceh->channel = _channel;
    return p_in;
  }
  return 0;
}

enum {
	H_CHANNEL
};

String SetChannel::read_handler(Element *e, void *thunk) {
	SetChannel *td = (SetChannel *) e;
	switch ((uintptr_t) thunk) {
	case H_CHANNEL:
		return String(td->_channel) + "\n";
	default:
		return String();
	}
}

int SetChannel::write_handler(const String &in_s, Element *e,
		void *vparam, ErrorHandler *errh) {

	SetChannel *f = (SetChannel *) e;
	String s = cp_uncomment(in_s);

	switch ((intptr_t) vparam) {
	case H_CHANNEL: {    //debug
		unsigned channel;
		if (!IntArg().parse(s, channel))
			return errh->error("channel parameter must be unsigned");
		f->_channel = channel;
		break;
	}
	}
	return 0;
}

void SetChannel::add_handlers() {
	add_read_handler("channel", read_handler, (void *) H_CHANNEL);
	add_write_handler("channel", write_handler, (void *) H_CHANNEL);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetChannel)
