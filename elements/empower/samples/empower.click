# 12 "/tmp/empower.click"
rates :: AvailableRates(DEFAULT 12 18 24 36 48 72 96 108);
# 16 "/tmp/empower.click"
ControlSocket@3 :: ControlSocket("TCP", 7777);
# 17 "/tmp/empower.click"
ers :: EmpowerRXStats(EL el);
# 19 "/tmp/empower.click"
wifi_cl :: Classifier(0/08%0c,  // data
                      0/00%0c);
# 24 "/tmp/empower.click"
switch_mngt :: PaintSwitch;
# 25 "/tmp/empower.click"
switch_data :: PaintSwitch;
# 29 "/tmp/empower.click"
FromDevice@9 :: FromDevice(moni0, PROMISC false, OUTBOUND true, SNIFFER false);
# 30 "/tmp/empower.click"
RadiotapDecap@10 :: RadiotapDecap;
# 31 "/tmp/empower.click"
FilterPhyErr@11 :: FilterPhyErr;
# 33 "/tmp/empower.click"
WifiDupeFilter@12 :: WifiDupeFilter;
# 34 "/tmp/empower.click"
Paint@13 :: Paint(0);
# 37 "/tmp/empower.click"
sched_0 :: PrioSched;
# 39 "/tmp/empower.click"
SetChannel@15 :: SetChannel(CHANNEL 5180);
# 40 "/tmp/empower.click"
RadiotapEncap@16 :: RadiotapEncap;
# 41 "/tmp/empower.click"
ToDevice@17 :: ToDevice(moni0);
# 44 "/tmp/empower.click"
Queue@18 :: Queue(50);
# 48 "/tmp/empower.click"
Queue@19 :: Queue;
# 53 "/tmp/empower.click"
FromDevice@21 :: FromDevice(moni1, PROMISC false, OUTBOUND true, SNIFFER false);
# 54 "/tmp/empower.click"
RadiotapDecap@22 :: RadiotapDecap;
# 55 "/tmp/empower.click"
FilterPhyErr@23 :: FilterPhyErr;
# 57 "/tmp/empower.click"
WifiDupeFilter@24 :: WifiDupeFilter;
# 58 "/tmp/empower.click"
Paint@25 :: Paint(1);
# 61 "/tmp/empower.click"
sched_1 :: PrioSched;
# 63 "/tmp/empower.click"
SetChannel@27 :: SetChannel(CHANNEL 2437);
# 64 "/tmp/empower.click"
RadiotapEncap@28 :: RadiotapEncap;
# 65 "/tmp/empower.click"
ToDevice@29 :: ToDevice(moni1);
# 68 "/tmp/empower.click"
Queue@30 :: Queue(50);
# 72 "/tmp/empower.click"
Queue@31 :: Queue;
# 75 "/tmp/empower.click"
FromHost@32 :: FromHost(empower0);
# 76 "/tmp/empower.click"
EmpowerWifiEncap@33 :: EmpowerWifiEncap(EL el,
                      DEBUG false);
# 78 "/tmp/empower.click"
WifiSeq@34 :: WifiSeq;
# 81 "/tmp/empower.click"
ctrl :: Socket(TCP, 192.168.0.1, 4433, CLIENT true, VERBOSE true, RECONNECT_CALL el.reconnect);
# 82 "/tmp/empower.click"
downlink :: Counter;
# 83 "/tmp/empower.click"
el :: EmpowerLVAPManager(HWADDRS " 04:F0:21:09:F9:92 D4:CA:6D:14:C1:F8",
                                DPID 04:F0:21:09:F9:92,
                                EBS ebs,
                                EAUTHR eauthr,
                                EASSOR eassor,
				RE re,
                                RCS " rc_0/rate_control rc_1/rate_control",
                                PERIOD 5000,
                                DEBUGFS " /sys/kernel/debug/ieee80211/phy0/ath9k/bssid_extra /sys/kernel/debug/ieee80211/phy1/ath9k/bssid_extra",
                                ERS ers,
                                UPLINK uplink,
                                DOWNLINK downlink,
                                DEBUG false);
