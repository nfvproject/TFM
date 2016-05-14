// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_TFM_HH
#define CLICK_TFM_HH
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

#define S_NORMAL 0
#define S_MFSTART 1
#define S_RCVRDPKT 3
#define S_RCVLINFLY 6
#define S_INSD_MFSTART 9
#define S_INSD_RCVRDPKT 11
#define S_INSD_RCVLINFLY 14
#define E_STARTMF 1
#define E_STATEINSTALLED 8
#define E_RCVRDPKT 2
#define E_RCVLINFLY 3
#define MS_DELAY 50
class Tfm : public Element,public Storage { 
    public:

    Tfm();
    ~Tfm();

    const char *class_name() const		{ return "Tfm"; }
    const char *port_count() const		{ return "-/1"; }
    const char *processing() const              { return PUSH; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler*);

    //Packet *simple_action(Packet *);
    void push(int port, Packet *p);
    //buffer B1 and B2 ,push and release
    //B1 buffer infly pkts, B2 buffer redirect pkts
    inline bool PushB1(Packet*);
    inline int ReleaseB1();
    inline bool PushB2(Packet*);
    inline int ReleaseB2();
    inline void Reset(bool);
    inline Packet *Cleartag(Packet *);
    void add_handlers();
    inline Packet *smaction(Packet *);
    inline char *Printstate(int i);
    inline int conn_active_open();
    inline void send_finish_ack();
    inline void conn_write_append_newline(char *buf, int len);
  private:
    int b_stateinstalled;
    int current_state;
    float tstart;
    float tfinish;
    int bptime;
    int sockfd;
    struct Stat
    {
	int normal_num;
	int infly_num;
	int redirect_num;
	int lastinfly_num;
    };
    struct Stat stat;
   // Packet* B1[100];
    //Packet* B2[100];
    //int nb1;
    //int nb2;
    int k;
   // int k;
   // static int b_lastinfly;
   // static int i;
  protected:
    index_type capacity;
    //Buffer B1 
    Packet* volatile * _q1;
    //index_type B1capacity;
    volatile index_type B1head;
    volatile index_type B1tail;
    //Buffer B2
    Packet* volatile * _q2;
    //index_type B2capacity;
    volatile index_type B2head;
    volatile index_type B2tail;
    static int write_handler(const String&, Element*, void*, ErrorHandler*);
    static String read_handler(Element*, void*);
    index_type next_i(index_type i) const {
        return (i!=capacity ? i+1 : 0);
    }
    String getstate(int i) const {
        if(i==0) return "S_NORMAL"; 
        else if(i==1) return "S_MFSTART"; 
        else if(i==3) return "S_RCVRDPKT"; 
        else if(i==6) return "S_RCVLINFLY";
        else if(i==9) return "S_INSD_MFSTART"; 
        else if(i==11) return "S_INSD_RCVRDPKT"; 
        else if(i==14) return "S_INSD_RCVLINFLY";
        else return "error";
    }
    
};

CLICK_ENDDECLS
#endif
