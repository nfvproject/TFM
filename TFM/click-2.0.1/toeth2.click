ControlSocket(TCP,4433)
q::Queue(10000);
src1 :: RatedSource(\<00>, LENGTH 22, RATE 1000, LIMIT 1000, STOP 1)
-> UDPIPEncap(10.10.10.10, 2233, 1.1.1.1, 1122)
-> EtherEncap(0x0800,44:37:e6:97:c3:1d,e8:61:1f:10:6d:53)->q;
src2 :: RatedSource(\<00>, LENGTH 22, RATE 1000, LIMIT 1000, STOP 1)
-> UDPIPEncap(10.10.10.1, 2233, 1.1.1.1, 1122)
-> EtherEncap(0x0800,44:37:e6:97:c3:1d,e8:61:1f:10:6d:53)->q;
q-> ToDevice(eth2);
