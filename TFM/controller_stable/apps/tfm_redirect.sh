#!/bin/bash
SRC_IP="10.21.2.187"
SRC_MAC="e8:61:1f:10:6c:d1"
DST_IP="10.21.2.160"
DST_MAC="e8:61:1f:10:6d:53"

SWITCH_DPID="00:00:a0:42:3f:26:68:21"
SWITCH_IP="10.21.2.186"
SWITCH_MAC="e8:61:1f:10:6a:cb"
SWITCH_TRA_MAC="e8:61:1f:10:6a:cb"
SWITCH_SRC_MAC="e8:61:1f:10:6a:ca"
SWITCH_DST_MAC="a0:42:3f:26:68:21"
CONTROLLER_IP="10.21.2.185:8080"


CLICK_PORT="4433"
#FLOW_FILTER="10.24.0.0/16"
FLOW_FILTER="10.10.10.10/32"
SRC_DEF_FILTER="false"
DST_DEF_FILTER="false"
TRACEFILE="./tracefile.pcap"
#replay trace in rate x pps
REPLAY_RATE="1000"
rm -f src_in
mknod src_in p
exec 8<> src_in
rm -f dst_in
mknod dst_in p
exec 9<> dst_in

telnet $DST_IP $CLICK_PORT <&9 & 
telnet $SRC_IP $CLICK_PORT <&8 & 
echo "write tfm.mfstart 1" >> dst_in &&
echo "write srcredirect.pattern0  $FLOW_FILTER" >> src_in
