hw::EmpowerHypervisor(DEBUG true)
  -> RatedUnqueue(RATE 5)
  -> Print(OUT)
  -> Discard();

s1::RatedSource(LENGTH 1000, RATE 10, LIMIT 100)
  -> EtherEncap(0x06BB, 00:15:6D:84:13:5D, FF:FF:FF:FF:FF:FF)
  -> WifiEncap(0x02, aa:bb:cc:dd:ee:ff)
  -> Print(IN)
  -> hw;

s2::RatedSource(LENGTH 1000, RATE 10, LIMIT 100)
  -> EtherEncap(0x06BB, 11:22:33:44:55:66, FF:FF:FF:FF:FF:FF)
  -> WifiEncap(0x02, ad:ad:cd:ee:11:33)
  -> Print(IN)
  -> hw;
