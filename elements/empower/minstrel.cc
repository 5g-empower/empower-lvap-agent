/*
 * minstrel.{cc,hh} -- sets wifi txrate annotation on a packet
 * Roberto Riggio
 *
 * Copyright (c) 2010 CREATE-NET
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
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <clicknet/wifi.h>
#include "minstrel.hh"

CLICK_DECLS

Minstrel::Minstrel() 
  : _tx_policies(0), _timer(this), _lookaround_rate(20), _offset(0),
	_active(true), _period(500), _ewma_level(75), _debug(false) {
}

Minstrel::~Minstrel() {
}

void Minstrel::run_timer(Timer *)
{
	for (MinstrelIter iter = _neighbors.begin(); iter.live(); iter++) {
		MinstrelDstInfo *nfo = &iter.value();
		int max_tp = 0, index_max_tp = 0, index_max_tp2 = 0;
		int max_prob = 0, index_max_prob = 0;
		uint32_t usecs;
		int i;
		uint32_t p;
		for (i = 0; i < nfo->rates.size(); i++) {
			if (_transm_time.find(nfo->rates[i]) == _transm_time.end()) {
				if (nfo->ht)
					usecs = calc_usecs_wifi_packet_ht(1500, nfo->rates[i], 0);
				else
					usecs = calc_usecs_wifi_packet(1500, nfo->rates[i], 0);

				_transm_time.set(nfo->rates[i], usecs);
			}
			else
			{
				usecs = _transm_time.get(nfo->rates[i]);
			}
			if (!usecs) {
				usecs = 1000000;
			}
			/* To avoid rounding issues, probabilities scale from 0 (0%)
			 * to 18000 (100%) */
			if (nfo->attempts[i]) {
				p = (nfo->successes[i] * 18000) / nfo->attempts[i];
				nfo->hist_successes[i] += nfo->successes[i];
				nfo->hist_successes_bytes[i] += nfo->successes_bytes[i];
				nfo->hist_attempts[i] += nfo->attempts[i];
				nfo->hist_attempts_bytes[i] += nfo->attempts_bytes[i];
				nfo->cur_prob[i] = p;
				p = ((p * (100 - _ewma_level)) + (nfo->probability[i] * _ewma_level)) / 100;
				nfo->probability[i] = p;
				nfo->cur_tp[i] = p * (1000000 / usecs);
			}
			nfo->last_successes[i] = nfo->successes[i];
			nfo->last_successes_bytes[i] = nfo->successes_bytes[i];
			nfo->last_attempts[i] = nfo->attempts[i];
			nfo->last_attempts_bytes[i] = nfo->attempts_bytes[i];
			nfo->successes[i] = 0;
			nfo->successes_bytes[i] = 0;
			nfo->attempts[i] = 0;
			nfo->attempts_bytes[i] = 0;
			/* Sample less often below the 10% chance of success.
			 * Sample less often above the 95% chance of success. */
			if ((nfo->probability[i] > 17100) || (nfo->probability[i] < 1800)) {
				nfo->sample_limit[i] = 4;
			} else {
				nfo->sample_limit[i] = -1;
			}
		}
		for (i = 0; i < nfo->rates.size(); i++) {
			if (max_tp < nfo->cur_tp[i]) {
				index_max_tp = i;
				max_tp = nfo->cur_tp[i];
			}
			if (max_prob < nfo->probability[i]) {
				index_max_prob = i;
				max_prob = nfo->probability[i];
			}
		}
		max_tp = 0;
		for (i = 0; i < nfo->rates.size(); i++) {
			if (i == index_max_tp) {
				continue;
			}
			if (max_tp < nfo->cur_tp[i]) {
				index_max_tp2 = i;
				max_tp = nfo->cur_tp[i];
			}
		}
		nfo->max_tp_rate = index_max_tp;
		nfo->max_tp_rate2 = index_max_tp2;
		nfo->max_prob_rate = index_max_prob;
	}
	_timer.schedule_after_msec(_period);
}

int Minstrel::initialize(ErrorHandler *)
{
	_timer.initialize(this);
	_timer.schedule_now();
	return 0;
}

int Minstrel::configure(Vector<String> &conf, ErrorHandler *errh)
{

	int ret = Args(conf, this, errh)
		      .read("OFFSET", _offset)
		      .read_m("TP", ElementCastArg("TransmissionPolicies"), _tx_policies)
		      .read("LOOKAROUND_RATE", _lookaround_rate)
		      .read("EWMA_LEVEL", _ewma_level)
		      .read("PERIOD", _period)
		      .read("ACTIVE",  _active)
		      .read("DEBUG",  _debug)
		      .complete();

	return ret;

}

