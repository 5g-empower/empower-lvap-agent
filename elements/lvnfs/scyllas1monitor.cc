#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <click/args.hh>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>
#include "scyllas1monitor.hh"
#include "liblte_s1ap.h"
#include "liblte_mme.h"
CLICK_DECLS

/* Array of UE S1AP monitored elements. */
Vector <struct S1APMonitorElement> S1APMonElelist;

ScyllaS1Monitor::ScyllaS1Monitor() :
		_debug(false), _offset(12) {
}

ScyllaS1Monitor::~ScyllaS1Monitor() {
}

int ScyllaS1Monitor::configure(Vector<String> &conf, ErrorHandler *errh) {
	return Args(conf, this, errh).read("DEBUG", _debug)
								 .read("OFFSET", _offset)
								 .complete();
}

Packet *
ScyllaS1Monitor::simple_action(Packet *p) {

	struct click_ip *ip = (struct click_ip *) (p->data() + _offset);

	if (ip->ip_p != IP_PROTO_SCTP) {
		return p;
	}

	struct click_sctp *sctp = (struct click_sctp *) (p->data() + _offset + ip->ip_hl * 4);

	uint8_t *ptr =  (uint8_t *) sctp;
	uint8_t *end = ptr + (p->length() - _offset - ip->ip_hl * 4);

	ptr += sizeof(struct click_sctp);

	while (ptr < end) {
		struct click_sctp_chunk *chunk = (struct click_sctp_chunk *) ptr;
		if (chunk->type() == 0) {
			struct click_sctp_data_chunk *data = (struct click_sctp_data_chunk *) ptr;
			if (data->ppi() == 18) {
				parse_s1ap(data);
			}
		}
		if (chunk->length() % 4 == 0) {
			ptr += chunk->length();
		} else {
			ptr += (chunk->length() + 4 - chunk->length() % 4);
		}
	}

	return p;

}

