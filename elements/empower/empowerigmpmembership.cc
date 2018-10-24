/*
 * empowerigmpmembership.{cc,hh} -- handle IGMP packets (EmPOWER Access Point)
 * Estefania Coronado
 *
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
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <click/error.hh>
#include <click/ipaddress.hh>
#include <clicknet/wifi.h>
#include <clicknet/ip.h>
#include <clicknet/ether.h>
#include "empowerpacket.hh"
#include "empowerlvapmanager.hh"
#include "igmppacket.hh"
#include "empowerigmpmembership.hh"
#include "empowermulticasttable.hh"
CLICK_DECLS

EmpowerIgmpMembership::EmpowerIgmpMembership() :
		_el(0), _mtbl(0), _debug(false) {
}

EmpowerIgmpMembership::~EmpowerIgmpMembership() {
}

int EmpowerIgmpMembership::configure(Vector<String> &conf, ErrorHandler *errh) {

	int ret = Args(conf, this, errh)
              .read_m("EL", ElementCastArg("EmpowerLVAPManager"), _el)
			  .read_m("MTBL", ElementCastArg("EmpowerMulticastTable"), _mtbl)
			  .read("DEBUG", _debug).complete();

	return ret;

}

void EmpowerIgmpMembership::push(int, Packet *p) {

	const click_ip *ip = p->ip_header();

	void *igmpmessage = (void *) (p->data() + 14 + ip->ip_hl * 4);
	/* IGMP messages
	 * RFC 1112 IGMPv1: 11= query 12= join group
	 * RFC 2236 IGMPv2: 11= query 16= join group 17= leave group
	 * RFC 3376 IGMPv3: 11= query 22= join or leave group
	 */

	if (p->length() < sizeof(struct click_ether)) {
		click_chatter("%{element} :: %s :: packet too small: %d vs %d",
				      this,
					  __func__,
					  p->length(),
					  sizeof(struct click_ether));
		p->kill();
		return;
	}

	click_ether *eh = (click_ether *) p->data();
	EtherAddress src = EtherAddress(eh->ether_shost);
	Vector<IPAddress> mcast_addresses;
	Vector<enum empower_igmp_record_type> igmp_types;
	EmpowerStationState *ess = _el->get_ess(src);

	if (!ess) {
		click_chatter("%{element} :: %s :: Unknown station %s",
				      this, __func__,
				      src.unparse().c_str());
		p->kill();
		return;
	}

	unsigned short grouprecord_counter;
	igmpv1andv2message * v1andv2message;
	igmpv3report * v3report;

	switch (*(char *) igmpmessage) {
	case 0x11: {
		// TODO. Query received from other AP.
		p->kill();
		return;
	}
	case 0x12: {
		if (click_in_cksum((unsigned char*) igmpmessage, sizeof(igmpv1andv2message)) != 0) {
			click_chatter("%{element} :: %s :: IGMPv1 report wrong checksum!", this, __func__);
			p->kill();
			return;
		}
		igmp_types.push_back(V1_MEMBERSHIP_REPORT);
		mcast_addresses.push_back(IPAddress(ip->ip_dst));
		_mtbl->add_group(IPAddress(ip->ip_dst));
		_mtbl->join_group(src, IPAddress(ip->ip_dst));
		break;
	}
	case 0x16: {
		if (click_in_cksum((unsigned char*) igmpmessage, sizeof(igmpv1andv2message)) != 0) {
			click_chatter("%{element} :: %s :: IGMPv2 join wrong checksum!", this, __func__);
			p->kill();
			return;
		}
		igmp_types.push_back(V2_JOIN_GROUP);
		mcast_addresses.push_back(IPAddress(ip->ip_dst));
		_mtbl->add_group(IPAddress(ip->ip_dst));
		_mtbl->join_group(src, IPAddress(ip->ip_dst));
		break;
	}
	case 0x17: {
		if (click_in_cksum((unsigned char*) igmpmessage, sizeof(igmpv1andv2message)) != 0) {
			click_chatter("%{element} :: %s :: IGMPv2 leave wrong checksum!", this, __func__);
			p->kill();
			return;
		}
		v1andv2message = (igmpv1andv2message *) igmpmessage;
		igmp_types.push_back(V2_LEAVE_GROUP);
		mcast_addresses.push_back(IPAddress(v1andv2message->group));
		_mtbl->leave_group(src, IPAddress(v1andv2message->group));
		break;
	}
	case 0x22: {
		if (click_in_cksum((unsigned char*) igmpmessage, ((int) (ntohs(ip->ip_len) - (ip->ip_hl * 4)))) != 0) {
			click_chatter("%{element} :: %s :: IGMPv3 wrong checksum!", this, __func__);
			p->kill();
			return;
		}
		v3report = (igmpv3report *) igmpmessage;
		for (grouprecord_counter = 0; grouprecord_counter < ntohs(v3report->no_of_grouprecords); grouprecord_counter++) {
			switch (v3report->grouprecords[grouprecord_counter].type) {
			case 0x01:
				click_chatter("%{element} :: %s :: IGMPv3 include mode", this, __func__);
				igmp_types.push_back(V3_MODE_IS_INCLUDE);
				mcast_addresses.push_back(IPAddress(v3report->grouprecords[grouprecord_counter].multicast_address));
				_mtbl->leave_group(src, IPAddress(v3report->grouprecords[grouprecord_counter].multicast_address));
				break;
			case 0x02:
				click_chatter("%{element} :: %s :: IGMPv3 exclude mode", this, __func__);
				igmp_types.push_back(V3_MODE_IS_EXCLUDE);
				mcast_addresses.push_back(IPAddress(v3report->grouprecords[grouprecord_counter].multicast_address));
				_mtbl->add_group(IPAddress(v3report->grouprecords[grouprecord_counter].multicast_address));
				_mtbl->join_group(src, IPAddress(v3report->grouprecords[grouprecord_counter].multicast_address));
				break;
			case 0x03:
				click_chatter("%{element} :: %s :: IGMPv3 change to include mode", this, __func__);
				igmp_types.push_back(V3_CHANGE_TO_INCLUDE_MODE);
				mcast_addresses.push_back(IPAddress(v3report->grouprecords[grouprecord_counter].multicast_address));
				_mtbl->leave_group(src, IPAddress(v3report->grouprecords[grouprecord_counter].multicast_address));
				break;
			case 0x04:
				click_chatter("%{element} :: %s :: IGMPv3 change to exclude mode", this, __func__);
				igmp_types.push_back(V3_CHANGE_TO_EXCLUDE_MODE);
				mcast_addresses.push_back(IPAddress(v3report->grouprecords[grouprecord_counter].multicast_address));
				_mtbl->add_group(IPAddress(v3report->grouprecords[grouprecord_counter].multicast_address));
				_mtbl->join_group(src,IPAddress(v3report->grouprecords[grouprecord_counter].multicast_address));
				break;
			case 0x05:
				//TODO: "ALLOW_NEW_SOURCES". Sources management
				igmp_types.push_back(V3_ALLOW_NEW_SOURCES);
				mcast_addresses.push_back(IPAddress(v3report->grouprecords[grouprecord_counter].multicast_address));
				break;
			case 0x06:
				//TODO: "BLOCK_OLD_SOURCES". Sources management
				igmp_types.push_back(V3_BLOCK_OLD_SOURCES);
				mcast_addresses.push_back(IPAddress(v3report->grouprecords[grouprecord_counter].multicast_address));
				break;
			default:
				click_chatter("%{element} :: %s :: Unknown type in IGMP grouprecord or bad group record pointer", this, __func__);
				p->kill();
				return;
			}
		}
		break;
	}
	default: {
		click_chatter("%{element} :: %s :: Unknown IGMP message", this, __func__);
		p->kill();
		return;
	}
	}

	if (ess && !mcast_addresses.empty() && !igmp_types.empty() && mcast_addresses.size() == igmp_types.size()) {
		if (_debug) {
			click_chatter("%{element} :: %s :: IGMP packet received from %s, number of addresses %d, number of IGMP reports %d.",
						  this,
						  __func__,
						  src.unparse().c_str(),
						  mcast_addresses.size(),
						  igmp_types.size());
		}
		_el->send_igmp_report(src, &mcast_addresses, &igmp_types);
	} else {
		if (_debug) {
			click_chatter("%{element} :: %s :: Invalid IGMP packet received from %s, number of addresses %d, number of IGMP reports %d.",
						  this,
						  __func__,
						  src.unparse().c_str(),
						  mcast_addresses.size(),
						  igmp_types.size());
		}
	}

	p->kill();
	return;

}


enum {
	H_DEBUG
};

String EmpowerIgmpMembership::read_handler(Element *e, void *thunk) {
	EmpowerIgmpMembership *td = (EmpowerIgmpMembership *) e;
	switch ((uintptr_t) thunk) {
	case H_DEBUG:
		return String(td->_debug) + "\n";
	default:
		return String();
	}
}

int EmpowerIgmpMembership::write_handler(const String &in_s, Element *e,
		void *vparam, ErrorHandler *errh) {

	EmpowerIgmpMembership *f = (EmpowerIgmpMembership *) e;
	String s = cp_uncomment(in_s);

	switch ((intptr_t) vparam) {
	case H_DEBUG: {
		bool debug;
		if (!BoolArg().parse(s, debug))
			return errh->error("debug parameter must be boolean");
		f->_debug = debug;
		break;
	}
	}
	return 0;
}

void EmpowerIgmpMembership::add_handlers() {
	add_read_handler("debug", read_handler, (void *) H_DEBUG);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EmpowerIgmpMembership)
