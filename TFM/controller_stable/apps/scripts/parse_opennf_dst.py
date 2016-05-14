#!/usr/bin/python

from __future__ import division
import sys
import dpkt
import struct
import socket
import string
import binascii

pkt_num = 0
offset = 0
dst_received = 0
def get_pkt_desc_send(line):
################ip pkt in pcap file##############
    #ip = dpkt.ip.IP(pkt)
    #if ip.p != dpkt.ip.IP_PROTO_UDP:
        #return None
#################################################
    """
    if line[:6] == "EVTPRO":
        parts= string.split(line,",")
        times= string.split(parts[1],".")
    """
    parts = string.split(line,":")
    if len(parts) < 4:
        return
    #print parts[2]
    types = string.split(parts[2]," ")
    if len(types) < 3 or types[2] != "time":
        return
    #if types[1] != "time":
    #    return
 
    times = string.split(parts[3]," ")
    #print times[1]
    #print times[2]
    ms = filter(str.isdigit,times[2])
    ms = ms.rjust(4,'0')
    initime = (times[1][:10]+ms)[:14]
    pkt = dict()
    pkt['num'] = 0
    pkt['srcip'] = ''
    pkt['dstip'] = ''
    pkt['srcport'] = ''
    pkt['dstport'] = ''
    pkt['initime'] = ''
    pkt['outtime'] = ''
    pkt['delay'] = 0
    pkt['reorder'] = 0
    pkt['type'] = '' 
    
    global pkt_num
    pkt_num =pkt_num+1
    pkt['type'] = "normal"
    pkt['num'] = pkt_num
    #itime = b[8:16]
    pkt['initime'] = initime
    #itimed = b[24:32]
    #pkt['delay'] = float(times[0]+times[1])/1000
    #print ("%d---%s----%f-----%f"%(pkt['num'],pkt['type'],pkt['delay'],pkt['initime']))
    return pkt
def get_pkt_desc_dst(line,flow):
    if line[:6] != "EVTPRO":
        return
    parts= string.split(line,",")
    times= string.split(parts[1],".")
    dst_time = times[0]+times[1][0:4]
    #print dst_time
    global offset
    flow[offset]['type'] = "redirect"
    flow[offset]['outtime'] = dst_time
    flow[offset]['delay'] = float(dst_time) - float(flow[offset]['initime'])
    offset +=1
    return
def get_pkt_desc_src(line,flow):
    return

def handle_send_file(filename):
    flow = list()
    f=open(filename,'r')
    for line in f:
        pkt = get_pkt_desc_send(line)
        if pkt != None:
            flow.append(pkt)
    global offset
    global dst_received
    global pkt_num
    offset = pkt_num - dst_received
    print ("pkt num:%d"%pkt_num)
    return flow

def handle_src_file(filename,flow):
    f=open(filename,'r')
    for line in f:
        get_pkt_desc_src(line,flow)
    return flow

def handle_dst_file(filename,flow):
    f=open(filename,'r')
    for line in f:
        get_pkt_desc_dst(line,flow)
    return flow
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
     
##################caclu redirect infly normal pkt##################
    print("#####################################caculate infly,redirect,normal pkt#########################################")
    result = dict()
    typelist = ['redirect','infly','normal']
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
        if result[_type]['maxdelay']>950:
            
            maxdelay = result[_type]['maxdelay'] 
            result[_type]['maxdelay'] = 1000-result[_type]['minidelay']
            result[_type]['minidelay'] = 1000-maxdelay
            result[_type]['averagedelay'] = 1000-result[_type]['averagedelay']
            
            result[_type]['maxdelay'] = result[_type]['maxdelay'] - result[_type]['minidelay']+0.05
            result[_type]['averagedelay'] = result[_type]['averagedelay'] - result[_type]['minidelay']+0.05
            result[_type]['minidelay'] = 0.05
        print result[_type]
############lost############
    #lost
    flowlose = flow
    flowlose.sort(key=lambda x:x['num'])
    lost = dict()
    lost['redirect'] = 0
    lost['infly'] = 0
    lost['normal'] = 0
    lostnum = list()
    prenum = flow[0]['num']-1
    
    for i in range(flow[0]['num'],flow[-1]['num']):
        for pkt in flowlose:
            if i == pkt['num']:
                del flowlose[0]
                break;
            if i == flow[-1]['num']:
                lost = lost+1;
                lostnum.append(i)
    
    for pkt in flowlose:
        if prenum+1 != pkt['num']:
            lost[pkt['type']] = lost[pkt['type']]+pkt['num']-prenum-1
            lostscope = (str)(prenum+1)+"-"+(str)(pkt['num']-1) 
            lostnum.append(lostscope)
        prenum = pkt['num']
##################print result#####################
    print("#############################summary##############################")
    print("first packet number: %d , last pkt number: %d"%(flow[0]['num'],flow[-1]['num']))
    print ("infly packets lost: %d"%(lost['infly']))
    print ("redirect packets lost: %d"%(lost['redirect']))
    print ("normal packets lost: %d"%(lost['normal']))
    print("received packet: %d"%(flow[-1]['num']-flow[0]['num']-lost['infly']-lost['redirect']-lost['normal']+1))
    print("###################################################################")
"""

if __name__ == '__main__':
    global dst_received
    if len(sys.argv) == 1:
        print("argv error")
    else:
        dst_received = int(sys.argv[1])
        flow = handle_send_file('./send.txt')
        #flow = handle_src_file('./src.txt',flow)
        flow = handle_dst_file('./dst.txt',flow)
        for pkt in flow:
            print ("num-%d, delay-%f"%(pkt['num'],pkt['delay']/10))
            print ("init-%s, out-%s"%(pkt['initime'],pkt['outtime']))
        print flow[-1]
    #else:
     #   handle_file(sys.argv[1])
