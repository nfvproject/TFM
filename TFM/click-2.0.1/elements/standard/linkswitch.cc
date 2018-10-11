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
#include "linkswitch.hh"
#include <click/args.hh>
#include <click/error.hh>
CLICK_DECLS

Linkswitch::Linkswitch()
{
}
int
Linkswitch::initialize(ErrorHandler *errh)
{
}
Linkswitch::~Linkswitch()
{
}
int
Linkswitch::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh).read_mp("SWITCH", _switch).complete();
}

void 
Linkswitch::push(int port, Packet *p)
{
    if (_switch == 1)
    {
        output(0).push(p);
    }
    else
    {
        p->kill();
    }
    return;
}
void
Linkswitch::add_handlers()
{
    add_write_handler("switch", write_handler, 0);
}
int
Linkswitch::write_handler(const String &s, Element *e, void *vparam,
                             ErrorHandler *errh)
{
    Linkswitch *is = (Linkswitch *)e;
    int which = reinterpret_cast<intptr_t>(vparam);
    switch (which) {
      case 0:
        bool _s;
        if (!BoolArg().parse(s, _s))
            return errh->error("mfstart parameter must be boolean");
        is->_switch = _s;
        return 0;
      default:
        return errh->error("internal error");
    }
}
CLICK_ENDDECLS
EXPORT_ELEMENT(Linkswitch)
ELEMENT_MT_SAFE(Linkswitch)
