#ifndef CLICK_EMPOWER_CQMLINK_HH
#define CLICK_EMPOWER_CQMLINK_HH
#include <click/straccum.hh>
#include <click/etheraddress.hh>
#include <click/hashcode.hh>
#include <click/timer.hh>
#include <click/vector.hh>
#include "frame.hh"
CLICK_DECLS

class CqmLink {
public:

    CqmLink();
    ~CqmLink();

    void estimator(unsigned, bool);

    void add_sample(Frame *);
    void add_cbt_sample(Frame *);

    String unparse();

    Timestamp lastEstimateTime;
    Timestamp currentTime;

    EtherAddress sourceAddr;

    double rssiCdf;
    uint64_t numFramesCount;
    double pdr;

    double rssiCdf_0;
    uint64_t numFramesCount_0;
    double pdr_0;

    uint64_t rssiCdfCount_l;
    uint64_t numFramesCount_l;
    double pdr_l;

    uint16_t silentWindowCount;
    uint16_t lastSeqNum;
    uint16_t currentSeqNum;

    double channel_busy_time;
    double channel_busy_fraction;
    double data_bits_recv;
    double throughput;
    double available_BW;

    double p_pdr;
    double p_channel_busy_fraction; // risk of channel busy fraction exceeding set threshold.
    double p_throughput;
    double p_available_BW;

    double xi;

    //sics
    double rssiThreshold;
    double rssi_tolerance;
    double cbt_threshold;
    double cbt_tolerance;
    double throughput_threshold;
    double throughput_tolerance;
    double pdr_threshold;
    double pdr_tolerance;

    uint16_t window_count;

};

CLICK_ENDDECLS
#endif /* CLICK_EMPOWER_CQMLINK_HH */
