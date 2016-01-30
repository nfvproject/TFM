ControlSocket(TCP,4433)
src :: RatedSource(\<00>, LENGTH 22, RATE 1, LIMIT 20, STOP 1)
-> UDPIPEncap(10.10.10.10, 2233, 1.1.1.1, 1122)
-> EtherEncap(0x0800, e8:61:1f:10:6d:53, a0:42:3f:26:68:21)
-> ToDevice(eth1);
