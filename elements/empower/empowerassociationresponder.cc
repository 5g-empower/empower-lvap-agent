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
#include "minstrel.hh"
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

	// if the lvap is not authenticated then ignore request
	if (!ess->_authentication_status) {
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
	uint8_t *rates_x = NULL;
	uint8_t *htcaps = NULL;

	while (ptr < end) {
		switch (*ptr) {
		case WIFI_ELEMID_SSID:
			ssid_l = ptr;
			break;
		case WIFI_ELEMID_RATES:
			rates_l = ptr;
			break;
		case WIFI_ELEMID_XRATES:
			rates_x = ptr;
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
	Vector<int> rates;

	String ssid;

	if (ssid_l && ssid_l[1]) {
		ssid = String((char *) ssid_l + 2,
				WIFI_MIN((int)ssid_l[1], WIFI_NWID_MAXSIZE));
	}

	if (!ssid) {
		p->kill();
		return;
	}

	StringAccum sa;

	sa << "AssocReq: src " << src;
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

	sa << " rates {";

	if (rates_l) {
	    int max_len =  WIFI_MIN((int)rates_l[1], WIFI_RATES_MAXSIZE);
		for (int x = 0; x < max_len; x++) {
			uint8_t rate = rates_l[x + 2];
			rates.push_back((int)(rate & WIFI_RATE_VAL));
			if (rate & WIFI_RATE_BASIC ) {
				sa << " *" << (int) (rate ^ WIFI_RATE_BASIC);
			} else {
				sa << " " << (int) rate;
			}
		}
	}

	sa << " }";

	if (rates_x) {
	    int len = rates_x[1];
		for (int x = 0; x < len; x++) {
			uint8_t rate = rates_x[x + 2];
			rates.push_back((int)(rate & WIFI_RATE_VAL));
			if (rate & WIFI_RATE_BASIC ) {
				sa << " *" << (int) (rate ^ WIFI_RATE_BASIC);
			} else {
				sa << " " << (int) rate;
			}
		}
	}

	struct click_wifi_ht_caps *ht;

	if (htcaps) {

		ht = (struct click_wifi_ht_caps *) htcaps;

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
			sa << " MPDU Density: 4us";
		} else if (mpdu_density == 6) {
			sa << " MPDU Density: 8us";
		} else if (mpdu_density == 7) {
			sa << " MPDU Density: 16us";
		}

		sa << " SUPPORTED MCSes {";
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

	// if bssid does not match then ignore request
	if (ess->_bssid != bssid) {
		p->kill();
		return;
	}

	// always ask to the controller because we may want to reject this request
	int band = _el->ifaces()->get(ess->_iface_id)->_band;
	if (htcaps && (band == EMPOWER_BT_HT20)) {
		_el->send_association_request(src, bssid, ssid, true, ht->ht_caps_info);
	} else {
		_el->send_association_request(src, bssid, ssid, false, 0);
	}

	p->kill();
	return;

}

void EmpowerAssociationResponder::send_association_response(EtherAddress dst) {

    EmpowerStationState *ess = _el->get_ess(dst);

    String ssid = ess->_ssid;
    uint16_t status = WIFI_STATUS_SUCCESS;
    int iface_id = ess->_iface_id;
    int channel = _el->ifaces()->get(ess->_iface_id)->_channel;

	if (_debug) {
		click_chatter("%{element} :: %s :: dst %s assoc_id %d ssid %s channel %u",
				      this,
				      __func__,
				      dst.unparse().c_str(),
				      ess->_assoc_id,
					  ssid.c_str(),
					  channel);
	}

	int max_len = sizeof(struct click_wifi) + 2 + /* cap_info */
											  2 + /* status  */
											  2 + /* assoc_id */
											  2 + WIFI_RATES_MAXSIZE + /* rates */
											  2 + WIFI_RATES_MAXSIZE + /* xrates */
											  2 + 26 + /* ht capabilities */
											  2 + 22 + /* ht information */
											  2 + 24 + /* wmm parameter element */
											  0;

	WritablePacket *p = Packet::make(max_len);

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
				      this,
				      __func__);
		return;
	}

	struct click_wifi *w = (struct click_wifi *) p->data();

	w->i_fc[0] = WIFI_FC0_VERSION_0 | WIFI_FC0_TYPE_MGT | WIFI_FC0_SUBTYPE_ASSOC_RESP;
	w->i_fc[1] = WIFI_FC1_DIR_NODS;

	memcpy(w->i_addr1, dst.data(), 6);
	memcpy(w->i_addr2, ess->_bssid.data(), 6);
	memcpy(w->i_addr3, ess->_bssid.data(), 6);

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
	TransmissionPolicies * tx_table = _el->get_tx_policies(iface_id);
	Vector<int> rates = tx_table->lookup(ess->_sta)->_mcs;
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

	/* extended supported rates */
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

	/* 802.11n fields */
	ResourceElement *elm = _el->ifaces()->get(iface_id);
	if (elm->_band == EMPOWER_BT_HT20) {

		/* ht capabilities */
		struct click_wifi_ht_caps *ht = (struct click_wifi_ht_caps *) ptr;
		ht->type = WIFI_HT_CAPS_TYPE;
		ht->len = WIFI_HT_CAPS_SIZE;

		//ht->ht_caps_info |= WIFI_HT_CI_LDPC;
		//ht->ht_caps_info |= WIFI_HT_CI_CHANNEL_WIDTH_SET;
		ht->ht_caps_info |= WIFI_HT_CI_SM_PS_DISABLED << WIFI_HT_CI_SM_PS_SHIFT;
		ht->ht_caps_info |= WIFI_HT_CI_HT_GF;
		ht->ht_caps_info |= WIFI_HT_CI_SGI_20;
		ht->ht_caps_info |= WIFI_HT_CI_SGI_40;
		//ht->ht_caps_info |= WIFI_HT_CI_TX_STBC;
		//ht->ht_caps_info |= WIFI_HT_CI_RX_STBC_1SS << WIFI_HT_CI_RX_STBC_SHIFT;
		//ht->ht_caps_info |= WIFI_HT_CI_HT_DBACK;
		ht->ht_caps_info |= WIFI_HT_CI_HT_MAX_AMSDU; /* max A-MSDU length 7935 */
		//ht->ht_caps_info |= WIFI_HT_CI_HT_DSSS_CCK;
		//ht->ht_caps_info |= WIFI_HT_CI_HT_PSMP;
		//ht->ht_caps_info |= WIFI_HT_CI_HT_INTOLLERANT;
		//ht->ht_caps_info |= WIFI_HT_CI_HT_LSIG_TXOP;

		ht->ampdu_params = 0x1b; /* 8191 MPDU Length, 8usec density */

		ht->rx_supported_mcs[0] = 0xff; /* MCS 0-7 */
		ht->rx_supported_mcs[1] = 0xff; /* MCS 8-15 */

		ht->ht_extended_caps = 0x0400;

		ptr += 2 + WIFI_HT_CAPS_SIZE;
		actual_length += 2 + WIFI_HT_CAPS_SIZE;

		/* ht information */
		struct click_wifi_ht_info *ht_info = (struct click_wifi_ht_info *) ptr;
		ht_info->type = WIFI_ELEMID_HTINFO;
		ht_info->len = WIFI_HT_INFO_SIZE;

		ht_info->primary_channel = (uint8_t) channel;
		ht_info->ht_info_1_3 = 0x08;
		ht_info->ht_info_2_3 = 0x0004;
		ht_info->ht_info_3_3 = 0x0;

		ptr += 2 + WIFI_HT_INFO_SIZE;
		actual_length += 2 + WIFI_HT_INFO_SIZE;

		struct click_wifi_wmm *wmm_info = (struct click_wifi_wmm *) ptr;

		wmm_info->tag_type = WIFI_ELEMID_VENDOR;
		wmm_info->len = WIFI_WME_LEN;

		const char* oui_temp = WIFI_WME_OUI;
		memcpy(wmm_info->oui, oui_temp, WIFI_WME_OUI_LEN);

		wmm_info->type = WIFI_WME_TYPE;
		wmm_info->subtype = WIFI_WME_SUBTYPE;
		wmm_info->version = WIFI_WME_VERSION;
		wmm_info->qosinfo = (1 << WIFI_WME_APSD_SHIFT) | WIFI_WME_APSD_MASK;
		wmm_info->reserved = 0x0;

		wmm_info->acparam[0].aci = WIFI_WME_AC_BE_ACI;
		wmm_info->acparam[0].ecw = WIFI_WME_AC_BE_ECW;
		wmm_info->acparam[0].txop = WIFI_WME_AC_BE_TXOP;

		wmm_info->acparam[1].aci = WIFI_WME_AC_BK_ACI;
		wmm_info->acparam[1].ecw = WIFI_WME_AC_BK_ECW;
		wmm_info->acparam[1].txop = WIFI_WME_AC_BK_TXOP;

		wmm_info->acparam[2].aci = WIFI_WME_AC_VI_ACI;
		wmm_info->acparam[2].ecw = WIFI_WME_AC_VI_ECW;
		wmm_info->acparam[2].txop = WIFI_WME_AC_VI_TXOP;

		wmm_info->acparam[3].aci = WIFI_WME_AC_VO_ACI;
		wmm_info->acparam[3].ecw = WIFI_WME_AC_VO_ECW;
		wmm_info->acparam[3].txop = WIFI_WME_AC_VO_TXOP;

		ptr += 2 + WIFI_WME_LEN;
		actual_length += 2 + WIFI_WME_LEN;
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
