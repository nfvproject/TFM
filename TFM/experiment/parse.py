#!/usr/bin/python

from __future__ import division
import sys
import dpkt
import struct
import socket
import string
import binascii

pkt_num = 1
offset = 0
dst_received = 0
#time
sendinit = 0
dstnor = 0



flag_src_stop = 0
def get_pkt_struct():
    pkt = dict()
    pkt['num'] = 0
    pkt['srcip'] = ''
    pkt['dstip'] = ''
    pkt['srcport'] = ''
    pkt['dstport'] = ''
    pkt['initime'] = 0

    pkt['outtime'] = 0
    pkt['outtimenf'] = 0
    pkt['outtimetfm'] = 0
    pkt['outtimepcap'] = 0
    pkt['outtimeudp'] = 0

    pkt['delay'] = 0
    pkt['tfmdelay'] = 0
    pkt['nfdelay'] = 0
    pkt['reorder'] = 0
    pkt['type'] = '' 
    return pkt

def get_file_type(filename):
    if filename.find('txt') != -1:
        return "txt"
    elif filename.find('pcap') != -1:
        return "pcap"
    else:
        print("file type is wrong, txt or pcap")
        return

def get_file_desc(srcfile,dstfile,sendfile):
    files = dict()
    for location in ['dst','src','send']:
        files[location] = dict()
        files[location]['filename'] = dstfile
        files[location]['type'] = get_file_type(dstfile)
    return files

def get_pkt_desc_pcap(ts,pktp,filelocation):
    global pkt_num
    pkt = get_pkt_struct()
    pkt['num'] = pkt_num
    pkt_num +=1
    pkt['type'] = filelocation
    if filelocation == "send":
        pkt['initime'] = ts
        return pkt
    #src or dst 
    eth = dpkt.ethernet.Ethernet(pktp)
    if eth.type != dpkt.ethernet.ETH_TYPE_IP:
        return None
    ip = eth.data
    if ip.p == dpkt.ip.IP_PROTO_TCP:
        pkt['outtimetfm'] = ts
        payload = ip.data
    elif ip.p == dpkt.ip.IP_PROTO_UDP:
        pkt['outtimetfm'] = ts
        payload = ip.data
        b = binascii.hexlify(payload.data)
        pkt['num'] = int(b[0:8],16)
        pkt['delay'] = int(b[32:40],16)/1000000
    else:
        return

    tos = ip.tos & (0x01)
    flag = (ip.off >> 15) & 0x1
    if flag ==1 and tos ==1:
        pkt['type'] = "redirect"
    elif flag == 1:
        pkt['type'] = "infly"
    else:
        pkt['type'] = filelocation

    pkt['srcip'] = socket.inet_ntoa(ip.src)
    pkt['dstip'] = socket.inet_ntoa(ip.dst)
    pkt['srcport'] = payload.sport
    pkt['dstport'] = payload.dport

    return pkt
def get_pkt_desc_txt(line,filelocation):
    global pkt_num
    if filelocation == "send":
        parts = string.split(line,":")
        if len(parts) < 4:
            return
        types = string.split(parts[2]," ")
        if len(types) < 3 or types[2] != "time":
            return
        times = string.split(parts[3]," ")
        ms = filter(str.isdigit,times[2])
        ss = filter(str.isdigit,times[1])
        initime = float(ss) + float(float(ms)/1000000)

        pkt =  get_pkt_struct()
        global pkt_num
        pkt['type'] = "send"
        pkt['num'] = pkt_num
        pkt['initime'] = initime
        pkt_num =pkt_num+1
        return pkt
    #src or dst file
    else:
        global sendinit
        if line[:1].isdigit() == False:
            return
        pkt = get_pkt_struct()
        global dstnor
        global dstred
        parts= string.split(line,",")
        outtime = float(parts[1][:17])
        if outtime < sendinit:
            return
        types = filter(str.isdigit,parts[2])
        if types == "1":
            pkt['type'] = "infly"
        elif types == "2":
            pkt['type'] = "redirect"
        else:
            pkt['type'] = filelocation
        if dstnor > outtime:
            print ("error:dst pkt out time early than pkt send out time,discard this pkt")
            return
        pkt['outtimenf'] = outtime
        pkt['num'] = pkt_num
        pkt_num =pkt_num+1
        return pkt
def handle_file(filename,filetype,filelocation):
    flow = list()
    global pkt_num
    pkt_num = 1
    if filetype == "pcap":
        pcap = dpkt.pcap.Reader(open(filename))
        for ts, pkt in pcap:
            pkt = get_pkt_desc_pcap(ts,pkt,filelocation)
            if pkt != None:
                flow.append(pkt)
    elif filetype == "txt":
        f=open(filename,'r')
        for line in f:
            pkt = get_pkt_desc_txt(line,filelocation)
            if pkt != None:
                flow.append(pkt)
    else:
        print ("wrong file type")
    return flow

