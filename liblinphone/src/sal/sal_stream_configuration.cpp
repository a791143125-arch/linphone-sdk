/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of Liblinphone
 * (see https://gitlab.linphone.org/BC/public/liblinphone).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "sal/sal_stream_configuration.h"
#include "bellesip_sal/sal_impl.h"
#include "c-wrapper/internal/c-tools.h"
#include "linphone/utils/utils.h"
#include "utils/payload-type-handler.h"

LINPHONE_BEGIN_NAMESPACE

#define keywordcmp(key, b) strncmp(key, b, sizeof(key))

SalStreamConfiguration::SalStreamConfiguration() {
	payloads.clear();
	crypto.clear();
}

SalStreamConfiguration::~SalStreamConfiguration() {
	PayloadTypeHandler::clearPayloadList(payloads);
}

SalStreamConfiguration::SalStreamConfiguration(const SalStreamConfiguration &other) {
	proto = other.proto;
	proto_other = other.proto_other;
	rtp_ssrc = other.rtp_ssrc;
	rtcp_cname = other.rtcp_cname;
	for (const auto &pt : other.payloads) {
		payloads.push_back(payload_type_clone(pt));
	}
	ptime = other.ptime;
	maxptime = other.maxptime;
	dir = other.dir;
	crypto = other.crypto;
	max_rate = other.max_rate;
	bundle_only = other.bundle_only;
	implicit_rtcp_fb = other.implicit_rtcp_fb;
	rtcp_fb = other.rtcp_fb;
	rtcp_xr = other.rtcp_xr;
	mid = other.mid;
	mid_rtp_ext_header_id = other.mid_rtp_ext_header_id;
	mixer_to_client_extension_id = other.mixer_to_client_extension_id;
	client_to_mixer_extension_id = other.client_to_mixer_extension_id;
	frame_marking_extension_id = other.frame_marking_extension_id;
	conference_ssrc = other.conference_ssrc;
	set_nortpproxy = other.set_nortpproxy;
	rtcp_mux = other.rtcp_mux;
	haveZrtpHash = other.haveZrtpHash;
	haveLimeIk = other.haveLimeIk;
	memcpy(zrtphash, other.zrtphash, sizeof(zrtphash));
	dtls_fingerprint = other.dtls_fingerprint;
	dtls_role = other.dtls_role;
	ttl = other.ttl;
	index = other.index;
	tcapIndex = other.tcapIndex;
	acapIndexes = other.acapIndexes;
	delete_media_attributes = other.delete_media_attributes;
	delete_session_attributes = other.delete_session_attributes;
	sctp_local_port = other.sctp_local_port;
	sctp_remote_port = other.sctp_remote_port;
	dcmap = other.dcmap;
}

SalStreamConfiguration &SalStreamConfiguration::operator=(const SalStreamConfiguration &other) {
	index = other.index;
	proto = other.proto;
	proto_other = other.proto_other;
	rtp_ssrc = other.rtp_ssrc;
	rtcp_cname = other.rtcp_cname;
	replacePayloads(other.payloads);
	ptime = other.ptime;
	maxptime = other.maxptime;
	dir = other.dir;
	crypto = other.crypto;
	max_rate = other.max_rate;
	bundle_only = other.bundle_only;
	implicit_rtcp_fb = other.implicit_rtcp_fb;
	delete_media_attributes = other.delete_media_attributes;
	delete_session_attributes = other.delete_session_attributes;
	rtcp_fb = other.rtcp_fb;
	rtcp_xr = other.rtcp_xr;
	mid = other.mid;
	mid_rtp_ext_header_id = other.mid_rtp_ext_header_id;
	mixer_to_client_extension_id = other.mixer_to_client_extension_id;
	client_to_mixer_extension_id = other.client_to_mixer_extension_id;
	frame_marking_extension_id = other.frame_marking_extension_id;
	conference_ssrc = other.conference_ssrc;
	set_nortpproxy = other.set_nortpproxy;
	rtcp_mux = other.rtcp_mux;
	haveZrtpHash = other.haveZrtpHash;
	haveLimeIk = other.haveLimeIk;
	memcpy(zrtphash, other.zrtphash, sizeof(zrtphash));
	dtls_fingerprint = other.dtls_fingerprint;
	dtls_role = other.dtls_role;
	ttl = other.ttl;
	tcapIndex = other.tcapIndex;
	acapIndexes = other.acapIndexes;
	sctp_local_port = other.sctp_local_port;
	sctp_remote_port = other.sctp_remote_port;
	dcmap = other.dcmap;

	return *this;
}

SalStreamConfiguration::SalStreamConfiguration(SalStreamConfiguration&& other) noexcept
	: index(other.index),
	  proto(std::move(other.proto)),
	  proto_other(std::move(other.proto_other)),
	  rtp_ssrc(other.rtp_ssrc),
	  rtcp_cname(std::move(other.rtcp_cname)),
	  payloads(std::move(other.payloads)),
	  ptime(other.ptime),
	  maxptime(other.maxptime),
	  dir(other.dir),
	  crypto(std::move(other.crypto)),
	  max_rate(other.max_rate),
	  bundle_only(other.bundle_only),
	  implicit_rtcp_fb(other.implicit_rtcp_fb),
	  delete_media_attributes(other.delete_media_attributes),
	  delete_session_attributes(other.delete_session_attributes),
	  rtcp_fb(std::move(other.rtcp_fb)),
	  rtcp_xr(std::move(other.rtcp_xr)),
	  mid(std::move(other.mid)),
	  mid_rtp_ext_header_id(other.mid_rtp_ext_header_id),
	  mixer_to_client_extension_id(other.mixer_to_client_extension_id),
	  client_to_mixer_extension_id(other.client_to_mixer_extension_id),
	  frame_marking_extension_id(other.frame_marking_extension_id),
	  conference_ssrc(other.conference_ssrc),
	  set_nortpproxy(other.set_nortpproxy),
	  rtcp_mux(other.rtcp_mux),
	  haveZrtpHash(other.haveZrtpHash),
	  haveLimeIk(other.haveLimeIk),
	  dtls_fingerprint(std::move(other.dtls_fingerprint)),
	  dtls_role(other.dtls_role),
	  ttl(other.ttl),
	  dcmap(std::move(other.dcmap)),
	  sctp_local_port(other.sctp_local_port),
	  sctp_remote_port(other.sctp_remote_port),
	  tcapIndex(other.tcapIndex),
	  acapIndexes(std::move(other.acapIndexes))
{
	// Copy the zrtphash array
	memcpy(zrtphash, other.zrtphash, sizeof(zrtphash));
	memset(other.zrtphash, 0, sizeof(other.zrtphash));

	// Reset the source object's payloads to avoid double-free
	other.payloads.clear();
}

SalStreamConfiguration& SalStreamConfiguration::operator=(SalStreamConfiguration&& other) noexcept {
	if (this == &other) {
		return *this;
	}

	// Free existing resources in the current object
	PayloadTypeHandler::clearPayloadList(payloads);

	index = other.index;
	proto = other.proto;
	proto_other = std::move(other.proto_other);
	rtp_ssrc = other.rtp_ssrc;
	rtcp_cname = std::move(other.rtcp_cname);
	payloads = std::move(other.payloads);
	ptime = other.ptime;
	maxptime = other.maxptime;
	dir = other.dir;
	crypto = std::move(other.crypto);
	max_rate = other.max_rate;
	bundle_only = other.bundle_only;
	implicit_rtcp_fb = other.implicit_rtcp_fb;
	delete_media_attributes = other.delete_media_attributes;
	delete_session_attributes = other.delete_session_attributes;
	rtcp_fb = std::move(other.rtcp_fb);
	rtcp_xr = std::move(other.rtcp_xr);
	mid = std::move(other.mid);
	mid_rtp_ext_header_id = other.mid_rtp_ext_header_id;
	mixer_to_client_extension_id = other.mixer_to_client_extension_id;
	client_to_mixer_extension_id = other.client_to_mixer_extension_id;
	frame_marking_extension_id = other.frame_marking_extension_id;
	conference_ssrc = other.conference_ssrc;
	set_nortpproxy = other.set_nortpproxy;
	rtcp_mux = other.rtcp_mux;
	haveZrtpHash = other.haveZrtpHash;
	haveLimeIk = other.haveLimeIk;
	memcpy(zrtphash, other.zrtphash, sizeof(zrtphash));
	dtls_fingerprint = std::move(other.dtls_fingerprint);
	dtls_role = other.dtls_role;
	ttl = other.ttl;
	tcapIndex = other.tcapIndex;
	acapIndexes = std::move(other.acapIndexes);
	sctp_local_port = other.sctp_local_port;
	sctp_remote_port = other.sctp_remote_port;
	dcmap = std::move(other.dcmap);

	other.payloads.clear();
	memset(other.zrtphash, 0, sizeof(other.zrtphash));

	return *this;
}

bool SalStreamConfiguration::isRecvOnly(const OrtpPayloadType *p) {
	return (p->flags & PAYLOAD_TYPE_FLAG_CAN_RECV) && !(p->flags & PAYLOAD_TYPE_FLAG_CAN_SEND);
}

bool SalStreamConfiguration::isSamePayloadType(const PayloadType *p1, const PayloadType *p2) {
	if (p1->type != p2->type) return false;
	if (strcmp(p1->mime_type, p2->mime_type) != 0) return false;
	if (p1->clock_rate != p2->clock_rate) return false;
	if (p1->channels != p2->channels) return false;
	if (payload_type_get_number(p1) != payload_type_get_number(p2)) return false;
	/*
	 Do not compare fmtp right now: they are modified internally when the call is started
	*/
	/*
	if (!fmtp_equals(p1->recv_fmtp,p2->recv_fmtp) ||
	    !fmtp_equals(p1->send_fmtp,p2->send_fmtp))
	    return false;
	*/
	return true;
}

bool SalStreamConfiguration::isSamePayloadList(const std::list<PayloadType *> &l1, const std::list<PayloadType *> &l2) {
	auto p1 = l1.cbegin();
	auto p2 = l2.cbegin();

	for (; (p1 != l1.cend() && p2 != l2.cend()); ++p1, ++p2) {
		if (!isSamePayloadType(*p1, *p2)) return false;
	}

	if (p1 != l1.cend()) {
		/*skip possible recv-only payloads*/
		for (; p1 != l1.cend() && isRecvOnly(*p1); ++p1) {
			ms_message("Skipping recv-only payload type...");
		}
	}

	if (p1 != l1.cend() || p2 != l2.cend()) {
		/*means one list is longer than the other*/
		return false;
	}

	return true;
}

bool SalStreamConfiguration::operator==(const SalStreamConfiguration &other) const {
	return equal(other) == SAL_MEDIA_DESCRIPTION_UNCHANGED;
}

bool SalStreamConfiguration::operator!=(const SalStreamConfiguration &other) const {
	return !(*this == other);
}

int SalStreamConfiguration::equal(const SalStreamConfiguration &other) const {
	int result = SAL_MEDIA_DESCRIPTION_UNCHANGED;

	/* A different proto should result in SAL_MEDIA_DESCRIPTION_NETWORK_CHANGED but the encryption change
	   needs a stream restart for now, so use SAL_MEDIA_DESCRIPTION_CODEC_CHANGED */
	if (proto != other.proto) result |= SAL_MEDIA_DESCRIPTION_CODEC_CHANGED;
	for (auto crypto1 = crypto.cbegin(), crypto2 = other.crypto.cbegin();
	     (crypto1 != crypto.cend() && crypto2 != other.crypto.cend()); ++crypto1, ++crypto2) {
		if ((crypto1->tag != crypto2->tag) || (crypto1->algo != crypto2->algo)) {
			result |= SAL_MEDIA_DESCRIPTION_CRYPTO_POLICY_CHANGED;
		}
		if (crypto1->master_key.compare(crypto2->master_key)) {
			result |= SAL_MEDIA_DESCRIPTION_CRYPTO_KEYS_CHANGED;
		}
	}

	const auto thisCryptoSize = crypto.size();
	const auto otherCryptoSize = other.crypto.size();
	if (thisCryptoSize != otherCryptoSize) {
		result |= SAL_MEDIA_DESCRIPTION_CRYPTO_POLICY_CHANGED;
		result |= SAL_MEDIA_DESCRIPTION_CRYPTO_KEYS_CHANGED;
	}

	if (((thisCryptoSize > 0) && (otherCryptoSize == 0)) || ((thisCryptoSize == 0) && (otherCryptoSize > 0))) {
		result |= SAL_MEDIA_DESCRIPTION_CRYPTO_TYPE_CHANGED;
	}

	if (!isSamePayloadList(payloads, other.payloads)) {
		result |= SAL_MEDIA_DESCRIPTION_CODEC_CHANGED;
	}
	// Codec changed if either ptime is valid (i.e. greater than 0) and the other is not
	if (((ptime > 0) ^ (other.ptime > 0))) result |= SAL_MEDIA_DESCRIPTION_PTIME_CHANGED;
	// If both ptimes are valid, check that their valid is the same
	if ((ptime > 0) && (other.ptime > 0) && (ptime != other.ptime)) result |= SAL_MEDIA_DESCRIPTION_PTIME_CHANGED;
	if (dir != other.dir) result |= SAL_MEDIA_DESCRIPTION_DIRECTION_CHANGED;

	/*DTLS*/
	if (dtls_role != other.dtls_role) result |= SAL_MEDIA_DESCRIPTION_CRYPTO_KEYS_CHANGED;

	if (((dtls_role == SalDtlsRoleInvalid) && (other.dtls_role != SalDtlsRoleInvalid)) ||
	    ((dtls_role != SalDtlsRoleInvalid) && (other.dtls_role == SalDtlsRoleInvalid)))
		result |= SAL_MEDIA_DESCRIPTION_CRYPTO_TYPE_CHANGED;
	if (dtls_fingerprint.compare(other.dtls_fingerprint) != 0) result |= SAL_MEDIA_DESCRIPTION_CRYPTO_KEYS_CHANGED;

	/*ZRTP*/
	if (haveZrtpHash != other.haveZrtpHash) {
		result |= SAL_MEDIA_DESCRIPTION_CRYPTO_KEYS_CHANGED;
		result |= SAL_MEDIA_DESCRIPTION_CRYPTO_TYPE_CHANGED;
	}
	if (haveZrtpHash && other.haveZrtpHash && (strcmp((const char *)zrtphash, (const char *)other.zrtphash) != 0))
		result |= SAL_MEDIA_DESCRIPTION_CRYPTO_KEYS_CHANGED;

	/* Extensions */
	if (mixer_to_client_extension_id != other.mixer_to_client_extension_id)
		result |= SAL_MEDIA_DESCRIPTION_MIXER_TO_CLIENT_EXTENSION_CHANGED;
	if (client_to_mixer_extension_id != other.client_to_mixer_extension_id)
		result |= SAL_MEDIA_DESCRIPTION_CLIENT_TO_MIXER_EXTENSION_CHANGED;
	if (frame_marking_extension_id != other.frame_marking_extension_id)
		result |= SAL_MEDIA_DESCRIPTION_FRAME_MARKING_EXTENSION_CHANGED;

	/* datachannel */
	if (sctp_local_port != other.sctp_local_port || sctp_remote_port != other.sctp_remote_port)
		result |= SAL_MEDIA_DESCRIPTION_DATACHANNEL_CHANGED;
	if (dcmap != other.dcmap)
		result |= SAL_MEDIA_DESCRIPTION_DATACHANNEL_CHANGED;

	return result;
}

void SalStreamConfiguration::disable() {
	/* Remove potential bundle parameters. A disabled stream is moved out of the bundle. */
	mid.clear();
	bundle_only = false;
	dir = SalStreamInactive;
}

/*these are switch case, so that when a new proto is added we can't forget to modify this function*/
bool SalStreamConfiguration::hasAvpf() const {
	switch (proto) {
		case SalProtoRtpAvpf:
		case SalProtoRtpSavpf:
		case SalProtoUdpTlsRtpSavpf:
			return true;
		case SalProtoRtpAvp:
		case SalProtoRtpSavp:
		case SalProtoUdpTlsRtpSavp:
		case SalProtoUdpDtlsSctp:
		case SalProtoOther:
			return false;
	}
	return false;
}

bool SalStreamConfiguration::hasImplicitAvpf() const {
	return implicit_rtcp_fb;
}

/*these are switch case, so that when a new proto is added we can't forget to modify this function*/
bool SalStreamConfiguration::hasSrtp() const {
	switch (proto) {
		case SalProtoRtpSavp:
		case SalProtoRtpSavpf:
			return true;
		case SalProtoRtpAvp:
		case SalProtoRtpAvpf:
		case SalProtoUdpTlsRtpSavpf:
		case SalProtoUdpTlsRtpSavp:
		case SalProtoUdpDtlsSctp:
		case SalProtoOther:
			return false;
	}
	return false;
}

bool SalStreamConfiguration::hasDtls() const {
	switch (proto) {
		case SalProtoUdpTlsRtpSavpf:
		case SalProtoUdpTlsRtpSavp:
		case SalProtoUdpDtlsSctp:
			return true;
		case SalProtoRtpSavp:
		case SalProtoRtpSavpf:
		case SalProtoRtpAvp:
		case SalProtoRtpAvpf:
		case SalProtoOther:
			return false;
	}
	return false;
}

bool SalStreamConfiguration::hasDataChannel() const {
	switch (proto) {
		case SalProtoUdpDtlsSctp:
			return true;
		case SalProtoUdpTlsRtpSavpf:
		case SalProtoUdpTlsRtpSavp:
		case SalProtoRtpSavp:
		case SalProtoRtpSavpf:
		case SalProtoRtpAvp:
		case SalProtoRtpAvpf:
		case SalProtoOther:
			return false;
	}
	return false;
}

bool SalStreamConfiguration::hasZrtpHash() const {
	return (haveZrtpHash == 1);
}

const uint8_t *SalStreamConfiguration::getZrtpHash() const {
	return zrtphash;
}

bool SalStreamConfiguration::hasZrtp() const {
	if (haveZrtpHash == 1) {
		switch (proto) {
			case SalProtoRtpAvp:
			case SalProtoRtpAvpf:
				return true;
			case SalProtoUdpTlsRtpSavpf:
			case SalProtoUdpTlsRtpSavp:
			case SalProtoRtpSavp:
			case SalProtoRtpSavpf:
			case SalProtoUdpDtlsSctp:
			case SalProtoOther:
				return false;
		}
	}
	return false;
}

bool SalStreamConfiguration::hasLimeIk() const {
	if (haveLimeIk == 1) return true;
	return false;
}

const SalMediaProto &SalStreamConfiguration::getProto() const {
	return proto;
}

const std::string SalStreamConfiguration::getProtoAsString() const {
	if (proto == SalProtoOther) return proto_other;
	else return LinphonePrivate::Utils::toString(proto);
}

SalStreamDir SalStreamConfiguration::getDirection() const {
	return dir;
}

const std::list<OrtpPayloadType *> &SalStreamConfiguration::getPayloads() const {
	return payloads;
}

const int &SalStreamConfiguration::getMaxRate() const {
	return max_rate;
}

const std::string &SalStreamConfiguration::getMid() const {
	return mid;
}

const int &SalStreamConfiguration::getMidRtpExtHeaderId() const {
	return mid_rtp_ext_header_id;
}

void SalStreamConfiguration::enableAvpfForStream() {
	for (auto &pt : payloads) {
		payload_type_set_flag(pt, PAYLOAD_TYPE_RTCP_FEEDBACK_ENABLED);
	}
}
void SalStreamConfiguration::disableAvpfForStream() {
	for (auto &pt : payloads) {
		payload_type_unset_flag(pt, PAYLOAD_TYPE_RTCP_FEEDBACK_ENABLED);
	}
}

void SalStreamConfiguration::mergeAcaps(const std::list<std::list<unsigned int>> &acaps) {
	// Avoid adding duplicates
	for (const auto &newIdxs : acaps) {
		bool found = false;
		for (const auto &idxs : acapIndexes) {
			found |= ((idxs.size() == newIdxs.size()) && std::equal(idxs.begin(), idxs.end(), newIdxs.begin()));
		}
		if (!found) {
			acapIndexes.push_back(newIdxs);
		}
	}
}

const std::list<std::list<unsigned int>> &SalStreamConfiguration::getAcapIndexes() const {
	return acapIndexes;
}

const unsigned int &SalStreamConfiguration::getTcapIndex() const {
	return tcapIndex;
}

std::string SalStreamConfiguration::getSdpString() const {

	std::string acapString;
	// Iterate over all acaps sets. For every set, get the index of all its members
	for (const auto &acapSet : acapIndexes) {
		// Do not append | on first element
		if (!acapString.empty()) {
			acapString.append("|");
		}
		const auto &firstAcapIdx = acapSet.front();
		for (const auto &acapIdx : acapSet) {
			if (acapIdx != firstAcapIdx) {
				acapString.append(",");
			}
			if (acapIdx != 0) {
				acapString.append(std::to_string(acapIdx));
			}
		}
	}

	std::string tcapString;

	if (tcapIndex != 0) {
		tcapString = std::to_string(tcapIndex);
	}

	std::string deleteAttrs;
	if (delete_media_attributes && delete_session_attributes) {
		deleteAttrs = "-ms";
	} else if (delete_session_attributes) {
		deleteAttrs = "-s";
	} else if (delete_media_attributes) {
		deleteAttrs = "-m";
	}

	std::string sdpString;

	if (!deleteAttrs.empty() && !acapString.empty()) {
		sdpString += "a=" + deleteAttrs + ":" + acapString;
	} else if (!deleteAttrs.empty()) {
		sdpString += "a=" + deleteAttrs;
	} else if (!acapString.empty()) {
		sdpString += "a=" + acapString;
	}

	if (!tcapString.empty()) {
		if (!sdpString.empty()) {
			sdpString += " ";
		}
		sdpString += "t=" + tcapString;
	}
	return sdpString;
}

const int &SalStreamConfiguration::getMixerToClientExtensionId() const {
	return mixer_to_client_extension_id;
}

const int &SalStreamConfiguration::getClientToMixerExtensionId() const {
	return client_to_mixer_extension_id;
}

const int &SalStreamConfiguration::getFrameMarkingExtensionId() const {
	return frame_marking_extension_id;
}

std::string SalStreamConfiguration::cryptoToSdpValue(const SalSrtpCryptoAlgo &crypto) {
	std::string sdpValue;
	MSCryptoSuiteNameParams desc;
	if (ms_crypto_suite_to_name_params(crypto.algo, &desc) == 0) {
		sdpValue = std::to_string(crypto.tag) + " " + desc.name + " inline:" + crypto.master_key;
		if (desc.params) {
			sdpValue += " ";
			sdpValue += desc.params;
		}
	}

	return sdpValue;
}

void SalStreamConfiguration::replacePayloads(const std::list<OrtpPayloadType *> &newPayloads) {
	PayloadTypeHandler::clearPayloadList(payloads);
	for (const auto &pt : newPayloads) {
		payloads.push_back(payload_type_clone(pt));
	}
}

std::string SalStreamConfiguration::getSetupAttributeForDtlsRole(const SalDtlsRole &role) {
	std::string setupAttrValue;
	switch (role) {
		case SalDtlsRoleIsClient:
			setupAttrValue = "active";
			break;
		case SalDtlsRoleIsServer:
			setupAttrValue = "passive";
			break;
		case SalDtlsRoleInvalid:
			break;
		case SalDtlsRoleUnset:
		default:
			setupAttrValue = "actpass";
			break;
	}
	return setupAttrValue;
}

SalDtlsRole SalStreamConfiguration::getDtlsRoleFromSetupAttribute(const std::string setupAttr) {
	SalDtlsRole role = SalDtlsRoleInvalid;
	if (setupAttr.compare("actpass") == 0) {
		role = SalDtlsRoleUnset;
	} else if (setupAttr.compare("active") == 0) {
		role = SalDtlsRoleIsClient;
	} else if (setupAttr.compare("passive") == 0) {
		role = SalDtlsRoleIsServer;
	}
	return role;
}

SalSrtpCryptoAlgo SalStreamConfiguration::fillStrpCryptoAlgoFromString(const std::string &value) {
	unsigned int tag;
	char name[257] = {0}, masterKey[129] = {0}, parameters[257] = {0};
	const auto nb = sscanf(value.c_str(), "%u %256s inline:%128s %256[A-Z_ ]", &tag, name, masterKey, parameters);

	SalSrtpCryptoAlgo keyEnc;
	keyEnc.algo = MS_CRYPTO_SUITE_INVALID;
	if (nb >= 3) {
		MSCryptoSuiteNameParams np;
		np.name = name;
		np.params = parameters[0] != '\0' ? parameters : NULL;
		const auto cs = ms_crypto_suite_build_from_name_params(&np);

		keyEnc.algo = cs;
		if (cs == MS_CRYPTO_SUITE_INVALID) {
			ms_warning("Failed to parse crypto-algo: '%s'", name);
		} else {
			keyEnc.tag = tag;
			keyEnc.master_key = masterKey;
			// Erase all characters after | if it is found
			size_t sep = keyEnc.master_key.find("|");
			if (sep != std::string::npos)
				keyEnc.master_key.erase(keyEnc.master_key.begin() + static_cast<long>(sep), keyEnc.master_key.end());
		}
	} else {
		lError() << "Unable to extract crypto key informations from crypto argument value " << value;
	}
	return keyEnc;
}
uint16_t SalStreamConfiguration::getSctpLocalPort() const {
	return sctp_local_port;
}
uint16_t SalStreamConfiguration::getSctpRemotePort() const {
	return sctp_remote_port;
}
void SalStreamConfiguration::setSctpLocalPort(uint16_t port) {
	sctp_local_port = port;
}
void SalStreamConfiguration::setSctpRemotePort(uint16_t port) {
	sctp_remote_port = port;
}

const std::vector<SalDataChannelMap> &SalStreamConfiguration::getDataChannelMap() const {
	return dcmap;
}

bool SalDataChannelMap::operator==(const SalDataChannelMap &other) const {
	return 	dcsa == other.dcsa 
		&& label == other.label
		&& subprotocol == other.subprotocol
		&& max_retr == other.max_retr
		&& max_time == other.max_time
		&& stream_id == other.stream_id
		&& priority == other.priority
		&& ordered == other.ordered;
}
// produce a string to be set in sdp : 
std::string SalDataChannelMap::toSdpDcmapAttr() const {
	std::string ret = std::to_string(stream_id) + ' ';
	if (ordered) {
		ret += R"(ordered=")" + std::string((*ordered?"true":"false")) + R"(";)";
	}
	if (!subprotocol.empty()) {
		ret += R"(subprotocol=")" + subprotocol + R"(";)";
	}
	if (!label.empty()) {
		ret += R"(label=")" + label + R"(";)";
	}
	if (max_retr) {
		ret += R"(max-retr=")" + std::to_string(*max_retr) + R"(";)";
	}
	if (max_time) {
		ret += R"(max-time=")" + std::to_string(*max_time) + R"(";)";
	}
	if (priority) {
		ret += R"(priority=")" + std::to_string(*priority) + R"(";)";
	}
	// remove the last ; if any (or the ' ' after ther stream id if we have only that)
	ret.pop_back();
	return ret;
}
std::vector<std::string> SalDataChannelMap::toSdpDcsaAttrs() const {
	std::vector<std::string> ret{};
	for (const auto &dcsaStr : dcsa) {
		ret.push_back(std::to_string(stream_id)+' '+dcsaStr);
	}
	return ret;
}

// local helpers function
namespace {
	std::string decodeEscapedString(const std::string &s) {
		if (s.size() < 2 || s.front() != '"' || s.back() != '"') throw std::runtime_error("dcmap invalid string param "+s);
        
		// remove quotes
		std::string raw = s.substr(1, s.size() - 2);
		std::string result;
		result.reserve(raw.size());

		for (size_t i = 0; i < raw.size(); ++i) {
			// Parse escaped-char: % HEXDIG HEXDIG
			if (raw[i] == '%') {
				if (i + 2 >= raw.size()) throw std::runtime_error("dcmap attribute "+s+": Invalid escape sequence");
                    		result += static_cast<char>(std::stoi(raw.substr(i + 1, 2), nullptr, 16));
                    		i += 2;
            		} else {
                		result += raw[i];
           	 	}
        	}
        	return result;
    	}
} // anonymous namespace

// parse a parameter name=value or name="value"
void SalDataChannelMap::parseParam(const std::string &s) {
	// split into name/value
	auto pos = s.find('=');
	if (pos == std::string::npos) return;
	std::string name = s.substr(0, pos);
	std::string val = s.substr(pos + 1);

	if (name == "ordered") {
		ordered = (val == "true");
	} else if (name == "label") {
		label = decodeEscapedString(val);
        } else if (name == "subprotocol") {
		subprotocol = decodeEscapedString(val);
	} else if (name == "max-retr") {
		max_retr = std::stoul(val);
	} else if (name == "max-time") {
		max_time = std::stoul(val);
	} else if (name == "priority") {
		unsigned long p = std::stoul(val);
		if (p <= 65535) priority = static_cast<uint16_t>(p);
	} else {
		throw std::runtime_error("Invalid dcmap parameter "+s);
	}
}

// build from the attribute value
std::optional<SalDataChannelMap> SalDataChannelMap::from_string(const std::string &attrValue) {
	std::stringstream ss(attrValue);
	// stream-id (must be < 2^16) is mandatory
	uint32_t long_id;
	if (!(ss >> long_id) || long_id > 65535) {
		return std::nullopt;
	}

	SalDataChannelMap dcmap(static_cast<uint16_t>(long_id));

	// get options (remove possible whitespace)
	std::string options;
	std::getline(ss >> std::ws, options);
	std::stringstream oss(options);

	// Parse options
	std::string option;
        while (std::getline(oss>>std::ws, option, ';')) {
		try {
			dcmap.parseParam(option);
		} catch (const std::exception& e) {
     		   lError()<<"dcmap attribute parsing failed on "<<option<<" : "<<e.what();
		}
	}

	// Check mutual exclusivity
	if (dcmap.max_retr && dcmap.max_time) {
		return std::nullopt;
    	}
	return dcmap;
}
	
void SalDataChannelMap::setLabel(const std::string &s) {
	label = s;
}
void SalDataChannelMap::setSubprotocol(const std::string &s) {
	subprotocol = s;
}
void SalDataChannelMap::setMaxRetr(uint32_t value) {
	if (max_time) {
		lError()<<"Dcmap SDP attribute error: max_retr and max_time are mutually exclusive. Ignore setMaxRetr to "<<value;
	} else {
		max_retr = value;
	}
}
void SalDataChannelMap::setMaxTime(uint32_t value) {
	if (max_retr) {
		lError()<<"Dcmap SDP attribute error: max_retr and max_time are mutually exclusive. Ignore setMaxTime to "<<value;
	} else {
		max_time = value;
	}
}
void SalDataChannelMap::setPriority(uint16_t value) {
	priority = value;
}
void SalDataChannelMap::setOrdered(bool value) {
	ordered = value;
}
	
uint16_t SalDataChannelMap::getId() const {
	return stream_id;
}
const std::string &SalDataChannelMap::getLabel() const {
	return label;
}
const std::string &SalDataChannelMap::getSubprotocol() const {
	return subprotocol;
}
std::optional<uint32_t> SalDataChannelMap::getMaxRetr() const {
	return max_retr;
}
std::optional<uint32_t> SalDataChannelMap::getMaxTime() const {
	return max_time;
}
std::optional<uint16_t> SalDataChannelMap::getPriority() const {
	return priority;
}
std::optional<bool> SalDataChannelMap::getOrdered() const {
	return ordered;
}
// what kind of datachannel can we accept
// Todo: should that be settable from liblinphone API
bool SalDataChannelMap::isSupported() const {
	// We support only our custom subprotocol
	// Todo: add restrictions on reliability and ordered param?
	if (subprotocol == "bcdcp") return true;
	return false;
}

LINPHONE_END_NAMESPACE
