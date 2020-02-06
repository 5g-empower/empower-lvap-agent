/*
 * empowerbeaconsource.{cc,hh} -- sends 802.11 beacon packets (EmPOWER Access Point)
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
#include "empowerbeaconsource.hh"
#include <click/args.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
#include <clicknet/wifi.h>
#include "minstrel.hh"
#include "empowerpacket.hh"
#include "empowerlvapmanager.hh"
CLICK_DECLS

EmpowerBeaconSource::EmpowerBeaconSource() :
		_el(0), _period(500), _timer(this), _debug(false) {
}

EmpowerBeaconSource::~EmpowerBeaconSource() {
}

int EmpowerBeaconSource::configure(Vector<String> &conf, ErrorHandler *errh) {

	int ret = Args(conf, this, errh)
              .read_m("EL", ElementCastArg("EmpowerLVAPManager"), _el)
			  .read("PERIOD", _period)
			  .read("DEBUG", _debug).complete();

	return ret;

}

int EmpowerBeaconSource::initialize(ErrorHandler *) {
	_timer.initialize(this);
	_timer.schedule_now();
	return 0;
}

void EmpowerBeaconSource::run_timer(Timer *) {

	// send LVAP beacon
	for (LVAPIter it = _el->lvaps()->begin(); it.live(); it++) {
		int current_channel = _el->ifaces()->get(it.value()._iface_id)->_channel;
		for (int i = 0; i < it.value()._networks.size(); i++) {
			EtherAddress bssid = it.value()._networks[i]._bssid;
			String ssid = it.value()._networks[i]._ssid;
			if (it.value()._bssid == bssid && it.value()._ssid == ssid && it.value()._csa_active) {
				send_lvap_csa_beacon(&it.value());
			} else {
				send_beacon(it.value()._sta, bssid, ssid, current_channel, it.value()._iface_id, false, false, 0, 0, 0);
			}
		}
	}

	// send VAP beacons
	for (VAPIter it = _el->vaps()->begin(); it.live(); it++) {
		int current_channel = _el->ifaces()->get(it.value()._iface_id)->_channel;
		send_beacon(EtherAddress::make_broadcast(), it.value()._bssid,
				it.value()._ssid, current_channel, it.value()._iface_id,
				false, false, 0, 0, 0);
	}

	// re-schedule the timer with some jitter
	_timer.schedule_after_msec(_period);

}

void EmpowerBeaconSource::send_lvap_csa_beacon(EmpowerStationState *ess) {

	int current_channel = _el->ifaces()->get(ess->_iface_id)->_channel;

	if (_debug) {
		click_chatter("%{element} :: %s :: sending CSA to %s current channel %u target channel %u csa mode %u csa count %u",
					  this,
					  __func__,
					  ess->_sta.unparse().c_str(),
					  current_channel,
					  ess->_csa_switch_channel,
					  ess->_csa_switch_mode,
					  ess->_csa_switch_count);
	}

	send_beacon(ess->_sta, ess->_bssid, ess->_ssid, current_channel,
			ess->_iface_id, false, true, ess->_csa_switch_mode,
			ess->_csa_switch_count, ess->_csa_switch_channel);

	ess->_csa_switch_count--;

	if (ess->_csa_switch_count < 0) {

		click_chatter("%{element} :: %s :: CSA procedure for %s is over, removing LVAP",
					  this,
					  __func__,
					  ess->_sta.unparse().c_str());

		// remove lvap
		_el->remove_lvap(ess->_sta);
		// send del lvap response
		_el->send_add_del_lvap_response(EMPOWER_PT_DEL_LVAP_RESPONSE, ess->_sta, ess->_xid, 0);

	}

}

void EmpowerBeaconSource::send_beacon(EtherAddress dst, EtherAddress bssid,
		String ssid, int channel, int iface_id, bool probe, bool csa_active,
		int csa_mode, int csa_count, int csa_channel) {

	if (_debug) {
		click_chatter("%{element} :: %s :: dst %s bssid %s ssid %s channel %u iface_id %u",
					  this,
					  __func__,
					  dst.unparse().c_str(),
					  bssid.unparse().c_str(),
					  ssid.c_str(),
					  channel,
					  iface_id);
	}

	/* order elements by standard
	 * needed by sloppy 802.11b driver implementations
	 * to be able to connect to 802.11g APs
	 */
	int max_len = sizeof(struct click_wifi) +
		8 + /* timestamp */
		2 + /* beacon interval */
		2 + /* cap_info */
		2 + ssid.length() + /* ssid */
		2 + WIFI_RATES_MAXSIZE + /* rates */
		2 + 1 + /* ds param */
		2 + WIFI_RATES_MAXSIZE + /* xrates */
		2 + 4 + /* tim */
		2 + 26 + /* ht capabilities */
		2 + 22 + /* ht information */
		2 + 24 + /* wmm parameter element */
		0;

	// this is an actual LVAP and the CSA is active, notice that CSA will not
	// be activated for LVAP attached to shared SSIDs
	if (csa_active) {
		max_len += 2 + 3; /* channel switch announcement */
	}

	WritablePacket *p = Packet::make(max_len);
	memset(p->data(), 0, p->length());

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
				      this,
				      __func__);
		return;
	}

	struct click_wifi *w = (struct click_wifi *) p->data();

	w->i_fc[0] = WIFI_FC0_VERSION_0 | WIFI_FC0_TYPE_MGT;
	if (probe) {
		w->i_fc[0] |= WIFI_FC0_SUBTYPE_PROBE_RESP;
	} else {
		w->i_fc[0] |= WIFI_FC0_SUBTYPE_BEACON;
	}

	w->i_fc[1] = WIFI_FC1_DIR_NODS;

    memcpy(w->i_addr1, dst.data(), 6);
	memcpy(w->i_addr2, bssid.data(), 6);
	memcpy(w->i_addr3, bssid.data(), 6);

	w->i_dur = 0;
	w->i_seq = 0;

	uint8_t *ptr;

	ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);
	int actual_length = sizeof(struct click_wifi);

	/* timestamp is set in the hal. */
	memset(ptr, 0, 8);
	ptr += 8;
	actual_length += 8;

	/* beacon period */
	uint16_t beacon_int = (uint16_t) _period;
	*(uint16_t *) ptr = cpu_to_le16(beacon_int);
	ptr += 2;
	actual_length += 2;

	/* cap info */
	uint16_t cap_info = 0;
	cap_info |= WIFI_CAPINFO_ESS;
	*(uint16_t *) ptr = cpu_to_le16(cap_info);
	ptr += 2;
	actual_length += 2;

	/* ssid */
	ptr[0] = WIFI_ELEMID_SSID;
	ptr[1] = ssid.length();

	memcpy(ptr + 2, ssid.data(), ssid.length());
	ptr += 2 + ssid.length();
	actual_length += 2 + ssid.length();

	/* rates */
	TransmissionPolicies * tx_table = _el->get_tx_policies(iface_id);
	Vector<int> rates = tx_table->lookup(bssid)->_mcs;
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

	/* ds parameter set */
	if (channel < 14) {
		ptr[0] = WIFI_ELEMID_DSPARMS;
		ptr[1] = 1;
		ptr[2] = (uint8_t) channel;
		ptr += 2 + 1;
		actual_length += 2 + 1;
	}

	/* tim */
	if (!probe) {
		ptr[0] = WIFI_ELEMID_TIM;
		ptr[1] = 4;
		ptr[2] = 0; //count
		ptr[3] = 1; //period
		ptr[4] = 0; //bitmap control
		ptr[5] = 0; //partial virtual bitmap
		ptr += 2 + 4;
		actual_length += 2 + 4;
	}

	/* Channel switch */
	// channel switch mode mode: 0 = no requirements on the receiving STA, 1 = no further frames until the scheduled channel switch
	// channel switch count count: 0 indicates at any time after the beacon frame. 1 indicates the switch occurs immediately before the next beacon
	if (csa_active) {
		ptr[0] = WIFI_ELEMID_CSA;
		ptr[1] = 3; // length
		ptr[2] = (uint8_t) csa_mode;
		ptr[3] = (uint8_t) csa_channel;
		ptr[4] = (uint8_t) csa_count;
		ptr += 2 + 3;
		actual_length += 2 + 3;
	}

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
	SET_PAINT_ANNO(p, iface_id);
	output(0).push(p);

}

