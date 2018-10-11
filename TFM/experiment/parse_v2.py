#!/usr/bin/python

from __future__ import division
import sys
import dpkt
import struct
import socket
import string
import binascii

types = "src"
def get_pkt_stcture():
    pkt = dict()
    pkt['num'] = 0
    pkt['srcip'] = ''
    pkt['dstip'] = ''
    pkt['srcport'] = ''
    pkt['dstport'] = ''
    pkt['initime'] = 0
    pkt['outtime'] = 0
    pkt['delay'] = 0
    pkt['reorder'] = 0
    pkt['type'] = '' 
    return pkt
def get_pkt_desc(ts, pkt):
    global types;
    eth = dpkt.ethernet.Ethernet(pkt)
    if eth.type != dpkt.ethernet.ETH_TYPE_IP:
        return None
    ip = eth.data
    if ip.p != dpkt.ip.IP_PROTO_UDP:
        return None
################ip pkt in pcap file##############
    #ip = dpkt.ip.IP(pkt)
    #if ip.p != dpkt.ip.IP_PROTO_UDP:
        #return None
#################################################
    pkt = get_pkt_stcture()
    udp = ip.data
    tos = ip.tos & (0x01)
    #lag = ip.off & (0x80)
    flag = (ip.off >> 15) & 0x1
    if flag ==1 and tos ==1:
        pkt['type'] = "redirect"
	types = "dst"
    elif flag == 1:
        pkt['type'] = "infly"
    else:
        pkt['type'] = types

    pkt['srcip'] = socket.inet_ntoa(ip.src)
    pkt['dstip'] = socket.inet_ntoa(ip.dst)
    pkt['srcport'] = udp.sport
    pkt['dstport'] = udp.dport
    b = binascii.hexlify(udp.data)
    pkt['num'] = int(b[0:8],16)
    #itime = b[8:16]
    initime = float(int(b[8:16],16))+float(int(b[16:24],16))/1000000000
    #pkt['initime'] = float(initime)
    pkt['initime'] = initime
    #itimed = b[24:32]
    #print ("inits-:%d"%int(b[24:32],16))
    pkt['delay'] = int(b[32:40],16)/1000000
    pkt['outtime'] = pkt['initime'] + pkt['delay']/1000
    #print ("%d---%s----%f-----%f"%(pkt['num'],pkt['type'],pkt['delay'],pkt['initime'])
    #print ("%d:%f"%(pkt['num'],ts))
    return pkt

def handle_pcap(filename):
    flow = list()
    pcap = dpkt.pcap.Reader(open(filename))
    for ts, pkt in pcap:
        pkt = get_pkt_desc(ts, pkt)
        if pkt != None:
            flow.append(pkt)
    return flow

def Print_stat(flowsrc,flowdst):
    flow = flowsrc+flowdst
    """
    pnum = flow[0]['num']-1 
    for pkt in flow:
        if pkt['num'] != pnum + 1:
            print pkt['num'] 
        pnum = pkt['num']
    maxdelay = 0
    totaldelay = 0
    
    for pkt in flow:
        if pkt['delay'] > maxdelay:
            maxdelay = pkt['delay'] 
        totaldelay =  totaldelay + pkt['delay']
    print ("maxdelay is %d , average delay is %d"%(maxdelay, totaldelay/(flow[-1]['num']-flow[0]['num']+1)))
    """ 
##################caclu redirect infly normal pkt##################
    print("#####################################caculate infly,redirect,normal pkt#########################################")
    result = dict()
    #typelist = ['redirect','infly','normal']
    typelist = ['redirect','infly','src','dst']
    for _type in typelist:
        result[_type] = dict()
        result[_type]['type'] = _type
        result[_type]['pktnum'] = 0
        result[_type]['maxdelay'] = 0
        result[_type]['minidelay'] = 10000
        result[_type]['sumdelay'] = 0
        result[_type]['averagedelay'] = 0
    for pkt in flow:
        _type = pkt['type']
        result[_type]['pktnum'] += 1
        result[_type]['sumdelay'] += pkt['delay']
        if pkt['delay'] > result[_type]['maxdelay']:
            result[_type]['maxdelay'] = pkt['delay'] 
        if  pkt['delay'] < result[_type]['minidelay']:
            result[_type]['minidelay'] = pkt['delay']
    for _type in typelist:
        try:
            result[_type]['averagedelay'] = result[_type]['sumdelay']/result[_type]['pktnum']
        except:
            print ("pkt num of %s is 0"%result[_type]['type'])
    for _type in typelist: 
        print result[_type]
############lost############
    #lost
    flowlose = flow
    flowlose.sort(key=lambda x:x['num'])
    lost = dict()
    lost['redirect'] = 0
    lost['infly'] = 0
    lost['src'] = 0
    lost['dst'] = 0
    lostnum = list()
    prenum = flow[0]['num']-1
    """
    for i in range(flow[0]['num'],flow[-1]['num']):
        for pkt in flowlose:
            if i == pkt['num']:
                del flowlose[0]
                break;
            if i == flow[-1]['num']:
                lost = lost+1;
                lostnum.append(i)
    """
    for pkt in flowlose:
        if prenum+1 != pkt['num']:
            lost[pkt['type']] = lost[pkt['type']]+pkt['num']-prenum-1
            lostscope = (str)(prenum+1)+"-"+(str)(pkt['num']-1) 
            lostnum.append(lostscope)
        prenum = pkt['num']
##################print result#####################
    print("#############################summary##############################")
    print("first packet number: %d , last pkt number: %d"%(flow[0]['num'],flow[-1]['num']))
    if flowsrc and flowdst: 
	    print("pkt processed in src:%d-%d, pkt processed in dst:%d-%d"%(flowsrc[0]['num'],flowsrc[-1]['num'],flowdst[0]['num'],flowdst[-1]['num']))
    print ("infly packets lost: %d"%(lost['infly']))
    print ("redirect packets lost: %d"%(lost['redirect']))
    print ("normal packets lost: %d"%(lost['normal']))
    print("received packet: %d"%(flow[-1]['num']-flow[0]['num']-lost['infly']-lost['redirect']-lost['normal']+1))
    print("###################################################################")
    print lostnum
    write_to_file(flow,"tfm_time.txt")

def write_to_file(flow,filename):    
    f=open(filename,'w')     
    for pkt in flow:
        print("num:%d,  init:%f,   outtime:%f"%(pkt['num'],pkt['initime'],pkt['outtime']))
        line = str(pkt['num']) + " " + str(pkt['initime']-flow[0]['initime']) + " " + str(pkt['outtime']-flow[0]['initime'])+'\n'
        f.write(line)
    f.close()

if __name__ == '__main__':
    if len(sys.argv) == 1:
        print("python parse_v2.py -h  app help")
    else:
        flowsrc = list();
        flowdst = list();
        flowsrc = handle_pcap(sys.argv[1])
	if len(sys.argv) == 3:
	    flowdst = handle_pcap(sys.argv[2])
        Print_stat(flowsrc,flowdst)
