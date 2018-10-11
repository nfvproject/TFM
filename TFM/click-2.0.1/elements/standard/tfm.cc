// -*- mode: c++; c-basic-offset: 4 -*-
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
#include "tfm.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <json/json.h>
CLICK_DECLS

Tfm::Tfm()
    : _q1(0),_q2(0)
{
    current_state = S_NORMAL;
    b_stateinstalled = 0;
    tstart = 0;
    tfinish = 0;
    bptime = 0;
    sockfd = 0;
    //nb1 = 0;
    //nb2=0;
}
int
Tfm::initialize(ErrorHandler *errh)
{
    B1head = 0;
    B1tail = 0;
    B2head = 0;
    B2tail = 0;
    capacity = 10000000;
    assert(!_q1 || !_q2 );
    _q1 = (Packet **) CLICK_LALLOC(sizeof(Packet *) * capacity);
    _q2 = (Packet **) CLICK_LALLOC(sizeof(Packet *) * capacity);
    if (_q1 == 0 || _q2 == 0)
        return errh->error("out of memory");
    return 0;
}
Tfm::~Tfm()
{
    CLICK_LFREE(_q1, sizeof(Packet *) * capacity);
    CLICK_LFREE(_q2, sizeof(Packet *) * capacity);
}

void 
Tfm::conn_write_append_newline(char *buf, int len)
{
    int tmplen = htonl(len);
    write(sockfd, &tmplen, sizeof(tmplen));
    int result = write(sockfd, (void *)buf, len);
}
void 
Tfm::send_finish_ack()
{
    char jsonstr[100] = "{\"id\":1,\"type\":\"tfm-migrate-finish\",\"count\":1}";
    conn_write_append_newline(jsonstr, strlen(jsonstr));
}
int
Tfm::conn_active_open()
{
    int fd, recvbytes;
    struct hostent *host;
    struct sockaddr_in serv_addr;
 
    if (( fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("socket error!");
        return 0;
    }
    bzero(&serv_addr,sizeof(serv_addr));
    serv_addr.sin_family= AF_INET;
    serv_addr.sin_port = htons(7790);
    serv_addr.sin_addr.s_addr= inet_addr("10.21.2.185");
 
    if (connect(fd, (struct sockaddr *)&serv_addr,sizeof(struct sockaddr)) == -1) {
        printf("connect error!");
        return 0;
    }  
    return fd;
}

int
Tfm::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh).read_mp("LENGTH", k).complete();
}

