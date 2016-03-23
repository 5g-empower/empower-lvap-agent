/*
 * empowerassociationresponder.{cc,hh} -- sends 802.11 association responses from requests (EmPOWER Access Point)
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
#include "empowerassociationresponder.hh"
#include <click/args.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <click/error.hh>
#include <clicknet/wifi.h>
#include <elements/wifi/minstrel.hh>
#include "empowerpacket.hh"
#include "empowerlvapmanager.hh"
CLICK_DECLS

EmpowerAssociationResponder::EmpowerAssociationResponder() :
		_el(0), _debug(false) {
}

EmpowerAssociationResponder::~EmpowerAssociationResponder() {
}

int EmpowerAssociationResponder::configure(Vector<String> &conf,
		ErrorHandler *errh) {

	int ret = Args(conf, this, errh)
              .read_m("EL", ElementCastArg("EmpowerLVAPManager"), _el)
			  .read("DEBUG", _debug).complete();

	return ret;

}

void EmpowerAssociationResponder::push(int, Packet *p) {

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

	if ((subtype != WIFI_FC0_SUBTYPE_ASSOC_REQ) && (subtype != WIFI_FC0_SUBTYPE_REASSOC_REQ)) {
		click_chatter("%{element} :: %s :: Received non-association request packet",
				      this,
				      __func__);
		p->kill();
		return;
	}

	EtherAddress src = EtherAddress(w->i_addr2);

	EmpowerStationState *ess = _el->lvaps()->get_pointer(src);

	//If we're not aware of this LVAP, ignore
	if (!ess) {
		click_chatter("%{element} :: %s :: Unknown station %s",
				      this,
				      __func__,
				      src.unparse().c_str());
		p->kill();
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

	EtherAddress dst = EtherAddress(w->i_addr1);
	EtherAddress bssid = EtherAddress(w->i_addr3);

	uint8_t *ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);

	/* capability */
	uint16_t capability = le16_to_cpu(*(uint16_t *) ptr);
	ptr += 2;

	/* listen interval */
	uint16_t lint = le16_to_cpu(*(uint16_t *) ptr);
	ptr += 2;

	/* re-assoc frame bear also the current ap field */
	if (subtype == WIFI_FC0_SUBTYPE_REASSOC_REQ) {
		ptr += 6;
	}

	uint8_t *end = (uint8_t *) p->data() + p->length();

	uint8_t *ssid_l = NULL;
	uint8_t *rates_l = NULL;
	uint8_t *htcaps = NULL;

	while (ptr < end) {
		switch (*ptr) {
		case WIFI_ELEMID_SSID:
			ssid_l = ptr;
			break;
		case WIFI_ELEMID_RATES:
			rates_l = ptr;
			break;
		case WIFI_ELEMID_HTCAPS:
			htcaps = ptr;
			break;
		default:
			if (_debug) {
				click_chatter("%{element} :: %s :: Ignored element id %u %u",
						      this,
						      __func__,
						      *ptr,
						      ptr[1]);
			}
		}
		ptr += ptr[1] + 2;
	}

	Vector<int> ht_rates;
	Vector<int> basic_rates;
	Vector<int> rates;
	if (rates_l) {
		for (int x = 0; x < WIFI_MIN((int)rates_l[1], WIFI_RATES_MAXSIZE); x++) {
			uint8_t rate = rates_l[x + 2];
			if (rate & WIFI_RATE_BASIC) {
				basic_rates.push_back((int) (rate & WIFI_RATE_VAL));
			} else {
				rates.push_back((int) (rate & WIFI_RATE_VAL));
			}
		}
	}

	String ssid;

	if (ssid_l && ssid_l[1]) {
		ssid = String((char *) ssid_l + 2,
				WIFI_MIN((int)ssid_l[1], WIFI_NWID_MAXSIZE));
	}

	if (!ssid) {
		click_chatter("%{element} :: %s :: Blank SSID from %s",
					  this,
					  __func__,
					  src.unparse().c_str());
		p->kill();
		return;
	}

	StringAccum sa;

	sa << "src " << src;
	sa << " dst " << dst;
	sa << " bssid " << bssid;
	sa << " ssid " << ssid;
	sa << " [ ";
	if (capability & WIFI_CAPINFO_ESS) {
		sa << "ESS ";
	}
	if (capability & WIFI_CAPINFO_IBSS) {
		sa << "IBSS ";
	}
	if (capability & WIFI_CAPINFO_CF_POLLABLE) {
		sa << "CF_POLLABLE ";
	}
	if (capability & WIFI_CAPINFO_CF_POLLREQ) {
		sa << "CF_POLLREQ ";
	}
	if (capability & WIFI_CAPINFO_PRIVACY) {
		sa << "PRIVACY ";
	}
	sa << "] ";

	sa << "listen_int " << lint << " ";

	sa << "( { ";
	for (int x = 0; x < basic_rates.size(); x++) {
		sa << basic_rates[x] << " ";
	}
	sa << "} ";
	for (int x = 0; x < rates.size(); x++) {
		sa << rates[x] << " ";
	}
	sa << ")";

	if (htcaps) {
		struct click_wifi_ht_caps *ht = (struct click_wifi_ht_caps *) htcaps;

		sa << " HT_CAPS [";

		if (ht->ht_caps_info & WIFI_HT_CI_LDPC) {
			sa << " LDPC";
		}

		if (ht->ht_caps_info & WIFI_HT_CI_CHANNEL_WIDTH_SET) {
			sa << " 20/40MHz";
		}

		int sm_ps = (ht->ht_caps_info & WIFI_HT_CI_SM_PS_MASK) >> WIFI_HT_CI_SM_PS_SHIFT;

		if (sm_ps == WIFI_HT_CI_SM_PS_STATIC) {
			sa << " PS_STATIC";
		} else if (sm_ps == WIFI_HT_CI_SM_PS_DYNAMIC) {
			sa << " PS_DYNAMIC";
		} else if (sm_ps == WIFI_HT_CI_SM_PS_DISABLED) {
			sa << " PS_DISABLED";
		}

		if (ht->ht_caps_info & WIFI_HT_CI_HT_GF) {
			sa << " HT Greenfield";
		}

		if (ht->ht_caps_info & WIFI_HT_CI_SGI_20) {
			sa << " SGI20";
		}

		if (ht->ht_caps_info & WIFI_HT_CI_SGI_40) {
			sa << " SGI40";
		}

		if (ht->ht_caps_info & WIFI_HT_CI_TX_STBC) {
			sa << " TX_STBC";
		}

		int rx_stbc = (ht->ht_caps_info & WIFI_HT_CI_RX_STBC_MASK) >> WIFI_HT_CI_RX_STBC_SHIFT;

		if (rx_stbc == WIFI_HT_CI_RX_STBC_1SS) {
			sa << " RX_STBC_1";
		} else if (rx_stbc == WIFI_HT_CI_RX_STBC_2SS) {
			sa << " RX_STBC_2";
		} else if (rx_stbc == WIFI_HT_CI_RX_STBC_3SS) {
			sa << " RX_STBC_3";
		}

		if (ht->ht_caps_info & WIFI_HT_CI_HT_DBACK) {
			sa << " Delayed Block ACK";
		}

		if (ht->ht_caps_info & WIFI_HT_CI_HT_MAX_AMSDU) {
			sa << " A-MSDU Length: 7935";
		} else {
			sa << " A-MSDU Length: 3839";
		}

		if (ht->ht_caps_info & WIFI_HT_CI_HT_DSSS_CCK) {
			sa << " DSSS_CCK_40MHz";
		}

		if (ht->ht_caps_info & WIFI_HT_CI_HT_INTOLLERANT) {
			sa << " 40MHz_Intollerant";
		}

		if (ht->ht_caps_info & WIFI_HT_CI_HT_LSIG_TXOP) {
			sa << " LSIG_TXOP";
		}

		int max_ampdu_length = ht->ampdu_params & WIFI_HT_CI_AMDU_PARAMS_MAX_AMPDU_LENGTH_MASK >> WIFI_HT_CI_AMDU_PARAMS_MAX_AMPDU_LENGTH_SHIFT;
		sa << " RX A-MPDU Length: " << (pow(2, 13 + max_ampdu_length) - 1);

		int mpdu_density = (ht->ampdu_params & WIFI_HT_CI_AMDU_PARAMS_MPDU_DENSITY_MASK) >> WIFI_HT_CI_AMDU_PARAMS_MPDU_DENSITY_SHIFT;
		if (mpdu_density == 0) {
			sa << " MPDU Density: no restrictions";
		} else if (mpdu_density == 1) {
			sa << " MPDU Density: 1/4us";
		} else if (mpdu_density == 2) {
			sa << " MPDU Density: 1/2us";
		} else if (mpdu_density == 3) {
			sa << " MPDU Density: 1us";
		} else if (mpdu_density == 4) {
			sa << " MPDU Density: 2us";
		} else if (mpdu_density == 5) {
			sa << " MPDU Density: 5us";
		} else if (mpdu_density == 6) {
			sa << " MPDU Density: 6us";
		} else if (mpdu_density == 7) {
			sa << " MPDU Density: 7us";
		}

		sa << " mcs {";
		for (int i = 0; i < 76; i++) {
			int c = (int) i / 8;
			int d = i % 8;
			if (ht->rx_supported_mcs[c] & (1<<d)) {
				ht_rates.push_back(i);
				sa << " " << (int) i;
			} else {
				break;
			}
		}
		sa << " }";

		uint16_t max_tp;
		memcpy(&max_tp, &ht->rx_supported_mcs[10], 2);

		sa << " Rx Highest TP: " << max_tp;

		for (int i = 0; i < 16; i++) {
			click_chatter("rx_supported_mcs[%u]=%u", i, ht->rx_supported_mcs[i]);
		}

		if ((ht->rx_supported_mcs[12] & WIFI_HT_CI_SM12_TX_MCS_SET_DEFINED) && !(ht->rx_supported_mcs[12] & WIFI_HT_CI_SM12_TX_RX_MCS_SET_NOT_EQUAL)) {
			sa << " TX " << ht_rates.size() / 8 << "SS";
		}

		if ((ht->rx_supported_mcs[12] & WIFI_HT_CI_SM12_TX_MCS_SET_DEFINED) && (ht->rx_supported_mcs[12] & WIFI_HT_CI_SM12_TX_RX_MCS_SET_NOT_EQUAL)) {
			int max_ss= ht->rx_supported_mcs[11] & WIFI_HT_CI_SM12_TX_MAX_SS_MASK >> WIFI_HT_CI_SM12_TX_MAX_SS_SHIFT;
			if (max_ss == 0) {
				sa << " TX 1SS";
			} else if (max_ss == 1){
				sa << " TX 2SS";
			} else if (max_ss == 2){
				sa << " TX 3SS";
			} else if (max_ss == 3){
				sa << " TX 4SS";
			}
		}

		if (ht->rx_supported_mcs[11] & WIFI_HT_CI_SM12_TX_UEQM) {
			sa << " UEQM";
		}

		sa << " ]";
	}

	if (_debug) {
		click_chatter("%{element} :: %s :: %s",
				      this,
				      __func__,
				      sa.take_string().c_str());
	}

	//If the bssid does not match, ignore
	if (ess->_lvap_bssid != bssid) {
		click_chatter("%{element} :: %s :: BSSID does not match, expected %s received %s",
				      this,
				      __func__,
				      ess->_lvap_bssid.unparse().c_str(),
				      bssid.unparse().c_str());
		p->kill();
		return;
	}

	Vector<String>::const_iterator it = ess->_ssids.begin();

	while (it != ess->_ssids.end()) {
		if (*it == ssid && ess->_lvap_bssid ==  bssid) {
			break;
		}
		it++;
	}

	if (it == ess->_ssids.end()) {

		VAPIter vit = _el->vaps()->begin();

		while (vit != _el->vaps()->end()) {
			if (vit.value()._ssid == ssid && vit.value()._net_bssid == bssid) {
				break;
			}
			vit++;
		}

		if (vit == _el->vaps()->end()) {

			click_chatter("%{element} :: %s :: Invalid SSID %s from %s",
						  this,
						  __func__,
						  ssid.c_str(),
						  src.unparse().c_str());
			p->kill();
			return;

		}
	}

	_el->send_association_request(src, bssid, ssid);
	p->kill();

}

