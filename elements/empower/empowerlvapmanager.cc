/*
 * empowerlvapmanager.{cc,hh} -- manages LVAPS (EmPOWER Access Point)
 * Roberto Riggio
 *
 * Copyright (c) 2013 CREATE-NET
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
#include "empowerlvapmanager.hh"
#include <click/straccum.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>
#include <clicknet/ether.h>
#include <elements/standard/counter.hh>
#include <elements/wifi/minstrel.hh>
#include <elements/wifi/transmissionpolicy.hh>
#include "empowerpacket.hh"
#include "empowerbeaconsource.hh"
#include "empoweropenauthresponder.hh"
#include "empowerassociationresponder.hh"
#include "empowerdeauthresponder.hh"
#include "empowerdisassocresponder.hh"
#include "empowerrxstats.hh"
#include "empowercqm.hh"
CLICK_DECLS

EmpowerLVAPManager::EmpowerLVAPManager() :
		_e11k(0), _ebs(0), _eauthr(0), _eassor(0), _edeauthr(0), _ers(0),
		_cqm(0), _mtbl(0), _timer(this), _seq(0), _period(5000), _debug(false) {
}

EmpowerLVAPManager::~EmpowerLVAPManager() {
}

int EmpowerLVAPManager::initialize(ErrorHandler *) {
	_timer.initialize(this);
	_timer.schedule_now();
	compute_bssid_mask();
	return 0;
}

void EmpowerLVAPManager::run_timer(Timer *) {
	// send hello packet
	send_hello();
	// re-schedule the timer with some jitter
	unsigned max_jitter = _period / 10;
	unsigned j = click_random(0, 2 * max_jitter);
	_timer.schedule_after_msec(_period + j - max_jitter);
}

void cp_slashvec(const String &str, Vector<String> &conf) {
	int start = 0;
	int pos = str.find_left('/');
	int len = str.length();
	do {
	    conf.push_back(str.substring(start, pos - start));
		start = pos + 1;
	} while ((pos = str.find_left('/', start)) != -1);
	conf.push_back(str.substring(start, len - start));
}

int EmpowerLVAPManager::configure(Vector<String> &conf,
		ErrorHandler *errh) {

	int res = 0;
	String debugfs_strings;
	String rcs_strings;
	String res_strings;

	res = Args(conf, this, errh).read_m("WTP", _wtp)
						        .read_m("E11K", ElementCastArg("Empower11k"), _e11k)
						        .read_m("EBS", ElementCastArg("EmpowerBeaconSource"), _ebs)
			                    .read_m("EAUTHR", ElementCastArg("EmpowerOpenAuthResponder"), _eauthr)
			                    .read_m("EASSOR", ElementCastArg("EmpowerAssociationResponder"), _eassor)
								.read_m("EDEAUTHR", ElementCastArg("EmpowerDeAuthResponder"), _edeauthr)
			                    .read_m("DEBUGFS", debugfs_strings)
			                    .read_m("RCS", rcs_strings)
			                    .read_m("RES", res_strings)
			                    .read_m("ERS", ElementCastArg("EmpowerRXStats"), _ers)
			                    .read("CQM", ElementCastArg("EmpowerCQM"), _cqm)
								.read("MTBL", ElementCastArg("EmpowerMulticastTable"), _mtbl)
								.read("PERIOD", _period)
			                    .read("DEBUG", _debug)
			                    .complete();

	cp_spacevec(debugfs_strings, _debugfs_strings);

	for (int i = 0; i < _debugfs_strings.size(); i++) {
		_masks.push_back(EtherAddress::make_broadcast());
	}

	Vector<String> tokens;
	cp_spacevec(rcs_strings, tokens);

	for (int i = 0; i < tokens.size(); i++) {
		Minstrel * rc;
		if (!ElementCastArg("Minstrel").parse(tokens[i], rc, Args(conf, this, errh))) {
			return errh->error("error param %s: must be a Minstrel element", tokens[i].c_str());
		}
		_rcs.push_back(rc);
	}

	if (_rcs.size() != _masks.size()) {
		return errh->error("rcs has %u values, while masks has %u values", _rcs.size(), _masks.size());
	}

	tokens.clear();
	cp_spacevec(res_strings, tokens);

	for (int x = 0; x < tokens.size(); x++) {

		Vector<String> tokens_re;

		cp_slashvec(tokens[x], tokens_re);

		if (tokens_re.size() != 3) {
			return errh->error(
					"error param %s: must be in the form <hwaddr>/<channel>/band>",
					tokens[x].c_str());
		}

		EtherAddress hwaddr;
		int channel = -1;
		empower_bands_types band = EMPOWER_BT_L20;

		EtherAddressArg().parse(tokens_re[0], hwaddr);
		IntArg().parse(tokens_re[1], channel);

		if (tokens_re[2] == "L20") {
			band = EMPOWER_BT_L20;
		} else if (tokens_re[2] == "HT20") {
			band = EMPOWER_BT_HT20;
		}

		ResourceElement *elm = new ResourceElement(hwaddr, channel, band);
		_ifaces_to_elements.set(x, elm);

	}

	return res;

}

void EmpowerLVAPManager::send_busyness_trigger(uint32_t trigger_id, uint32_t iface, uint32_t current) {

    WritablePacket *p = Packet::make(sizeof(empower_busyness_trigger));

    ResourceElement* re = iface_to_element(iface);

    if (!p) {
        click_chatter("%{element} :: %s :: cannot make packet!",
                      this,
                      __func__);
        return;
    }

    memset(p->data(), 0, p->length());

    empower_busyness_trigger*request = (struct empower_busyness_trigger *) (p->data());
    request->set_version(_empower_version);
    request->set_length(sizeof(empower_busyness_trigger));
    request->set_type(EMPOWER_PT_BUSYNESS_TRIGGER);
    request->set_seq(get_next_seq());
    request->set_trigger_id(trigger_id);
    request->set_wtp(_wtp);
    request->set_channel(re->_channel);
    request->set_band(re->_band);
    request->set_hwaddr(re->_hwaddr);
    request->set_current(current);

    send_message(p);

}

void EmpowerLVAPManager::send_rssi_trigger(uint32_t trigger_id, uint32_t iface, uint8_t current) {

	WritablePacket *p = Packet::make(sizeof(empower_rssi_trigger));

	ResourceElement* re = iface_to_element(iface);

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
				      this,
				      __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_rssi_trigger*request = (struct empower_rssi_trigger *) (p->data());
	request->set_version(_empower_version);
	request->set_length(sizeof(empower_rssi_trigger));
	request->set_type(EMPOWER_PT_RSSI_TRIGGER);
	request->set_seq(get_next_seq());
	request->set_trigger_id(trigger_id);
	request->set_wtp(_wtp);
	request->set_channel(re->_channel);
	request->set_band(re->_band);
	request->set_hwaddr(re->_hwaddr);
	request->set_current(current);

	send_message(p);

}

void EmpowerLVAPManager::send_association_request(EtherAddress src, EtherAddress bssid, String ssid, EtherAddress hwaddr, int channel, empower_bands_types band, empower_bands_types supported_band) {

	WritablePacket *p = Packet::make(sizeof(empower_assoc_request) + ssid.length());

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
				      this,
				      __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_assoc_request *request = (struct empower_assoc_request *) (p->data());
	request->set_version(_empower_version);
	request->set_length(sizeof(empower_assoc_request) + ssid.length());
	request->set_type(EMPOWER_PT_ASSOC_REQUEST);
	request->set_seq(get_next_seq());
	request->set_wtp(_wtp);
	request->set_sta(src);
	request->set_bssid(bssid);
	request->set_ssid(ssid);
	request->set_channel(channel);
	request->set_band(band);
	request->set_supported_band(supported_band);
	request->set_hwaddr(hwaddr);

	send_message(p);

}

void EmpowerLVAPManager::send_auth_request(EtherAddress src, EtherAddress bssid) {

	WritablePacket *p = Packet::make(sizeof(empower_auth_request));

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
				      this,
				      __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_auth_request *request = (struct empower_auth_request *) (p->data());
	request->set_version(_empower_version);
	request->set_length(sizeof(empower_auth_request));
	request->set_type(EMPOWER_PT_AUTH_REQUEST);
	request->set_seq(get_next_seq());
	request->set_wtp(_wtp);
	request->set_sta(src);
	request->set_bssid(bssid);

	send_message(p);

}

void EmpowerLVAPManager::send_probe_request(EtherAddress src, String ssid, EtherAddress hwaddr, int channel, empower_bands_types band, empower_bands_types supported_band) {

	WritablePacket *p = Packet::make(sizeof(empower_probe_request) + ssid.length());

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
					  this,
					  __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_probe_request *request = (struct empower_probe_request *) (p->data());
	request->set_version(_empower_version);
	request->set_length(sizeof(empower_probe_request) + ssid.length());
	request->set_type(EMPOWER_PT_PROBE_REQUEST);
	request->set_seq(get_next_seq());
	request->set_wtp(_wtp);
	request->set_sta(src);
	request->set_ssid(ssid);
	request->set_hwaddr(hwaddr);
	request->set_band(band);
	request->set_channel(channel);
	request->set_supported_band(supported_band);

	send_message(p);

}

void EmpowerLVAPManager::send_message(Packet *p) {
	if (_ports.size() == 0) {
		click_chatter("%{element} :: %s :: ports not set!",
				      this,
				      __func__);
		p->kill();
		return;
	}
	output(0).push(p);
}

void EmpowerLVAPManager::send_hello() {

	WritablePacket *p = Packet::make(sizeof(empower_hello));

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
				      this,
				      __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_hello *hello = (struct empower_hello *) (p->data());

	hello->set_version(_empower_version);
	hello->set_length(sizeof(empower_hello));
	hello->set_type(EMPOWER_PT_HELLO);
	hello->set_seq(get_next_seq());
	hello->set_period(_period);
	hello->set_wtp(_wtp);

	if (_debug) {
		click_chatter("%{element} :: %s :: sending hello (%u)!",
				      this,
				      __func__,
					  hello->seq());
	}

	send_message(p);

}

void EmpowerLVAPManager::send_status_lvap(EtherAddress sta) {

	Vector<String> ssids;

	int len = sizeof(empower_status_lvap);

	EmpowerStationState ess = _lvaps.get(sta);

	len += 1 + ess._ssid.length();
	ssids.push_back(ess._ssid);

	for (int i = 0; i < ess._ssids.size(); i++) {
		len += 1 + ess._ssids[i].length();
		ssids.push_back(ess._ssids[i]);
	}

	WritablePacket *p = Packet::make(len);

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
					  this,
					  __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_status_lvap *status = (struct empower_status_lvap *) (p->data());
	status->set_version(_empower_version);
	status->set_length(len);
	status->set_type(EMPOWER_PT_STATUS_LVAP);
	status->set_seq(get_next_seq());
	status->set_assoc_id(ess._assoc_id);
	if (ess._set_mask)
		status->set_flag(EMPOWER_STATUS_LVAP_SET_MASK);
	if (ess._authentication_status)
		status->set_flag(EMPOWER_STATUS_LVAP_AUTHENTICATED);
	if (ess._association_status)
		status->set_flag(EMPOWER_STATUS_LVAP_ASSOCIATED);
	status->set_wtp(_wtp);
	status->set_sta(ess._sta);
	status->set_encap(ess._encap);
	status->set_net_bssid(ess._net_bssid);
	status->set_lvap_bssid(ess._lvap_bssid);
	status->set_hwaddr(ess._hwaddr);
	status->set_channel(ess._channel);
	status->set_band(ess._band);
	status->set_supported_band(ess._supported_band);

	uint8_t *ptr = (uint8_t *) status;
	ptr += sizeof(struct empower_status_lvap);
	uint8_t *end = ptr + (len - sizeof(struct empower_status_lvap));

	for (int i = 0; i < ssids.size(); i++) {
		assert (ptr <= end);
		ssid_entry *entry = (ssid_entry *) ptr;
		entry->set_length(ssids[i].length());
		entry->set_ssid(ssids[i]);
		ptr += entry->length() + 1;
	}

	send_message(p);

}

void EmpowerLVAPManager::send_status_vap(EtherAddress bssid) {

	Vector<String> ssids;

	EmpowerVAPState evs = _vaps.get(bssid);

	int len = sizeof(empower_status_vap) + evs._ssid.length();

	WritablePacket *p = Packet::make(len);

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
					  this,
					  __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_status_vap *status = (struct empower_status_vap *) (p->data());
	status->set_version(_empower_version);
	status->set_length(len);
	status->set_type(EMPOWER_PT_STATUS_VAP);
	status->set_seq(get_next_seq());
	status->set_wtp(_wtp);
	status->set_net_bssid(evs._net_bssid);
	status->set_hwaddr(evs._hwaddr);
	status->set_channel(evs._channel);
	status->set_band(evs._band);
	status->set_ssid(evs._ssid);

	send_message(p);

}

void EmpowerLVAPManager::send_status_port(EtherAddress sta, int iface) {

	ResourceElement* re = iface_to_element(iface);

	if (!re) {
		click_chatter("%{element} :: %s :: invalid iface id %u!",
					  this,
					  __func__,
					  iface);
		return;
	}

	send_status_port(sta, iface, re->_hwaddr, re->_channel, re->_band);

}

void EmpowerLVAPManager::send_status_port(EtherAddress sta, EtherAddress hwaddr,
		int channel, empower_bands_types band) {

	int iface = element_to_iface(hwaddr, channel, band);

	if (iface == -1) {
		click_chatter("%{element} :: %s :: invalid resource element (%s, %u, %u)!",
					  this,
					  __func__,
					  hwaddr.unparse().c_str(),
					  channel,
					  band);
		return;
	}

	send_status_port(sta, iface, hwaddr, channel, band);

}

void EmpowerLVAPManager::send_status_port(EtherAddress sta, int iface, EtherAddress hwaddr,
		int channel, empower_bands_types band) {

	TxPolicyInfo * tx_policy = _rcs[iface]->tx_policies()->tx_table()->find(sta);

	int len = sizeof(empower_status_port) + tx_policy->_mcs.size() + tx_policy->_ht_mcs.size();

	WritablePacket *p = Packet::make(len);

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
					  this,
					  __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_status_port *status = (struct empower_status_port *) (p->data());
	status->set_version(_empower_version);
	status->set_length(len);
	status->set_type(EMPOWER_PT_STATUS_PORT);
	status->set_seq(get_next_seq());
	if (tx_policy->_no_ack)
		status->set_flag(EMPOWER_STATUS_PORT_NOACK);
	status->set_rts_cts(tx_policy->_rts_cts);
	status->set_wtp(_wtp);
	status->set_sta(sta);
	status->set_nb_mcs(tx_policy->_mcs.size());
	status->set_nb_ht_mcs(tx_policy->_ht_mcs.size());
	status->set_hwaddr(hwaddr);
	status->set_channel(channel);
	status->set_band(band);
	status->set_ur_mcast_count(tx_policy->_ur_mcast_count);

	uint8_t *ptr = (uint8_t *) status;
	ptr += sizeof(struct empower_status_port);
	uint8_t *end = ptr + (len - sizeof(struct empower_status_port));

	for (int i = 0; i < tx_policy->_mcs.size(); i++) {
		assert (ptr <= end);
		*ptr = (uint8_t) tx_policy->_mcs[i];
		ptr++;
	}

	for (int i = 0; i < tx_policy->_ht_mcs.size(); i++) {
		assert (ptr <= end);
		*ptr = (uint8_t) tx_policy->_ht_mcs[i];
		ptr++;
	}

	send_message(p);

}

void EmpowerLVAPManager::send_img_response(int type, uint32_t graph_id,
		EtherAddress hwaddr, uint8_t channel, empower_bands_types band) {

	int iface_id = element_to_iface(hwaddr, channel, band);

	if (iface_id == -1) {
		click_chatter("%{element} :: %s :: invalid resource element (%s, %u, %u)!",
					  this,
					  __func__,
					  hwaddr.unparse().c_str(),
					  channel,
					  band);
		return;
	}

	// Select stations active on the specified resource element (iface_id)

	Vector<DstInfo> neighbors;
	_ers->lock.acquire_read();

	if (type == EMPOWER_PT_UCQM_RESPONSE) {
		for (NTIter iter = _ers->stas.begin(); iter.live(); iter++) {
			if (iter.value()._iface_id != iface_id) {
				continue;
			}
			neighbors.push_back(iter.value());
		}
	} else {
		for (NTIter iter = _ers->aps.begin(); iter.live(); iter++) {
			if (iter.value()._iface_id != iface_id) {
				continue;
			}
			neighbors.push_back(iter.value());
		}
	}

	_ers->lock.release_read();

	int len = sizeof(empower_cqm_response) + neighbors.size() * sizeof(cqm_entry);
	WritablePacket *p = Packet::make(len);

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
					  this,
					  __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_cqm_response *imgs = (struct empower_cqm_response *) (p->data());
	imgs->set_version(_empower_version);
	imgs->set_length(len);
	imgs->set_type(type);
	imgs->set_seq(get_next_seq());
	imgs->set_graph_id(graph_id);
	imgs->set_wtp(_wtp);
	imgs->set_nb_entries(neighbors.size());

	uint8_t *ptr = (uint8_t *) imgs;
	ptr += sizeof(struct empower_cqm_response);

	uint8_t *end = ptr + (len - sizeof(struct empower_cqm_response));

	for (int i = 0; i < neighbors.size(); i++) {
		assert (ptr <= end);
		cqm_entry *entry = (cqm_entry *) ptr;
		entry->set_sta(neighbors[i]._eth);
		entry->set_last_rssi_avg(neighbors[i]._last_rssi);
		entry->set_last_rssi_std(neighbors[i]._last_std);
		entry->set_last_packets(neighbors[i]._last_packets);
		entry->set_hist_packets(neighbors[i]._hist_packets);
		entry->set_mov_rssi(neighbors[i]._sma_rssi->avg());
		ptr += sizeof(struct cqm_entry);
	}

	send_message(p);

}

void EmpowerLVAPManager::send_busyness_response(uint32_t busyness_id, EtherAddress hwaddr, uint8_t channel, empower_bands_types band) {

	int iface_id = element_to_iface(hwaddr, channel, band);

	if (iface_id == -1) {
		click_chatter("%{element} :: %s :: invalid resource element (%s, %u, %u)!",
					  this,
					  __func__,
					  hwaddr.unparse().c_str(),
					  channel,
					  band);
		return;
	}

	_ers->lock.acquire_read();
	BusynessInfo *nfo = _ers->busyness.get_pointer(iface_id);
	uint32_t busyness = (uint32_t) nfo->_sma_busyness->avg();
	_ers->lock.release_read();

	int len = sizeof(empower_busyness_response);
	WritablePacket *p = Packet::make(len);

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
					  this,
					  __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_busyness_response *busy = (struct empower_busyness_response *) (p->data());
	busy->set_version(_empower_version);
	busy->set_length(len);
	busy->set_type(EMPOWER_PT_BUSYNESS_RESPONSE);
	busy->set_seq(get_next_seq());
	busy->set_busyness_id(busyness_id);
	busy->set_wtp(_wtp);
	busy->set_prob(busyness);

	send_message(p);

}

void EmpowerLVAPManager::send_summary_trigger(SummaryTrigger * summary) {

	int len = sizeof(empower_summary_trigger) + summary->_frames.size() * sizeof(summary_entry);
	WritablePacket *p = Packet::make(len);

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
				      this,
				      __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_summary_trigger* request = (struct empower_summary_trigger *) (p->data());
	request->set_version(_empower_version);
	request->set_length(len);
	request->set_type(EMPOWER_PT_SUMMARY_TRIGGER);
	request->set_seq(get_next_seq());
	request->set_trigger_id(summary->_trigger_id);
	request->set_wtp(_wtp);
	request->set_nb_frames(summary->_frames.size());

	uint8_t *ptr = (uint8_t *) request;
	ptr += sizeof(struct empower_summary_trigger);
	uint8_t *end = ptr + (len - sizeof(struct empower_summary_trigger));

	for (int i = 0; i < summary->_frames.size(); i++) {
		assert (ptr <= end);
		summary_entry *entry = (summary_entry *) ptr;
		entry->set_ra(summary->_frames[i]._ra);
		entry->set_ta(summary->_frames[i]._ta);
		entry->set_tsft(summary->_frames[i]._tsft);
		entry->set_flags(summary->_frames[i]._flags);
		entry->set_seq(summary->_frames[i]._seq);
		entry->set_rssi(summary->_frames[i]._rssi);
		entry->set_rate(summary->_frames[i]._rate);
		entry->set_length(summary->_frames[i]._length);
		entry->set_type(summary->_frames[i]._type);
		entry->set_subtype(summary->_frames[i]._subtype);
		ptr += sizeof(struct summary_entry);
	}

	summary->_frames.clear();

	send_message(p);

}

void EmpowerLVAPManager::send_lvap_stats_response(EtherAddress lvap, uint32_t lvap_stats_id) {

	EmpowerStationState ess = _lvaps.get(lvap);
	MinstrelDstInfo *nfo = _rcs.at(ess._iface_id)->neighbors()->findp(lvap);

	if (!nfo) {
		click_chatter("%{element} :: %s :: no rate information for %s",
					  this,
					  __func__,
					  lvap.unparse().c_str());
		return;
	}

	int len = sizeof(empower_lvap_stats_response) + nfo->rates.size() * sizeof(lvap_stats_entry);
	WritablePacket *p = Packet::make(len);

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
					  this,
					  __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_lvap_stats_response *lvap_stats = (struct empower_lvap_stats_response *) (p->data());
	lvap_stats->set_version(_empower_version);
	lvap_stats->set_length(len);
	lvap_stats->set_type(EMPOWER_PT_LVAP_STATS_RESPONSE);
	lvap_stats->set_seq(get_next_seq());
	lvap_stats->set_lvap_stats_id(lvap_stats_id);
	lvap_stats->set_wtp(_wtp);
	lvap_stats->set_nb_entries(nfo->rates.size());

	uint8_t *ptr = (uint8_t *) lvap_stats;
	ptr += sizeof(struct empower_lvap_stats_response);
	uint8_t *end = ptr + (len - sizeof(struct empower_lvap_stats_response));

	for (int i = 0; i < nfo->rates.size(); i++) {
		assert (ptr <= end);
		lvap_stats_entry *entry = (lvap_stats_entry *) ptr;
		entry->set_rate(nfo->rates[i]);
		entry->set_prob((uint32_t) nfo->probability[i]);
		entry->set_cur_prob((uint32_t) nfo->cur_prob[i]);
		ptr += sizeof(struct lvap_stats_entry);
	}

	send_message(p);

}

void EmpowerLVAPManager::send_counters_response(EtherAddress sta, uint32_t counters_id) {

	TxPolicyInfo * txp = get_txp(sta);

	if (!txp) {
		click_chatter("%{element} :: %s :: unable to find TXP for station %s!",
					  this,
					  __func__,
					  sta.unparse().c_str());
		return;
	}

	int len = sizeof(empower_counters_response);
	len += txp->_tx.size() * 6; // the tx samples
	len += txp->_rx.size() * 6; // the rx samples

	WritablePacket *p = Packet::make(len);

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
					  this,
					  __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_counters_response *counters = (struct empower_counters_response *) (p->data());
	counters->set_version(_empower_version);
	counters->set_length(len);
	counters->set_type(EMPOWER_PT_COUNTERS_RESPONSE);
	counters->set_seq(get_next_seq());
	counters->set_counters_id(counters_id);
	counters->set_wtp(_wtp);
	counters->set_sta(sta);
	counters->set_nb_tx(txp->_tx.size());
	counters->set_nb_rx(txp->_rx.size());

	uint8_t *ptr = (uint8_t *) counters;
	ptr += sizeof(struct empower_counters_response);

	uint8_t *end = ptr + (len - sizeof(struct empower_counters_response));

	for (CBytesIter iter = txp->_tx.begin(); iter.live(); iter++) {
		assert (ptr <= end);
		counters_entry *entry = (counters_entry *) ptr;
		entry->set_size(iter.key());
		entry->set_count(iter.value());
		ptr += sizeof(struct counters_entry);
	}

	for (CBytesIter iter = txp->_rx.begin(); iter.live(); iter++) {
		assert (ptr <= end);
		counters_entry *entry = (counters_entry *) ptr;
		entry->set_size(iter.key());
		entry->set_count(iter.value());
		ptr += sizeof(struct counters_entry);
	}

	send_message(p);

}

void EmpowerLVAPManager::send_wtp_counters_response(uint32_t counters_id) {

	int len = sizeof(empower_counters_response);

	int nb_tx = 0;
	int nb_rx = 0;

	for (LVAPIter it = _lvaps.begin(); it.live(); it++) {
		TxPolicyInfo * txp = get_txp(it.key());
		len += txp->_tx.size() * 12; // the tx samples
		len += txp->_rx.size() * 12; // the rx samples
		nb_tx += txp->_tx.size();
		nb_rx += txp->_rx.size();
	}

	WritablePacket *p = Packet::make(len);

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
					  this,
					  __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_wtp_counters_response *counters = (struct empower_wtp_counters_response *) (p->data());
	counters->set_version(_empower_version);
	counters->set_length(len);
	counters->set_type(EMPOWER_PT_WTP_COUNTERS_RESPONSE);
	counters->set_seq(get_next_seq());
	counters->set_counters_id(counters_id);
	counters->set_wtp(_wtp);
	counters->set_nb_tx(nb_tx);
	counters->set_nb_rx(nb_rx);

	uint8_t *ptr = (uint8_t *) counters;
	ptr += sizeof(struct empower_wtp_counters_response);

	uint8_t *end = ptr + (len - sizeof(struct empower_counters_response));

	for (LVAPIter it = _lvaps.begin(); it.live(); it++) {
		TxPolicyInfo * txp = get_txp(it.key());
		for (CBytesIter iter = txp->_tx.begin(); iter.live(); iter++) {
			assert (ptr <= end);
			wtp_counters_entry *entry = (wtp_counters_entry *) ptr;
			entry->set_size(iter.key());
			entry->set_count(iter.value());
			entry->set_sta(it.key());
			ptr += sizeof(struct wtp_counters_entry);
		}
	}

	for (LVAPIter it = _lvaps.begin(); it.live(); it++) {
		TxPolicyInfo * txp = get_txp(it.key());
		for (CBytesIter iter = txp->_rx.begin(); iter.live(); iter++) {
			assert (ptr <= end);
			wtp_counters_entry *entry = (wtp_counters_entry *) ptr;
			entry->set_size(iter.key());
			entry->set_count(iter.value());
			entry->set_sta(it.key());
			ptr += sizeof(struct wtp_counters_entry);
		}
	}

	send_message(p);

}


void EmpowerLVAPManager::send_cqm_links_response(uint32_t cqm_links_id) {

	_cqm->lock.acquire_read();

	int nb_links = _cqm->links.size();
	int len = sizeof(empower_cqm_links_response) + nb_links * sizeof(empower_cqm_link);

	WritablePacket *p = Packet::make(len);

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
					  this,
					  __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_cqm_links_response *counters = (struct empower_cqm_links_response *) (p->data());
	counters->set_version(_empower_version);
	counters->set_length(len);
	counters->set_type(EMPOWER_PT_CQM_LINKS_RESPONSE);
	counters->set_seq(get_next_seq());
	counters->set_cqm_links_id(cqm_links_id);
	counters->set_wtp(_wtp);
	counters->set_nb_links(nb_links);

	uint8_t *ptr = (uint8_t *) counters;
	ptr += sizeof(struct empower_cqm_links_response);

	uint8_t *end = ptr + (len - sizeof(struct empower_cqm_links_response));

	for (CLTIter iter = _cqm->links.begin(); iter.live(); iter++) {
		assert (ptr <= end);
		EtherAddress ta = iter.value().sourceAddr;
		uint32_t p_pdr = (uint32_t)(iter.value().p_pdr_last * 18000.0);
		uint32_t p_available_bw = (uint32_t)(iter.value().p_available_bw_last * 18000.0);
		uint32_t p_throughput = (uint32_t)(iter.value().p_throughput_last * 18000.0);
		uint32_t p_channel_busy_fraction = (uint32_t)(iter.value().p_channel_busy_fraction_last * 18000.0);
		uint32_t p_attainable_throughput = (uint32_t)(iter.value().p_attainable_throughput * 18000.0);
		empower_cqm_link *entry = (empower_cqm_link *) ptr;
		entry->set_ta(ta);
		entry->set_p_pdr(p_pdr);
		entry->set_p_available_bw(p_available_bw);
		entry->set_p_throughput(p_throughput);
		entry->set_p_channel_busy_fraction(p_channel_busy_fraction);
		entry->set_p_attainable_throughput(p_attainable_throughput);
		ptr += sizeof(struct empower_cqm_link);
	}

	_cqm->lock.release_read();

	send_message(p);

}

void EmpowerLVAPManager::send_incomming_mcast_address(EtherAddress mcast_address, int iface) {

	int len = sizeof(empower_incom_mcast_addr);
	WritablePacket *p = Packet::make(len);

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
					  this,
					  __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	struct empower_incom_mcast_addr *mcast_addr = (struct empower_incom_mcast_addr *) (p->data());

	mcast_addr->set_version(_empower_version);
	mcast_addr->set_length(sizeof(empower_incom_mcast_addr));
	mcast_addr->set_type(EMPOWER_PT_INCOM_MCAST_REQUEST);
	mcast_addr->set_seq(get_next_seq());
	mcast_addr->set_mcast_addr(mcast_address);
	mcast_addr->set_wtp(_wtp);
	mcast_addr->set_iface(iface);

	click_chatter("%{element} :: %s :: New mcast address %s address from iface %d in wtp %s",
				  this,
				  __func__, mcast_address.unparse().c_str(), iface, _wtp.unparse().c_str());

	send_message(p);
}

void EmpowerLVAPManager::send_igmp_report(EtherAddress src, Vector<IPAddress>* mcast_addresses, Vector<enum empower_igmp_record_type>* igmp_types) {

	int grouprecord_counter;

	for (grouprecord_counter = 0; grouprecord_counter < mcast_addresses->size(); grouprecord_counter++) {

		//send message to the controller
		int len = sizeof(empower_igmp_request);
		WritablePacket *p = Packet::make(len);

		if (!p) {
			click_chatter("%{element} :: %s :: cannot make packet!",
						  this,
						  __func__);
			return;
		}

		memset(p->data(), 0, p->length());

		struct empower_igmp_request *igmp_request = (struct empower_igmp_request *) (p->data());

		igmp_request->set_version(_empower_version);
		igmp_request->set_length(sizeof(empower_igmp_request));
		igmp_request->set_type(EMPOWER_PT_IGMP_REQUEST);
		igmp_request->set_seq(get_next_seq());
		igmp_request->set_mcast_addr(mcast_addresses->at(grouprecord_counter));
		igmp_request->set_wtp(_wtp);
		igmp_request->set_sta(src);
		igmp_request->set_igmp_type(igmp_types->at(grouprecord_counter));

		click_chatter("%{element} :: %s :: IGMP request type %d from sta %s for the mcast address %s from wtp %s",
					  this,
					  __func__,
					  igmp_types->at(grouprecord_counter),
					  src.unparse().c_str(),
					  mcast_addresses->at(grouprecord_counter).unparse().c_str(),
					  _wtp.unparse().c_str());

		send_message(p);
	}
}

void EmpowerLVAPManager::send_txp_counters_response(uint32_t counters_id, EtherAddress hwaddr, uint8_t channel, empower_bands_types band, EtherAddress mcast) {

	int iface_id = element_to_iface(hwaddr, channel, band);

	if (iface_id == -1) {
		click_chatter("%{element} :: %s :: invalid resource element (%s, %u, %u)!",
					  this,
					  __func__,
					  hwaddr.unparse().c_str(),
					  channel,
					  band);
		return;
	}

	TxPolicyInfo * tx_policy = _rcs[iface_id]->tx_policies()->tx_table()->find(mcast);

	if (!tx_policy) {
		int len = sizeof(empower_txp_counters_response);
		WritablePacket *p = Packet::make(len);
		if (!p) {
			click_chatter("%{element} :: %s :: cannot make packet!",
						  this,
						  __func__);
			return;
		}

		memset(p->data(), 0, p->length());
		empower_txp_counters_response *counters = (struct empower_txp_counters_response *) (p->data());
		counters->set_version(_empower_version);
		counters->set_length(len);
		counters->set_type(EMPOWER_PT_TXP_COUNTERS_RESPONSE);
		counters->set_seq(get_next_seq());
		counters->set_counters_id(counters_id);
		counters->set_wtp(_wtp);
		counters->set_nb_tx(0);
		send_message(p);
		return;
	}

	int len = sizeof(empower_txp_counters_response);
	len += tx_policy->_tx.size() * 6; // the tx samples

	WritablePacket *p = Packet::make(len);

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
					  this,
					  __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_txp_counters_response *counters = (struct empower_txp_counters_response *) (p->data());
	counters->set_version(_empower_version);
	counters->set_length(len);
	counters->set_type(EMPOWER_PT_TXP_COUNTERS_RESPONSE);
	counters->set_seq(get_next_seq());
	counters->set_counters_id(counters_id);
	counters->set_wtp(_wtp);
	counters->set_nb_tx(tx_policy->_tx.size());

	uint8_t *ptr = (uint8_t *) counters;
	ptr += sizeof(struct empower_txp_counters_response);

	uint8_t *end = ptr + (len - sizeof(struct empower_txp_counters_response));

	for (CBytesIter iter = tx_policy->_tx.begin(); iter.live(); iter++) {
		assert (ptr <= end);
		counters_entry *entry = (counters_entry *) ptr;
		entry->set_size(iter.key());
		entry->set_count(iter.value());
		ptr += sizeof(struct counters_entry);
	}

	send_message(p);

}

void EmpowerLVAPManager::send_caps() {

	_ports_lock.acquire_read();

	int len = sizeof(empower_caps);
	len += _ifaces_to_elements.size() * sizeof(struct resource_elements_entry);
	len += _ports.size() * sizeof(struct port_elements_entry);

	WritablePacket *p = Packet::make(len);

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
					  this,
					  __func__);
		return;
	}

	if (_debug) {
		click_chatter("%{element} :: %s :: sending caps response!",
				      this,
				      __func__);
	}

	memset(p->data(), 0, p->length());

	empower_caps *caps = (struct empower_caps *) (p->data());
	caps->set_version(_empower_version);
	caps->set_length(len);
	caps->set_type(EMPOWER_PT_CAPS_RESPONSE);
	caps->set_seq(get_next_seq());
	caps->set_wtp(_wtp);
	caps->set_nb_resources_elements(_ifaces_to_elements.size());
	caps->set_nb_ports_elements(_ports.size());

	uint8_t *ptr = (uint8_t *) caps;
	ptr += sizeof(struct empower_caps);

	uint8_t *end = ptr + (len - sizeof(struct empower_caps));

	for (REIter iter = _ifaces_to_elements.begin(); iter.live(); iter++) {
		assert (ptr <= end);
		ResourceElement *elm = iter.value();
		resource_elements_entry *entry = (resource_elements_entry *) ptr;
		entry->set_hwaddr(elm->_hwaddr);
		entry->set_channel(elm->_channel);
		entry->set_band(elm->_band);
		ptr += sizeof(struct resource_elements_entry);
	}

	for (PortsIter iter = _ports.begin(); iter.live(); iter++) {
		assert (ptr <= end);
		port_elements_entry *entry = (port_elements_entry *) ptr;
		entry->set_hwaddr(iter.value()._hwaddr);
		entry->set_iface(iter.value()._iface);
		entry->set_port_id(iter.value()._port_id);
		ptr += sizeof(struct port_elements_entry);
	}

	_ports_lock.release_read();

	send_message(p);

}

int EmpowerLVAPManager::handle_caps_request(Packet *p, uint32_t offset) {

	send_caps();

	return 0;

}

int EmpowerLVAPManager::handle_add_vap(Packet *p, uint32_t offset) {

	struct empower_add_vap *add_vap = (struct empower_add_vap *) (p->data() + offset);

	EtherAddress net_bssid = add_vap->net_bssid();
	String ssid = add_vap->ssid();
	EtherAddress hwaddr = add_vap->hwaddr();
	int channel = add_vap->channel();
	empower_bands_types band = (empower_bands_types) add_vap->band();

	int iface = element_to_iface(hwaddr, channel, band);

	if (iface == -1) {
		   click_chatter("%{element} :: %s :: invalid resource element (%s, %u, %u)!",
									 this,
									 __func__,
									 hwaddr.unparse().c_str(),
									 channel,
									 band);
		   return 0;
	}

	if (_debug) {
		click_chatter("%{element} :: %s :: bssid %s ssid %s ",
				      this,
				      __func__,
					  net_bssid.unparse().c_str(),
					  ssid.c_str());
	}

	if (_vaps.find(net_bssid) == _vaps.end()) {

		EmpowerVAPState state;
		state._net_bssid = net_bssid;
		state._channel = channel;
		state._hwaddr= hwaddr;
		state._band = band;
		state._ssid = ssid;
		state._iface_id = iface;
		_vaps.set(net_bssid, state);

		/* Regenerate the BSSID mask */
		compute_bssid_mask();

		return 0;

	}

	return 0;

}

