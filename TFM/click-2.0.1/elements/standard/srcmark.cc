// -*- mode: c++; c-baic-offset: 4 -*-
/*
 * strip.{cc,hh} -- element strips bytes from front of packet
 * Robert Morris, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "srcmark.hh"
#include <click/args.hh>
#include <click/error.hh>
CLICK_DECLS

Srcmark::Srcmark()
{
}
int
Srcmark::initialize(ErrorHandler *errh)
{
}
Srcmark::~Srcmark()
{
}
int
Srcmark::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh).read_mp("LENGTH", k).complete();
}

void 
Srcmark::push(int port, Packet *p)
{
    if ((p = smaction(p)) != 0)
    {
        output(0).push(p);
    }
    return;
}
inline Packet *
Srcmark::smaction(Packet *p)
{
    assert(p->has_network_header());
    WritablePacket *q;
    if (!(q = p->uniqueify()))
	return 0;
    click_ip *ip = q->ip_header();
    uint16_t old_hw = (reinterpret_cast<uint16_t *>(ip))[0];
    //inpkt  normal:tos:flag = 0:0  ->  inflypkt:tos:flag = 0:1, redirect:tos:flag = 1:0->lastinfly tos:flag = 1:1      
    ip->ip_off = (ip->ip_off | htons(0x8000));   
    click_update_in_cksum(&ip->ip_sum, old_hw, reinterpret_cast<uint16_t *>(ip)[0]);
    return q;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Srcmark)
ELEMENT_MT_SAFE(Srcmark)
