//FromDevice(eth1)->Strip(14)->CheckIPHeader(CHECKSUM 0)->Discard;
FromDevice(eth2)->Print->Discard;