void EmpowerBeaconSource::push(int, Packet *p) {

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

	if (subtype != WIFI_FC0_SUBTYPE_PROBE_REQ) {
		click_chatter("%{element} :: %s :: Received non-probe request packet",
				      this,
				      __func__);
		p->kill();
		return;
	}

	uint8_t *ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);
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

	EtherAddress src = EtherAddress(w->i_addr2);
	String ssid = "";

    if (ssid_l && ssid_l[1]) {
		ssid = String((char *) ssid_l + 2, WIFI_MIN((int)ssid_l[1], WIFI_NWID_MAXSIZE));
	}

    StringAccum sa;
	Vector<int> rates;
	Vector<int> ht_rates;

	sa << "ProbeReq: " << src << " ssid ";

	if (ssid == "") {
		sa << "Broadcast";
	} else {
		sa << ssid;
	}

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

	/* print rates information */
	if (_debug) {
		click_chatter("%{element} :: %s :: %s",
				      this,
				      __func__,
				      sa.take_string().c_str());
	}

	// always ask to the controller because we may want to reject this request
	ResourceElement *el = _el->ifaces()->get(iface_id);
	if (htcaps && (el->_band == EMPOWER_BT_HT20)) {
		_el->send_probe_request(el->_iface_id, src, ssid, true, ht->ht_caps_info);
	} else {
		_el->send_probe_request(el->_iface_id, src, ssid, false, 0);
	}

	/* probe processed */
	p->kill();

}

