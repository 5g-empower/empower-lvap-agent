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
#include <elements/wifi/minstrel.hh>
#include "empowerpacket.hh"
#include "empowerlvapmanager.hh"
CLICK_DECLS

EmpowerBeaconSource::EmpowerBeaconSource() :
		_el(0), _period(100), _timer(this), _debug(false) {
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
		for (int i = 0; i < it.value()._ssids.size(); i++) {
			send_beacon(it.key(), it.value()._net_bssid, it.value()._ssids[i],
					it.value()._channel, it.value()._iface_id, false);
		}
	}

	// send VAP beacons
	for (VAPIter it = _el->vaps()->begin(); it.live(); it++) {
		send_beacon(EtherAddress::make_broadcast(), it.value()._net_bssid,
				it.value()._ssid, it.value()._channel, it.value()._iface_id,
				false);
	}

	// re-schedule the timer with some jitter
	_timer.schedule_after_msec(_period);

}

void EmpowerBeaconSource::send_beacon(EtherAddress dst, EtherAddress bssid,
		String ssid, int channel, int iface_id, bool probe) {

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
		2 + 1 + /* ds parms */
		2 + 4 + /* tim */
		/* 802.11g Information fields */
		2 + WIFI_RATES_MAXSIZE + /* xrates */
		0;

	WritablePacket *p = Packet::make(max_len);
	memset(p->data(), 0, p->length());

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
				      this,
				      __func__);
		return;
	}

	struct click_wifi *w = (struct click_wifi *) p->data();
	struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
	ceh->flags |= WIFI_EXTRA_TX_NOACK;

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

	uint16_t beacon_int = (uint16_t) _period;
	*(uint16_t *) ptr = cpu_to_le16(beacon_int);
	ptr += 2;
	actual_length += 2;

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
	Minstrel * rc = _el->rcs()->at(iface_id);
	AvailableRates * rtable = rc->rtable();

	Vector<int> rates = rtable->lookup(bssid);
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

	/* channel */
	ptr[0] = WIFI_ELEMID_DSPARMS;
	ptr[1] = 1;
	ptr[2] = (uint8_t) channel;
	ptr += 2 + 1;
	actual_length += 2 + 1;

	/* tim */
	ptr[0] = WIFI_ELEMID_TIM;
	ptr[1] = 4;

	ptr[2] = 0; //count
	ptr[3] = 1; //period
	ptr[4] = 0; //bitmap control
	ptr[5] = 0; //partial virtual bitmap
	ptr += 2 + 4;
	actual_length += 2 + 4;

	/* 802.11g fields */
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
			rates.push_front(rate);
			if (rate & WIFI_RATE_BASIC ) {
				sa << " *" << (int) (rate ^ WIFI_RATE_BASIC);
			}else {
				sa << " " << (int) rate;
			}
		}
	}

	if (rates_x) {
	    int len = rates_x[1];
		for (int x = 0; x < len; x++) {
			uint8_t rate = rates_x[x + 2];
			rates.push_front(rate);
			if (rate & WIFI_RATE_BASIC ) {
				sa << " *" << (int) (rate ^ WIFI_RATE_BASIC);
			}else {
				sa << " " << (int) rate;
			}
		}
	}

	Minstrel * rc = _el->rcs()->at(iface_id);
	AvailableRates * rtable = rc->rtable();

	rtable->insert(src, rates);

	sa << " }";

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

		if ((ht->rx_supported_mcs[12] & WIFI_HT_CI_SM12_TX_MCS_SET_DEFINED) && !(ht->rx_supported_mcs[12] & WIFI_HT_CI_SM12_TX_RX_MCS_SET_NOT_EQUAL)) {
			sa << " TX " << ht_rates.size() / 8 << "SS";
		}

		if ((ht->rx_supported_mcs[12] & WIFI_HT_CI_SM12_TX_MCS_SET_DEFINED) && ht->rx_supported_mcs[12] & WIFI_HT_CI_SM12_TX_RX_MCS_SET_NOT_EQUAL) {
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

	AvailableRates * rtable_ht = rc->rtable_ht();
	rtable_ht->insert(src, ht_rates);

	/* print rates information */
	if (_debug) {
		click_chatter("%{element} :: %s :: %s",
				      this,
				      __func__,
				      sa.take_string().c_str());
	}

	/* If we're not aware of this LVAP, then send to the controller.
	 * The controller may also decide not to reply for example if the
	 * STA is already handled by another LVAP (which is in charge for
	 * generating the probe response).
	 */

	EmpowerStationState *ess = _el->lvaps()->get_pointer(src);

	if (!ess) {
		if (_debug) {
			click_chatter("%{element} :: %s :: sending probe request to ctrl %s",
					      this,
					      __func__,
					      src.unparse().c_str());
		}
		_el->send_probe_request(src, ssid, iface_id);
		p->kill();
		return;
	}

	// if this is an uplink only lvap then ignore request
	if (!ess->_set_mask) {
		p->kill();
		return;
	}

	/* If the client is performing an active scan, then
     * then respond from all available SSIDs. Else, if
     * the client is probing for a particular SSID, check
     * if we're indeed hosting that SSID and respond
     * accordingly.
     */

	if (ssid == "") {

		// reply with lvap's ssid
		for (int i = 0; i < ess->_ssids.size(); i++) {
			send_beacon(src, ess->_net_bssid, ess->_ssids[i], ess->_channel,
					ess->_iface_id, true);
		}

		// reply also with all vaps
		for (VAPIter it = _el->vaps()->begin(); it.live(); it++) {
			send_beacon(src, it.value()._net_bssid, it.value()._ssid,
					it.value()._channel, it.value()._iface_id, true);
		}

	} else {

		// reply with lvap's ssid
		for (int i = 0; i < ess->_ssids.size(); i++) {
			if (ess->_ssids[i] == ssid) {
				send_beacon(src, ess->_net_bssid, ssid, ess->_channel,
						ess->_iface_id, true);
				break;
			}
		}

		// reply also with all vaps
		for (VAPIter it = _el->vaps()->begin(); it.live(); it++) {
			if (it.value()._ssid == ssid) {
				send_beacon(src, it.value()._net_bssid, it.value()._ssid,
						it.value()._channel, it.value()._iface_id, true);
			}
		}

	}

	/* probe processed */
	p->kill();

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
