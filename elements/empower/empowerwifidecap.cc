/*
 * empowerwifidecap.{cc,hh} -- decapsultates 802.11 packets (EmPOWER Access Point)
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
#include "empowerwifidecap.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>
#include <clicknet/ether.h>
#include <click/packet_anno.hh>
#include <click/etheraddress.hh>
#include "minstrel.hh"
#include "empowerlvapmanager.hh"
CLICK_DECLS

EmpowerWifiDecap::EmpowerWifiDecap() :
		_el(0), _debug(false) {
}

EmpowerWifiDecap::~EmpowerWifiDecap() {
}

int EmpowerWifiDecap::configure(Vector<String> &conf,
		ErrorHandler *errh) {

	return Args(conf, this, errh)
			.read_m("EL", ElementCastArg("EmpowerLVAPManager"), _el)
			.read("DEBUG", _debug)
			.complete();

}

void
EmpowerWifiDecap::push(int, Packet *p) {

	if (p->length() < sizeof(struct click_wifi)) {
		click_chatter("%{element} :: %s :: packet too small: %d vs %d",
				      this,
				      __func__,
				      p->length(),
				      sizeof(struct click_wifi));
		p->kill();
		return;
	}

	struct click_wifi *w = (struct click_wifi *) p->data();

	unsigned wifi_header_size = sizeof(struct click_wifi);

	if ((w->i_fc[1] & WIFI_FC1_DIR_MASK) == WIFI_FC1_DIR_DSTODS)
		wifi_header_size += WIFI_ADDR_LEN;

	if (WIFI_QOS_HAS_SEQ(w))
		wifi_header_size += sizeof(uint16_t);

	struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

	if ((ceh->magic == WIFI_EXTRA_MAGIC) && ceh->pad && (wifi_header_size & 3))
		wifi_header_size += 4 - (wifi_header_size & 3);

	if (p->length() < wifi_header_size) {
		if (_debug) {
			click_chatter("%{element} :: %s :: invalid length: %d < %d",
					      this,
					      __func__,
					      p->length(),
					      wifi_header_size + sizeof(struct click_llc));
		}
		p->kill();
		return;
	}

	if (w->i_fc[1] & WIFI_FC1_WEP) {
		p->kill();
		return;
	}

	uint8_t dir = w->i_fc[1] & WIFI_FC1_DIR_MASK;

	EtherAddress dst;
	EtherAddress src;
	EtherAddress bssid;

	switch (dir) {
	case WIFI_FC1_DIR_NODS:
		dst = EtherAddress(w->i_addr1);
		src = EtherAddress(w->i_addr2);
		bssid = EtherAddress(w->i_addr3);
		break;
	case WIFI_FC1_DIR_TODS:
		bssid = EtherAddress(w->i_addr1);
		src = EtherAddress(w->i_addr2);
		dst = EtherAddress(w->i_addr3);
		break;
	case WIFI_FC1_DIR_FROMDS:
		dst = EtherAddress(w->i_addr1);
		bssid = EtherAddress(w->i_addr2);
		src = EtherAddress(w->i_addr3);
		break;
	case WIFI_FC1_DIR_DSTODS:
		dst = EtherAddress(w->i_addr1);
		src = EtherAddress(w->i_addr2);
		bssid = EtherAddress(w->i_addr3);
		break;
	default:
		click_chatter("%{element} :: %s :: invalid dir %d",
				      this,
				      __func__,
				      dir);
		p->kill();
		return;
	}

    EmpowerStationState *ess = _el->get_ess(src);

    if (!ess) {
		p->kill();
		return;
	}

	if (ess->_bssid != bssid) {
		p->kill();
		return;
	}

	if (!ess->_authentication_status) {
		click_chatter("%{element} :: %s :: station %s not authenticated",
				      this,
				      __func__,
				      src.unparse().c_str());
		p->kill();
		return;
	}

	if (!ess->_association_status) {
		click_chatter("%{element} :: %s :: station %s not associated",
				      this,
				      __func__,
				      src.unparse().c_str());
		p->kill();
		return;
	}

	/* broadcast uplink only frame, silently ignore */
	if ((dst.is_broadcast() || dst.is_group()) && !ess->_set_mask) {
		p->kill();
		return;
	}

	WritablePacket *p_out = p->uniqueify();
	if (!p_out) {
		return;
	}

	TxPolicyInfo * txp = _el->get_txp(src);

	// frame must be encapsulated in another Ethernet frame
	if (ess->_encap) {

		p_out = p_out->push_mac_header(14);

		if (!p_out) {
			return;
		}

		uint16_t ether_type = 0xBBBB;

		memcpy(p_out->data(), ess->_encap.data(), 6);
		memcpy(p_out->data() + 6, ess->_sta.data(), 6);
		memcpy(p_out->data() + 12, &ether_type, 2);

		txp->update_rx(p_out->length());

		if (Packet *clone = p->clone())
			output(1).push(clone);

		output(0).push(p);

		return;

	}

	// normal wifi decap
	uint16_t ether_type;
	if (!memcmp(WIFI_LLC_HEADER, p_out->data() + wifi_header_size, WIFI_LLC_HEADER_LEN)) {
		memcpy(&ether_type, p_out->data() + wifi_header_size + sizeof(click_llc) - 2, 2);
	} else {
		p_out->kill();
		return;
	}

	p_out->pull(wifi_header_size + sizeof(struct click_llc));

	p_out = p_out->push_mac_header(14);

	if (!p_out) {
		return;
	}

	memcpy(p_out->data(), dst.data(), 6);
	memcpy(p_out->data() + 6, src.data(), 6);
	memcpy(p_out->data() + 12, &ether_type, 2);

	txp->update_rx(p_out->length());

	if (Packet *clone = p->clone())
		output(1).push(clone);

	output(0).push(p);

}

enum {
	H_DEBUG
};

String EmpowerWifiDecap::read_handler(Element *e, void *thunk) {
	EmpowerWifiDecap *td = (EmpowerWifiDecap *) e;
	switch ((uintptr_t) thunk) {
	case H_DEBUG:
		return String(td->_debug) + "\n";
	default:
		return String();
	}
}

int EmpowerWifiDecap::write_handler(const String &in_s, Element *e,
		void *vparam, ErrorHandler *errh) {

	EmpowerWifiDecap *f = (EmpowerWifiDecap *) e;
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

void EmpowerWifiDecap::add_handlers() {
	add_read_handler("debug", read_handler, (void *) H_DEBUG);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EmpowerWifiDecap)
ELEMENT_REQUIRES(userlevel)