void EmpowerBeaconSource::send_probe_response(EmpowerStationState *ess, String ssid) {

	if (!ess) {
		click_chatter("%{element} :: %s :: invalid ess",
				      this,
				      __func__);
	}

	int current_channel = _el->ifaces()->get(ess->_iface_id)->_channel;

	if (ssid == "") {

		// reply with all ssids
		for (int i = 0; i < ess->_networks.size(); i++) {
			send_beacon(ess->_sta, ess->_networks[i]._bssid, ess->_networks[i]._ssid,
					current_channel, ess->_iface_id, true, false, 0, 0, 0);
		}

		// reply also with all vaps
		for (VAPIter it = _el->vaps()->begin(); it.live(); it++) {
			send_beacon(ess->_sta, it.value()._bssid, it.value()._ssid,
					current_channel, it.value()._iface_id, true, false, 0,
					0, 0);
		}

	} else {

		// reply with lvap's ssid
		for (int i = 0; i < ess->_networks.size(); i++) {
			if (ess->_networks[i]._ssid == ssid) {
				send_beacon(ess->_sta, ess->_networks[i]._bssid, ess->_networks[i]._ssid,
						current_channel, ess->_iface_id, true, false, 0, 0, 0);
				break;
			}
		}

		// reply also with all vaps
		for (VAPIter it = _el->vaps()->begin(); it.live(); it++) {
			if (it.value()._ssid == ssid) {
				send_beacon(ess->_sta, it.value()._bssid, it.value()._ssid,
						current_channel, it.value()._iface_id, true, false,
						0, 0, 0);
			}
		}

	}

}

enum {
	H_DEBUG,
};

String EmpowerBeaconSource::read_handler(Element *e, void *thunk) {
	EmpowerBeaconSource *td = (EmpowerBeaconSource *) e;
	switch ((uintptr_t) thunk) {
	case H_DEBUG:
		return String(td->_debug) + "\n";
	default:
		return String();
	}
}

int EmpowerBeaconSource::write_handler(const String &in_s, Element *e,
		void *vparam, ErrorHandler *errh) {

	EmpowerBeaconSource *f = (EmpowerBeaconSource *) e;
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

void EmpowerBeaconSource::add_handlers() {
	add_read_handler("debug", read_handler, (void *) H_DEBUG);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EmpowerBeaconSource)
