/*
 * empoweropenauthresponder.{cc,hh} -- send 802.11 authentication response packets (EmPOWER Access Point)
 * John Bicket, Roberto Riggio
 *
 * Copyright (c) 2013 CREATE-NET
 * Copyright (c) 2004 Massachusetts Institute of Technology
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
#include <clicknet/wifi.h>
#include <click/etheraddress.hh>
#include <click/args.hh>
#include <click/packet_anno.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/llc.h>
#include <click/straccum.hh>
#include <click/vector.hh>
#include <click/packet_anno.hh>
#include <click/error.hh>
#include "empoweropenauthresponder.hh"
#include "empowerlvapmanager.hh"
CLICK_DECLS

EmpowerOpenAuthResponder::EmpowerOpenAuthResponder() :
		_el(0), _debug(false) {
}

EmpowerOpenAuthResponder::~EmpowerOpenAuthResponder() {
}

int EmpowerOpenAuthResponder::configure(Vector<String> &conf,
		ErrorHandler *errh) {

	return Args(conf, this, errh)
			.read_m("EL", ElementCastArg("EmpowerLVAPManager"), _el)
			.read("DEBUG", _debug).complete();

}

void EmpowerOpenAuthResponder::push(int, Packet *p) {

	if (p->length() < sizeof(struct click_wifi)) {
		click_chatter("%{element} :: %s :: Packet too small: %d Vs. %d",
				      this,
				      __func__,
				      p->length(),
				      sizeof(struct click_wifi));
		p->kill();
		return;
	}

	struct click_wifi *w = (struct click_wifi *) p->data();

	uint8_t iface_id = PAINT_ANNO(p);
	uint8_t type = w->i_fc[0] & WIFI_FC0_TYPE_MASK;

	if (type != WIFI_FC0_TYPE_MGT) {
		click_chatter("%{element} :: %s :: Received non-management packet",
				      this,
				      __func__);
		p->kill();
		return;
	}

	uint8_t subtype = w->i_fc[0] & WIFI_FC0_SUBTYPE_MASK;

	if (subtype != WIFI_FC0_SUBTYPE_AUTH) {
		click_chatter("%{element} :: %s :: Received non-authentication request packet",
				      this,
				      __func__);
		p->kill();
		return;
	}

	uint8_t *ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);

	uint16_t algo = le16_to_cpu(*(uint16_t *) ptr);
	ptr += 2;

	uint16_t seq = le16_to_cpu(*(uint16_t *) ptr);
	ptr += 2;

	uint16_t status = le16_to_cpu(*(uint16_t *) ptr);
	ptr += 2;

	EtherAddress src = EtherAddress(w->i_addr2);

    EmpowerStationState *ess = _el->get_ess(src);

    // If we're not aware of this LVAP, ignore
	if (!ess) {
		click_chatter("%{element} :: %s :: Unknown station %s",
				      this,
				      __func__,
				      src.unparse().c_str());
		p->kill();
		return;
	}

    if (ess->_csa_active) {
		click_chatter("%{element} :: %s :: lvap %s csa active ignoring request.",
				      this,
				      __func__,
				      ess->_sta.unparse().c_str());
    	return;
    }

	// if this is an uplink only lvap then ignore request
	if (!ess->_set_mask) {
		p->kill();
		return;
	}

    // If auth request is coming from different channel, ignore
	if (ess->_iface_id != iface_id) {
		click_chatter("%{element} :: %s :: %s is on iface %u, message coming from %u",
				      this,
				      __func__,
				      src.unparse().c_str(),
					  ess->_iface_id,
					  iface_id);
		p->kill();
		return;
	}

	if (algo != WIFI_AUTH_ALG_OPEN) {
		click_chatter("%{element} :: %s :: Algorithm %d from %s not supported",
				      this,
				      __func__,
				      algo,
				      src.unparse().c_str());
		p->kill();
		return;
	}

	if (seq != 1) {
		click_chatter("%{element} :: %s :: Algorithm %u weird sequence number %d",
				      this,
				      __func__,
				      algo,
				      seq);
		p->kill();
		return;
	}

	if (_debug) {
		click_chatter("%{element} :: %s :: Algorithm %u sequence number %u status %u",
				      this,
				      __func__,
				      algo,
				      seq,
				      status);
	}

	EtherAddress bssid = EtherAddress(w->i_addr3);

	// always ask to the controller because we may want to reject this request
	_el->send_auth_request(src, bssid);

	p->kill();

}

void EmpowerOpenAuthResponder::send_auth_response(EtherAddress dst) {

    EmpowerStationState *ess = _el->get_ess(dst);
	EtherAddress bssid = ess->_bssid;
	int iface_id = ess->_iface_id;
	uint16_t seq = 2;
	uint16_t status = WIFI_STATUS_SUCCESS;

	if (_debug) {
		click_chatter("%{element} :: %s :: authentication %s bssid %s sequence number %u status %u",
				      this,
				      __func__,
				      dst.unparse().c_str(),
					  bssid.unparse().c_str(),
				      seq,
				      status);

	}

	int len = sizeof(struct click_wifi) + 2 + /* alg */
		2 + /* seq */
		2 + /* status */
		0;

	WritablePacket *p = Packet::make(len);

	if (p == 0)
		return;

	struct click_wifi *w = (struct click_wifi *) p->data();

	w->i_fc[0] = WIFI_FC0_VERSION_0 | WIFI_FC0_TYPE_MGT | WIFI_FC0_SUBTYPE_AUTH;
	w->i_fc[1] = WIFI_FC1_DIR_NODS;

	memcpy(w->i_addr1, dst.data(), 6);
	memcpy(w->i_addr2, bssid.data(), 6);
	memcpy(w->i_addr3, bssid.data(), 6);

	w->i_dur = 0;
	w->i_seq = 0;

	uint8_t *ptr;

	ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);

	*(uint16_t *) ptr = cpu_to_le16(WIFI_AUTH_ALG_OPEN);
	ptr += 2;

	*(uint16_t *) ptr = cpu_to_le16(seq);
	ptr += 2;

	*(uint16_t *) ptr = cpu_to_le16(status);
	ptr += 2;

	_el->send_status_lvap(dst);
	SET_PAINT_ANNO(p, iface_id);
	output(0).push(p);

}

enum {
	H_DEBUG
};

String EmpowerOpenAuthResponder::read_handler(Element *e, void *thunk) {
	EmpowerOpenAuthResponder *td = (EmpowerOpenAuthResponder *) e;
	switch ((uintptr_t) thunk) {
	case H_DEBUG:
		return String(td->_debug) + "\n";
	default:
		return String();
	}
}

int EmpowerOpenAuthResponder::write_handler(const String &in_s, Element *e,
		void *vparam, ErrorHandler *errh) {

	EmpowerOpenAuthResponder *f = (EmpowerOpenAuthResponder *) e;
	String s = cp_uncomment(in_s);

	switch ((intptr_t) vparam) {
	case H_DEBUG: {    //debug
		bool debug;
		if (!BoolArg().parse(s, debug))
			return errh->error("debug parameter must be boolean");
		f->_debug = debug;
		break;
	}
	}
	return 0;
}

void EmpowerOpenAuthResponder::add_handlers() {
	add_read_handler("debug", read_handler, (void *) H_DEBUG);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EmpowerOpenAuthResponder)
