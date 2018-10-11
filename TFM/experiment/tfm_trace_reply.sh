#!/bin/bash
tcpreplay -p 50000 -i eth2 --dbug=5 /root/project/experiment/send.pcap >/root/project/experiment/send.txt 2>&1