# 96 "/tmp/empower.click"
uplink :: Counter;
# 100 "/tmp/empower.click"
EmpowerWifiDecap@39 :: EmpowerWifiDecap(EL el,
                        DEBUG false);
# 102 "/tmp/empower.click"
ToHost@40 :: ToHost(empower0);
# 105 "/tmp/empower.click"
mgt_cl :: Classifier(0/40%f0,  // probe req
                            0/b0%f0,  // auth req
                            0/00%f0,  // assoc req
                            0/20%f0,  // reassoc req
                            0/c0%f0,  // deauth
                            0/a0%f0);
# 113 "/tmp/empower.click"
ebs :: EmpowerBeaconSource(RT rates,
                                  EL el,
                                  PERIOD 100, 
                                  DEBUG false);
# 117 "/tmp/empower.click"
WifiSeq@43 :: WifiSeq;
# 121 "/tmp/empower.click"
eauthr :: EmpowerOpenAuthResponder(EL el, DEBUG false);
# 125 "/tmp/empower.click"
eassor :: EmpowerAssociationResponder(RT rates,
                                             EL el,
                                             DEBUG false);
# 134 "/tmp/empower.click"
EmpowerDeAuthResponder@46 :: EmpowerDeAuthResponder(EL el, DEBUG false);
# 135 "/tmp/empower.click"
Discard@47 :: Discard;
# 138 "/tmp/empower.click"
EmpowerDisassocResponder@48 :: EmpowerDisassocResponder(EL el, DEBUG false);
# 139 "/tmp/empower.click"
Discard@49 :: Discard;
# 4 "/tmp/empower.click"
rc_0/filter_tx :: FilterTX;
# 6 "/tmp/empower.click"
rc_0/rate_control :: Minstrel(OFFSET 4, RT rates);
# 7 "/tmp/empower.click"
rc_0/Discard@3 :: Discard;
# 4 "/tmp/empower.click"
rc_1/filter_tx :: FilterTX;
# 6 "/tmp/empower.click"
rc_1/rate_control :: Minstrel(OFFSET 4, RT rates);
# 7 "/tmp/empower.click"
rc_1/Discard@3 :: Discard;
# 0 "<click-align>"
AlignmentInfo@click_align@54 :: AlignmentInfo(wifi_cl  4 2,
  mgt_cl  4 2);
# 136 ""
FromDevice@9 -> RadiotapDecap@10
    -> FilterPhyErr@11
    -> rc_0/filter_tx
    -> WifiDupeFilter@12
    -> Paint@13
    -> ers
    -> wifi_cl
    -> EmpowerWifiDecap@39
    -> ToHost@40;
FromDevice@21 -> RadiotapDecap@22
    -> FilterPhyErr@23
    -> rc_1/filter_tx
    -> WifiDupeFilter@24
    -> Paint@25
    -> ers;
switch_mngt [1] -> Queue@30
    -> sched_1
    -> rc_1/rate_control
    -> SetChannel@27
    -> RadiotapEncap@28
    -> ToDevice@29;
switch_data [1] -> Queue@31
    -> [1] sched_1;
FromHost@32 -> EmpowerWifiEncap@33
    -> WifiSeq@34
    -> switch_data
    -> Queue@19
    -> [1] sched_0;
wifi_cl [1] -> mgt_cl
    -> ebs
    -> WifiSeq@43
    -> switch_mngt
    -> Queue@18
    -> sched_0
    -> rc_0/rate_control
    -> SetChannel@15
    -> RadiotapEncap@16
    -> ToDevice@17;
mgt_cl [1] -> eauthr
    -> switch_mngt;
mgt_cl [2] -> eassor
    -> switch_mngt;
mgt_cl [3] -> eassor;
mgt_cl [4] -> EmpowerDeAuthResponder@46
    -> Discard@47;
mgt_cl [5] -> EmpowerDisassocResponder@48
    -> Discard@49;
rc_0/filter_tx [1] -> [1] rc_0/rate_control;
rc_0/rate_control [1] -> rc_0/Discard@3;
rc_1/filter_tx [1] -> [1] rc_1/rate_control;
rc_1/rate_control [1] -> rc_1/Discard@3;
ctrl -> downlink
    -> el
    -> uplink
    -> ctrl;