int EmpowerLVAPManager::handle_del_vap(Packet *p, uint32_t offset) {

	struct empower_del_vap *q = (struct empower_del_vap *) (p->data() + offset);
	EtherAddress net_bssid = q->net_bssid();

	if (_debug) {
		click_chatter("%{element} :: %s :: sta %s",
				      this,
				      __func__,
					  net_bssid.unparse_colon().c_str());
	}

	// First make sure that this VAP isn't here already, in which
	// case we'll just ignore the request
	if (_vaps.find(net_bssid) == _vaps.end()) {
		click_chatter("%{element} :: %s :: Ignoring VAP delete request because the agent isn't hosting the VAP",
				      this,
				      __func__);

		return -1;
	}

	_vaps.erase(_vaps.find(net_bssid));

	// Remove this VAP's BSSID from the mask
	compute_bssid_mask();

	return 0;

}

int EmpowerLVAPManager::handle_add_lvap(Packet *p, uint32_t offset) {

	struct empower_add_lvap *add_lvap = (struct empower_add_lvap *) (p->data() + offset);

	EtherAddress sta = add_lvap->sta();
	EtherAddress net_bssid = add_lvap->net_bssid();
	EtherAddress lvap_bssid = add_lvap->lvap_bssid();
	Vector<String> ssids;

	uint8_t *ptr = (uint8_t *) add_lvap;
	ptr += sizeof(struct empower_add_lvap);
	uint8_t *end = ptr + (add_lvap->length() - sizeof(struct empower_add_lvap));

	while (ptr != end) {
		assert (ptr <= end);
		ssid_entry *entry = (ssid_entry *) ptr;
		ssids.push_back(entry->ssid());
		ptr += entry->length() + 1;
	}

	if (ssids.size() < 2) {
		click_chatter("%{element} :: %s :: invalid ssids size (%u)",
				      this,
				      __func__,
				      ssids.size());
		return 0;
	}

	String ssid = *ssids.begin();
	ssids.erase(ssids.begin());

	int assoc_id = add_lvap->assoc_id();
	EtherAddress hwaddr = add_lvap->hwaddr();
	int channel = add_lvap->channel();
	empower_bands_types band = (empower_bands_types) add_lvap->band();
	empower_bands_types supported_band = (empower_bands_types) add_lvap->supported_band();
	bool authentication_state = add_lvap->flag(EMPOWER_STATUS_LVAP_AUTHENTICATED);
	bool association_state = add_lvap->flag(EMPOWER_STATUS_LVAP_ASSOCIATED);
	bool set_mask = add_lvap->flag(EMPOWER_STATUS_LVAP_SET_MASK);
	EtherAddress encap = add_lvap->encap();
	uint32_t module_id = add_lvap->module_id();

    int iface = element_to_iface(hwaddr, channel, band);

	if (iface == -1) {
		   click_chatter("%{element} :: %s :: invalid resource element (%s, %u, %u)!",
									 this,
									 __func__,
									 hwaddr.unparse().c_str(),
									 channel,
									 band);
		   return 0;
	}

	if (_debug) {
	    StringAccum sa;
    	sa << ssids[0];
    	if (ssids.size() > 1) {
			for (int i = 1; i < ssids.size(); i++) {
				sa << ", " << ssids[i];
			}
    	}
		click_chatter("%{element} :: %s :: sta %s net_bssid %s lvap_bssid %s ssid %s [ %s ] assoc_id %d %s %s %s",
					  this,
					  __func__,
					  sta.unparse_colon().c_str(),
					  net_bssid.unparse().c_str(),
					  lvap_bssid.unparse().c_str(),
					  ssid.c_str(),
				      sa.take_string().c_str(),
				      assoc_id,
				      set_mask ? "DL+UL" : "UL",
				      authentication_state ? "AUTH" : "NO_AUTH",
				      association_state ? "ASSOC" : "NO_ASSOC");
	}

	// if no lvap can be found, then create it
	if (_lvaps.find(sta) == _lvaps.end()) {

		EmpowerStationState state;
		state._sta = sta;
		state._net_bssid = net_bssid;
		state._lvap_bssid = lvap_bssid;
		state._encap = encap;
		state._ssids = ssids;
		state._assoc_id = assoc_id;
		state._hwaddr = hwaddr;
		state._channel = channel;
		state._band = band;
		state._supported_band = supported_band;
		state._set_mask = set_mask;
		state._authentication_status = authentication_state;
		state._association_status = association_state;
		state._set_mask = set_mask;
		state._ssid = ssid;
		state._iface_id = iface;

		// set the CSA values to their default
		state._csa_active = false;
		state._csa_switch_count = 0;
		state._csa_switch_mode = 1;
		state._target_hwaddr = EtherAddress::make_broadcast();
		state._target_band = EMPOWER_BT_L20;
		state._target_channel = 0;

		// set the add/del lvap response ids to zero
		state._add_lvap_module_id = 0;
		state._del_lvap_module_id = 0;

		_lvaps.set(sta, state);

		/* Regenerate the BSSID mask */
		compute_bssid_mask();

		/* send add lvap response message */
		send_add_del_lvap_response(EMPOWER_PT_ADD_LVAP_RESPONSE, state._sta, module_id, 0);

		return 0;

	}

	EmpowerStationState *ess = _lvaps.get_pointer(sta);

	// if a csa procedure is active, then the target block MUST be a block
	// hosted by this WTP. If not then abort. Otherwise ignore add_lvap.
	if (ess->_csa_active) {

		// lookup interface id
		int target_iface = element_to_iface(ess->_target_hwaddr, ess->_target_channel, ess->_target_band);
		int incoming_iface = element_to_iface(hwaddr, channel, band);

		click_chatter("%{element} :: %s :: sta %s csa active, target hwaddr %s target channel %u target band %u iface_id %d",
					  this,
					  __func__,
					  ess->_sta.unparse_colon().c_str(),
					  ess->_target_hwaddr.unparse().c_str(),
					  ess->_target_channel,
					  ess->_target_band,
					  target_iface);

		click_chatter("%{element} :: %s :: sta %s csa active, incoming hwaddr %s target channel %u target band %u iface_id %d",
					  this,
					  __func__,
					  ess->_sta.unparse_colon().c_str(),
					  hwaddr.unparse().c_str(),
					  channel,
					  band,
					  incoming_iface);

		// if CSA is active an add lvap can be received, but the target block must be local
		assert(target_iface >= 0);

		// if CSA is active an add lvap can be received, but the incoming block must be local
		assert(incoming_iface >= 0);

		// and they must be the same
		assert(incoming_iface == target_iface);

	    // save module id
	    ess->_add_lvap_module_id = module_id;

	    return 0;
	}

	ess->_lvap_bssid = lvap_bssid;
	ess->_ssids = ssids;
	ess->_encap = encap;
	ess->_assoc_id = assoc_id;
	ess->_authentication_status = authentication_state;
	ess->_association_status = association_state;
	ess->_supported_band = supported_band;
	ess->_set_mask = set_mask;
	ess->_ssid = ssid;

	/* send add lvap response message */
	send_add_del_lvap_response(EMPOWER_PT_ADD_LVAP_RESPONSE, ess->_sta, module_id, 0);

	return 0;

}