def Print_stat(flow):
    print("#####################################caculate infly,redirect,normal pkt#########################################")
    result = dict()
    pkt_type_list = ['redirect','infly','src','dst']
    delay_type_list = ['delay','tfmdelay','nfdelay']
    for _type in pkt_type_list:
        result[_type] = dict()
        result[_type]['type'] = _type
        result[_type]['pktnum'] = 0
        for delay_type in delay_type_list:
            result[_type][delay_type] = dict()
            result[_type][delay_type]['maxdelay'] = 0
            result[_type][delay_type]['minidelay'] = 10000
            result[_type][delay_type]['sumdelay'] = 0
            result[_type][delay_type]['averagedelay'] = 0
    for pkt in flow:
        _type = pkt['type']
	if _type == "send":   #some pkts lost
             break
        result[_type]['pktnum'] += 1
        for delay_type in delay_type_list:
            result[_type][delay_type]['sumdelay'] += pkt[delay_type]
            if pkt[delay_type] > result[_type][delay_type]['maxdelay']:
                result[_type][delay_type]['maxdelay'] = pkt[delay_type] 
            if  pkt[delay_type] < result[_type][delay_type]['minidelay']:
                result[_type][delay_type]['minidelay'] = pkt[delay_type]
    for _type in pkt_type_list:
        for delay_type in delay_type_list:
            try:
                result[_type][delay_type]['averagedelay'] = result[_type][delay_type]['sumdelay']/result[_type]['pktnum']
            except:
                print ("pkt num of %s is 0"%result[_type]['type'])
    f=open("result.txt",'a')  
    resultw = ""
    for _type in pkt_type_list: 
        linep= "------------------------------------------------------------------------------------------------------------\n"
        lines= 'pkt_type:'+result[_type]['type']+"   "+'pkt_num:'+str(result[_type]['pktnum'])+"\n"
        resultw = resultw + linep + lines
        print linep
        print lines
        #print ("pkt_type:%s     pkt_num:%d"%(result[_type]['type'],result[_type]['pktnum']))
        for delay_type in delay_type_list:
            print (delay_type,result[_type][delay_type])
            resultw = resultw + delay_type +':'+str(result[_type][delay_type])+'\n'
        print linep
    f.write(resultw)
    f.close()
############lost############
    #lost
    """
    flowlose = flow
    flowlose.sort(key=lambda x:x['num'])
    lost = dict()
    lost['redirect'] = 0
    lost['infly'] = 0
    lost['normal'] = 0
    lostnum = list()
    prenum = flow[0]['num']-1
       for pkt in flowlose:
        if prenum+1 != pkt['num']:
            lost[pkt['type']] = lost[pkt['type']]+pkt['num']-prenum-1
            lostscope = (str)(prenum+1)+"-"+(str)(pkt['num']-1) 
            lostnum.append(lostscope)
        prenum = pkt['num']
        """
##################print result#####################
    print("#############################summary##############################")
    print("first packet number: %d , last pkt number: %d"%(flow[0]['num'],flow[-1]['num']))
    #if flowsrc and flowdst: 
    #	    print("pkt processed in src:%d-%d, pkt processed in dst:%d-%d"%(flowsrc[0]['num'],flowsrc[-1]['num'],flowdst[0]['num'],flowdst[-1]['num']))
    #print ("infly packets lost: %d"%(lost['infly']))
    #print ("redirect packets lost: %d"%(lost['redirect']))
    #print ("normal packets lost: %d"%(lost['normal']))
    #print("received packet: %d"%(flow[-1]['num']-flow[0]['num']-lost['infly']-lost['redirect']-lost['normal']+1))
    print("###################################################################")
    write_to_file(flow, sys.argv[1][1:]+"_time.txt")

def write_to_file(flow,filename):    
    f=open(filename,'w')     
    line = ("1:num---2:initime---3:outtimetfm---4:outtimenf---5:outtime\n")
    f.write(line)
    initime = flow[0]['initime']
    for pkt in flow:
        #print("num:%d,  init:%f,   outtimetfm:%f"%(pkt['num'],pkt['initime'],pkt['outtime']))
        line = str(pkt['num']) + " " + str(pkt['initime']-initime) + " " + str(pkt['outtimetfm']-initime) + " " + str(pkt['outtimenf']-initime)+" "+str(pkt['outtime']-initime)+'\n'
        f.write(line)
    f.close()

