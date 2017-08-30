/*
 * randomdrop.{cc,hh} -- randomly drop packets
 * Roberto Riggio
 *
 * Copyright (c) 2017 CREATE-NET
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
#include "randomdrop.hh"
#include <click/args.hh>
#include <click/error.hh>
CLICK_DECLS

RandomDrop::RandomDrop() : _prob(0)
{
}

int
RandomDrop::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh).read_mp("DROP", _prob).complete();
}

void
RandomDrop::push(int, Packet *p)
{
  if (false)
    p->kill();
  else
    output(0).push(p);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(RandomDrop)
ELEMENT_MT_SAFE(RandomDrop)
