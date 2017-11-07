#ifndef CLICK_EMPOWER_CQMLINK_HH
#define CLICK_EMPOWER_CQMLINK_HH
#include <click/straccum.hh>
#include <click/etheraddress.hh>
#include <click/hashcode.hh>
#include <click/timer.hh>
#include <click/vector.hh>
#include "frame.hh"
CLICK_DECLS

class EmpowerCQM;

class CqmLink {
public:

	CqmLink();
	~CqmLink();

	void estimator(unsigned, bool);

	void add_sample(uint32_t, uint8_t, uint16_t);
	void add_cbt_sample(uint32_t);

	String unparse();

	EmpowerCQM *cqm;

	int iface_id;
	unsigned samples;

	Timestamp lastEstimateTime;
	Timestamp currentTime;

	EtherAddress sourceAddr;

	double rssiCdf;
	uint64_t numFramesCount;
	double pdr;

	double rssiCdf_0;
	uint64_t numFramesCount_0;
	double pdr_0;

	uint64_t rssiCdf_l;
	uint64_t numFramesCount_l;
	double pdr_l;

	uint16_t silentWindowCount;
	uint16_t lastSeqNum;
	uint16_t currentSeqNum;

    double channel_busy_time;
    double channel_busy_fraction;
    double data_bits_recv;
    double throughput;
    double available_bw;
    double attainable_throughput;

    double p_pdr;
    double p_channel_busy_fraction;
    double p_throughput;
    double p_available_bw;
    double p_attainable_throughput;

    double p_pdr_last;
    double p_channel_busy_fraction_last;
    double p_throughput_last;
    double p_available_bw_last;
    double p_attainable_throughput_last;

	double xi;

	double rssi_threshold;
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