void EmpowerLVAPManager::send_add_del_lvap_response(uint8_t type, EtherAddress sta, uint32_t module_id, uint32_t status) {

	WritablePacket *p = Packet::make(sizeof(empower_add_del_lvap_response));

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
					  this,
					  __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_add_del_lvap_response *resp = (struct empower_add_del_lvap_response *) (p->data());
	resp->set_version(_empower_version);
	resp->set_length(sizeof(empower_add_del_lvap_response));
	resp->set_type(type);
	resp->set_seq(get_next_seq());
	resp->set_sta(sta);
	resp->set_module_id(module_id);
	resp->set_status(status);
	resp->set_wtp(_wtp);

	send_message(p);

}


int EmpowerLVAPManager::handle_set_port(Packet *p, uint32_t offset) {

	struct empower_set_port *q = (struct empower_set_port *) (p->data() + offset);

	EtherAddress addr = q->addr();
	EtherAddress hwaddr = q->hwaddr();
	int channel = q->channel();
	empower_bands_types band = (empower_bands_types) q->band();

	bool no_ack = q->flag(EMPOWER_STATUS_PORT_NOACK);
	uint16_t rts_cts = q->rts_cts();
	empower_tx_mcast_type tx_mcast = q->tx_mcast();
	uint8_t ur = q->ur_mcast_count();
	Vector<int> mcs;
	Vector<int> ht_mcs;

	uint8_t *ptr = (uint8_t *) q;
	ptr += sizeof(struct empower_set_port);
	uint8_t *end = ptr + (q->length() - sizeof(struct empower_set_port));

	for (int x = 0; x < q->nb_mcs(); x++) {
		assert (ptr <= end);
		mcs.push_back(*ptr);
		ptr++;
	}

	assert(mcs.size() == q->nb_mcs());

	for (int x = 0; x < q->nb_ht_mcs(); x++) {
		assert (ptr <= end);
		ht_mcs.push_back(*ptr);
		ptr++;
	}

	assert(ht_mcs.size() == q->nb_ht_mcs());

	int iface = element_to_iface(hwaddr, channel, band);

	if (iface == -1) {
		   click_chatter("%{element} :: %s :: invalid resource element (%s, %u, %u)!",
									 this,
									 __func__,
									 hwaddr.unparse().c_str(),
									 channel,
									 band);
		   return 0;
	}

	_rcs[iface]->tx_policies()->insert(addr, mcs, ht_mcs, no_ack, tx_mcast, ur, rts_cts);
	_rcs[iface]->forget_station(addr);

	MinstrelDstInfo *nfo = _rcs.at(iface)->neighbors()->findp(addr);

	if (!nfo || !nfo->rates.size()) {
		if (_debug) {
			click_chatter("%{element} :: %s :: adding %s",
					      this,
						  __func__,
						  addr.unparse().c_str());
		}
		TxPolicyInfo * txp = _rcs[iface]->tx_policies()->tx_table()->find(addr);
		nfo = _rcs.at(iface)->insert_neighbor(addr, txp);
	}

	return 0;

}

