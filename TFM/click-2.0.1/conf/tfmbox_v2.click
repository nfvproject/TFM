ControlSocket(TCP,4433)
tagclassifier::Classifier(15/00%01 20/00%80, 15/01%01 20/00%80, 15/00%01 20/80%80, 15/01%01 20/80%80);
normalclassifier::IPClassifier(false,-);
inflyclassifier::IPClassifier(false,-);
redirectclassifier::IPClassifier(false,false,-);
lastinflyclassifier::IPClassifier(false,-);
NFclassifier::IPClassifier(false,false,-);
TFM::Tfm(1);
//ToNF1::ToDevice(br1);
//ToNF2::ToDevice(br1);
NF1mem::Queue();
NF2mem::Queue();
strip::Strip(14);

FromDevice(eth1,PROMISC 1,BURST 10)->CheckIPHeader(OFFSET 14, CHECKSUM 0)->tagclassifier;
tagclassifier[0]->normalclassifier[1]-> [0]TFM;
tagclassifier[2]->inflyclassifier[1]-> [1]TFM;
tagclassifier[1]->redirectclassifier[2]-> [2]TFM;
tagclassifier[3]->lastinflyclassifier[1]-> [3]TFM;
inflyclassifier[0]->[0]TFM;
redirectclassifier[1]->[0]TFM;
lastinflyclassifier[0]->[0]TFM;
TFM[0]->NFclassifier[0]->NF1mem->Discard;
NFclassifier[1]->NF2mem->Discard;
NFclassifier[2]->Discard;
redirectclassifier[0]->strip;

//src router
srctag::Srcmark(1);
//srcfilter::Classifddier(1/00%01,1/01%01);
qout::Queue(10000);
eth::EtherEncap(0x0800, 00:0c:29:c8:41:f3, a0:42:3f:26:68:20)
//normalclassifier[1]->strip->srcfilter;
//srcfilter[0]->srctag->qout;
//srcfilter[1]->srctag->qout;
normalclassifier[0]->strip->srctag->qout;
qout->eth->ToDevice(eth1);
~