void 
Tfm::push(int port, Packet *p)
{
    /*
    float delays_state_ins = 0;
    delays_state_ins = (float)Timestamp::now().nsec()/1000000 - tstart;
    if (delays_state_ins > MS_DELAY && tfinish == 0)
    {
        printf("delays_state_ins is %f\n",delays_state_ins);
        b_stateinstalled = 1;
        tfinish = 1;
    }
    */
    if (port == 0)
    {
	stat.normal_num++;
    }
    else if (port == 1)
    {
	stat.redirect_num++;
    }
    else if (port == 2)
    {
	stat.infly_num++;
    }
    else
    {
	stat.lastinfly_num++;
    }
   
    int event = 0;
    if(b_stateinstalled==1)
    {
        if( current_state > S_INSD_MFSTART-1)
        {}
        else
        {
            b_stateinstalled = 0;
            ReleaseB1();
            if(current_state == S_RCVLINFLY)
            {
                ReleaseB2();
                //printf("Stateinstalled->Release B1\n");
            }
            current_state += E_STATEINSTALLED;
            //printf("Stateinstalled->Release B1\n");
            bptime = 1;
        }
    }
    //if a normal pkt,just output without state transition
    if (current_state == S_NORMAL || port == 0)
    {
        output(0).push(p);
        return;
    }
    //port0->normal pkts,port1->redirectpkts,port2->inflypkts,port3->lastinflypkts
    //printf("port == %d\n",port);
    switch (current_state)
    {
        case S_MFSTART:
      	    //printf("___________________\nstate:S_MFSTART\n");
            if (port == 1)
            {
                    //printf("Notify Controller to update flowtable");
                    //buffer pkts to B2
                PushB2(p);
                event=E_RCVRDPKT;
            }	
            else if (port == 2)
            {
                    //buffer pkts to B1
                PushB1(p);
            }	
            else
            {
                //printf("Notify Controller to update flowtable");
                //drop last in-fly pkts
                //printf("drop lastinflypkt");
                p->kill();
                event = 5;
            }	
            break;
        case S_RCVRDPKT:
      	    //printf("___________________\nstate:S_RCVRDPKT\n");
            if (port == 1)
            {
                    //buffer pkts to B2
                PushB2(p);
            }	
            else if (port == 2)
            {
                    //buffer pkts 1o B1
                PushB1(p);
            }	
            else
            {
                    //drop last in-fly pkts
                //printf("drop lastinflypkt");
                event = E_RCVLINFLY;
                p->kill();
            }	
            break;
        case S_RCVLINFLY:
      	    //printf("___________________\nstate:S_RCVLINFLY\n");
            if (port == 1)
            {
                    //buffer pkts to B2
                PushB2(p);
            }	
            else if (port == 2)
            {
                    //buffer pkts to B1
                PushB1(p);
            }	
            else
            {
                    //buffer pkts to B1
                //printf("drop lastinflypkt");
                p->kill();
            }	
            break;
        case S_INSD_MFSTART:
      	    //printf("___________________\nstate:S_INSD_MFSTART\n");
            if (port == 1)
            {
                //printf("Notify Controller to update flowtable\n");
                PushB2(p);
                event=E_RCVRDPKT;
            }	
            else if (port == 2)
            {
                output(0).push(Cleartag(p));
            }	
            else
            {
                //printf("Notify Controller to update flowtable\n");
                    //buffer pkts to B1
                ReleaseB2();
                //printf("drop lastinflypkt\n");
                p->kill();
                event = 5;
            }	
            break;
        case S_INSD_RCVRDPKT:
      	    //printf("___________________\nstate:S_INSD_RCVRDPKT\n");
            if (port == 1)
            {
                    //buffer pkts pto B1
                PushB2(p);
            }	
            else if (port == 2)
            {
                    //buffer pkts to B1
                output(0).push(Cleartag(p));
            }	
            else 
            { //buffer pkts to B1 printf("drop lastinflypkt");
                ReleaseB2();
                event = E_RCVLINFLY;
            }	
            break;
        case S_INSD_RCVLINFLY:
      	    //printf("___________________\nstate:S_INSD_RCVLINFLY\n");
            if (tfinish == 0)
            {
                //connection with controller is available
                if (sockfd != 0)
                {
                    send_finish_ack();
                }
                tfinish = (float)Timestamp::now().nsec()/1000000-tstart;
                printf("move time is %f\n",tfinish);
                printf("normal_pkts_num: %d\n",stat.normal_num);
                printf("infly_pkts_num: %d\n",stat.infly_num);
                printf("redirect_pkts_num: %d\n",stat.redirect_num);
                printf("lastinfly_pkts_num: %d\n",stat.lastinfly_num);
            }
            if (port == 3)
            {
                    //buffer pkts to B1
                //printf("drop lastinflypkt\n");
            }	
            else
            {
                output(0).push(Cleartag(p));
            }	
            break;
        default:
            break;
    }
    current_state+=event;
    if (bptime == 1 || event != 0)
    {
        printf("current state %s, time is %f\n",Printstate(current_state), (float)Timestamp::now().nsec()/1000000);
        bptime = 0;
    }
}
inline char * 
Tfm::Printstate(int i)
{
        char *state = "";
        if(i==0) state = "S_NORMAL"; 
        else if(i==1) state = "S_MFSTART"; 
        else if(i==3) state = "S_RCVRDPKT"; 
        else if(i==6) state = "S_RCVLINFLY";
        else if(i==9) state = "S_INSD_MFSTART"; 
        else if(i==11) state = "S_INSD_RCVRDPKT"; 
        else if(i==14) state = "S_INSD_RCVLINFLY";
        else state = "error";
        return state;
}

