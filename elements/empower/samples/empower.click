elementclass RateControl {
  $rates|

  filter_tx :: FilterTX()

  input -> filter_tx -> output;

  rate_control :: Minstrel(OFFSET 4, TP $rates);
  filter_tx [1] -> [1] rate_control [1] -> Discard();
  input [1] -> rate_control -> [1] output;

};

ControlSocket("TCP", 7777);

ers :: EmpowerRXStats(EL el);

wifi_cl :: Classifier(0/08%0c,  // data
                      0/00%0c); // mgt

ers -> wifi_cl;

tee :: EmpowerTee(2, EL el);

switch_mngt :: PaintSwitch();

rates_default_0 :: TransmissionPolicy(MCS "12 18 24 36 48 72 96 108", HT_MCS "0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15");
rates_0 :: TransmissionPolicies(DEFAULT rates_default_0);

rc_0 :: RateControl(rates_0);
eqm_0 :: EmpowerQOSManager(EL el, RC rc_0/rate_control, IFACE_ID 0, DEBUG false);

FromDevice(moni0, PROMISC false, OUTBOUND true, SNIFFER false, BURST 1000)
  -> RadiotapDecap()
  -> FilterPhyErr()
  -> rc_0
  -> WifiDupeFilter()
  -> Paint(0)
  -> ers;

sched_0 :: PrioSched()
  -> WifiSeq()
  -> [1] rc_0 [1]
  -> RadiotapEncap()
  -> ToDevice (moni0);

switch_mngt[0]
  -> Queue(50)
  -> [0] sched_0;

tee[0]
  -> MarkIPHeader(14)
  -> Paint(0)
  -> eqm_0
  -> [1] sched_0;

rates_default_1 :: TransmissionPolicy(MCS "2 4 11 22 12 18 24 36 48 72 96 108", HT_MCS "0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15");
rates_1 :: TransmissionPolicies(DEFAULT rates_default_1);

rc_1 :: RateControl(rates_1);
eqm_1 :: EmpowerQOSManager(EL el, RC rc_1/rate_control, IFACE_ID 1, DEBUG false);

FromDevice(moni1, PROMISC false, OUTBOUND true, SNIFFER false, BURST 1000)
  -> RadiotapDecap()
  -> FilterPhyErr()
  -> rc_1
  -> WifiDupeFilter()
  -> Paint(1)
  -> ers;

sched_1 :: PrioSched()
  -> WifiSeq()
  -> [1] rc_1 [1]
  -> RadiotapEncap()
  -> ToDevice (moni1);

switch_mngt[1]
  -> Queue(50)
  -> [0] sched_1;

tee[1]
  -> MarkIPHeader(14)
  -> Paint(1)
  -> eqm_1
  -> [1] sched_1;

kt :: KernelTap(10.0.0.1/24, BURST 500, DEV_NAME empower0)
  -> tee;

ctrl :: Socket(TCP, 192.168.100.158, 4433, CLIENT true, VERBOSE true, RECONNECT_CALL el.reconnect)
    -> el :: EmpowerLVAPManager(WTP 00:0D:B9:30:3E:18,
                                EBS ebs,
                                EAUTHR eauthr,
                                EASSOR eassor,
                                EDEAUTHR edeauthr,
                                E11K e11k,
                                RES " 04:F0:21:09:F9:9E/36/HT20 D4:CA:6D:14:C2:09/6/HT20",
                                RCS " rc_0/rate_control rc_1/rate_control",
                                PERIOD 5000,
                                DEBUGFS " /sys/kernel/debug/ieee80211/phy0/netdev:moni0/../ath9k/bssid_extra /sys/kernel/debug/ieee80211/phy1/netdev:moni1/../ath9k/bssid_extra",
                                ERS ers,
                                EQMS " eqm_0 eqm_1",
                                DEBUG false)
    -> ctrl;

  wifi_cl [0]
    -> wifi_decap :: EmpowerWifiDecap(EL el, DEBUG false)
    -> kt;

  wifi_decap [1] -> tee;

  wifi_cl [1]
    -> mgt_cl :: Classifier(0/40%f0,  // probe req
                            0/b0%f0,  // auth req
                            0/00%f0,  // assoc req
                            0/20%f0,  // reassoc req
                            0/c0%f0,  // deauth
                            0/a0%f0,  // disassoc
                            0/d0%f0); // action

  mgt_cl [0]
    -> ebs :: EmpowerBeaconSource(EL el, DEBUG false)
    -> switch_mngt;

  mgt_cl [1]
    -> eauthr :: EmpowerOpenAuthResponder(EL el, DEBUG false)
    -> switch_mngt;

  mgt_cl [2]
    -> eassor :: EmpowerAssociationResponder(EL el, DEBUG false)
    -> switch_mngt;

  mgt_cl [3]
    -> eassor;

  mgt_cl [4]
    -> edeauthr :: EmpowerDeAuthResponder(EL el, DEBUG false)
    -> switch_mngt;

  mgt_cl [5]
    -> EmpowerDisassocResponder(EL el, DEBUG false)
    -> Discard();

  mgt_cl [6]
    -> e11k :: Empower11k(EL el, DEBUG false)
    -> switch_mngt;

