elementclass RateControl {
  $rates,$ht_rates|

  filter_tx :: FilterTX()

  input -> filter_tx -> output;

  rate_control :: Minstrel(OFFSET 4, RT $rates, RT_HT $ht_rates);
  filter_tx [1] -> [1] rate_control [1] -> Discard();
  input [1] -> rate_control -> [1] output;

};

ControlSocket("TCP", 7777);

ers :: EmpowerRXStats(EL el)

wifi_cl :: Classifier(0/08%0c,  // data
                      0/00%0c); // mgt

ers -> wifi_cl;

switch_mngt :: PaintSwitch();
switch_data :: PaintSwitch();

rates_0 :: AvailableRates(DEFAULT 2 4 11 22 12 18 24 36 48 72 96 108);
rates_ht_0 :: AvailableRates();
rc_0 :: RateControl(rates_0, rates_ht_0);

FromDevice(moni0, PROMISC false, OUTBOUND true, SNIFFER false)
  -> RadiotapDecap()
  -> FilterPhyErr()
  -> rc_0
  -> WifiDupeFilter()
  -> Paint(0)
  -> ers;

sched_0 :: PrioSched()
  -> WifiSeq()
  -> [1] rc_0 [1]
  -> SetChannel(CHANNEL 5180)
  -> RadiotapEncap()
  -> ToDevice (moni0);

switch_mngt[0]
  -> Queue(50)
  -> [0] sched_0;

switch_data[0]
  -> Queue()
  -> [1] sched_0;

rates_1 :: AvailableRates(DEFAULT 2 4 11 22 12 18 24 36 48 72 96 108);
rates_ht_1 :: AvailableRates();
rc_1 :: RateControl(rates_1, rates_ht_1);

FromDevice(moni1, PROMISC false, OUTBOUND true, SNIFFER false)
  -> RadiotapDecap()
  -> FilterPhyErr()
  -> rc_1
  -> WifiDupeFilter()
  -> Paint(1)
  -> ers;

sched_1 :: PrioSched()
  -> WifiSeq()
  -> [1] rc_1 [1]
  -> SetChannel(CHANNEL 2437)
  -> RadiotapEncap()
  -> ToDevice (moni1);

switch_mngt[1]
  -> Queue(50)
  -> [0] sched_1;

switch_data[1]
  -> Queue()
  -> [1] sched_1;

FromHost(empower0)
  -> wifi_encap :: EmpowerWifiEncap(EL el, DEBUG false)
  -> switch_data;

ctrl :: Socket(TCP, 127.0.0.1, 4433, CLIENT true, VERBOSE true, RECONNECT_CALL el.reconnect)
    -> downlink :: Counter()
    -> el :: EmpowerLVAPManager(EMPOWER_IFACE empower0,
                                HWADDRS " 04:F0:21:09:F9:92 D4:CA:6D:14:C1:F8",
                                WTP 00:0D:B9:2F:57:8C,
                                EBS ebs,
                                EAUTHR eauthr,
                                EASSOR eassor,
				RES " 36/20 6/20",
                                RCS " rc_0/rate_control rc_1/rate_control",
                                PERIOD 5000,
                                DEBUGFS " /sys/kernel/debug/ieee80211/phy0/ath9k/bssid_extra /sys/kernel/debug/ieee80211/phy1/ath9k/bssid_extra",
                                ERS ers,
                                UPLINK uplink,
                                DOWNLINK downlink,
                                DEBUG false)
    -> uplink :: Counter()
    -> ctrl;

  wifi_cl [0]
    -> wifi_decap :: EmpowerWifiDecap(EL el, DEBUG false)
    -> ToHost(empower0);

  wifi_decap [1] -> wifi_encap;

  wifi_cl [1]
    -> mgt_cl :: Classifier(0/40%f0,  // probe req
                            0/b0%f0,  // auth req
                            0/00%f0,  // assoc req
                            0/20%f0,  // reassoc req
                            0/c0%f0,  // deauth
                            0/a0%f0); // disassoc

  mgt_cl [0]
    -> ebs :: EmpowerBeaconSource(
                                  EL el,
                                  PERIOD 100, 
                                  DEBUG false)
    -> switch_mngt;

  mgt_cl [1]
    -> eauthr :: EmpowerOpenAuthResponder(EL el, DEBUG false)
    -> switch_mngt;

  mgt_cl [2]
    -> eassor :: EmpowerAssociationResponder(
                                             EL el,
                                             DEBUG false)
    -> switch_mngt;

  mgt_cl [3]
    -> eassor;

  mgt_cl [4]
    -> EmpowerDeAuthResponder(EL el, DEBUG false)
    -> Discard();

  mgt_cl [5]
    ->  EmpowerDisassocResponder(EL el, DEBUG false)
    ->Discard();

