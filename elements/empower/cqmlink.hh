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

    uint8_t senderType;
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

    double xi;

	int ifaceId;

	double rssiThreshold;

    CqmLink() {

    	senderType = 0;
		sourceAddr = EtherAddress();

	    rssiCdf = 0;
	    numFramesCount = 0;
	    pdr = 0;

	    rssiCdf_0 = 0;
	    numFramesCount_0 = 0;
	    pdr_0 = 0;

	    rssiCdfCount_l = 0;
	    numFramesCount_l = 0;
	    pdr_l = 0;

	    silentWindowCount = 0;
	    lastSeqNum = 0;
	    currentSeqNum = 0;

	    xi = 0;

	    ifaceId = -1;

		rssiThreshold = 0;

    }

    void estimator() {

		// Bayesian analysis: Update previous window posterior as prior for this window
		rssiCdf_0 = rssiCdf;
		numFramesCount_0 = numFramesCount;
		pdr_0 = pdr;

		// Bayesian analysis: Compute Posterior
		if (numFramesCount_l > 0) {

			// at least one frame received in window
			rssiCdf = (double) (rssiCdf_0
					* (double) numFramesCount_0
					+ (double) rssiCdfCount_l)
					/ (double) (numFramesCount_0 + numFramesCount_l);

			if ((currentSeqNum - lastSeqNum) < 0) {
				pdr_l = (double) (numFramesCount_l)
						/ (double) (4095 - lastSeqNum + currentSeqNum + 1);
			} else {
				pdr_l = (double) (numFramesCount_l)
						/ (double) (currentSeqNum - lastSeqNum);
			}

			pdr = (pdr_0 * (double) numFramesCount_0
					+ pdr_l * (double) numFramesCount_l)
					/ (double) (numFramesCount_0 + numFramesCount_l);

			numFramesCount = numFramesCount_l;
			lastSeqNum = currentSeqNum;

		} else {

			// no frames received in window. (Silent window)
			// Current estimates remain the same as the prior or the previous window's estimates
			rssiCdf = rssiCdf_0;
			numFramesCount = numFramesCount_0;
			pdr = pdr_0;
			silentWindowCount++;

		}

		numFramesCount_l = 0;
		rssiCdfCount_l = 0;
		xi = rssiCdf * pdr;
		lastEstimateTime = currentTime;

	}

	void add_sample(Frame *frame) {
		numFramesCount_l++;
		rssiCdfCount_l = rssiCdfCount_l + (frame->_rssi > rssiThreshold ? 1 : 0);
		currentSeqNum = frame->_seq;
		currentTime.assign_now();
	}

	String unparse() {
		StringAccum sa;
		sa << sourceAddr.unparse();
		sa << (senderType == 0 ? " STA" : " AP");
		sa << " last_estimated " << lastEstimateTime;
		sa << " silent_window " << silentWindowCount;
		sa << " iface_id " << ifaceId << "\n";
		return sa.take_string();
	}

};

CLICK_ENDDECLS
#endif /* CLICK_EMPOWER_CQMLINK_HH */
