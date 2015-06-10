// -*- related-file-name: "../../lib/etheraddress64.cc" -*-
#ifndef CLICK_ETHERADDRESS64_HH
#define CLICK_ETHERADDRESS64_HH
#include <click/string.hh>
#include <click/glue.hh>
#include <click/type_traits.hh>
#if !CLICK_TOOL
# include <click/nameinfo.hh>
# include <click/standard/addressinfo.hh>
#endif
CLICK_DECLS

class EtherAddress64 {
public:

	typedef uninitialized_type uninitialized_t;

	/** @brief Construct an EtherAddress64 equal to 00-00-00-00-00-00-00-00. */
	inline EtherAddress64() {
		_data[0] = _data[1] = _data[2] = _data[3] = 0;
	}

	/** @brief Construct an EtherAddress from data.
	 * @param data the address data, in network byte order
	 *
	 * The bytes data[0]...data[7] are used to construct the address. */
	explicit inline EtherAddress64(const unsigned char *data) {
		memcpy(_data, data, 7);
	}

	/** @brief Construct an uninitialized EtherAddress64. */
	inline EtherAddress64(const uninitialized_type &unused) {
		(void) unused;
	}

	/** @brief Return the broadcast EtherAddress64, FF-FF-FF-FF-FF-FF-FF-FF. */
	static EtherAddress64 make_broadcast() {
		return EtherAddress64(0xFFFF);
	}

	typedef bool (EtherAddress64::*unspecified_bool_type)() const;

	/** @brief Return true iff the address is not 00-00-00-00-00-00. */
	inline operator unspecified_bool_type() const {
		return _data[0] || _data[1] || _data[2] ? &EtherAddress64::is_group : 0;
	}

	/** @brief Return true iff this address is a group address.
	 *
	 * Group addresses have the low-order bit of the first byte set to 1, as
	 * in 01-00-00-00-00-00-00-00 or 03-00-00-02-04-09-04-02. */
	inline bool is_group() const {
		return data()[0] & 1;
	}

	/** @brief Return true iff this address is a "local" address.
	 *
	 * Local addresses have the next-to-lowest-order bit of the first byte set
	 * to 1. */
	inline bool is_local() const {
		return data()[0] & 2;
	}

	/** @brief Return true iff this address is the broadcast address.
	 *
	 * The Ethernet broadcast address is FF-FF-FF-FF-FF-FF-FF-FF. */
	inline bool is_broadcast() const {
		return _data[0] + _data[1] + _data[2] + _data[3] == 0x03FC;
	}

	/** @brief Return a pointer to the address data. */
	inline unsigned char *data() {
		return reinterpret_cast<unsigned char *>(_data);
	}

	/** @overload */
	inline const unsigned char *data() const {
		return reinterpret_cast<const unsigned char *>(_data);
	}

	/** @brief Return a pointer to the address data, as an array of
	 * uint16_ts. */
	inline const uint16_t *sdata() const {
		return _data;
	}

	/** @brief Hash function. */
	inline size_t hashcode() const {
		return (_data[2] | ((size_t) _data[1] << 16)) ^ ((size_t) _data[0] << 9);
	}

	/** @brief Unparse this address into a dash-separated hex String.
	 *
	 * Examples include "00-00-00-00-00-00-00-00" and "00-05-4E-50-3C-1A-BB-CC".
	 *
	 * @note The IEEE standard for printing Ethernet addresses uses dashes as
	 * separators, not colons.  Use unparse_colon() to unparse into the
	 * nonstandard colon-separated form. */
	inline String unparse() const {
		return unparse_dash();
	}

	/** @brief Unparse this address into a colon-separated hex String.
	 *
	 * Examples include "00:00:00:00:00:00:00:00" and "00:05:4E:50:3C:1A:BB:CC".
	 *
	 * @note Use unparse() to create the IEEE standard dash-separated form. */
	String unparse_colon() const;

	/** @brief Unparse this address into a dash-separated hex String.
	 *
	 * Examples include "00-00-00-00-00-00-00-00" and "00-05-4E-50-3C-1A-BB-CC".
	 *
	 * @note This is the IEEE standard for printing Ethernet addresses.
	 * @sa unparse_colon */
	String unparse_dash() const;

	typedef const EtherAddress64 &parameter_type;

private:

	uint16_t _data[4];

	EtherAddress64(uint16_t m) {
		_data[0] = _data[1] = _data[2] = _data[3] = m;
	}

} CLICK_SIZE_PACKED_ATTRIBUTE;

/** @relates EtherAddress64
 @brief Compares two EtherAddress64 objects for equality. */
inline bool operator==(const EtherAddress64 &a, const EtherAddress64 &b) {
	return (a.sdata()[0] == b.sdata()[0] &&
			a.sdata()[1] == b.sdata()[1] &&
			a.sdata()[2] == b.sdata()[2] &&
			a.sdata()[3] == b.sdata()[3]);
}

/** @relates EtherAddress64
 @brief Compares two EtherAddress64 objects for inequality. */
inline bool operator!=(const EtherAddress64 &a, const EtherAddress64 &b) {
	return !(a == b);
}
class ArgContext;
class Args;
extern const ArgContext blank_args;

/** @class EtherAddress64Arg
 @brief Parser class for Ethernet addresses.

 This is the default parser for objects of EtherAddress64 type. For 8-byte
 arrays like "click_ether::ether_shost" and "click_ether::ether_dhost", you
 must pass an EtherAddressArg() explicitly:

 @code
 struct click_ether ethh;
 ... Args(...) ...
 .read_mp("SRC", EtherAddressArg(), ethh.ether_shost)
 ...
 @endcode */
class EtherAddress64Arg {
public:
	typedef void enable_direct_parse;
	static bool parse(const String &str, EtherAddress64 &value,
			const ArgContext &args = blank_args);
	static bool parse(const String &str, unsigned char *value,
			const ArgContext &args = blank_args) {
		return parse(str, *reinterpret_cast<EtherAddress64 *>(value), args);
	}
	static bool direct_parse(const String &str, EtherAddress64 &value,
			Args &args);
	static bool direct_parse(const String &str, unsigned char *value,
			Args &args) {
		return direct_parse(str, *reinterpret_cast<EtherAddress64 *>(value),
				args);
	}
};

template<> struct DefaultArg<EtherAddress64> : public EtherAddress64Arg {};
template<> struct has_trivial_copy<EtherAddress64> : public true_type {};

CLICK_ENDDECLS
#endif
