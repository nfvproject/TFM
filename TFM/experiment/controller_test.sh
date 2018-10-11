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
#clean flowtable
curl -X DELETE -d '{"name":"flow_1"}' http://$CONTROLLER_IP/wm/staticflowpusher/json
curl -X DELETE -d '{"name":"flow_2"}' http://$CONTROLLER_IP/wm/staticflowpusher/json
curl -X DELETE -d '{"name":"flow_3"}' http://$CONTROLLER_IP/wm/staticflowpusher/json
curl -X DELETE -d '{"name":"flow_4"}' http://$CONTROLLER_IP/wm/staticflowpusher/json
curl -X DELETE -d '{"name":"flow_5"}' http://$CONTROLLER_IP/wm/staticflowpusher/json
#rm -f switch_in
#mknod switch_in p
#exec 7<> switch_in
rm -f src_in
mknod src_in p
exec 8<> src_in
rm -f dst_in
mknod dst_in p
exec 9<> dst_in
echo "(Controller):connect to Switch $SWITCH_IP:$CLICK_PORT"
#telnet $SWITCH_IP $CLICK_PORT <&7 &

echo "(Controller):connect to srcInst $SRC_IP:$CLICK_PORT"
telnet $SRC_IP $CLICK_PORT <&8 &

echo "(Controller):connect to dstInst $DST_IP:$CLICK_PORT"
telnet $DST_IP $CLICK_PORT <&9 &
sleep 2

echo "write srcredirect.pattern1  $FLOW_FILTER" >> dst_in
echo "write srcredirect.pattern1  $FLOW_FILTER" >> src_in

echo "(Controller):Configure Switch"
#echo "(Switch):Configure port1-srchost"
#echo "write ethsrc.src $SWITCH_MAC" >> switch_in
#echo "write ethsrc.dst $SRC_MAC" >> switch_in
#echo "(Switch):Configure port2-dsthost"
#echo "write ethdst.src $SWITCH_MAC" >> switch_in
#echo "write ethdst.dst $DST_MAC" >> switch_in
set -x
echo "(Switch):updatetable-forward pkt to srchost"
#echo "write srclinkswitch.switch 1" >> switch_in
curl -d '{"switch": "'$SWITCH_DPID'","name":"flow_1","cookie":"0", "priority":"200","in_port":"3","eth_type":"0x0800","ipv4_src":"'$FLOW_FILTER'","active":"true","instruction_apply_actions":"output=2"}' http://$CONTROLLER_IP/wm/staticflowpusher/json

echo "(Switch):update flowtable,forward pkts from src to dst" &&
date +%s.%N &&
curl -d '{"switch": "'$SWITCH_DPID'","name":"flow_2","cookie":"0", "priority":"200","in_port":"2","eth_type":"0x0800","ipv4_src":"'$FLOW_FILTER'","active":"true","instruction_apply_actions":"output=1"}' http://$CONTROLLER_IP/wm/staticflowpusher/json &&
sleep 1

echo "(flow)start"
#nohup ssh root@10.21.2.160 click /root/project/tfm/tfm_trace_tcp.click >/dev/null 2>&1 &
nohup ssh root@10.21.2.160 click /root/project/experiment/tfm_trace.click >/dev/null 2>&1 &
#nohup ssh root@10.21.2.160 bash /root/project/experiment/tfm_trace_reply.sh >/dev/null 2>&1 &
#nohup click ./tfm_trace.click >/dev/null 2>&1 &
#nohup tcpreplay -i eth0 -p $REPLAY_RATE $TRACEFILE  >/dev/null 2>&1 &
sleep 1

echo "--------------------------------------------------------------------------------"
echo "(Controller):start to migrate flow"
echo "(DST):Command dst TFM-Router walk into state-mfstart "
date +%s.%N &&
echo "write tfm.mfstart 1" >> dst_in &&
echo "write tfm.stateinstall 1" >> dst_in
#echo "(Controller):state have been installed, notify dst" &&


echo "(SRC):Command src TFM-Router to redirect flow-$FLOW_FILTER to dst" &&
date +%s.%N &&
#echo "write eth.src $SRC_MAC" >> src_in &&
#echo "write eth.dst $DST_MAC" >> src_in &&
echo "write srcredirect.pattern2  $FLOW_FILTER" >> src_in &&
echo "write srcredirect.pattern1  false" >> src_in &&
#echo "write srcredirect.pattern1  $SRC_DEF_FILTER" >> src_in
date +%s.%N &&
echo "(Controller):start to move state from $SRC_IP to $DST_IP"
#sleep 0.1
#sleep 1

date +%s.%N
echo "(Switch):update flowtable,redirect pkt to src and dst with tag redirect"
curl -d '{"switch": "'$SWITCH_DPID'","name":"flow_3","cookie":"0", "priority":"210","in_port":"3","eth_type":"0x0800","ipv4_src":"'$FLOW_FILTER'","active":"true","instruction_apply_actions":"set_field=ip_ecn->1,output=1,output=2"}' http://$CONTROLLER_IP/wm/staticflowpusher/json
#curl -d '{"switch": "'$SWITCH_DPID'","name":"flow_3","cookie":"0", "priority":"210","in_port":"3","eth_type":"0x0800","ipv4_src":"'$FLOW_FILTER'","active":"true","instruction_apply_actions":"set_field=ip_ecn->1,set_field=eth_dst->'$DST_MAC',output=2"}' http://$CONTROLLER_IP/wm/staticflowpusher/json
#echo "write tagredirect.tagopen 1" >> switch_in    
#echo "write dstlinkswitch.switch 1" >> switch_in

#sleep 1
echo "(Dst):received the redirect pkt update flowtable,just redirect pkts to dst"
echo "(Switch):update flowtable,just redirect pkts to dst"
date +%s.%N
curl -d '{"switch": "'$SWITCH_DPID'","name":"flow_4","cookie":"0", "priority":"220","in_port":"3","eth_type":"0x0800","ipv4_src":"'$FLOW_FILTER'","active":"true","instruction_apply_actions":"set_field=ip_ecn->1,set_field=eth_dst->'$DST_MAC',output=1"}' http://$CONTROLLER_IP/wm/staticflowpusher/json
date +%s.%N
#echo "write srclinkswitch.switch 0" >> switch_in

#sleep 0.1
#echo "write tagredirect.tagopen 0" >> switch_in    

sleep 2
echo "(Controller):Read dst state"
echo "read tfm.state" >> dst_in

sleep 2
echo "(Controller):migration of flow finish"
echo "(Controller):start to reset ... ...."
echo "(Dst):reset TFM-Router to state-normal"
echo "write tfm.reset 1" >> dst_in
echo "(SRC):Close filter and stop redirecting"
#echo "write srcredirect.pattern0 $SRC_DEF_FILTER" >> src_in
#echo "write srcredirect.pattern1  $DST_DEF_FILTER" >> dst_in
echo "(Switch):update flowtable,cancel tag"
curl -d '{"switch": "'$SWITCH_DPID'","name":"flow_5","cookie":"0", "priority":"230","in_port":"3","eth_type":"0x0800","ipv4_src":"'$FLOW_FILTER'","active":"true","instruction_apply_actions":"set_field=eth_dst->'$DST_MAC',output=1"}' http://$CONTROLLER_IP/wm/staticflowpusher/json
#echo "write tagredirect.tagopen 0" >> switch_in    

