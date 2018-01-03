q :: Queue(1024);

FromDevice(eth0)
	-> Print(ok)
	-> MarkIPHeader(14)
	-> c0 :: IPClassifier(ip proto 17, ip proto 132)

to_eth0 :: ToDevice(eth0);

c0[0]
	-> Print(GTP)
	-> MarkIPHeader(14)
	-> d0 :: IPClassifier(dst host 192.168.200.10, dst host 192.168.200.20);

c0[1]
	-> Print(S1Monitor)
	-> ScyllaS1Monitor(OFFSET 14)
	-> EtherRewrite(f0:1f:af:60:a6:56, 00:00:24:d1:de:ff)
	-> q;

d0[0]
	-> Print(DL)
	-> Strip(42)
	-> StripGTPHeader
	-> Print(DL_IP)
	-> GTPEncap
	-> Print(DL_GTP_ENCAP_OK)
	-> UDPIPEncap(192.168.200.20, 2152, 192.168.200.10, 2152)
	-> CheckIPHeader
	-> EtherEncap(0x0800,f0:1f:af:60:a6:56, 00:00:24:d1:de:ff)
	-> Print(DL_Encap_ok)
	-> q;

d0[1]
	-> Print(UL)
	-> Strip(42)
	-> StripGTPHeader
	-> Print(UL_IP)
	-> GTPEncap
	-> Print(UL_GTP_ENCAP_OK)
	-> UDPIPEncap(192.168.200.10, 2152, 192.168.200.20, 2152)
	-> CheckIPHeader	
	-> EtherEncap(0x0800,f0:1f:af:60:a6:56, 00:00:24:d1:de:ff)
	-> Print(UL_Encap_ok)
	-> q;

q -> to_eth0;