inline Packet *
Tfm::Cleartag(Packet *p)
{
    //to monitor pkt delay, disable this function in beta version
    /*
    assert(p->has_network_header());
    WritablePacket *q;
    if (!(q = p->uniqueify()))
	return 0;
    click_ip *ip = q->ip_header();
    uint16_t old_hw = (reinterpret_cast<uint16_t *>(ip))[0];
    ip->ip_tos = (ip->ip_tos & 0xfe);   //set last bit of tos to 0
    ip->ip_off = (ip->ip_off & htons(0x7fff));   //set first bit of flag to 0
    click_update_in_cksum(&ip->ip_sum, old_hw, reinterpret_cast<uint16_t *>(ip)[0]);
    return q;
    */
    return p;
}
inline void 
Tfm::Reset(bool reset)
{
    if(reset == 0) {}
    else
    {
        ReleaseB1();
        ReleaseB2();
        current_state = 0;
        b_stateinstalled = 0;
    }
    return;

}
inline bool
Tfm::PushB1(Packet *p)
{
    assert(p);
    Storage::index_type h = B1head, t = B1tail, nt = next_i(t);
    if (nt != h) {
        _q1[t] = p;
        packet_memory_barrier(_q1[t], B1tail);
        B1tail = nt;
        return true;
    } else {
        p->kill();
        return false;
        printf("B1 is overloaded");
    }
}
inline int
Tfm::ReleaseB1()
{
    Storage::index_type h = B1head, t = B1tail;
    int release_num = 0;
    while (h != t) {
        Packet *p = _q1[h];
        //printf("push");
        packet_memory_barrier(_q1[h], B1head);
        B1head = next_i(h);
        h = B1head;
        assert(p);
        release_num ++;
        output(0).push(Cleartag(p));
    }
    return release_num;
}
inline Packet *
Tfm::smaction(Packet *p)
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
//buffer redirect pkts
inline bool
Tfm::PushB2(Packet *p)
{
    assert(p);
    p = smaction(p);
    Storage::index_type h = B2head, t = B2tail, nt = next_i(t);
    if (nt != h) {
        _q2[t] = p;
        packet_memory_barrier(_q2[t], B2tail);
        //printf("buffer redirect pkts");
        B2tail = nt;
        return true;
    } else {
        p->kill();
        return false;
        printf("B2 is overloaded");
    }
}
inline int
Tfm::ReleaseB2()
{
    Storage::index_type h = B2head, t = B2tail;
    int release_num = 0;
    while (h != t) {
        Packet *p = _q2[h];
        //printf("push");
        packet_memory_barrier(_q2[h], B2head);
        B2head = next_i(h);
        h = B2head;
        assert(p);
        release_num ++;
        output(0).push(Cleartag(p));
    }
    return release_num;
}
void
Tfm::add_handlers()
{
    add_read_handler("state", read_handler, 0);
    add_write_handler("mfstart", write_handler, 0);
    add_write_handler("stateinstall", write_handler, 1);
    add_write_handler("reset", write_handler, 2);
}

int
Tfm::write_handler(const String &s, Element *e, void *vparam,
                             ErrorHandler *errh)
{
    Tfm *is = (Tfm *)e;
    int which = reinterpret_cast<intptr_t>(vparam);
    switch (which) {
      case 0:
        bool mfstart;
        if (!BoolArg().parse(s, mfstart))
            return errh->error("mfstart parameter must be boolean");
        if(is->current_state == S_NORMAL)
        {
            is->current_state = mfstart;
            is->bptime = 1;
	    is->sockfd = is->conn_active_open();
            is->tstart = (float)Timestamp::now().nsec()/1000000;
            printf("current state %s, time is %f\n",is->Printstate(is->current_state), is->tstart);
        }
        return 0;
      case 1:
        bool stateinstall;
        if (!BoolArg().parse(s, stateinstall))
            return errh->error("stateinstall parameter must be boolean");
        is->b_stateinstalled = stateinstall;
        return 0;
      case 2:
        bool reset;
        if (!BoolArg().parse(s, reset))
            return errh->error("reset parameter must be boolean");
        is->Reset(reset);
        return 0;
      default:
        return errh->error("internal error");
    }
}
String
Tfm::read_handler(Element *e, void *thunk)
{
    Tfm *is = static_cast<Tfm *>(e);
    int which = reinterpret_cast<intptr_t>(thunk);
    switch (which)
    {
        case 0:     return is->getstate(is->current_state);
        default:
            return "error";
    }
}
/*void
Tfm::add_handlers()
{
    add_data_handlers("stateinstalled", Handler::OP_READ | Handler::OP_WRITE | Handler::CHECKBOX | Handler::CALM, &b_stateinstalled);
}*/
CLICK_ENDDECLS
EXPORT_ELEMENT(Tfm)
ELEMENT_MT_SAFE(Tfm)
