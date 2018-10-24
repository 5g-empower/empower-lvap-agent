/*
 * empowerdeauthresponder.{cc,hh} -- handles 802.11 deauthentication packets (EmPOWER Access Point)
 * Roberto Riggio
 *
 * Copyright (c) 2013 CREATE-NET
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
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/llc.h>
#include <click/straccum.hh>
#include <click/vector.hh>
#include <click/packet_anno.hh>
#include <click/error.hh>
#include "empowerdeauthresponder.hh"
#include "empowerlvapmanager.hh"
CLICK_DECLS

EmpowerDeAuthResponder::EmpowerDeAuthResponder() :
		_el(0), _debug(false) {
}

EmpowerDeAuthResponder::~EmpowerDeAuthResponder() {
}

int EmpowerDeAuthResponder::configure(Vector<String> &conf,
		ErrorHandler *errh) {

	return Args(conf, this, errh)
			.read_m("EL", ElementCastArg("EmpowerLVAPManager"), _el)
			.read("DEBUG", _debug).complete();

}

void EmpowerDeAuthResponder::push(int, Packet *p) {

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

	uint8_t type = w->i_fc[0] & WIFI_FC0_TYPE_MASK;

	if (type != WIFI_FC0_TYPE_MGT) {
		click_chatter("%{element} :: %s :: Received non-management packet",
				      this,
				      __func__);
		p->kill();
		return;
	}

	uint8_t subtype = w->i_fc[0] & WIFI_FC0_SUBTYPE_MASK;

	if (subtype != WIFI_FC0_SUBTYPE_DEAUTH) {
		click_chatter("%{element} :: %s :: Received non-deauthentication packet",
				      this,
				      __func__);
		p->kill();
		return;
	}

	uint8_t *ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);

	uint16_t reason = le16_to_cpu(*(uint16_t *) ptr);

	EtherAddress src = EtherAddress(w->i_addr2);

    EmpowerStationState *ess = _el->get_ess(src);

    //If we're not aware of this LVAP, ignore
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

	EtherAddress dst = EtherAddress(w->i_addr1);
	EtherAddress bssid = EtherAddress(w->i_addr3);

	if (dst != bssid) {
		click_chatter("%{element} :: %s :: Strange dst/bssid combination %s/%s, ignoring",
				      this,
				      __func__,
				      dst.unparse().c_str(),
					  bssid.unparse().c_str());
		p->kill();
		return;
	}

	if (_debug) {
		click_chatter("%{element} :: %s :: src %s dst %s bssid %s",
				      this,
				      __func__,
				      src.unparse().c_str(),
					  dst.unparse().c_str(),
					  bssid.unparse().c_str());
	}

	//If the bssid does not match, ignore
	if (ess->_bssid != bssid) {
		click_chatter("%{element} :: %s :: BSSIDs do not match, expected %s received %s",
				      this,
				      __func__,
				      ess->_bssid.unparse().c_str(),
				      bssid.unparse().c_str());
		p->kill();
		return;
	}

	if (_debug) {
		click_chatter("%{element} :: %s :: Reason %u",
				      this,
				      __func__,
				      reason);
	}

	ess->_association_status = false;
	ess->_authentication_status = false;
	ess->_ssid = "";
	ess->_bssid = EtherAddress();

	_el->send_status_lvap(src);

	p->kill();

}

void EmpowerDeAuthResponder::send_deauth_request(EtherAddress dst, uint16_t reason, int iface_id)
{
	EmpowerStationState *ess = _el->get_ess(dst);

	if (_debug) {
		click_chatter("%{element} :: %s :: sending deauthentication request to %s",
					  this,
					  __func__,
					  dst.unparse().c_str());
	}

	int max_len = sizeof(struct click_wifi) + 2 + /* reason_code */ +
											  0;
	WritablePacket *p = Packet::make(max_len);

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
					  this,
					  __func__);
		return;
	}

	struct click_wifi *w = (struct click_wifi *) p->data();

	w->i_fc[0] = WIFI_FC0_VERSION_0 | WIFI_FC0_TYPE_MGT | WIFI_FC0_SUBTYPE_DEAUTH;
	w->i_fc[1] = WIFI_FC1_DIR_NODS;

	memcpy(w->i_addr1, dst.data(), 6);
	memcpy(w->i_addr2, ess->_bssid.data(), 6);
	memcpy(w->i_addr3, ess->_bssid.data(), 6);

	w->i_dur = 0;
	w->i_seq = 0;

	uint8_t *ptr;

	ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);

	*(uint16_t *) ptr = cpu_to_le16(reason);
	ptr += 2;

	click_chatter("%{element} :: %s :: Sending deauth message to sta %s bssid %s",
                  this,
				  __func__,
				  dst.unparse_colon().c_str(),
				  ess->_bssid.unparse_colon().c_str());

	SET_PAINT_ANNO(p, iface_id);
	output(0).push(p);

	return;
}

enum {
	H_DEBUG
};

String EmpowerDeAuthResponder::read_handler(Element *e, void *thunk) {
	EmpowerDeAuthResponder *td = (EmpowerDeAuthResponder *) e;
	switch ((uintptr_t) thunk) {
	case H_DEBUG:
		return String(td->_debug) + "\n";
	default:
		return String();
	}
}

int EmpowerDeAuthResponder::write_handler(const String &in_s, Element *e,
		void *vparam, ErrorHandler *errh) {

	EmpowerDeAuthResponder *f = (EmpowerDeAuthResponder *) e;
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

void EmpowerDeAuthResponder::add_handlers() {
	add_read_handler("debug", read_handler, (void *) H_DEBUG);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EmpowerDeAuthResponder)