int EmpowerLVAPManager::handle_add_busyness_trigger(Packet *p, uint32_t offset) {
	struct empower_add_busyness_trigger *q = (struct empower_add_busyness_trigger *) (p->data() + offset);
	EtherAddress hwaddr = q->hwaddr();
	empower_bands_types band = (empower_bands_types) q->band();
	uint8_t channel = q->channel();
	int iface = element_to_iface(hwaddr, channel, band);
	_ers->add_busyness_trigger(iface, q->trigger_id(), static_cast<empower_trigger_relation>(q->relation()), q->value(), q->period());
	return 0;
}

int EmpowerLVAPManager::handle_del_busyness_trigger(Packet *p, uint32_t offset) {
	struct empower_del_busyness_trigger *q = (struct empower_del_busyness_trigger *) (p->data() + offset);
	_ers->del_busyness_trigger(q->trigger_id());
	return 0;
}

int EmpowerLVAPManager::handle_add_rssi_trigger(Packet *p, uint32_t offset) {
	struct empower_add_rssi_trigger *q = (struct empower_add_rssi_trigger *) (p->data() + offset);
	_ers->add_rssi_trigger(q->sta(), q->trigger_id(), static_cast<empower_trigger_relation>(q->relation()), q->value(), q->period());
	return 0;
}

