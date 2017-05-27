/*
 * empowermulticasttable.{cc,hh} -- handle IGMP groups (EmPOWER Access Point)
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
#include "igmppacket.hh"
#include "empowerigmpmembership.hh"
#include "empowermulticasttable.hh"
CLICK_DECLS

EmpowerMulticastTable::EmpowerMulticastTable() :
	_debug(false) {
}

EmpowerMulticastTable::~EmpowerMulticastTable() {
}

int EmpowerMulticastTable::configure(Vector<String> &conf, ErrorHandler *errh) {

	int ret = Args(conf, this, errh).read("DEBUG", _debug).complete();

	return ret;

}

bool EmpowerMulticastTable::addgroup(IPAddress group) {

	const unsigned char *p = group.data();

	click_chatter("%{element} :: %s :: Adding IGMP group %d.%d.%d.%d",
				  this,
				  __func__,
				  p[0], p[1], p[2], p[3]);

	EmpowerMulticastGroup newgroup;
	newgroup.group = group;

	Vector<EmpowerMulticastGroup>::iterator i;
	for (i = multicastgroups.begin(); i != multicastgroups.end(); i++) {
		if (IPAddress((*i).group) == IPAddress(group))
			return false;
	}

	newgroup.mac_group = ip_mcast_addr_to_mac(group);
	multicastgroups.push_back(newgroup);

	return true;

}

bool EmpowerMulticastTable::joingroup(EtherAddress sta, IPAddress group)
{

	Vector<EmpowerMulticastGroup>::iterator i;

	for (i = multicastgroups.begin(); i != multicastgroups.end(); i++)
	{
		if ((*i).group.addr() == group.addr())
		{
			Vector<EmpowerMulticastReceiver>::iterator a;
			for (a = (*i).receivers.begin(); a != (*i).receivers.end(); a++)
			{
				if ((*a).sta == sta)
				{
					click_chatter("%{element} :: %s :: Station %s already in IGMP group %d.%d.%d.%d!",
							this, __func__, sta.unparse().c_str(), (*i).group.data()[0], (*i).group.data()[1],
							(*i).group.data()[2], (*i).group.data()[3]);
					return false;
				}
			}
			EmpowerMulticastReceiver new_receiver;
			new_receiver.sta = sta;
			(*i).receivers.push_back(new_receiver);
			click_chatter("%{element} :: %s :: Station %s added to IGMP group %d.%d.%d.%d!",
					      this,
						  __func__,
						  sta.unparse().c_str(),
						  (*i).group.data()[0],
						  (*i).group.data()[1],
						  (*i).group.data()[2],
						  (*i).group.data()[3]);

			return true;
		}
	}

	return false;
}

bool EmpowerMulticastTable::leavegroup(EtherAddress sta, IPAddress group)
{

	Vector<EmpowerMulticastGroup>::iterator i;

	for (i = multicastgroups.begin(); i != multicastgroups.end(); i++)
	{
		if ((*i).group.addr() == group.addr())
		{
			Vector<EmpowerMulticastReceiver>::iterator a;
			for (a = (*i).receivers.begin(); a != (*i).receivers.end(); a++)
			{
				if ((*a).sta == sta)
				{
					(*i).receivers.erase(a);
					click_chatter("%{element} :: %s :: Station %s added removed from IGMP group %d.%d.%d.%d",
										this, __func__, sta.unparse().c_str(),
										(*i).group.data()[0], (*i).group.data()[1],
										(*i).group.data()[2], (*i).group.data()[3]);
					// The group is deleted if no more receivers belong to it
					if ((*i).receivers.begin() == (*i).receivers.end())
					{
						multicastgroups.erase(i);
						click_chatter("%{element} :: %s :: IGMP group %d.%d.%d.%d is empty. It is about to be deleted",
																this, __func__, (*i).group.data()[0], (*i).group.data()[1],
																(*i).group.data()[2], (*i).group.data()[3]);
					}
					return true;
				}
			}

		}
	}
	click_chatter("%{element} :: %s :: IGMP leave group request received from station %s not found in group %d.%d.%d.%d",
														this, __func__, sta.unparse().c_str(), (*i).group.data()[0], (*i).group.data()[1],
														(*i).group.data()[2], (*i).group.data()[3]);
	return false;
}

Vector<EmpowerMulticastTable::EmpowerMulticastReceiver>* EmpowerMulticastTable::getIGMPreceivers(EtherAddress group)
{
	Vector<struct EmpowerMulticastGroup>::iterator i;

	for (i = multicastgroups.begin(); i != multicastgroups.end(); i++)
	{
		if (i->mac_group == group)
		{
			return &(i->receivers);
		}
	}

	click_chatter("%{element} :: %s :: IGMP group %s not found.",
														  this,
														  __func__, group.unparse().c_str());

	return NULL;
}

bool EmpowerMulticastTable::leaveallgroups(EtherAddress sta)
{

	click_chatter("%{element} :: %s :: Station %s is about to leave all IGMP groups",
										  this,
										  __func__, sta.unparse().c_str());

	Vector<EmpowerMulticastGroup>::iterator i;

	for (i = multicastgroups.begin(); i != multicastgroups.end();)
	{
		Vector<EmpowerMulticastReceiver>::iterator a;
		for (a = (*i).receivers.begin(); a != (*i).receivers.end(); a++)
		{
			if ((*a).sta == sta)
			{
				(*i).receivers.erase(a);
				click_chatter("%{element} :: %s :: Station %s removed from  IGMP group %d.%d.%d.%d",
									this, __func__, sta.unparse().c_str(),
									(*i).group.data()[0], (*i).group.data()[1],
									(*i).group.data()[2], (*i).group.data()[3]);

				break;
			}
		}

		// The group is deleted if no more receivers belong to it
		if ((*i).receivers.begin() == (*i).receivers.end())
		{
			i = multicastgroups.erase(i);
			click_chatter("%{element} :: %s :: IGMP group %d.%d.%d.%d is empty. It is about to be deleted",
													this, __func__, (*i).group.data()[0], (*i).group.data()[1],
													(*i).group.data()[2], (*i).group.data()[3]);
		}
		else
			i++;
	}

	return true;
}

enum {
	H_DEBUG, H_MULTICAST_TABLE
};

String EmpowerMulticastTable::read_handler(Element *e, void *thunk) {
	EmpowerMulticastTable *td = (EmpowerMulticastTable *) e;
	switch ((uintptr_t) thunk) {
	case H_DEBUG:
		return String(td->_debug) + "\n";
	case H_MULTICAST_TABLE: {
		StringAccum sa;
		Vector<EmpowerMulticastGroup>::iterator i;
		for (i = td->multicastgroups.begin(); i != td->multicastgroups.end(); i++) {
			sa << i->group.unparse() << " " <<  i->mac_group.unparse();

			Vector<EmpowerMulticastReceiver>::iterator a;
			sa << " receivers [ ";
			for (a = (*i).receivers.begin(); a != (*i).receivers.end(); a++) {
				sa << (*a).sta.unparse();
				if (a != (*i).receivers.end())
					sa << ", ";
			}
			sa << "]\n";
		}
		return sa.take_string();
	}
	default:
		return String();
	}
}

int EmpowerMulticastTable::write_handler(const String &in_s, Element *e,
		void *vparam, ErrorHandler *errh) {

	EmpowerMulticastTable *f = (EmpowerMulticastTable *) e;
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

void EmpowerMulticastTable::add_handlers() {
	add_read_handler("debug", read_handler, (void *) H_DEBUG);
	add_read_handler("multicast_table", read_handler, (void *) H_MULTICAST_TABLE);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EmpowerMulticastTable)
