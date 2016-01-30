ControlSocket(TCP,4433)
tosclassify::Classifier(15/00%01 20/00%80, 15/01%01 20/00%80, 15/00%01 20/80%80, 15/01%01 20/80%80);
srcredirect::IPClassifier(false,false,-);
FromDevice(eth1,PROMISC 1,BURST 10)->CheckIPHeader(OFFSET 14, CHECKSUM 0)->srcredirect[1]->tosclassify;
tfm::Tfm(1);
tosclassify[0]-> [0]tfm;
tosclassify[1]-> [1]tfm;
tosclassify[2]-> [2]tfm;
tosclassify[3]-> [3]tfm;
tfm[0]->Queue(2000000)->ToDevice(br1)->StoreUDPTimeSeqRecord(OFFSET 14, DELTA true)->ToDump(dstout.pcap)->Discard();
//tfm[0]->Queue(10000)->ToDevice(br1,BURST 100,METHOD LINUX)->StoreUDPTimeSeqRecord(OFFSET 14, DELTA true)->ToDump(dstout.pcap)->Discard();
//tfm[0]->ToDevice(br1,METHOD LINUX)->StoreUDPTimeSeqRecord(OFFSET 14, DELTA true)->ToDump(dstout.pcap)->Discard();
srcredirect[2]->Discard;

//src router
srcmark::Srcmark(1);
srcfilter::Classifier(1/00%01,1/01%01);
qout::Queue(10000);
eth::EtherEncap(0x0800, 00:0c:29:c8:41:f3, a0:42:3f:26:68:20)

srcredirect[0]->Strip(14)->srcfilter;
srcfilter[0]->srcmark->qout;
srcfilter[1]->srcmark->qout;
qout->eth->ToDevice(eth1);