int EmpowerLVAPManager::handle_del_rssi_trigger(Packet *p, uint32_t offset) {
	struct empower_del_rssi_trigger *q = (struct empower_del_rssi_trigger *) (p->data() + offset);
	_ers->del_rssi_trigger(q->trigger_id());
	return 0;
}

int EmpowerLVAPManager::handle_add_summary_trigger(Packet *p, uint32_t offset) {
	struct empower_add_summary_trigger *q = (struct empower_add_summary_trigger *) (p->data() + offset);
	EtherAddress hwaddr = q->hwaddr();
	empower_bands_types band = (empower_bands_types) q->band();
	uint8_t channel = q->channel();
	int iface = element_to_iface(hwaddr, channel, band);
	_ers->add_summary_trigger(iface, q->addr(), q->trigger_id(), q->limit(), q->period());
	return 0;
}

int EmpowerLVAPManager::handle_del_summary_trigger(Packet *p, uint32_t offset) {
	struct empower_del_summary_trigger *q = (struct empower_del_summary_trigger *) (p->data() + offset);
	_ers->del_summary_trigger(q->trigger_id());
	return 0;
}

int EmpowerLVAPManager::handle_del_lvap(Packet *p, uint32_t offset) {

	struct empower_del_lvap *q = (struct empower_del_lvap *) (p->data() + offset);
	EtherAddress sta = q->sta();
	uint32_t module_id = q->module_id();

	if (_debug) {
		click_chatter("%{element} :: %s :: sta %s",
				      this,
				      __func__,
				      sta.unparse_colon().c_str());
	}

	// First make sure that this LVAP isn't here already, in which
	// case we'll just ignore the request
	if (_lvaps.find(sta) == _lvaps.end()) {
		click_chatter("%{element} :: %s :: Ignoring LVAP delete request because the agent isn't hosting the LVAP",
				      this,
				      __func__);

		return -1;
	}

	EmpowerStationState *ess = _lvaps.get_pointer(sta);

	// If the BSSIDs are different, this is a shared lvap and a deauth message should be sent before removing the lvap
	// CSA is not supported for shared LVAPs
	if (ess->_lvap_bssid != ess->_net_bssid) {
		// send de-auth
		_edeauthr->send_deauth_request(ess->_sta, 0x0001, ess->_iface_id);
		// The receiver must be flush from all the groups in the multicast table
		if (_mtbl) {
			_mtbl->leaveallgroups(ess->_sta);
		}
		// remove lvap
		remove_lvap(ess);
		// send del lvap response message
		send_add_del_lvap_response(EMPOWER_PT_DEL_LVAP_RESPONSE, ess->_sta, module_id, 0);
		return 0;
	}

	// if this is an uplink only LVAP the CSA is not needed
	if (!ess->_set_mask) {
		// remove lvap
		remove_lvap(ess);
		// send del lvap response message
		send_add_del_lvap_response(EMPOWER_PT_DEL_LVAP_RESPONSE, ess->_sta, module_id, 0);
		return 0;
	}

	// if channel is different then start CSA procedure, if target channel is zero it means
	// that no target block is actually provided, ignore rest of the fields.
	// Notice that if the target block is hosted in this wtp, the then following add_lvap
	// message must be ignored and an add_lvap message must be generated locally at the end
	// of the CSA procedure
	if ((q->target_channel() != 0) && (q->target_channel() != ess->_channel)) {

		click_chatter("%{element} :: %s :: sta %s target channel %u is different from current channel %u, starting csa",
				      this,
				      __func__,
					  ess->_sta.unparse().c_str(),
					  q->target_channel(),
					  ess->_channel);

		ess->_csa_active = true;
		ess->_csa_switch_count = q->csa_switch_count();
		ess->_csa_switch_mode = q->csa_switch_mode();

		ess->_target_band = (empower_bands_types) q->target_band();
		ess->_target_channel = q->target_channel();
		ess->_target_hwaddr = q->target_hwaddr();

		ess->_del_lvap_module_id = module_id;

		return 0;

	}

	// remove lvap
	remove_lvap(ess);
	// send del lvap response message
	send_add_del_lvap_response(EMPOWER_PT_DEL_LVAP_RESPONSE, ess->_sta, module_id, 0);
	return 0;

}

