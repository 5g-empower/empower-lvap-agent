# 14 "/tmp/empower.click"
ControlSocket@1 :: ControlSocket("TCP", 7777);
# 16 "/tmp/empower.click"
ers :: EmpowerRXStats(EL el);
# 18 "/tmp/empower.click"
wifi_cl :: Classifier(0/08%0c,  // data
                      0/00%0c);
# 23 "/tmp/empower.click"
switch_mngt :: PaintSwitch;
# 24 "/tmp/empower.click"
switch_data :: PaintSwitch;
# 26 "/tmp/empower.click"
rates_default_0 :: TransmissionPolicy(MCS "12 18 24 36 48 72 96 108");
# 27 "/tmp/empower.click"
rates_0 :: TransmissionPolicies(DEFAULT rates_default_0);
# 31 "/tmp/empower.click"
FromDevice@9 :: FromDevice(moni0, PROMISC false, OUTBOUND true, SNIFFER false);
# 32 "/tmp/empower.click"
RadiotapDecap@10 :: RadiotapDecap;
# 33 "/tmp/empower.click"
FilterPhyErr@11 :: FilterPhyErr;
# 35 "/tmp/empower.click"
WifiDupeFilter@12 :: WifiDupeFilter;
# 36 "/tmp/empower.click"
Paint@13 :: Paint(0);
# 39 "/tmp/empower.click"
sched_0 :: PrioSched;
# 40 "/tmp/empower.click"
WifiSeq@15 :: WifiSeq;
# 42 "/tmp/empower.click"
RadiotapEncap@16 :: RadiotapEncap;
# 43 "/tmp/empower.click"
ToDevice@17 :: ToDevice(moni0);
# 46 "/tmp/empower.click"
Queue@18 :: Queue(50);
# 50 "/tmp/empower.click"
Queue@19 :: Queue;
# 53 "/tmp/empower.click"
rates_default_1 :: TransmissionPolicy(MCS "2 4 11 22 12 18 24 36 48 72 96 108");
# 54 "/tmp/empower.click"
rates_1 :: TransmissionPolicies(DEFAULT rates_default_1);
# 58 "/tmp/empower.click"
FromDevice@23 :: FromDevice(moni1, PROMISC false, OUTBOUND true, SNIFFER false);
# 59 "/tmp/empower.click"
RadiotapDecap@24 :: RadiotapDecap;
# 60 "/tmp/empower.click"
FilterPhyErr@25 :: FilterPhyErr;
# 62 "/tmp/empower.click"
WifiDupeFilter@26 :: WifiDupeFilter;
# 63 "/tmp/empower.click"
Paint@27 :: Paint(1);
# 66 "/tmp/empower.click"
sched_1 :: PrioSched;
# 67 "/tmp/empower.click"
WifiSeq@29 :: WifiSeq;
# 69 "/tmp/empower.click"
RadiotapEncap@30 :: RadiotapEncap;
# 70 "/tmp/empower.click"
ToDevice@31 :: ToDevice(moni1);
# 73 "/tmp/empower.click"
Queue@32 :: Queue(50);
# 77 "/tmp/empower.click"
Queue@33 :: Queue;
# 80 "/tmp/empower.click"
FromHost@34 :: FromHost(empower0);
# 81 "/tmp/empower.click"
wifi_encap :: EmpowerWifiEncap(EL el, DEBUG false);
# 84 "/tmp/empower.click"
ctrl :: Socket(TCP, 192.168.1.8, 4433, CLIENT true, VERBOSE true, RECONNECT_CALL el.reconnect);
# 85 "/tmp/empower.click"
downlink :: Counter;
# 88 "/tmp/empower.click"
el :: EmpowerLVAPManager(EMPOWER_IFACE empower0,
                                WTP 00:0D:B9:2F:56:64,
                                EBS ebs,
                                EAUTHR eauthr,
                                EASSOR eassor,
                                EDEAUTHR edeauthr,
				E11K e11k,
                                RES " 04:F0:21:09:F9:98/36/20 D4:CA:6D:14:C2:10/6/20",
                                RCS " rc_0/rate_control rc_1/rate_control",
                                PERIOD 5000,
                                DEBUGFS " /sys/kernel/debug/ieee80211/phy0/ath9k/bssid_extra /sys/kernel/debug/ieee80211/phy1/ath9k/bssid_extra",
                                ERS ers,
                                UPLINK uplink,
                                DOWNLINK downlink,
                                DEBUG false);
