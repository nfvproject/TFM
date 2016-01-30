// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_LINKSWITCH_HH
#define CLICK_LINKSWITCH_HH
#include <click/element.hh>
#include <click/standard/storage.hh>
CLICK_DECLS

/*
 * =c
 * Strip(LENGTH)
 * =s basicmod
 * strips bytes from front of packets
 * =d
 * Deletes the first LENGTH bytes from each packet.
 * =e
 * Use this to get rid of the Ethernet header:
 *
 *   Strip(14)
 * =a StripToNetworkHeader, StripIPHeader, EtherEncap, IPEncap, Truncate
 */

class Linkswitch : public Element,public Storage { public:

    Linkswitch();
    ~Linkswitch();

    const char *class_name() const		{ return "Linkswitch"; }
    const char *port_count() const		{ return "1/1"; }
    const char *processing() const      { return PUSH; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler*);

    //Packet *simple_action(Packet *);
    void push(int port, Packet *p);
    void add_handlers();
    //buffer B1 and B2 ,push and release
    //B1 buffer infly pkts, B2 buffer redirect pkts
    private:

    static int write_handler(const String&, Element*, void*, ErrorHandler*);
    int _switch;
};

CLICK_ENDDECLS
#endif
