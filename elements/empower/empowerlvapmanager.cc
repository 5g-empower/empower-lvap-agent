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
#include "empowerqosmanager.hh"
#include "empowerregmon.hh"
CLICK_DECLS

EmpowerLVAPManager::EmpowerLVAPManager() :
		_e11k(0), _ebs(0), _eauthr(0), _eassor(0), _edeauthr(0), _ers(0),
		_mtbl(0), _seq(0), _debug(false) {
}

EmpowerLVAPManager::~EmpowerLVAPManager() {
}

int EmpowerLVAPManager::initialize(ErrorHandler *) {
	compute_bssid_mask();
	return 0;
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
	String eqms_strings;
	String regmon_strings;

	res = Args(conf, this, errh).read_m("WTP", _wtp)
						        .read_m("E11K", ElementCastArg("Empower11k"), _e11k)
						        .read_m("EBS", ElementCastArg("EmpowerBeaconSource"), _ebs)
			                    .read_m("EAUTHR", ElementCastArg("EmpowerOpenAuthResponder"), _eauthr)
			                    .read_m("EASSOR", ElementCastArg("EmpowerAssociationResponder"), _eassor)
								.read_m("EDEAUTHR", ElementCastArg("EmpowerDeAuthResponder"), _edeauthr)
			                    .read_m("DEBUGFS", debugfs_strings)
			                    .read_m("EQMS", eqms_strings)
			                    .read_m("RCS", rcs_strings)
			                    .read_m("RES", res_strings)
								.read("REGMONS", regmon_strings)
			                    .read_m("ERS", ElementCastArg("EmpowerRXStats"), _ers)
								.read("MTBL", ElementCastArg("EmpowerMulticastTable"), _mtbl)
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

		ResourceElement *elm = new ResourceElement(x, hwaddr, channel, band);
		_ifaces.set(x, elm);

	}

	tokens.clear();
	cp_spacevec(eqms_strings, tokens);

	for (int i = 0; i < tokens.size(); i++) {
		EmpowerQOSManager * eqm;
		if (!ElementCastArg("EmpowerQOSManager").parse(tokens[i], eqm, Args(conf, this, errh))) {
			return errh->error("error param %s: must be an EmpowerQOSManager element", tokens[i].c_str());
		}
		_eqms.push_back(eqm);
	}

	if (_eqms.size() != _masks.size()) {
		return errh->error("eqms has %u values, while masks has %u values", _eqms.size(), _masks.size());
	}

	tokens.clear();
	cp_spacevec(regmon_strings, tokens);
	for (int i = 0; i < tokens.size(); i++) {
		EmpowerRegmon *regmon;
		if (!ElementCastArg("EmpowerRegmon").parse(tokens[i], regmon, Args(conf, this, errh))) {
			return errh->error("error param %s: must be a EmpowerRegmon element", tokens[i].c_str());
		}
		_regmons.push_back(regmon);
	}

	if (_regmons.size() != _masks.size()) {
		return errh->error("regmons has %u values, while masks has %u values", _regmons.size(), _masks.size());
	}

	return res;

}

void EmpowerLVAPManager::send_rssi_trigger(uint32_t iface_id, uint32_t xid, uint8_t current) {

	WritablePacket *p = Packet::make(sizeof(empower_rssi_trigger));

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
				      this,
				      __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_rssi_trigger*request = (empower_rssi_trigger *) (p->data());
	request->set_version(_empower_version);
	request->set_length(sizeof(empower_rssi_trigger));
	request->set_type(EMPOWER_PT_RSSI_TRIGGER);
	request->set_seq(get_next_seq());
	request->set_xid(xid);
	request->set_wtp(_wtp);
	request->set_iface_id(iface_id);
	request->set_current(current);

	send_message(p);

}

void EmpowerLVAPManager::send_association_request(EtherAddress src, EtherAddress bssid, String ssid, bool ht_caps, uint16_t ht_caps_info) {

	WritablePacket *p = Packet::make(sizeof(empower_assoc_request) + ssid.length());

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
				      this,
				      __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_assoc_request *request = (empower_assoc_request *) (p->data());
	request->set_version(_empower_version);
	request->set_length(sizeof(empower_assoc_request) + ssid.length());
	request->set_type(EMPOWER_PT_ASSOC_REQUEST);
	request->set_seq(get_next_seq());
	request->set_wtp(_wtp);
	request->set_sta(src);
	request->set_bssid(bssid);
	request->set_ssid(ssid);

	if (ht_caps) {
		request->set_flag(EMPOWER_HT_CAPS);
		request->set_ht_caps_info(ht_caps_info);
	}

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

	empower_auth_request *request = (empower_auth_request *) (p->data());
	request->set_version(_empower_version);
	request->set_length(sizeof(empower_auth_request));
	request->set_type(EMPOWER_PT_AUTH_REQUEST);
	request->set_seq(get_next_seq());
	request->set_wtp(_wtp);
	request->set_sta(src);
	request->set_bssid(bssid);

	send_message(p);

}

void EmpowerLVAPManager::send_probe_request(uint32_t iface_id, EtherAddress src, String ssid, bool ht_caps, uint16_t ht_caps_info) {

	WritablePacket *p = Packet::make(sizeof(empower_probe_request) + ssid.length());

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
					  this,
					  __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_probe_request *request = (empower_probe_request *) (p->data());
	request->set_version(_empower_version);
	request->set_length(sizeof(empower_probe_request) + ssid.length());
	request->set_type(EMPOWER_PT_PROBE_REQUEST);
	request->set_seq(get_next_seq());
	request->set_wtp(_wtp);
	request->set_sta(src);
	request->set_ssid(ssid);
	request->set_iface_id(iface_id);

	if (ht_caps) {
		request->set_flag(EMPOWER_HT_CAPS);
		request->set_ht_caps_info(ht_caps_info);
	}

	send_message(p);

}

void EmpowerLVAPManager::send_message(Packet *p) {
	output(0).push(p);
}

void EmpowerLVAPManager::send_hello_response() {

	WritablePacket *p = Packet::make(sizeof(empower_hello_response));

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
				      this,
				      __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_hello_response *hello = (empower_hello_response *) (p->data());

	hello->set_version(_empower_version);
	hello->set_length(sizeof(empower_hello_response));
	hello->set_type(EMPOWER_PT_HELLO_RESPONSE);
	hello->set_seq(get_next_seq());
	hello->set_wtp(_wtp);

	send_message(p);

}