int EmpowerLVAPManager::remove_lvap(EmpowerStationState *ess) {

	// Forget station
	_rcs[ess->_iface_id]->tx_policies()->tx_table()->erase(ess->_sta);
	_rcs[ess->_iface_id]->forget_station(ess->_sta);

	// Erase lvap
	_lvaps.erase(_lvaps.find(ess->_sta));

	// Remove this VAP's BSSID from the mask
	compute_bssid_mask();

	return 0;

}

int EmpowerLVAPManager::handle_probe_response(Packet *p, uint32_t offset) {

	struct empower_probe_response *q = (struct empower_probe_response *) (p->data() + offset);
	EtherAddress sta = q->sta();
	String ssid = q->ssid();

	if (_debug) {
		click_chatter("%{element} :: %s :: sta %s",
				      this,
				      __func__,
				      sta.unparse_colon().c_str());
	}

	EmpowerStationState *ess = _lvaps.get_pointer(sta);

	if (!ess) {
		click_chatter("%{element} :: %s :: unknown LVAP %s ignoring",
				      this,
				      __func__,
				      sta.unparse_colon().c_str());
		return 0;
	}

	_ebs->send_probe_response(ess, ssid);

	return 0;

}

int EmpowerLVAPManager::handle_auth_response(Packet *p, uint32_t offset) {
	struct empower_auth_response *q = (struct empower_auth_response *) (p->data() + offset);
	EtherAddress sta = q->sta();
	if (_debug) {
		click_chatter("%{element} :: %s :: sta %s",
				      this,
				      __func__,
				      sta.unparse_colon().c_str());
	}
	EmpowerStationState *ess = _lvaps.get_pointer(sta);
	ess->_authentication_status = true;
	ess->_association_status = false;
	_eauthr->send_auth_response(ess->_sta, 2, WIFI_STATUS_SUCCESS, ess->_iface_id);
	return 0;
}

int EmpowerLVAPManager::handle_assoc_response(Packet *p, uint32_t offset) {
	struct empower_assoc_response *q = (struct empower_assoc_response *) (p->data() + offset);
	EtherAddress sta = q->sta();
	if (_debug) {
		click_chatter("%{element} :: %s :: sta %s",
				      this,
				      __func__,
				      sta.unparse_colon().c_str());
	}
	EmpowerStationState *ess = _lvaps.get_pointer(sta);
	int channel = ess->_channel;
	_eassor->send_association_response(ess->_sta, WIFI_STATUS_SUCCESS, channel, ess->_iface_id);
	return 0;
}

int EmpowerLVAPManager::handle_txp_counters_request(Packet *p, uint32_t offset) {
	struct empower_txp_counters_request *q = (struct empower_txp_counters_request *) (p->data() + offset);
	EtherAddress hwaddr = q->hwaddr();
	empower_bands_types band = (empower_bands_types) q->band();
	uint8_t channel = q->channel();
	EtherAddress mcast = q->mcast();
	send_txp_counters_response(q->counters_id(), hwaddr, channel, band, mcast);
	return 0;
}

int EmpowerLVAPManager::handle_counters_request(Packet *p, uint32_t offset) {
	struct empower_counters_request *q = (struct empower_counters_request *) (p->data() + offset);
	EtherAddress sta = q->sta();
	send_counters_response(sta, q->counters_id());
	return 0;
}

int EmpowerLVAPManager::handle_wtp_counters_request(Packet *p, uint32_t offset) {
	struct empower_wtp_counters_request *q = (struct empower_wtp_counters_request *) (p->data() + offset);
	send_wtp_counters_response(q->counters_id());
	return 0;
}

int EmpowerLVAPManager::handle_cqm_links_request(Packet *p, uint32_t offset) {
	struct empower_cqm_links_request *q = (struct empower_cqm_links_request *) (p->data() + offset);
	send_cqm_links_response(q->cqm_links_id());
	return 0;
}