void EmpowerAssociationResponder::send_association_response(EtherAddress dst,
		uint16_t status, int iface_id) {

    EmpowerStationState *ess = _el->lvaps()->get_pointer(dst);
	ess->_association_status = true;

	if (_debug) {
		click_chatter("%{element} :: %s :: association %s assoc_id %d",
				      this,
				      __func__,
				      dst.unparse().c_str(),
				      ess->_assoc_id);
	}

	int max_len = sizeof(struct click_wifi) + 2 + /* cap_info */
											  2 + /* status  */
											  2 + /* assoc_id */
											  2 + WIFI_RATES_MAXSIZE + /* rates */
											  2 + WIFI_RATES_MAXSIZE + /* xrates */
											  0;

	WritablePacket *p = Packet::make(max_len);

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
				      this,
				      __func__);
		return;
	}

	struct click_wifi *w = (struct click_wifi *) p->data();

	w->i_fc[0] = WIFI_FC0_VERSION_0 | WIFI_FC0_TYPE_MGT
			| WIFI_FC0_SUBTYPE_ASSOC_RESP;
	w->i_fc[1] = WIFI_FC1_DIR_NODS;

	memcpy(w->i_addr1, dst.data(), 6);
	memcpy(w->i_addr2, ess->_lvap_bssid.data(), 6);
	memcpy(w->i_addr3, ess->_lvap_bssid.data(), 6);

	w->i_dur = 0;
	w->i_seq = 0;

	uint8_t *ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);
	int actual_length = sizeof(struct click_wifi);

	uint16_t cap_info = 0;
	cap_info |= WIFI_CAPINFO_ESS;
	*(uint16_t *) ptr = cpu_to_le16(cap_info);
	ptr += 2;
	actual_length += 2;

	*(uint16_t *) ptr = cpu_to_le16(status);
	ptr += 2;
	actual_length += 2;

	*(uint16_t *) ptr = cpu_to_le16(0xc000 | ess->_assoc_id);
	ptr += 2;
	actual_length += 2;

	/* rates */

	Minstrel * rc = _el->rcs()->at(iface_id);
	AvailableRates * rtable = rc->rtable();

	Vector<int> rates = rtable->lookup(ess->_sta);
	ptr[0] = WIFI_ELEMID_RATES;
	ptr[1] = WIFI_MIN(WIFI_RATE_SIZE, rates.size());
	for (int x = 0; x < WIFI_MIN(WIFI_RATE_SIZE, rates.size()); x++) {
		ptr[2 + x] = (uint8_t) rates[x];
		if (rates[x] == 2 || rates[x] == 12) {
			ptr[2 + x] |= WIFI_RATE_BASIC;
		}
	}
	ptr += 2 + WIFI_MIN(WIFI_RATE_SIZE, rates.size());
	actual_length += 2 + WIFI_MIN(WIFI_RATE_SIZE, rates.size());

	int num_xrates = rates.size() - WIFI_RATE_SIZE;
	if (num_xrates > 0) {
		/* rates */
		ptr[0] = WIFI_ELEMID_XRATES;
		ptr[1] = num_xrates;
		for (int x = 0; x < num_xrates; x++) {
			ptr[2 + x] = (uint8_t) rates[x + WIFI_RATE_SIZE];
			if (rates[x + WIFI_RATE_SIZE] == 2 || rates[x + WIFI_RATE_SIZE] == 12) {
				ptr[2 + x] |= WIFI_RATE_BASIC;
			}
		}
		ptr += 2 + num_xrates;
		actual_length += 2 + num_xrates;
	}

	p->take(max_len - actual_length);

	_el->send_status_lvap(dst);

	SET_PAINT_ANNO(p, iface_id);
	output(0).push(p);

}

enum {
	H_DEBUG
};

String EmpowerAssociationResponder::read_handler(Element *e, void *thunk) {
	EmpowerAssociationResponder *td = (EmpowerAssociationResponder *) e;
	switch ((uintptr_t) thunk) {
	case H_DEBUG:
		return String(td->_debug) + "\n";
	default:
		return String();
	}
}

int EmpowerAssociationResponder::write_handler(const String &in_s, Element *e,
		void *vparam, ErrorHandler *errh) {

	EmpowerAssociationResponder *f = (EmpowerAssociationResponder *) e;
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

void EmpowerAssociationResponder::add_handlers() {
	add_read_handler("debug", read_handler, (void *) H_DEBUG);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EmpowerAssociationResponder)