void ScyllaS1Monitor::parse_s1ap(click_sctp_data_chunk *data) {

	uint8_t *payload = (uint8_t *) data;
	payload += sizeof(struct click_sctp_data_chunk);

	LIBLTE_S1AP_S1AP_PDU_STRUCT s1ap_pdu;
	LIBLTE_BYTE_MSG_STRUCT msg;

	msg.reset();
	msg.N_bytes = data->length();
	msg.msg = payload;

	if (LIBLTE_SUCCESS != liblte_s1ap_unpack_s1ap_pdu(&msg, &s1ap_pdu))
		return;

	/* initiatingMessage */
	if (s1ap_pdu.choice_type == LIBLTE_S1AP_S1AP_PDU_CHOICE_INITIATINGMESSAGE) {
		if (s1ap_pdu.choice.initiatingMessage.choice_type == LIBLTE_S1AP_INITIATINGMESSAGE_CHOICE_INITIALCONTEXTSETUPREQUEST) {

			LIBLTE_S1AP_MESSAGE_INITIALCONTEXTSETUPREQUEST_STRUCT *InitialContextSetupRequest = &s1ap_pdu.choice.initiatingMessage.choice.InitialContextSetupRequest;

			uint32_t MME_UE_S1AP_ID = InitialContextSetupRequest->MME_UE_S1AP_ID.MME_UE_S1AP_ID;
			uint32_t ENB_UE_S1AP_ID = InitialContextSetupRequest->eNB_UE_S1AP_ID.ENB_UE_S1AP_ID;

			LIBLTE_S1AP_E_RABTOBESETUPLISTCTXTSUREQ_STRUCT *E_RABToBeSetupListCtxtSUReq = &InitialContextSetupRequest->E_RABToBeSetupListCtxtSUReq;

			for (uint8_t i = 0; i < E_RABToBeSetupListCtxtSUReq->len; i++) {
				/* eRAB Id. */
				uint8_t e_RAB_ID = E_RABToBeSetupListCtxtSUReq->buffer[i].e_RAB_ID.E_RAB_ID;
				/* EPC IP address. */
				char EPC_IP[13];
				/* Tunnel End Point Id used for GTP traffic from UE to EPC. */
				char UE2EPC_teid[9];
				/* UE IP address. */
				char UE_IP[13];

				LIBLTE_S1AP_TRANSPORTLAYERADDRESS_STRUCT *transportLayerAddress = &E_RABToBeSetupListCtxtSUReq->buffer[i].transportLayerAddress;
				LIBLTE_S1AP_GTP_TEID_STRUCT *gTP_TEID = &E_RABToBeSetupListCtxtSUReq->buffer[i].gTP_TEID;

				/* IPv4 Address */
				if (transportLayerAddress->n_bits == 32) {

					uint8_t bytes[4];
					liblte_pack(transportLayerAddress->buffer, transportLayerAddress->n_bits, bytes);

					sprintf(EPC_IP, "%u.%u.%u.%u", bytes[0], bytes[1], bytes[2], bytes[3]);
				}

				sprintf(UE2EPC_teid, "%02x%02x%02x%02x", gTP_TEID->buffer[0], gTP_TEID->buffer[1], gTP_TEID->buffer[2], gTP_TEID->buffer[3]);

				if (E_RABToBeSetupListCtxtSUReq->buffer[i].nAS_PDU_present) {
					LIBLTE_S1AP_NAS_PDU_STRUCT *nAS_PDU = &E_RABToBeSetupListCtxtSUReq->buffer[i].nAS_PDU;

					LIBLTE_MME_ATTACH_ACCEPT_MSG_STRUCT attach_accept;
					LIBLTE_BYTE_MSG_STRUCT nas_msg;

					nas_msg.reset();
					nas_msg.N_bytes = nAS_PDU->n_octets;
					nas_msg.msg = nAS_PDU->buffer;

					if (LIBLTE_SUCCESS != liblte_mme_unpack_attach_accept_msg(&nas_msg, &attach_accept))
						return;

					LIBLTE_BYTE_MSG_STRUCT *esm_msg = &attach_accept.esm_msg;
					LIBLTE_MME_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_REQUEST_MSG_STRUCT act_def_eps_bearer_context_req;

					if (LIBLTE_SUCCESS != liblte_mme_unpack_activate_default_eps_bearer_context_request_msg(esm_msg, &act_def_eps_bearer_context_req))
						return;

					LIBLTE_MME_PDN_ADDRESS_STRUCT *pdn_addr = &act_def_eps_bearer_context_req.pdn_addr;

					if (pdn_addr->pdn_type == LIBLTE_MME_PDN_TYPE_IPV4) {

						sprintf(UE_IP, "%u.%u.%u.%u", pdn_addr->addr[0], pdn_addr->addr[1], pdn_addr->addr[2], pdn_addr->addr[3]);

						struct S1APMonitorElement ele = {
							.eNB_UE_S1AP_ID = ENB_UE_S1AP_ID,
							.MME_UE_S1AP_ID = MME_UE_S1AP_ID,
							.e_RAB_ID = e_RAB_ID,
							.EPC_IP = EPC_IP,
							.eNB_IP = "",
							.UE_IP = UE_IP,
							.UE2EPC_teid = UE2EPC_teid,
							.EPC2UE_teid = ""
						};

						S1APMonElelist.push_back(ele);
					}
				}
			}
		}
	}
	/* successfulOutcome */
	else if (s1ap_pdu.choice_type == LIBLTE_S1AP_S1AP_PDU_CHOICE_SUCCESSFULOUTCOME) {
		if (s1ap_pdu.choice.successfulOutcome.choice_type == LIBLTE_S1AP_SUCCESSFULOUTCOME_CHOICE_INITIALCONTEXTSETUPRESPONSE) {

			LIBLTE_S1AP_MESSAGE_INITIALCONTEXTSETUPRESPONSE_STRUCT *InitialContextSetupResponse = &s1ap_pdu.choice.successfulOutcome.choice.InitialContextSetupResponse;

			uint32_t MME_UE_S1AP_ID = InitialContextSetupResponse->MME_UE_S1AP_ID.MME_UE_S1AP_ID;
			uint32_t ENB_UE_S1AP_ID = InitialContextSetupResponse->eNB_UE_S1AP_ID.ENB_UE_S1AP_ID;

			LIBLTE_S1AP_E_RABSETUPLISTCTXTSURES_STRUCT *E_RABSetupListCtxtSURes = &InitialContextSetupResponse->E_RABSetupListCtxtSURes;

			for (uint8_t i = 0; i < E_RABSetupListCtxtSURes->len; i++) {

				/* eRAB Id. */
				uint8_t e_RAB_ID = E_RABSetupListCtxtSURes->buffer[i].e_RAB_ID.E_RAB_ID;
				/* eNB IP address. */
				char eNB_IP[13];
				/* Tunnel End Point Id used for GTP traffic from EPC to UE. */
				char EPC2UE_teid[9];

				LIBLTE_S1AP_TRANSPORTLAYERADDRESS_STRUCT *transportLayerAddress = &E_RABSetupListCtxtSURes->buffer[i].transportLayerAddress;
				LIBLTE_S1AP_GTP_TEID_STRUCT *gTP_TEID = &E_RABSetupListCtxtSURes->buffer[i].gTP_TEID;

				/* IPv4 Address */
				if (transportLayerAddress->n_bits == 32) {

					uint8_t bytes[4];
					liblte_pack(transportLayerAddress->buffer, transportLayerAddress->n_bits, bytes);

					sprintf(eNB_IP, "%u.%u.%u.%u", bytes[0], bytes[1], bytes[2], bytes[3]);
				}

				sprintf(EPC2UE_teid, "%02x%02x%02x%02x", gTP_TEID->buffer[0], gTP_TEID->buffer[1], gTP_TEID->buffer[2], gTP_TEID->buffer[3]);

				for(uint16_t i = 0; i != S1APMonElelist.size(); i++) {

					if (S1APMonElelist[i].eNB_UE_S1AP_ID == ENB_UE_S1AP_ID &&
						S1APMonElelist[i].MME_UE_S1AP_ID == MME_UE_S1AP_ID &&
						S1APMonElelist[i].e_RAB_ID == e_RAB_ID) {

						S1APMonElelist[i].eNB_IP = eNB_IP;
						S1APMonElelist[i].EPC2UE_teid = EPC2UE_teid;

						click_chatter("<--------------- Entry ---------------->");

						click_chatter("eNB_UE_S1AP_ID %u", S1APMonElelist[i].eNB_UE_S1AP_ID);
						click_chatter("MME_UE_S1AP_ID %u", S1APMonElelist[i].MME_UE_S1AP_ID);
						click_chatter("e_RAB_ID %u", S1APMonElelist[i].e_RAB_ID);
						click_chatter("EPC_IP %s", S1APMonElelist[i].EPC_IP.c_str());
						click_chatter("eNB_IP %s", S1APMonElelist[i].eNB_IP.c_str());
						click_chatter("UE_IP %s", S1APMonElelist[i].UE_IP.c_str());
						click_chatter("UE2EPC_teid %s", S1APMonElelist[i].UE2EPC_teid.c_str());
						click_chatter("EPC2UE_teid %s", S1APMonElelist[i].EPC2UE_teid.c_str());

						click_chatter("<-------------------------------------->");

						break;
					}
				}
			}
		}
	}

}

enum {
	H_DEBUG,
};

String ScyllaS1Monitor::read_handler(Element *e, void *thunk) {
	ScyllaS1Monitor *td = (ScyllaS1Monitor *) e;
	switch ((uintptr_t) thunk) {
	case H_DEBUG:
		return String(td->_debug) + "\n";
	default:
		return String();
	}
}

int ScyllaS1Monitor::write_handler(const String &in_s, Element *e,
		void *vparam, ErrorHandler *errh) {
	ScyllaS1Monitor *f = (ScyllaS1Monitor *) e;
	String s = cp_uncomment(in_s);
	switch ((intptr_t) vparam) {
	case H_DEBUG: {    //debug
		bool debug;
		if (!BoolArg().parse(s, debug))
			return errh->error("debug parameter must be boolean");
		f->_debug = debug;
		break;
	}
	}
	return 0;

}

void ScyllaS1Monitor::add_handlers() {
	add_read_handler("debug", read_handler, (void *) H_DEBUG);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ScyllaS1Monitor)
