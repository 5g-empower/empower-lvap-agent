InfiniteSource(LENGTH 1472, LIMIT 1000000, STOP false) 
  -> UDPIPEncap(1.2.3.4, 6000, 5.6.7.8, 8000) 
  -> EtherEncap(0x06BB, 00:15:6D:84:13:5D, 00:15:6D:84:13:55) 
  -> WifiEncap(0x00, 0:0:0:0:0:0)
  -> WifiSeq()
  -> tee :: Tee(4);

q::ScyllaWifiDupeFilter()
 -> Discard();

tee[0] -> q;
tee[1] -> q;
tee[2] -> q;
tee[3] -> q;

ControlSocket(TCP, 7777);