def Integrate_flow(flowsrc,flowdst,flowsend):
    sendnum = flowsend.__len__()    
    srcnum = flowsrc.__len__()    
    dstnum = flowdst.__len__()    
    print ("sendnum=%d,srcnum=%d,dstnum=%d,real=%d"%(sendnum,srcnum,dstnum,srcnum+dstnum))
    f=open("result.txt",'w') 
    resultw = "sendnum="+str(sendnum)+",srcnum="+str(srcnum)+",dstnum="+str(dstnum)+",real="+str(srcnum+dstnum)+'\n'
    f.write(resultw)
    f.close()
    if sendnum == 0 or srcnum == 0 or dstnum == 0:
        print ("send,src or dst pkts num is 0")
        return 0
    flowmerge = flowsrc+flowdst
    if flowsend.__len__() > flowmerge.__len__():
        flownum = flowmerge.__len__()
    else:
        flownum = flowsend.__len__()

    for i in range(0,flownum):
        flowsend[i]['type'] = flowmerge[i]['type'] 
        flowsend[i]['delay'] = flowmerge[i]['delay'] 
        if flowsend[i]['delay']  >900:
            flowsend[i]['delay'] = 1000-flowsend[i]['delay']
        flowsend[i]['outtime'] = flowsend[i]['initime'] + flowsend[i]['delay']/1000  
        flowsend[i]['outtimetfm'] = flowmerge[i]['outtimetfm'] 
        flowsend[i]['outtimenf'] = flowmerge[i]['outtimenf'] 
        flowsend[i]['tfmdelay'] = 1000*(flowsend[i]['outtimetfm'] - flowsend[i]['initime'])
        flowsend[i]['nfdelay'] = 1000*(flowsend[i]['outtimenf'] - flowsend[i]['initime'])

    return flowsend    

def parse_opennf(srcfile,dstfile,sendfile):
    global sendinit
    global dstnor
    flowsend = handle_file(sendfile,get_file_type(sendfile),'send')
    sendinit = flowsend[0]['initime']
    flowsrc = handle_file(srcfile,get_file_type(srcfile),'src')
    dstnor = flowsend[flowsrc.__len__()]['initime']
    flowdst = handle_file(dstfile,get_file_type(dstfile),'dst')
    print ("dst",flowdst[0])
    print ("src",flowsrc[0])
    print sendinit
    flow = Integrate_flow(flowsrc,flowdst,flowsend)
    return flow
    
def Debug_pkt(flow,debug_type):    
    if debug_type:
        for pkt in flow:
            if pkt['type'] == debug_type:
                print pkt

def flow_merge(tfmflow,nfflow):
    for i in range(0,tfmflow.__len__() ):
        tfmflow[i]['outtimenf'] =  nfflow[i]['outtimenf']
    return tfmflow
def parse_tfm(srcfile,dstfile,sendfile):
    if sendfile:
        flowsend = handle_file(sendfile,get_file_type(sendfile),"send")
        flowsrc = handle_file(srcfile,get_file_type(srcfile),"src")
        flowdst = handle_file(dstfile,get_file_type(dstfile),"dst")
        #fixme
        flowsrc = flow_merge(flowsrc,handle_file("src.txt","txt","src")) 
        flowdst = flow_merge(flowdst,handle_file("dst.txt","txt","dst")) 
        #
        flow = Integrate_flow(flowsrc,flowdst,flowsend)
    else:
        flowsrc = handle_file(srcfile,get_file_type(srcfile),"src")
        flowdst = handle_file(dstfile,get_file_type(dstfile),"dst")
        flow = flowsrc + flowdst
    return flow

if __name__ == '__main__':
    flow = list()
    if len(sys.argv) < 5:
        print("python parse.py [-tfm,-opennf] [debug_pkt_type] [srcfile] [dstfile] [sendfile]")
        print("-tfm:    parse result of tfm ")
        print("-opennf: parse result of opennf ")
    else:
        
        if len(sys.argv) == 5 and sys.argv[1] == "-tfm":
            print("parse type:tfm")
            print("no send file,get initime and outtime record in src and dst pcap file,record in udp payload")
            flow = parse_tfm(sys.argv[3],sys.argv[4],None)
        elif len(sys.argv) == 6 and sys.argv[1] == "-tfm":
            print("parse type:tfm")
            print("use send file,get initime from send and outtime record in src and dst file")
            flow = parse_tfm(sys.argv[3],sys.argv[4],sys.argv[5])
        elif sys.argv[1] == "-opennf":
            print("parse type:opennf")
            if len(sys.argv) < 6:
                exit(1)
                print("error: need send file")
            else:    
                flow = parse_opennf(sys.argv[3],sys.argv[4],sys.argv[5])
        else:
            print("python parse.py -h  app help")
            exit(1)
        
        if flow:
            Print_stat(flow)
            Debug_pkt(flow,(sys.argv[2]))
          


