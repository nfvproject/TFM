// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_TAGREDIRECT_HH
#define CLICK_TAGREDIRECT_HH
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

class Tagredirect : public Element,public Storage { public:

    Tagredirect();
    ~Tagredirect();

    const char *class_name() const		{ return "Tagredirect"; }
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
    int open;
};

CLICK_ENDDECLS
#endif