void Minstrel::process_feedback(Packet *p_in) {
	if (!p_in) {
		return;
	}
	uint8_t *dst_ptr = (uint8_t *) p_in->data() + _offset;
	EtherAddress dst = EtherAddress(dst_ptr);
	/* don't record info for bcast packets */
	if (dst.is_group()) {
		return;
	}
	struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p_in);
	int success = !(ceh->flags & WIFI_EXTRA_TX_FAIL);
	MinstrelDstInfo *nfo = _neighbors.findp(dst);
	/* rate wasn't set */
	if (!nfo) {
		if (_debug) {
			click_chatter("%{element} :: %s :: no info for %s",
					this, 
					__func__,
					dst.unparse().c_str());
		}
		return;
	}
	/* rate is HT but feedback is legacy */
	if (nfo->ht && !(ceh->flags & WIFI_EXTRA_MCS)) {
		if (_debug) {
			click_chatter("%{element} :: %s :: rate is HT but feedback is legacy %u",
					this,
					__func__,
					ceh->rate);
		}
		return;
	}
	nfo->add_result(ceh->rate, ceh->max_tries, success, p_in->length());
	return;
}

void Minstrel::assign_rate(Packet *p_in)
{

	if (!p_in) {
		return;
	}

	uint8_t *dst_ptr = (uint8_t *) p_in->data() + _offset;
	EtherAddress dst = EtherAddress(dst_ptr);
	struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p_in);

	memset((void*)ceh, 0, sizeof(struct click_wifi_extra));

	TxPolicyInfo * tx_policy = _tx_policies->supported(dst);
	if (dst.is_group()) {
		ceh->flags |= WIFI_EXTRA_TX_NOACK;
		if(!tx_policy || tx_policy->_ht_mcs.size() == 0) {
			Vector<int> rates = _tx_policies->lookup(dst)->_mcs;
			ceh->rate = (rates.size()) ? rates[0] : 2;
		}
		else {
			Vector<int> ht_rates = _tx_policies->lookup(dst)->_ht_mcs;
			ceh->rate = (ht_rates.size()) ? ht_rates[0] : 2;
			ceh->flags |= WIFI_EXTRA_MCS;
		}

		ceh->rate1 = -1;
		ceh->rate2 = -1;
		ceh->rate3 = -1;
		ceh->max_tries = 1;
		ceh->max_tries1 = 0;
		ceh->max_tries2 = 0;
		ceh->max_tries3 = 0;
		return;
	}

	struct click_wifi *w = (struct click_wifi *) p_in->data();

	int type = w->i_fc[0] & WIFI_FC0_TYPE_MASK;
	int subtype = w->i_fc[0] & WIFI_FC0_SUBTYPE_MASK;

	if (type == WIFI_FC0_TYPE_MGT) {
		if (subtype == WIFI_FC0_SUBTYPE_BEACON || subtype == WIFI_FC0_SUBTYPE_PROBE_RESP) {
			ceh->flags |= WIFI_EXTRA_TX_NOACK;
		}
		Vector<int> rates = _tx_policies->lookup(dst)->_mcs;
		ceh->rate = (rates.size()) ? rates[0] : 2;
		ceh->rate1 = -1;
		ceh->rate2 = -1;
		ceh->rate3 = -1;
		ceh->max_tries = 1;
		ceh->max_tries1 = 0;
		ceh->max_tries2 = 0;
		ceh->max_tries3 = 0;
		return;
	}

	MinstrelDstInfo *nfo = _neighbors.findp(dst);

	if (!nfo || !nfo->rates.size()) {
		if (_debug) {
			click_chatter("%{element} :: %s :: adding %s",
					this, 
					__func__,
					dst.unparse().c_str());
		}
		if (!tx_policy) {
			if (_debug) {
				click_chatter("%{element} :: %s :: rate info not found for %s",
						this, 
						__func__,
						dst.unparse().c_str());
			}
			Vector<int> rates = _tx_policies->lookup(dst)->_mcs;
			ceh->rate = (rates.size()) ? rates[0] : 2;
			ceh->rate1 = -1;
			ceh->rate2 = -1;
			ceh->rate3 = -1;
			ceh->max_tries = WIFI_MAX_RETRIES + 1;
			ceh->max_tries1 = 0;
			ceh->max_tries2 = 0;
			ceh->max_tries3 = 0;
			return;
		}
		if (tx_policy->_ht_mcs.size()) {
			_neighbors.insert(dst, MinstrelDstInfo(dst, tx_policy->_ht_mcs, true));
			nfo = _neighbors.findp(dst);
		} else {
			_neighbors.insert(dst, MinstrelDstInfo(dst, tx_policy->_mcs, false));
			nfo = _neighbors.findp(dst);
		}
	}

	int ndx;
	bool sample = false;
	int delta;

	ndx = nfo->max_tp_rate;
	nfo->packet_count++;
	delta = (nfo->packet_count * _lookaround_rate / 100) - (nfo->sample_count);

	/* delta > 0: sampling required */
	if (delta > 0) {
		if (nfo->packet_count >= 10000) {
			nfo->sample_count = 0;
			nfo->packet_count = 0;
		}
		if (nfo->rates.size() > 0) {
			int sample_ndx = click_random(0, nfo->rates.size() - 1);
			if (nfo->sample_limit[sample_ndx] != 0) {
				sample = true;
				ndx = sample_ndx;
				nfo->sample_count++;
				if (nfo->sample_limit[sample_ndx] > 0) {
					nfo->sample_limit[sample_ndx]--;
				}
			}
		}
	}
	/* If the sampling rate already has a probability
	 * of >95%, we shouldn't be attempting to use it,
	 * as this only wastes precious airtime */
	if (sample && (nfo->probability[ndx] > 17100)) {
		ndx = nfo->max_tp_rate;
		sample = false;
	}

	ceh->magic = WIFI_EXTRA_MAGIC;

	if (nfo->ht) {
		ceh->flags |= WIFI_EXTRA_MCS;
	}

	if (sample) {
		if (nfo->rates[ndx] < nfo->rates[nfo->max_tp_rate]) {
			ceh->rate = nfo->rates[nfo->max_tp_rate];
			ceh->rate1 = nfo->rates[ndx];
		} else {
			ceh->rate = nfo->rates[ndx];
			ceh->rate1 = nfo->rates[nfo->max_tp_rate];
		}
	} else {
		ceh->rate = nfo->rates[nfo->max_tp_rate];
		ceh->rate1 = nfo->rates[nfo->max_tp_rate2];
	}

	ceh->rate2 = nfo->rates[nfo->max_prob_rate];
	ceh->rate3 = nfo->rates[0];

	ceh->max_tries = 4;
	ceh->max_tries1 = 4;
	ceh->max_tries2 = 4;
	ceh->max_tries3 = 4;

	return;

}

