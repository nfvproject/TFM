// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_SRCMARK_HH
#define CLICK_SRCMARK_HH
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

class Srcmark : public Element,public Storage { public:

    Srcmark();
    ~Srcmark();

    const char *class_name() const		{ return "Srcmark"; }
    const char *port_count() const		{ return "-/1"; }
    const char *processing() const      { return PUSH; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler*);

    //Packet *simple_action(Packet *);
    void push(int port, Packet *p);
    inline Packet *smaction(Packet *p);
    //buffer B1 and B2 ,push and release
    //B1 buffer infly pkts, B2 buffer redirect pkts
    private:

    int k;
};

CLICK_ENDDECLS
#endif