void EmpowerLVAPManager::send_status_slice(uint32_t iface_id, String ssid, int dscp) {

	Slice slice = Slice(ssid, dscp);
	SliceQueue * queue = _eqms[iface_id]->slices()->find(slice).value();

	int len = sizeof(empower_status_slice);

	WritablePacket *p = Packet::make(len);

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
					  this,
					  __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_status_slice *status = (empower_status_slice *) (p->data());
	status->set_version(_empower_version);
	status->set_length(len);
	status->set_type(EMPOWER_PT_STATUS_SLICE);
	status->set_seq(get_next_seq());
	status->set_wtp(_wtp);
	status->set_dscp(queue->_slice._dscp);
	status->set_ssid(queue->_slice._ssid);
	status->set_quantum(queue->_quantum);
    status->set_iface_id(iface_id);
    status->set_scheduler(queue->_scheduler);

	if (queue->_amsdu_aggregation) {
		status->set_flag(EMPOWER_AMSDU_AGGREGATION);
	}

	send_message(p);
}


int EmpowerLVAPManager::handle_slice_stats_request(Packet *p, uint32_t offset) {
	empower_slice_stats_request *q = (empower_slice_stats_request *) (p->data() + offset);
	String ssid = q->ssid();
	uint8_t dscp = q->dscp();
	send_slice_stats_response(ssid, dscp, q->xid());
    return 0;
}

int EmpowerLVAPManager::handle_slice_status_request(Packet *, uint32_t) {

	for (REIter it_re = _ifaces.begin(); it_re.live(); it_re++) {
		int iface_id = it_re.key();
		for (SIter it = _eqms[iface_id]->slices()->begin(); it.live(); it++) {
			send_status_slice(iface_id, it.key()._ssid, it.key()._dscp);
		}
	}

	return 0;

}

int EmpowerLVAPManager::handle_port_status_request(Packet *, uint32_t) {

	for (REIter it_re = _ifaces.begin(); it_re.live(); it_re++) {
		int iface_id = it_re.key();
		for (TxTableIter it = _rcs[iface_id]->tx_policies()->tx_table()->begin(); it.live(); it++) {
			send_status_port(iface_id, it.key());
		}
	}

	return 0;

}

void EmpowerLVAPManager::send_slice_stats_response(String ssid, uint8_t dscp, uint32_t xid) {

	int len = sizeof(empower_slice_stats_response) + _eqms.size() * sizeof(empower_slice_stats_entry);

    WritablePacket *p = Packet::make(len);

    if (!p) {
        click_chatter("%{element} :: %s :: cannot make packet!",
                      this,
                      __func__);
        return;
    }

    memset(p->data(), 0, p->length());

    empower_slice_stats_response *stats = (empower_slice_stats_response *) (p->data());
    stats->set_version(_empower_version);
    stats->set_length(len);
    stats->set_type(EMPOWER_PT_SLICE_STATS_RESPONSE);
    stats->set_seq(get_next_seq());
    stats->set_xid(xid);
    stats->set_wtp(_wtp);
    stats->set_nb_entries(_eqms.size());

	uint8_t *ptr = (uint8_t *) stats;
	ptr += sizeof(empower_slice_stats_response);

	uint8_t *end = ptr + (len - sizeof(empower_slice_stats_response));

	Slice slice = Slice(ssid, dscp);

	for (int i = 0; i < _eqms.size(); i++) {

		assert (ptr <= end);

		empower_slice_stats_entry *entry = (empower_slice_stats_entry *) ptr;

		SIter itr = _eqms[i]->slices()->find(slice);

		entry->set_iface_id(i);
		entry->set_deficit_used(itr.value()->_deficit_used);
		entry->set_max_queue_length(itr.value()->_max_queue_length);
		entry->set_tx_bytes(itr.value()->_tx_bytes);
		entry->set_tx_packets(itr.value()->_tx_packets);

		ptr += sizeof(empower_slice_stats_entry);

	}

    send_message(p);

}

void EmpowerLVAPManager::send_status_lvap(EtherAddress sta) {

	EmpowerStationState *ess = _lvaps.get_pointer(sta);

	if (!ess) {
		click_chatter("%{element} :: %s :: unable to find lvap %s",
					  this,
					  __func__,
					  sta.unparse().c_str());
		return;
	}

	int len = sizeof(empower_status_lvap) + ess->_networks.size() * (6 + WIFI_NWID_MAXSIZE + 1);

	WritablePacket *p = Packet::make(len);

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
					  this,
					  __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_status_lvap *status = (empower_status_lvap *) (p->data());

	status->set_version(_empower_version);
	status->set_length(len);
	status->set_type(EMPOWER_PT_STATUS_LVAP);
	status->set_seq(get_next_seq());
	status->set_assoc_id(ess->_assoc_id);
	if (ess->_set_mask)
		status->set_flag(EMPOWER_STATUS_LVAP_SET_MASK);
	if (ess->_authentication_status)
		status->set_flag(EMPOWER_STATUS_LVAP_AUTHENTICATED);
	if (ess->_association_status)
		status->set_flag(EMPOWER_STATUS_LVAP_ASSOCIATED);
	status->set_wtp(_wtp);
	status->set_sta(ess->_sta);
	status->set_encap(ess->_encap);
	status->set_bssid(ess->_bssid);
	status->set_iface_id(ess->_iface_id);

	if (ess->_ht_caps) {
		status->set_flag(EMPOWER_STATUS_LVAP_HT_CAPS);
		status->set_ht_caps_info(ess->_ht_caps_info);
	}

	status->set_ssid(ess->_ssid);

	uint8_t *ptr = (uint8_t *) status;
	ptr += sizeof(empower_status_lvap);
	uint8_t *end = ptr + (len - sizeof(empower_status_lvap));

	for (int i = 0; i < ess->_networks.size(); i++) {
		assert (ptr <= end);
		ssid_entry *entry = (ssid_entry *) ptr;
		entry->set_ssid(ess->_networks[i]._ssid);
		entry->set_bssid(ess->_networks[i]._bssid);
		ptr += sizeof(ssid_entry);
	}

	send_message(p);

}