# 103 "/tmp/empower.click"
uplink :: Counter;
# 107 "/tmp/empower.click"
wifi_decap :: EmpowerWifiDecap(EL el, DEBUG false);
# 108 "/tmp/empower.click"
ToHost@41 :: ToHost(empower0);
# 113 "/tmp/empower.click"
mgt_cl :: Classifier(0/40%f0,  // probe req
                            0/b0%f0,  // auth req
                            0/00%f0,  // assoc req
                            0/20%f0,  // reassoc req
                            0/c0%f0,  // deauth
                            0/a0%f0,  // disassoc
                            0/d0%f0);
# 122 "/tmp/empower.click"
ebs :: EmpowerBeaconSource(EL el, PERIOD 100, DEBUG false);
# 126 "/tmp/empower.click"
eauthr :: EmpowerOpenAuthResponder(EL el, DEBUG false);
# 130 "/tmp/empower.click"
eassor :: EmpowerAssociationResponder(EL el, DEBUG false);
# 137 "/tmp/empower.click"
edeauthr :: EmpowerDeAuthResponder(EL el, DEBUG false);
# 141 "/tmp/empower.click"
EmpowerDisassocResponder@47 :: EmpowerDisassocResponder(EL el, DEBUG false);
# 142 "/tmp/empower.click"
Discard@48 :: Discard;
# 145 "/tmp/empower.click"
e11k :: Empower11k(EL el, DEBUG false);
# 4 "/tmp/empower.click"
rc_0/filter_tx :: FilterTX;
# 8 "/tmp/empower.click"
rc_0/rate_control :: Minstrel(OFFSET 4, TP rates_0);
# 9 "/tmp/empower.click"
rc_0/Discard@3 :: Discard;
# 4 "/tmp/empower.click"
rc_1/filter_tx :: FilterTX;
# 8 "/tmp/empower.click"
rc_1/rate_control :: Minstrel(OFFSET 4, TP rates_1);
# 9 "/tmp/empower.click"
rc_1/Discard@3 :: Discard;
# 0 "<click-align>"
AlignmentInfo@click_align@54 :: AlignmentInfo(wifi_cl  4 2,
  mgt_cl  4 2);
# 132 ""
FromDevice@9 -> RadiotapDecap@10
    -> FilterPhyErr@11
    -> rc_0/filter_tx
    -> WifiDupeFilter@12
    -> Paint@13
    -> ers
    -> wifi_cl
    -> wifi_decap
    -> ToHost@41;
FromDevice@23 -> RadiotapDecap@24
    -> FilterPhyErr@25
    -> rc_1/filter_tx
    -> WifiDupeFilter@26
    -> Paint@27
    -> ers;
switch_mngt [1] -> Queue@32
    -> sched_1
    -> WifiSeq@29
    -> rc_1/rate_control
    -> RadiotapEncap@30
    -> ToDevice@31;
switch_data [1] -> Queue@33
    -> [1] sched_1;
FromHost@34 -> wifi_encap
    -> switch_data
    -> Queue@19
    -> [1] sched_0;
wifi_decap [1] -> wifi_encap;
wifi_cl [1] -> mgt_cl
    -> ebs
    -> switch_mngt
    -> Queue@18
    -> sched_0
    -> WifiSeq@15
    -> rc_0/rate_control
    -> RadiotapEncap@16
    -> ToDevice@17;
mgt_cl [1] -> eauthr
    -> switch_mngt;
mgt_cl [2] -> eassor
    -> switch_mngt;
mgt_cl [3] -> eassor;
mgt_cl [4] -> edeauthr
    -> switch_mngt;
mgt_cl [5] -> EmpowerDisassocResponder@47
    -> Discard@48;
mgt_cl [6] -> e11k
    -> switch_mngt;
rc_0/filter_tx [1] -> [1] rc_0/rate_control;
rc_0/rate_control [1] -> rc_0/Discard@3;
rc_1/filter_tx [1] -> [1] rc_1/rate_control;
rc_1/rate_control [1] -> rc_1/Discard@3;
ctrl -> downlink
    -> el
    -> uplink
    -> ctrl;