int EmpowerLVAPManager::handle_busyness_request(Packet *p, uint32_t offset) {
	struct empower_busyness_request *q = (struct empower_busyness_request *) (p->data() + offset);
	EtherAddress hwaddr = q->hwaddr();
	empower_bands_types band = (empower_bands_types) q->band();
	uint8_t channel = q->channel();
	send_busyness_response(q->busyness_id(), hwaddr, channel, band);
	return 0;
}

int EmpowerLVAPManager::handle_uimg_request(Packet *p, uint32_t offset) {
	struct empower_cqm_request *q = (struct empower_cqm_request *) (p->data() + offset);
	EtherAddress hwaddr = q->hwaddr();
	empower_bands_types band = (empower_bands_types) q->band();
	uint8_t channel = q->channel();
	send_img_response(EMPOWER_PT_UCQM_RESPONSE, q->graph_id(), hwaddr, channel, band);
	return 0;
}

int EmpowerLVAPManager::handle_lvap_stats_request(Packet *p, uint32_t offset) {
	struct empower_lvap_stats_request *q = (struct empower_lvap_stats_request *) (p->data() + offset);
	EtherAddress sta = q->sta();
	send_lvap_stats_response(sta, q->lvap_stats_id());
	return 0;
}

int EmpowerLVAPManager::handle_nimg_request(Packet *p, uint32_t offset) {
	if (!_ers) {
		click_chatter("%{element} :: %s :: RXStats Element not available!",
					  this,
					  __func__);
		return 0;
	}
	struct empower_cqm_request *q = (struct empower_cqm_request *) (p->data() + offset);
	EtherAddress hwaddr = q->hwaddr();
	empower_bands_types band = (empower_bands_types) q->band();
	uint8_t channel = q->channel();
	send_img_response(EMPOWER_PT_NCQM_RESPONSE, q->graph_id(), hwaddr, channel, band);
	return 0;
}

int EmpowerLVAPManager::handle_incom_mcast_addr_response(Packet *p, uint32_t offset) {

	struct empower_incom_mcast_addr_response *q = (struct empower_incom_mcast_addr_response *) (p->data() + offset);
	EtherAddress mcast_addr = q->mcast_addr();
	int iface = q->iface();

	click_chatter("%{element} :: %s :: Receiving incom mcast address %s response for iface %d",
				  this,
				  __func__, mcast_addr.unparse().c_str(), iface);

	TxPolicyInfo* def_tx_policy = _rcs[iface]->tx_policies()->default_tx_policy();

	Vector<int> mcs;
	mcs.push_back(*(def_tx_policy->_mcs.begin()));

	Vector<int> ht_mcs;
	ht_mcs.push_back(*(def_tx_policy->_ht_mcs.begin()));

	_rcs[iface]->tx_policies()->insert(mcast_addr, mcs, ht_mcs, def_tx_policy->_no_ack, TX_MCAST_LEGACY, def_tx_policy->_ur_mcast_count, def_tx_policy->_rts_cts);

	for (TxTableIter it_txp = _rcs[iface]->tx_policies()->tx_table()->begin(); it_txp.live(); it_txp++) {
		click_chatter("%{element} :: %s :: Check policies %s, %d",
					  this,
					  __func__, it_txp.key().unparse().c_str(), it_txp.value()->_tx_mcast);
				}

	return 0;
}

int EmpowerLVAPManager::handle_del_mcast_addr(Packet *p, uint32_t offset) {

	struct empower_del_mcast_addr *q = (struct empower_del_mcast_addr *) (p->data() + offset);
	EtherAddress mcast_addr = q->mcast_addr();
	EtherAddress hwaddr = q->hwaddr();
	int channel = q->channel();
	empower_bands_types band = (empower_bands_types) q->band();
	int iface_id = element_to_iface(hwaddr, channel, band);

	click_chatter("%{element} :: %s :: Receiving delete mcast address %s response",
				  this,
				  __func__, mcast_addr.unparse().c_str());

	_rcs[iface_id]->tx_policies()->remove(mcast_addr);

	return 0;
}

int EmpowerLVAPManager::handle_del_mcast_receiver(Packet *p, uint32_t offset) {

	struct empower_del_mcast_receiver *q = (struct empower_del_mcast_receiver *) (p->data() + offset);
	EtherAddress sta = q->sta();

	click_chatter("%{element} :: %s :: Receiving delete mcast receiver %s response",
				  this,
				  __func__,
				  sta.unparse().c_str());

	if (_mtbl) {
		_mtbl->leaveallgroups(sta);
	}

	return 0;
}

void EmpowerLVAPManager::push(int, Packet *p) {

	/* This is a control packet coming from a Socket
	 * element.
	 */

	if (p->length() < sizeof(struct empower_header)) {
		click_chatter("%{element} :: %s :: Packet too small: %d Vs. %d",
				      this,
				      __func__,
				      p->length(),
				      sizeof(struct empower_header));
		p->kill();
		return;
	}

	uint32_t offset = 0;

	while (offset < p->length()) {
		struct empower_header *w = (struct empower_header *) (p->data() + offset);
		switch (w->type()) {
		case EMPOWER_PT_ADD_LVAP:
			handle_add_lvap(p, offset);
			break;
		case EMPOWER_PT_DEL_LVAP:
			handle_del_lvap(p, offset);
			break;
		case EMPOWER_PT_ADD_VAP:
			handle_add_vap(p, offset);
			break;
		case EMPOWER_PT_DEL_VAP:
			handle_del_vap(p, offset);
			break;
		case EMPOWER_PT_PROBE_RESPONSE:
			handle_probe_response(p, offset);
			break;
		case EMPOWER_PT_AUTH_RESPONSE:
			handle_auth_response(p, offset);
			break;
		case EMPOWER_PT_ASSOC_RESPONSE:
			handle_assoc_response(p, offset);
			break;
		case EMPOWER_PT_COUNTERS_REQUEST:
			handle_counters_request(p, offset);
			break;
		case EMPOWER_PT_WTP_COUNTERS_REQUEST:
			handle_wtp_counters_request(p, offset);
			break;
		case EMPOWER_PT_TXP_COUNTERS_REQUEST:
			handle_txp_counters_request(p, offset);
			break;
		case EMPOWER_PT_ADD_RSSI_TRIGGER:
			handle_add_rssi_trigger(p, offset);
			break;
		case EMPOWER_PT_DEL_RSSI_TRIGGER:
			handle_del_rssi_trigger(p, offset);
			break;
		case EMPOWER_PT_ADD_BUSYNESS_TRIGGER:
			handle_add_busyness_trigger(p, offset);
			break;
		case EMPOWER_PT_DEL_BUSYNESS_TRIGGER:
			handle_del_busyness_trigger(p, offset);
			break;
		case EMPOWER_PT_ADD_SUMMARY_TRIGGER:
			handle_add_summary_trigger(p, offset);
			break;
		case EMPOWER_PT_DEL_SUMMARY_TRIGGER:
			handle_del_summary_trigger(p, offset);
			break;
		case EMPOWER_PT_UCQM_REQUEST:
			handle_uimg_request(p, offset);
			break;
		case EMPOWER_PT_NCQM_REQUEST:
			handle_nimg_request(p, offset);
			break;
		case EMPOWER_PT_SET_PORT:
			handle_set_port(p, offset);
			break;
		case EMPOWER_PT_LVAP_STATS_REQUEST:
			handle_lvap_stats_request(p, offset);
			break;
		case EMPOWER_PT_BUSYNESS_REQUEST:
			handle_busyness_request(p, offset);
			break;
		case EMPOWER_PT_INCOM_MCAST_RESPONSE:
			handle_incom_mcast_addr_response(p, offset);
			break;
		case EMPOWER_PT_CQM_LINKS_REQUEST:
			handle_cqm_links_request(p, offset);
			break;
		case EMPOWER_PT_CAPS_REQUEST:
			handle_caps_request(p, offset);
			break;
		default:
			click_chatter("%{element} :: %s :: Unknown packet type: %d",
					      this,
					      __func__,
					      w->type());
		}
		offset += w->length();
	}

	p->kill();
	return;

}

Vector<EtherAddress>::iterator find(Vector<EtherAddress>::iterator begin, Vector<EtherAddress>::iterator end, EtherAddress element) {
	while (begin != end) {
		if (*begin == element) {
			break;
		} else {
			begin++;
		}
	}
	return begin;
}

/*
 * This re-computes the BSSID mask for this node
 * using all the BSSIDs of the VAPs, and sets the
 * hardware register accordingly.
 */