void EmpowerLVAPManager::send_status_vap(EtherAddress bssid) {

	Vector<String> ssids;

	EmpowerVAPState *evs = _vaps.get_pointer(bssid);

	if (!evs) {
		click_chatter("%{element} :: %s :: unable to find vap %s",
					  this,
					  __func__,
					  bssid.unparse().c_str());
		return;
	}

	int len = sizeof(empower_status_vap);

	WritablePacket *p = Packet::make(len);

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
					  this,
					  __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_status_vap *status = (empower_status_vap *) (p->data());
	status->set_version(_empower_version);
	status->set_length(len);
	status->set_type(EMPOWER_PT_STATUS_VAP);
	status->set_seq(get_next_seq());
	status->set_wtp(_wtp);
	status->set_bssid(evs->_bssid);
	status->set_iface_id(evs->_iface_id);
	status->set_ssid(evs->_ssid);

	send_message(p);

}

void EmpowerLVAPManager::send_status_port(uint32_t iface_id, EtherAddress sta) {

	TxPolicyInfo * tx_policy = _rcs[iface_id]->tx_policies()->tx_table()->find(sta);

	int len = sizeof(empower_status_port) + tx_policy->_mcs.size() + tx_policy->_ht_mcs.size();

	WritablePacket *p = Packet::make(len);

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
					  this,
					  __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_status_port *status = (empower_status_port *) (p->data());
	status->set_version(_empower_version);
	status->set_length(len);
	status->set_type(EMPOWER_PT_STATUS_PORT);
	status->set_seq(get_next_seq());
	if (tx_policy->_no_ack)
		status->set_flag(EMPOWER_STATUS_PORT_NOACK);
	status->set_rts_cts(tx_policy->_rts_cts);
	status->set_max_amsdu_len(tx_policy->_max_amsdu_len);
	status->set_wtp(_wtp);
	status->set_sta(sta);
	status->set_nb_mcs(tx_policy->_mcs.size());
	status->set_nb_ht_mcs(tx_policy->_ht_mcs.size());
	status->set_iface_id(iface_id);
	status->set_ur_mcast_count(tx_policy->_ur_mcast_count);
	status->set_tx_mcast(tx_policy->_tx_mcast);

	uint8_t *ptr = (uint8_t *) status;
	ptr += sizeof(empower_status_port);
	uint8_t *end = ptr + (len - sizeof(empower_status_port));

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

void EmpowerLVAPManager::send_img_response(uint32_t iface_id, int type, uint32_t xid) {

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

	empower_cqm_response *imgs = (empower_cqm_response *) (p->data());
	imgs->set_version(_empower_version);
	imgs->set_length(len);
	imgs->set_type(type);
	imgs->set_seq(get_next_seq());
	imgs->set_xid(xid);
	imgs->set_wtp(_wtp);
	imgs->set_iface_id(iface_id);
	imgs->set_nb_entries(neighbors.size());

	uint8_t *ptr = (uint8_t *) imgs;
	ptr += sizeof(empower_cqm_response);

	uint8_t *end = ptr + (len - sizeof(empower_cqm_response));

	for (int i = 0; i < neighbors.size(); i++) {
		assert (ptr <= end);
		cqm_entry *entry = (cqm_entry *) ptr;
		entry->set_addr(neighbors[i]._eth);
		entry->set_last_rssi_avg(neighbors[i]._last_rssi);
		entry->set_last_rssi_std(neighbors[i]._last_std);
		entry->set_last_packets(neighbors[i]._last_packets);
		entry->set_hist_packets(neighbors[i]._hist_packets);
		entry->set_mov_rssi(neighbors[i]._sma_rssi->avg());
		ptr += sizeof(cqm_entry);
	}

	send_message(p);

}

void EmpowerLVAPManager::send_wifi_stats_response(uint32_t iface_id, uint32_t xid) {

	int len = sizeof(empower_wifi_stats_response) + sizeof(wifi_stats_entry) * _regmons[iface_id]->registers(EMPOWER_REGMON_TX)->_size * 3;
	WritablePacket *p = Packet::make(len);

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
					  this,
					  __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_wifi_stats_response *stats = (empower_wifi_stats_response *) (p->data());
	stats->set_version(_empower_version);
	stats->set_length(len);
	stats->set_type(EMPOWER_PT_WIFI_STATS_RESPONSE);
	stats->set_seq(get_next_seq());
	stats->set_xid(xid);
	stats->set_wtp(_wtp);
	stats->set_iface_id(iface_id);
	stats->set_nb_entries(_regmons[iface_id]->registers(EMPOWER_REGMON_TX)->_size * 3);

	uint8_t *ptr = (uint8_t *) stats;
	ptr += sizeof(empower_wifi_stats_response);

	uint8_t *end = ptr + (len - sizeof(empower_wifi_stats_response));

	for (int i = 0; i < _regmons[iface_id]->registers(EMPOWER_REGMON_TX)->_size; i++) {
		assert (ptr <= end);
		wifi_stats_entry *entry = (wifi_stats_entry *) ptr;
		entry->set_type(_regmons[iface_id]->registers(EMPOWER_REGMON_TX)->_type);
		entry->set_timestamp(_regmons[iface_id]->registers(EMPOWER_REGMON_TX)->_timestamps[i]);
		entry->set_sample(_regmons[iface_id]->registers(EMPOWER_REGMON_TX)->_samples[i]);
		ptr += sizeof(wifi_stats_entry);
	}

	for (int i = 0; i < _regmons[iface_id]->registers(EMPOWER_REGMON_RX)->_size; i++) {
		assert (ptr <= end);
		wifi_stats_entry *entry = (wifi_stats_entry *) ptr;
		entry->set_type(_regmons[iface_id]->registers(EMPOWER_REGMON_RX)->_type);
		entry->set_timestamp(_regmons[iface_id]->registers(EMPOWER_REGMON_RX)->_timestamps[i]);
		entry->set_sample(_regmons[iface_id]->registers(EMPOWER_REGMON_RX)->_samples[i]);
		ptr += sizeof(wifi_stats_entry);
	}

	for (int i = 0; i < _regmons[iface_id]->registers(EMPOWER_REGMON_ED)->_size; i++) {
		assert (ptr <= end);
		wifi_stats_entry *entry = (wifi_stats_entry *) ptr;
		entry->set_type(_regmons[iface_id]->registers(EMPOWER_REGMON_ED)->_type);
		entry->set_timestamp(_regmons[iface_id]->registers(EMPOWER_REGMON_ED)->_timestamps[i]);
		entry->set_sample(_regmons[iface_id]->registers(EMPOWER_REGMON_ED)->_samples[i]);
		ptr += sizeof(wifi_stats_entry);
	}

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

	empower_summary_trigger* request = (empower_summary_trigger *) (p->data());
	request->set_version(_empower_version);
	request->set_length(len);
	request->set_type(EMPOWER_PT_SUMMARY_TRIGGER);
	request->set_seq(get_next_seq());
	request->set_xid(summary->_trigger_id);
	request->set_wtp(_wtp);
	request->set_nb_frames(summary->_frames.size());
	request->set_iface_id(summary->_iface_id);

	uint8_t *ptr = (uint8_t *) request;
	ptr += sizeof(empower_summary_trigger);
	uint8_t *end = ptr + (len - sizeof(empower_summary_trigger));

	for (int i = 0; i < summary->_frames.size(); i++) {
		assert (ptr <= end);
		summary_entry *entry = (summary_entry *) ptr;
		entry->set_ra(summary->_frames[i]._ra);
		entry->set_ta(summary->_frames[i]._ta);
		entry->set_tsft(summary->_frames[i]._tsft);
		entry->set_seq(summary->_frames[i]._seq);
		entry->set_rssi(summary->_frames[i]._rssi);
		entry->set_rate(summary->_frames[i]._rate);
		entry->set_length(summary->_frames[i]._length);
		entry->set_type(summary->_frames[i]._type);
		entry->set_subtype(summary->_frames[i]._subtype);
		ptr += sizeof(summary_entry);
	}

	summary->_frames.clear();

	send_message(p);

}

void EmpowerLVAPManager::send_lvap_stats_response(EtherAddress lvap, uint32_t xid) {

	EmpowerStationState *ess = _lvaps.get_pointer(lvap);

	if (!ess) {
		click_chatter("%{element} :: %s :: unable to find lvap %s",
					  this,
					  __func__,
					  lvap.unparse().c_str());
		return;
	}

	MinstrelDstInfo *nfo = _rcs.at(ess->_iface_id)->neighbors()->findp(lvap);

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

	empower_lvap_stats_response *lvap_stats = (empower_lvap_stats_response *) (p->data());
	lvap_stats->set_version(_empower_version);
	lvap_stats->set_length(len);
	lvap_stats->set_type(EMPOWER_PT_LVAP_STATS_RESPONSE);
	lvap_stats->set_seq(get_next_seq());
	lvap_stats->set_xid(xid);
	lvap_stats->set_wtp(_wtp);
	lvap_stats->set_iface_id(ess->_iface_id);
	lvap_stats->set_nb_entries(nfo->rates.size());

	uint8_t *ptr = (uint8_t *) lvap_stats;
	ptr += sizeof(empower_lvap_stats_response);
	uint8_t *end = ptr + (len - sizeof(empower_lvap_stats_response));

	for (int i = 0; i < nfo->rates.size(); i++) {
		assert (ptr <= end);
		lvap_stats_entry *entry = (lvap_stats_entry *) ptr;
		entry->set_rate(nfo->rates[i]);
		entry->set_prob((uint32_t) nfo->probability[i]);
		entry->set_cur_prob((uint32_t) nfo->cur_prob[i]);
		entry->set_cur_tp((uint32_t) nfo->cur_tp[i]);
		entry->set_last_attempts((uint32_t) nfo->last_attempts[i]);
		entry->set_last_successes((uint32_t) nfo->last_successes[i]);
		entry->set_hist_attempts((uint32_t) nfo->hist_attempts[i]);
		entry->set_hist_successes((uint32_t) nfo->hist_successes[i]);
		ptr += sizeof(lvap_stats_entry);
	}

	send_message(p);

}

void EmpowerLVAPManager::send_counters_response(EtherAddress sta, uint32_t xid) {

	TxPolicyInfo * txp = get_txp(sta);

    if (!txp) {

        click_chatter("%{element} :: %s :: unable to find TXP for address %s!",
                      this,
                      __func__,
                      sta.unparse().c_str());

        int len = sizeof(empower_counters_response);

        WritablePacket *p = Packet::make(len);

        if (!p) {
            click_chatter("%{element} :: %s :: cannot make packet!",
                          this,
                          __func__);
            return;
        }

        memset(p->data(), 0, p->length());
        empower_counters_response *counters = (empower_counters_response *) (p->data());
        counters->set_version(_empower_version);
        counters->set_length(len);
        counters->set_type(EMPOWER_PT_TXP_COUNTERS_RESPONSE);
        counters->set_seq(get_next_seq());
        counters->set_xid(xid);
        counters->set_wtp(_wtp);
        counters->set_nb_tx(0);

        send_message(p);

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

	empower_counters_response *counters = (empower_counters_response *) (p->data());
	counters->set_version(_empower_version);
	counters->set_length(len);
	counters->set_type(EMPOWER_PT_COUNTERS_RESPONSE);
	counters->set_seq(get_next_seq());
	counters->set_xid(xid);
	counters->set_wtp(_wtp);
	counters->set_sta(sta);
	counters->set_nb_tx(txp->_tx.size());
	counters->set_nb_rx(txp->_rx.size());

	uint8_t *ptr = (uint8_t *) counters;
	ptr += sizeof(empower_counters_response);

	uint8_t *end = ptr + (len - sizeof(empower_counters_response));

	for (CBytesIter iter = txp->_tx.begin(); iter.live(); iter++) {
		assert (ptr <= end);
		counters_entry *entry = (counters_entry *) ptr;
		entry->set_size(iter.key());
		entry->set_count(iter.value());
		ptr += sizeof(counters_entry);
	}

	for (CBytesIter iter = txp->_rx.begin(); iter.live(); iter++) {
		assert (ptr <= end);
		counters_entry *entry = (counters_entry *) ptr;
		entry->set_size(iter.key());
		entry->set_count(iter.value());
		ptr += sizeof(counters_entry);
	}

	send_message(p);

}

void EmpowerLVAPManager::send_incoming_mcast_address(uint32_t iface_id, EtherAddress mcast_address) {

	int len = sizeof(empower_incoming_mcast_address);
	WritablePacket *p = Packet::make(len);

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
					  this,
					  __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_incoming_mcast_address *incoming_mcast_addr = (empower_incoming_mcast_address *) (p->data());

	incoming_mcast_addr->set_version(_empower_version);
	incoming_mcast_addr->set_length(sizeof(empower_incoming_mcast_address));
	incoming_mcast_addr->set_type(EMPOWER_PT_INCOMING_MCAST_ADDRESS);
	incoming_mcast_addr->set_seq(get_next_seq());
	incoming_mcast_addr->set_mcast_addr(mcast_address);
	incoming_mcast_addr->set_wtp(_wtp);
	incoming_mcast_addr->set_iface_id(iface_id);

	send_message(p);

}

void EmpowerLVAPManager::send_igmp_report(EtherAddress src, Vector<IPAddress>* mcast_addresses, Vector<enum empower_igmp_record_type>* igmp_types) {

	int grouprecord_counter;

	for (grouprecord_counter = 0; grouprecord_counter < mcast_addresses->size(); grouprecord_counter++) {

		//send message to the controller
		int len = sizeof(empower_igmp_report);
		WritablePacket *p = Packet::make(len);

		if (!p) {
			click_chatter("%{element} :: %s :: cannot make packet!",
						  this,
						  __func__);
			return;
		}

		memset(p->data(), 0, p->length());

		empower_igmp_report *igmp_report = (empower_igmp_report *) (p->data());

		igmp_report->set_version(_empower_version);
		igmp_report->set_length(sizeof(empower_igmp_report));
		igmp_report->set_type(EMPOWER_PT_IGMP_REPORT);
		igmp_report->set_seq(get_next_seq());
		igmp_report->set_mcast_addr(mcast_addresses->at(grouprecord_counter));
		igmp_report->set_wtp(_wtp);
		igmp_report->set_sta(src);
		igmp_report->set_igmp_type(igmp_types->at(grouprecord_counter));

		send_message(p);
	}
}

void EmpowerLVAPManager::send_txp_counters_response(uint32_t iface_id, uint32_t xid, EtherAddress mcast) {

	TxPolicyInfo * txp = _rcs[iface_id]->tx_policies()->tx_table()->find(mcast);

    if (!txp) {

        click_chatter("%{element} :: %s :: unable to find TXP for address %s!",
                      this,
                      __func__,
                      mcast.unparse().c_str());

        int len = sizeof(empower_txp_counters_response);

        WritablePacket *p = Packet::make(len);

        if (!p) {
            click_chatter("%{element} :: %s :: cannot make packet!",
                          this,
                          __func__);
            return;
        }

        memset(p->data(), 0, p->length());
        empower_txp_counters_response *counters = (empower_txp_counters_response *) (p->data());
        counters->set_version(_empower_version);
        counters->set_length(len);
        counters->set_type(EMPOWER_PT_TXP_COUNTERS_RESPONSE);
        counters->set_seq(get_next_seq());
        counters->set_xid(xid);
        counters->set_wtp(_wtp);
        counters->set_nb_tx(0);

        send_message(p);

        return;

    }

	int len = sizeof(empower_txp_counters_response);
	len += txp->_tx.size() * 6; // the tx samples

	WritablePacket *p = Packet::make(len);

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
					  this,
					  __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_txp_counters_response *counters = (empower_txp_counters_response *) (p->data());
	counters->set_version(_empower_version);
	counters->set_length(len);
	counters->set_type(EMPOWER_PT_TXP_COUNTERS_RESPONSE);
	counters->set_seq(get_next_seq());
	counters->set_xid(xid);
	counters->set_wtp(_wtp);
	counters->set_nb_tx(txp->_tx.size());

	uint8_t *ptr = (uint8_t *) counters;
	ptr += sizeof(empower_txp_counters_response);

	uint8_t *end = ptr + (len - sizeof(empower_txp_counters_response));

	for (CBytesIter iter = txp->_tx.begin(); iter.live(); iter++) {
		assert (ptr <= end);
		counters_entry *entry = (counters_entry *) ptr;
		entry->set_size(iter.key());
		entry->set_count(iter.value());
		ptr += sizeof(counters_entry);
	}

	send_message(p);

}

void EmpowerLVAPManager::send_caps_response() {

	int len = sizeof(empower_caps_response) + _ifaces.size() * sizeof(resource_elements_entry);

	WritablePacket *p = Packet::make(len);

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
					  this,
					  __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_caps_response *caps = (empower_caps_response *) (p->data());
	caps->set_version(_empower_version);
	caps->set_length(len);
	caps->set_type(EMPOWER_PT_CAPS_RESPONSE);
	caps->set_seq(get_next_seq());
	caps->set_wtp(_wtp);
	caps->set_nb_resources_elements(_ifaces.size());

	uint8_t *ptr = (uint8_t *) caps;
	ptr += sizeof(empower_caps_response);

	uint8_t *end = ptr + (len - sizeof(empower_caps_response));

	for (REIter iter = _ifaces.begin(); iter.live(); iter++) {
		assert (ptr <= end);
		ResourceElement *elm = iter.value();
		resource_elements_entry *entry = (resource_elements_entry *) ptr;
		entry->set_iface_id(elm->_iface_id);
		entry->set_hwaddr(elm->_hwaddr);
		entry->set_channel(elm->_channel);
		entry->set_band(elm->_band);
		ptr += sizeof(resource_elements_entry);
	}

	send_message(p);

}

int EmpowerLVAPManager::handle_hello_request(Packet *, uint32_t) {
	// send hello response
	send_hello_response();
	return 0;
}

int EmpowerLVAPManager::handle_caps_request(Packet *, uint32_t) {
	// send caps response
	send_caps_response();
	return 0;
}

int EmpowerLVAPManager::handle_lvap_status_request(Packet *p, uint32_t offset) {
	// send LVAP status update messages
	for (LVAPIter it = _lvaps.begin(); it.live(); it++) {
		send_status_lvap(it.key());
	}
	return 0;
}

int EmpowerLVAPManager::handle_vap_status_request(Packet *, uint32_t) {
	// send VAP status update messages
	for (VAPIter it = _vaps.begin(); it.live(); it++) {
		send_status_vap(it.key());
	}
	return 0;
}

int EmpowerLVAPManager::handle_add_vap(Packet *p, uint32_t offset) {

	empower_add_vap *add_vap = (empower_add_vap *) (p->data() + offset);

	EtherAddress bssid = add_vap->bssid();
	String ssid = add_vap->ssid();
	uint32_t iface_id = add_vap->iface_id();

	if (_vaps.find(bssid) == _vaps.end()) {

		EmpowerVAPState state;
		state._bssid = bssid;
		state._ssid = ssid;
		state._iface_id = iface_id;
		_vaps.set(bssid, state);

		/* Regenerate the BSSID mask */
		compute_bssid_mask();

		/* create default slice */
		if (ssid != "") {
			// TODO: for the moment assume that at worst a 1500 bytes frame can be sent in 12000 usec
			_eqms[iface_id]->set_default_slice(ssid);
		}

		return 0;

	}

	return 0;

}

int EmpowerLVAPManager::handle_del_vap(Packet *p, uint32_t offset) {

	empower_del_vap *q = (empower_del_vap *) (p->data() + offset);
	EtherAddress bssid = q->bssid();

	// First make sure that this VAP isn't here already, in which
	// case we'll just ignore the request
	if (_vaps.find(bssid) == _vaps.end()) {
		click_chatter("%{element} :: %s :: Ignoring VAP delete request because the agent isn't hosting the VAP",
				      this,
				      __func__);

		return -1;
	}

	_vaps.erase(_vaps.find(bssid));

	// Remove this VAP's BSSID from the mask
	compute_bssid_mask();

	return 0;

}

int EmpowerLVAPManager::handle_add_lvap(Packet *p, uint32_t offset) {

	empower_add_lvap *add_lvap = (empower_add_lvap *) (p->data() + offset);

	EtherAddress sta = add_lvap->sta();
	EtherAddress bssid = add_lvap->bssid();
	String ssid = add_lvap->ssid();

	Vector<EmpowerNetwork> networks;

	uint8_t *ptr = (uint8_t *) add_lvap;
	ptr += sizeof(empower_add_lvap);
	uint8_t *end = ptr + (add_lvap->length() - sizeof(empower_add_lvap));

	while (ptr != end) {
		assert (ptr <= end);
		ssid_entry *entry = (ssid_entry *) ptr;
		EmpowerNetwork network = EmpowerNetwork(entry->bssid(), entry->ssid());
		networks.push_back(network);
		ptr += sizeof(ssid_entry);
	}

	if (networks.size() < 1) {
		click_chatter("%{element} :: %s :: invalid networks size (%u)",
				      this,
				      __func__,
					  networks.size());
		return 0;
	}

	int assoc_id = add_lvap->assoc_id();
	uint32_t iface_id = add_lvap->iface_id();
	bool authentication_state = add_lvap->flag(EMPOWER_STATUS_LVAP_AUTHENTICATED);
	bool association_state = add_lvap->flag(EMPOWER_STATUS_LVAP_ASSOCIATED);
	bool set_mask = add_lvap->flag(EMPOWER_STATUS_LVAP_SET_MASK);
	bool ht_caps = add_lvap->flag(EMPOWER_STATUS_LVAP_HT_CAPS);
	uint16_t ht_caps_info = add_lvap->ht_caps_info();
	EtherAddress encap = add_lvap->encap();
	uint32_t xid = add_lvap->xid();

	_lock.acquire_write();

	// if no lvap can be found, then create it
	if (_lvaps.find(sta) == _lvaps.end()) {

		EmpowerStationState state;
		state._sta = sta;
		state._bssid = bssid;
		state._ssid = ssid;
		state._encap = encap;
		state._networks = networks;
		state._assoc_id = assoc_id;
		state._iface_id = iface_id;
		state._ht_caps = ht_caps;
		state._set_mask = set_mask;
		state._authentication_status = authentication_state;
		state._association_status = association_state;
		state._set_mask = set_mask;
		state._ht_caps_info = ht_caps_info;

		// set the CSA values to their default
		state._csa_active = false;
		state._csa_switch_count = 0;
		state._csa_switch_mode = 1;
		state._csa_switch_channel = 0;

		// set the add/del lvap response ids to zero
		state._xid = 0;

		_lvaps.set(sta, state);

		/* Regenerate the BSSID mask */
		compute_bssid_mask();

		/* send add lvap response message */
		send_add_del_lvap_response(EMPOWER_PT_ADD_LVAP_RESPONSE, state._sta, xid, 0);

		_lock.release_write();

		/* create default slice */
		if (ssid != "") {
			// TODO: for the moment assume that at worst a 1500 bytes frame can be sent in 12000 usec
			_eqms[iface_id]->set_default_slice(ssid);
		}

		return 0;

	}

	/* just update lvap with new configuration */
	EmpowerStationState *ess = _lvaps.get_pointer(sta);

	ess->_bssid = bssid;
	ess->_ssid = ssid;
	ess->_networks = networks;
	ess->_encap = encap;
	ess->_authentication_status = authentication_state;
	ess->_association_status = association_state;
	ess->_ht_caps = ht_caps;
	ess->_set_mask = set_mask;
	ess->_ht_caps_info = ht_caps_info;

	/* send add lvap response message */
	send_add_del_lvap_response(EMPOWER_PT_ADD_LVAP_RESPONSE, ess->_sta, xid, 0);

	_lock.release_write();

	return 0;

}

void EmpowerLVAPManager::send_add_del_lvap_response(uint8_t type, EtherAddress sta, uint32_t xid, uint32_t status) {

	WritablePacket *p = Packet::make(sizeof(empower_add_del_lvap_response));

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
					  this,
					  __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_add_del_lvap_response *resp = (empower_add_del_lvap_response *) (p->data());
	resp->set_version(_empower_version);
	resp->set_length(sizeof(empower_add_del_lvap_response));
	resp->set_type(type);
	resp->set_seq(get_next_seq());
	resp->set_sta(sta);
	resp->set_xid(xid);
	resp->set_status(status);
	resp->set_wtp(_wtp);

	send_message(p);

}


int EmpowerLVAPManager::handle_set_port(Packet *p, uint32_t offset) {

	empower_set_port *q = (empower_set_port *) (p->data() + offset);

	EtherAddress addr = q->addr();
	uint32_t iface_id = q->iface_id();

	bool no_ack = q->flag(EMPOWER_STATUS_PORT_NOACK);
	uint16_t rts_cts = q->rts_cts();
	uint16_t max_amsdu_len = q->max_amsdu_len();
	empower_tx_mcast_type tx_mcast = q->tx_mcast();
	uint8_t ur = q->ur_mcast_count();
	Vector<int> mcs;
	Vector<int> ht_mcs;

	uint8_t *ptr = (uint8_t *) q;
	ptr += sizeof(empower_set_port);
	uint8_t *end = ptr + (q->length() - sizeof(empower_set_port));

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

	_rcs[iface_id]->tx_policies()->insert(addr, mcs, ht_mcs, no_ack, tx_mcast, ur, rts_cts, max_amsdu_len);
	_rcs[iface_id]->forget_station(addr);

	MinstrelDstInfo *nfo = _rcs.at(iface_id)->neighbors()->findp(addr);

	if (!nfo || !nfo->rates.size()) {
		TxPolicyInfo * txp = _rcs[iface_id]->tx_policies()->tx_table()->find(addr);
		nfo = _rcs.at(iface_id)->insert_neighbor(addr, txp);
	}

	send_status_port(iface_id, addr);

	return 0;

}

int EmpowerLVAPManager::handle_del_port(Packet *p, uint32_t offset) {

	empower_del_port *q = (empower_del_port *) (p->data() + offset);

	EtherAddress addr = q->addr();
	uint32_t iface_id = q->iface_id();

	_rcs[iface_id]->tx_policies()->remove(addr);
	_rcs[iface_id]->forget_station(addr);

	return 0;

}

int EmpowerLVAPManager::handle_add_rssi_trigger(Packet *p, uint32_t offset) {
	empower_add_rssi_trigger *q = (empower_add_rssi_trigger *) (p->data() + offset);
	_ers->add_rssi_trigger(q->sta(), q->xid(), static_cast<empower_trigger_relation>(q->relation()), q->value(), q->period());
	return 0;
}

int EmpowerLVAPManager::handle_del_rssi_trigger(Packet *p, uint32_t offset) {
	empower_del_rssi_trigger *q = (empower_del_rssi_trigger *) (p->data() + offset);
	_ers->del_rssi_trigger(q->xid());
	return 0;
}

int EmpowerLVAPManager::handle_add_summary_trigger(Packet *p, uint32_t offset) {
	empower_add_summary_trigger *q = (empower_add_summary_trigger *) (p->data() + offset);
	_ers->add_summary_trigger(q->iface_id(), q->addr(), q->xid(), q->limit(), q->period());
	return 0;
}

int EmpowerLVAPManager::handle_del_summary_trigger(Packet *p, uint32_t offset) {
	empower_del_summary_trigger *q = (empower_del_summary_trigger *) (p->data() + offset);
	_ers->del_summary_trigger(q->xid());
	return 0;
}

int EmpowerLVAPManager::handle_del_lvap(Packet *p, uint32_t offset) {

	empower_del_lvap *q = (empower_del_lvap *) (p->data() + offset);
	EtherAddress sta = q->sta();
	uint32_t xid = q->xid();

	// First make sure that this LVAP isn't here already, in which
	// case we'll just ignore the request
	if (_lvaps.find(sta) == _lvaps.end()) {
		return -1;
	}

	EmpowerStationState *ess = _lvaps.get_pointer(sta);

	// if this is an uplink only LVAP the CSA is not needed
	if (!ess->_set_mask) {
		// remove lvap
		remove_lvap(ess->_sta);
		// send del lvap response message
		send_add_del_lvap_response(EMPOWER_PT_DEL_LVAP_RESPONSE, ess->_sta, xid, 0);
		return 0;
	}

	// if a channel is specified then start the csa procedure
	if (q->csa_switch_channel() != 0) {

		click_chatter("%{element} :: %s :: sta %s channel %u is different from current channel %u, starting csa",
				      this,
				      __func__,
					  ess->_sta.unparse().c_str(),
					  q->csa_switch_channel(),
					  _ifaces[ess->_iface_id]->_channel);

		ess->_csa_active = true;
		ess->_csa_switch_count = q->csa_switch_count();
		ess->_csa_switch_mode = q->csa_switch_mode();
		ess->_csa_switch_channel = q->csa_switch_channel();
		ess->_xid = xid;

		return 0;

	}

	// remove lvap
	remove_lvap(ess->_sta);

	// send del lvap response message
	send_add_del_lvap_response(EMPOWER_PT_DEL_LVAP_RESPONSE, ess->_sta, xid, 0);

	return 0;

}

int EmpowerLVAPManager::handle_probe_response(Packet *p, uint32_t offset) {
	empower_probe_response *q = (empower_probe_response *) (p->data() + offset);
	EtherAddress sta = q->sta();
	String ssid = q->ssid();
	EmpowerStationState *ess = _lvaps.get_pointer(sta);
	_ebs->send_probe_response(ess, ssid);
	return 0;
}

int EmpowerLVAPManager::handle_auth_response(Packet *p, uint32_t offset) {
	empower_auth_response *q = (empower_auth_response *) (p->data() + offset);
	EtherAddress sta = q->sta();
	_eauthr->send_auth_response(sta);
	return 0;
}

int EmpowerLVAPManager::handle_assoc_response(Packet *p, uint32_t offset) {
	empower_assoc_response *q = (empower_assoc_response *) (p->data() + offset);
	_eassor->send_association_response(q->sta());
	return 0;
}

int EmpowerLVAPManager::handle_txp_counters_request(Packet *p, uint32_t offset) {
	empower_txp_counters_request *q = (empower_txp_counters_request *) (p->data() + offset);
	send_txp_counters_response(q->iface_id(), q->xid(), q->mcast());
	return 0;
}

int EmpowerLVAPManager::handle_counters_request(Packet *p, uint32_t offset) {
	empower_counters_request *q = (empower_counters_request *) (p->data() + offset);
	send_counters_response(q->sta(), q->xid());
	return 0;
}

int EmpowerLVAPManager::handle_wifi_stats_request(Packet *p, uint32_t offset) {
	empower_wifi_stats_request *q = (empower_wifi_stats_request *) (p->data() + offset);
	send_wifi_stats_response(q->iface_id(), q->xid());
	return 0;
}

int EmpowerLVAPManager::handle_uimg_request(Packet *p, uint32_t offset) {
	empower_cqm_request *q = (empower_cqm_request *) (p->data() + offset);
	send_img_response(q->iface_id(), EMPOWER_PT_UCQM_RESPONSE, q->xid());
	return 0;
}

int EmpowerLVAPManager::handle_nimg_request(Packet *p, uint32_t offset) {
	empower_cqm_request *q = (empower_cqm_request *) (p->data() + offset);
	send_img_response(q->iface_id(), EMPOWER_PT_NCQM_RESPONSE, q->xid());
	return 0;
}

int EmpowerLVAPManager::handle_lvap_stats_request(Packet *p, uint32_t offset) {
	empower_lvap_stats_request *q = (empower_lvap_stats_request *) (p->data() + offset);
	send_lvap_stats_response(q->sta(), q->xid());
	return 0;
}

int EmpowerLVAPManager::handle_set_slice(Packet *p, uint32_t offset) {

	empower_set_slice *add_slice = (empower_set_slice *) (p->data() + offset);

	int iface_id = add_slice->iface_id();

	int dscp = add_slice->dscp();
	String ssid = add_slice->ssid();
	uint32_t quantum = add_slice->quantum();
	bool amsdu_aggregation = add_slice->flags(EMPOWER_AMSDU_AGGREGATION);
	uint8_t scheduler = add_slice->scheduler();

	_eqms[iface_id]->set_slice(ssid, dscp, quantum, amsdu_aggregation, scheduler);

	return 0;

}

int EmpowerLVAPManager::handle_del_slice(Packet *p, uint32_t offset) {

	empower_del_slice *del_slice = (empower_del_slice *) (p->data() + offset);

	int iface_id = del_slice->iface_id();

	int dscp = del_slice->dscp();
	String ssid = del_slice->ssid();

	_eqms[iface_id]->del_slice(ssid, dscp);

	return 0;

}

void EmpowerLVAPManager::push(int, Packet *p) {

	/* This is a control packet coming from a Socket
	 * element.
	 */

	if (p->length() < sizeof(empower_header)) {
		click_chatter("%{element} :: %s :: Packet too small: %d Vs. %d",
				      this,
				      __func__,
				      p->length(),
				      sizeof(empower_header));
		p->kill();
		return;
	}

	uint32_t offset = 0;

	while (offset < p->length()) {
		empower_header *w = (empower_header *) (p->data() + offset);
		switch (w->type()) {
		case EMPOWER_PT_HELLO_REQUEST:
			handle_hello_request(p, offset);
			break;
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
		case EMPOWER_PT_TXP_COUNTERS_REQUEST:
			handle_txp_counters_request(p, offset);
			break;
		case EMPOWER_PT_ADD_RSSI_TRIGGER:
			handle_add_rssi_trigger(p, offset);
			break;
		case EMPOWER_PT_DEL_RSSI_TRIGGER:
			handle_del_rssi_trigger(p, offset);
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
		case EMPOWER_PT_DEL_PORT:
			handle_del_port(p, offset);
			break;
		case EMPOWER_PT_LVAP_STATS_REQUEST:
			handle_lvap_stats_request(p, offset);
			break;
		case EMPOWER_PT_WIFI_STATS_REQUEST:
			handle_wifi_stats_request(p, offset);
			break;
		case EMPOWER_PT_CAPS_REQUEST:
			handle_caps_request(p, offset);
			break;
		case EMPOWER_PT_LVAP_STATUS_REQ:
			handle_lvap_status_request(p, offset);
			break;
		case EMPOWER_PT_VAP_STATUS_REQ:
			handle_vap_status_request(p, offset);
			break;
		case EMPOWER_PT_SET_SLICE:
			handle_set_slice(p, offset);
			break;
		case EMPOWER_PT_DEL_SLICE:
			handle_del_slice(p, offset);
			break;
		case EMPOWER_PT_SLICE_STATS_REQUEST:
			handle_slice_stats_request(p, offset);
			break;
		case EMPOWER_PT_SLICE_STATUS_REQ:
			handle_slice_status_request(p, offset);
			break;
		case EMPOWER_PT_PORT_STATUS_REQ:
			handle_port_status_request(p, offset);
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

	for (unsigned j = 0; j < _ifaces.size(); j++) {

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
			// check if lvap is DL+UL
			if (!it.value()._set_mask) {
				continue;
			}
			// add each bssid in the networks
			const uint8_t *hw = (const uint8_t *) _ifaces[iface_id]->_hwaddr.data();
			for (int k = 0; k < it.value()._networks.size(); k ++) {
				const uint8_t *bssid = (const uint8_t *) it.value()._networks[k]._bssid.data();
				for (i = 0; i < 6; i++) {
					bssid_mask[i] &= ~(hw[i] ^ bssid[i]);
				}
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
			const uint8_t *hw = (const uint8_t *) _ifaces[iface_id]->_hwaddr.data();
			for (i = 0; i < 6; i++) {
				const uint8_t *bssid = (const uint8_t *) it.value()._bssid.data();
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
			sa << " bssid ";
		    sa << it.value()._bssid.unparse();
		    sa << " encap ";
		    sa << it.value()._encap.unparse();
		    sa << " ssid ";
		    sa << it.value()._ssid;
		    sa << " networks [ ";
	    	sa << it.value()._networks[0].unparse();
	    	if (it.value()._networks.size() > 1) {
				for (int i = 1; i < it.value()._networks.size(); i++) {
					sa << ", " << it.value()._networks[i].unparse();
				}
	    	}
			sa << " ] assoc_id ";
			sa << it.value()._assoc_id;
			sa << " hwaddr ";
			sa << td->_ifaces[it.value()._iface_id]->_hwaddr.unparse();
			sa << " channel ";
			sa << td->_ifaces[it.value()._iface_id]->_channel;
			sa << " band ";
			sa << td->_ifaces[it.value()._iface_id]->_band;
			sa << " iface_id ";
			sa << it.value()._iface_id;
			sa << " HT Caps ";
			if (it.value()._ht_caps){
				sa << "True";
			} else {
				sa << "False";
			}
			sa << "\n";
		}
		return sa.take_string();
	}
	case H_INTERFACES: {
		StringAccum sa;
		for (REIter iter = td->_ifaces.begin(); iter.live(); iter++) {
			sa << iter.key() << " -> " << iter.value()->unparse()  << "\n";
		}
		return sa.take_string();
	}
	case H_VAPS: {
	    StringAccum sa;
		for (VAPIter it = td->vaps()->begin(); it.live(); it++) {
			sa << "bssid ";
			sa << it.key().unparse();
			sa << " ssid ";
			sa << it.value()._ssid;
			sa << " iface_id ";
			sa << it.value()._iface_id;
			sa << " hwaddr ";
			sa << td->_ifaces[it.value()._iface_id]->_hwaddr.unparse();
			sa << " channel ";
			sa << td->_ifaces[it.value()._iface_id]->_channel;
			sa << " band ";
			sa << td->_ifaces[it.value()._iface_id]->_band;
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
		case H_RECONNECT: {
			// clear triggers
			f->_ers->clear_triggers();
		}
	}
	return 0;
}

void EmpowerLVAPManager::add_handlers() {
	add_read_handler("debug", read_handler, (void *) H_DEBUG);
	add_read_handler("lvaps", read_handler, (void *) H_LVAPS);
	add_read_handler("vaps", read_handler, (void *) H_VAPS);
	add_read_handler("masks", read_handler, (void *) H_MASKS);
	add_read_handler("bytes", read_handler, (void *) H_BYTES);
	add_read_handler("interfaces", read_handler, (void *) H_INTERFACES);
	add_write_handler("reconnect", write_handler, (void *) H_RECONNECT);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EmpowerLVAPManager)
ELEMENT_REQUIRES(userlevel EmpowerRXStats)