Packet* Minstrel::pull(int port) {
	Packet *p = input(port).pull();
	if (p && _active) {
		assign_rate(p);
	}
	return p;
}

void Minstrel::push(int port, Packet *p_in)
{
	if (!p_in) {
		return;
	}
	if (port != 0) {
		process_feedback(p_in);
	} else {
		assign_rate(p_in);
	}
	checked_output_push(port, p_in);
}

String Minstrel::print_rates()
{
	StringAccum sa;
	for (MinstrelIter iter = _neighbors.begin(); iter.live(); iter++) {
		MinstrelDstInfo *nfo = &iter.value();
		sa << nfo->unparse();
	}
	return sa.take_string();
}

enum {
	H_RATES, H_DEBUG
};

String Minstrel::read_handler(Element *e, void *thunk) {
	Minstrel *c = (Minstrel *) e;
	switch ((intptr_t) (thunk)) {
	case H_DEBUG:
		return String(c->_debug) + "\n";
	case H_RATES:
		return c->print_rates();
	default:
		return "<error>\n";
	}
}

int Minstrel::write_handler(const String &in_s, Element *e, void *vparam, ErrorHandler *errh) {
	Minstrel *d = (Minstrel *) e;
	String s = cp_uncomment(in_s);
	switch ((intptr_t) vparam) {
	case H_DEBUG: {
		bool debug;
		if (!cp_bool(s, &debug))
			return errh->error("debug parameter must be boolean");
		d->_debug = debug;
		break;
	}
	}
	return 0;
}

void Minstrel::add_handlers() {
	add_read_handler("rates", read_handler, H_RATES);
	add_read_handler("debug", read_handler, H_DEBUG);
	add_write_handler("debug", write_handler, H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Minstrel)