void EmpowerLVAPManager::compute_bssid_mask() {

	// clear _masks vector
	_masks.clear();

	for (unsigned j = 0; j < _ifaces_to_elements.size(); j++) {

		// start building mask
		uint8_t bssid_mask[6];
		int i;

		// Start with a mask of ff:ff:ff:ff:ff:ff
		for (i = 0; i < 6; i++) {
			bssid_mask[i] = 0xff;
		}

		// For each LVAP, update the bssid mask to include
		// the common bits of all LVAPs.
		for (LVAPIter it = _lvaps.begin(); it.live(); it++) {
			// check iface
			unsigned iface_id = it.value()._iface_id;
			if (iface_id != j) {
				continue;
			}
			// check set mask flag
			bool set_mask = it.value()._set_mask;
			if (!set_mask) {
				continue;
			}
			// add to mask
			for (i = 0; i < 6; i++) {
				const uint8_t *hw = (const uint8_t *) _ifaces_to_elements[iface_id]->_hwaddr.data();
				const uint8_t *bssid = (const uint8_t *) it.value()._net_bssid.data();
				bssid_mask[i] &= ~(hw[i] ^ bssid[i]);
			}
		}

		// For each VAP, update the bssid mask to include
		// the common bits of all VAPs.
		for (VAPIter it = _vaps.begin(); it.live(); it++) {
			// check iface
			unsigned iface_id = it.value()._iface_id;
			if (iface_id != j) {
				continue;
			}
			// add to mask
			for (i = 0; i < 6; i++) {
				const uint8_t *hw = (const uint8_t *) _ifaces_to_elements[iface_id]->_hwaddr.data();
				const uint8_t *bssid = (const uint8_t *) it.value()._net_bssid.data();
				bssid_mask[i] &= ~(hw[i] ^ bssid[i]);
			}
		}

		_masks.push_back(EtherAddress(bssid_mask));

	}

	// Update bssid masks register through debugfs
	for (int i = 0; i < _masks.size(); i++) {

		FILE *debugfs_file = fopen(_debugfs_strings[i].c_str(), "w");

		if (debugfs_file != NULL) {
			if (_debug) {
				click_chatter("%{element} :: %s :: %s",
							  this,
							  __func__,
							  _masks[i].unparse_colon().c_str());
			}
			fprintf(debugfs_file, "%s\n", _masks[i].unparse_colon().c_str());
			fclose(debugfs_file);
			continue;
		}

		click_chatter("%{element} :: %s :: unable to open debugfs file %s",
					  this,
					  __func__,
					  _debugfs_strings[i].c_str());

	}

}

enum {
	H_BYTES,
	H_PORTS,
	H_DEBUG,
	H_MASKS,
	H_LVAPS,
	H_VAPS,
	H_ADD_LVAP,
	H_DEL_LVAP,
	H_RECONNECT,
	H_INTERFACES,
};

String EmpowerLVAPManager::read_handler(Element *e, void *thunk) {
	EmpowerLVAPManager *td = (EmpowerLVAPManager *) e;
	switch ((uintptr_t) thunk) {
	case H_PORTS: {
	    td->_ports_lock.acquire_read();
	    StringAccum sa;
		for (PortsIter it = td->_ports.begin(); it.live(); it++) {
		    sa << it.value().unparse();
		    sa << "\n";
		}
	    td->_ports_lock.release_read();
		return sa.take_string();
	}
	case H_DEBUG:
		return String(td->_debug) + "\n";
	case H_MASKS: {
	    StringAccum sa;
	    for (int i = 0; i < td->_masks.size(); i++) {
	    	sa << i << ": " << td->_masks[i].unparse() << "\n";
	    }
		return sa.take_string();
	}
	case H_LVAPS: {
	    StringAccum sa;
		for (LVAPIter it = td->lvaps()->begin(); it.live(); it++) {
		    sa << "sta ";
		    sa << it.key().unparse();
		    if (it.value()._set_mask) {
			    sa << " DL+UL";
		    } else {
			    sa << " UL";
		    }
		    if (it.value()._association_status) {
			    sa << " ASSOC";
		    }
		    if (it.value()._authentication_status) {
			    sa << " AUTH";
		    }
			sa << " net_bssid ";
		    sa << it.value()._net_bssid.unparse();
			sa << " lvap_bssid ";
		    sa << it.value()._lvap_bssid.unparse();
		    sa << " encap ";
		    sa << it.value()._encap.unparse();
		    sa << " ssid ";
		    sa << it.value()._ssid;
		    sa << " ssids [ ";
	    	sa << it.value()._ssids[0];
		    for (int i = 1; i < it.value()._ssids.size(); i++) {
		    	sa << ", " << it.value()._ssids[i];
			}
			sa << " ] assoc_id ";
			sa << it.value()._assoc_id;
			sa << " hwaddr ";
			sa << it.value()._hwaddr.unparse();
			sa << " channel ";
			sa << it.value()._channel;
			sa << " band ";
			sa << it.value()._band;
			sa << " iface_id ";
			sa << it.value()._iface_id;
			sa << " supported_band ";
			sa << it.value()._supported_band;
			sa << "\n";
		}
		return sa.take_string();
	}
	case H_INTERFACES: {
		StringAccum sa;
		for (REIter iter = td->_ifaces_to_elements.begin(); iter.live(); iter++) {
			sa << iter.key() << " -> " << iter.value()->unparse()  << "\n";
		}
		return sa.take_string();
	}
	case H_VAPS: {
	    StringAccum sa;
		for (VAPIter it = td->vaps()->begin(); it.live(); it++) {
			sa << "net_bssid ";
			sa << it.key().unparse();
			sa << " ssid ";
			sa << it.value()._ssid;
			sa << " iface_id ";
			sa << it.value()._iface_id;
			sa << " hwaddr ";
			sa << it.value()._hwaddr.unparse();
			sa << " channel ";
			sa << it.value()._channel;
			sa << " band ";
			sa << it.value()._band;
			sa << "\n";
		}
		return sa.take_string();
	}
	case H_BYTES: {
		StringAccum sa;
		for (LVAPIter it = td->lvaps()->begin(); it.live(); it++) {
			TxPolicyInfo *txp = td->get_txp(it.key());
			sa << "!" << it.key().unparse() << "\n";
			sa << "!TX\n";
			CBytes tx = txp->_tx;
			Vector<int> lens_tx;
			for (CBytesIter iter = tx.begin(); iter.live(); iter++) {
				lens_tx.push_back(iter.key());
			}
			click_qsort(lens_tx.begin(), lens_tx.size());
			for (int i = 0; i < lens_tx.size(); i++) {
				CBytesIter itr = tx.find(lens_tx[i]);
				sa << itr.key() << " " << itr.value() << "\n";
			}
			sa << "!RX\n";
			CBytes rx = txp->_rx;
			Vector<int> lens_rx;
			for (CBytesIter iter = rx.begin(); iter.live(); iter++) {
				lens_rx.push_back(iter.key());
			}
			click_qsort(lens_rx.begin(), lens_rx.size());
			for (int i = 0; i < lens_rx.size(); i++) {
				CBytesIter itr = rx.find(lens_rx[i]);
				sa << itr.key() << " " << itr.value() << "\n";
			}
		}
		return sa.take_string();
	}
	default:
		return String();
	}
}

int EmpowerLVAPManager::write_handler(const String &in_s, Element *e,
		void *vparam, ErrorHandler *errh) {

	EmpowerLVAPManager *f = (EmpowerLVAPManager *) e;
	String s = cp_uncomment(in_s);

	switch ((intptr_t) vparam) {
	case H_DEBUG: {
		bool debug;
		if (!BoolArg().parse(s, debug))
			return errh->error("debug parameter must be boolean");
		f->_debug = debug;
		break;
	}
	case H_PORTS: {

		if (f->_ports.size() > 0) {
			return errh->error("error ports has already been set");
		}

		Vector<String> tokens;
		cp_spacevec(s, tokens);

		if (tokens.size() == 0)
			return errh->error("ports needs at least 3 parameters");

		if (tokens.size() % 3 != 0)
			return errh->error("ports parameters must be a multiple of 3");

		EtherAddress hwaddr;
		int port_id;
		String iface;

		f->_ports_lock.acquire_write();
		f->_ports.clear();

		for (int i = 0; i < tokens.size(); i+=3) {
			if (!EtherAddressArg().parse(tokens[i], hwaddr)) {
				return errh->error("error param %s: must start with an Ethernet address", tokens[i].c_str());
			}
			if (!IntArg().parse(tokens[i+1], port_id)) {
				return errh->error("error param %s: must start with an int", tokens[i+1].c_str());
			}
			if (!StringArg().parse(tokens[i+2], iface)) {
				return errh->error("error param %s: must start with a String", tokens[i+2].c_str());
			}
			f->_ports.find_insert(port_id, NetworkPort(hwaddr, iface, port_id));
		}

		f->_ports_lock.release_write();
		break;

	}
	case H_RECONNECT: {
		// clear triggers
		if (f->_ers) {
			f->_ers->clear_triggers();
		}
		// send LVAP status update messages
		for (LVAPIter it = f->_lvaps.begin(); it.live(); it++) {
			f->send_status_lvap(it.key());
		}
		// send VAP status update messages
		for (VAPIter it = f->_vaps.begin(); it.live(); it++) {
			f->send_status_vap(it.key());
		}
		// send tx policies
		for (REIter it_re = f->_ifaces_to_elements.begin(); it_re.live(); it_re++) {
			int iface_id = it_re.key();
			for (TxTableIter it_txp = f->get_tx_policies(iface_id)->tx_table()->begin(); it_txp.live(); it_txp++) {
				EtherAddress sta = it_txp.key();
				f->send_status_port(sta, iface_id, it_re.value()->_hwaddr, it_re.value()->_channel, it_re.value()->_band);
			}
		}
		break;
	}
	}
	return 0;
}

void EmpowerLVAPManager::add_handlers() {
	add_read_handler("debug", read_handler, (void *) H_DEBUG);
	add_read_handler("ports", read_handler, (void *) H_PORTS);
	add_read_handler("lvaps", read_handler, (void *) H_LVAPS);
	add_read_handler("vaps", read_handler, (void *) H_VAPS);
	add_read_handler("masks", read_handler, (void *) H_MASKS);
	add_read_handler("bytes", read_handler, (void *) H_BYTES);
	add_read_handler("interfaces", read_handler, (void *) H_INTERFACES);
	add_write_handler("reconnect", write_handler, (void *) H_RECONNECT);
	add_write_handler("ports", write_handler, (void *) H_PORTS);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EmpowerLVAPManager)
ELEMENT_REQUIRES(userlevel EmpowerRXStats)
