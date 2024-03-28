// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#ifndef _TLL_CHANNEL_UDP_H
#define _TLL_CHANNEL_UDP_H

#include "tll/channel/base.h"
#include "tll/util/size.h"
#include "tll/util/sockaddr.h"

#include <array>
#include <chrono>
#include <vector>

#include <ifaddrs.h>
#include <net/if.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __linux__
#include <linux/errqueue.h>
#include <linux/sockios.h>
#include <linux/net_tstamp.h>
#endif

#if defined(__APPLE__) && !defined(MSG_NOSIGNAL)
#  define MSG_NOSIGNAL 0
#endif//__APPLE__

namespace tll::channel::udp {

#ifdef __linux__
static constexpr std::string_view control_scheme = R"(yamls://
- name: Time
  id: 10
  fields:
    - {name: time, type: int64, options.type: duration, options.resolution: ns}
)";

static constexpr int time_msgid = 10;
#endif

template <typename T>
class Socket : public tll::channel::Base<T>
{
 protected:
	using Base = tll::channel::Base<T>;

	tll::network::sockaddr_any _addr;
	tll::network::sockaddr_any _peer;
	std::vector<char> _buf;
	std::vector<char> _buf_control;
	size_t _sndbuf = 0;
	size_t _rcvbuf = 0;
	unsigned _ttl = 0;

	std::array<long long, 8> _tx_seq;
	unsigned _tx_idx = 0;

	bool _timestamping = false;
	bool _timestamping_tx = false;

	bool _multi = false;
	bool _mcast_loop = true;
	std::optional<std::string> _mcast_interface;
	std::optional<in_addr> _mcast_source;

	int _mcast_ifindex = 0;
	std::optional<in_addr> _mcast_ifaddr4; // Only for ipv4 multicast structures, ipv6 use interface index

	int _nametoindex()
	{
		if (!_mcast_interface || _mcast_ifindex)
			return 0;
		if (this->_addr->sa_family == AF_INET) {
			_mcast_ifaddr4 = {};
			struct ifaddrs * ifa;
			if (getifaddrs(&ifa))
				return this->_log.fail(EINVAL, "Failed to get interface list: {}", strerror(errno));
			for (auto i = ifa; i; i = i->ifa_next) {
				if (i->ifa_name == *_mcast_interface && i->ifa_addr && i->ifa_addr->sa_family == AF_INET) {
					_mcast_ifaddr4 = ((const sockaddr_in *) i->ifa_addr)->sin_addr;
					break;
				}
			}
			freeifaddrs(ifa);
			if (!_mcast_ifaddr4)
				return this->_log.fail(EINVAL, "No ipv4 address for interface {}", *_mcast_interface);
			this->_log.debug("Interface {} addr {}", *_mcast_interface, *_mcast_ifaddr4);
		}

		_mcast_ifindex = if_nametoindex(this->_mcast_interface->c_str());
		if (_mcast_ifindex == 0)
			return this->_log.fail(EINVAL, "Interface '{}' not found: {}", *this->_mcast_interface, strerror(errno));
		return 0;
	}

	std::chrono::duration<int64_t, std::nano> _cmsg_timestamp(msghdr * msg)
	{
		using namespace std::chrono;
		nanoseconds r = {};
#ifdef __linux__
		for (auto cmsg = CMSG_FIRSTHDR(msg); cmsg; cmsg = CMSG_NXTHDR(msg, cmsg)) {
			if(cmsg->cmsg_level != SOL_SOCKET)
				continue;

			if (cmsg->cmsg_type == SO_TIMESTAMPING) {
				auto ts = (struct timespec *) CMSG_DATA(cmsg);
				if (ts[2].tv_sec || ts[2].tv_nsec) // Get HW timestamp if available
					r = seconds(ts[2].tv_sec) + nanoseconds(ts[2].tv_nsec);
				else
					r = seconds(ts->tv_sec) + nanoseconds(ts->tv_nsec);
			}
		}
#endif
		return r;
	}

	uint32_t _cmsg_seq(msghdr *msg)
	{
#ifdef __linux__
		for (auto cmsg = CMSG_FIRSTHDR(msg); cmsg; cmsg = CMSG_NXTHDR(msg, cmsg)) {
			if (!((cmsg->cmsg_level == SOL_IP && cmsg->cmsg_type == IP_RECVERR) ||
				(cmsg->cmsg_level == SOL_IPV6 && cmsg->cmsg_type == IPV6_RECVERR)))
				continue;

			auto err = (sock_extended_err *) CMSG_DATA(cmsg);
			if (err->ee_errno == ENOMSG && err->ee_origin == SO_EE_ORIGIN_TIMESTAMPING)
				return err->ee_data;
		}
#endif
		return 0;
	}

	int _process_errqueue()
	{
		iovec iov = {_buf.data(), _buf.size()};
		msghdr mhdr = {};
		mhdr.msg_iov = &iov;
		mhdr.msg_iovlen = 1;
		mhdr.msg_control = _buf_control.data();
		mhdr.msg_controllen = _buf_control.size();
		return _process_errqueue(&mhdr);
	}

	int _process_errqueue(msghdr * mhdr)
	{
#ifdef __linux__
		if (!_timestamping_tx)
			return 0;
		auto r = recvmsg(this->fd(), mhdr, MSG_NOSIGNAL | MSG_ERRQUEUE);
		if (r < 0) {
			if (errno == EAGAIN)
				return EAGAIN;
			return this->_log.fail(EINVAL, "Failed to receive errqueue message: {}", strerror(errno));
		}

		auto time = _cmsg_timestamp(mhdr);
		auto seq = _cmsg_seq(mhdr);

		tll_msg_t msg = {};
		if (_tx_idx - seq > _tx_seq.size())
			msg.seq = -1;
		else
			msg.seq = _tx_seq[seq % _tx_seq.size()];
		msg.type = TLL_MESSAGE_CONTROL;
		msg.msgid = time_msgid;
		msg.time = time.count();
		msg.data = &time;
		msg.size = sizeof(time);
		this->_callback(&msg);
#endif
		return 0;
	}

 public:
	static constexpr std::string_view channel_protocol() { return "udp"; }
	static constexpr auto process_policy() { return Base::ProcessPolicy::Custom; }

	int _init(const tll::Channel::Url &url, tll::Channel *master)
	{
		auto reader = this->channel_props_reader(url);
		auto size = reader.getT("size", util::Size {64 * 1024});
		_sndbuf = reader.getT("sndbuf", util::Size {0});
		_rcvbuf = reader.getT("rcvbuf", util::Size {0});
		_ttl = reader.getT("ttl", 0u);

		_timestamping = reader.getT("timestamping", false);
		_timestamping_tx = reader.getT("timestamping-tx", false);

		_multi = reader.getT("multicast", false);
		if (_multi) {
			_mcast_loop = reader.getT("loop", true);
			_mcast_interface = reader.get("interface");
			_mcast_source = reader.getT("source", std::optional<in_addr>());
		}
		if (!reader)
			return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());
		_buf.resize(size);
		if (_timestamping) {
#ifdef __linux__
			_buf_control.resize(256);
			if (_timestamping_tx) {
				this->_scheme_control.reset(this->context().scheme_load(control_scheme));
				if (!this->_scheme_control.get())
					return this->_log.fail(EINVAL, "Failed to load control scheme");
			}
#else
			this->_log.info("Packet timestamping supported only on linux");
#endif
		}
		return 0;
	}

	int _open(const tll::ConstConfig &)
	{
		using namespace tll::network;
		if (int r = nonblock(this->fd()))
			return this->_log.fail(EINVAL, "Failed to set nonblock: {}", strerror(r));

		_tx_idx = -1;
		_tx_seq = {};
		if (_timestamping) {
#ifdef __linux__
			int v = SOF_TIMESTAMPING_RX_SOFTWARE | SOF_TIMESTAMPING_RX_HARDWARE | SOF_TIMESTAMPING_RAW_HARDWARE | SOF_TIMESTAMPING_SOFTWARE;
			if (_timestamping_tx)
				v |= SOF_TIMESTAMPING_TX_SOFTWARE | SOF_TIMESTAMPING_TX_HARDWARE | SOF_TIMESTAMPING_OPT_TSONLY | SOF_TIMESTAMPING_OPT_ID;
			if (setsockoptT<int>(this->fd(), SOL_SOCKET, SO_TIMESTAMPING, v))
				return this->_log.fail(EINVAL, "Failed to set SO_TIMESTAMPING: {}", strerror(errno));
#endif
		}

		if (_sndbuf && setsockoptT<int>(this->fd(), SOL_SOCKET, SO_SNDBUF, _sndbuf))
			return this->_log.fail(EINVAL, "Failed to set sndbuf to {}: {}", _sndbuf, strerror(errno));

		if (_rcvbuf && setsockoptT<int>(this->fd(), SOL_SOCKET, SO_RCVBUF, _rcvbuf))
			return this->_log.fail(EINVAL, "Failed to set rcvbuf to {}: {}", _rcvbuf, strerror(errno));

		if (_ttl) {
			if (_multi && _addr->sa_family == AF_INET6) {
				if (setsockoptT<int>(this->fd(), IPPROTO_IPV6, IPV6_MULTICAST_HOPS, _ttl))
					return this->_log.fail(EINVAL, "Failed to set IPv6 multicast ttl to {}: {}", _ttl, strerror(errno));
			} else if (_multi && _addr->sa_family == AF_INET) {
				if (setsockoptT<int>(this->fd(), IPPROTO_IP, IP_MULTICAST_TTL, _ttl))
					return this->_log.fail(EINVAL, "Failed to set IP multicast ttl to {}: {}", _ttl, strerror(errno));
			} else if (_addr->sa_family == AF_INET6) {
				if (setsockoptT<int>(this->fd(), IPPROTO_IPV6, IPV6_UNICAST_HOPS, _ttl))
					return this->_log.fail(EINVAL, "Failed to set IPv6 ttl to {}: {}", _ttl, strerror(errno));
			} else if (setsockoptT<int>(this->fd(), IPPROTO_IP, IP_TTL, _ttl))
				return this->_log.fail(EINVAL, "Failed to set IP ttl to {}: {}", _ttl, strerror(errno));
		}
		if (_multi && _mcast_loop) {
			if (_addr->sa_family == AF_INET6) {
				if (setsockoptT<int>(this->fd(), IPPROTO_IPV6, IPV6_MULTICAST_LOOP, _mcast_loop))
					return this->_log.fail(EINVAL, "Failed to set IPv6 multicast loop: {}", strerror(errno));
			} else if (setsockoptT<int>(this->fd(), IPPROTO_IP, IP_MULTICAST_LOOP, _mcast_loop))
				return this->_log.fail(EINVAL, "Failed to set IP multicast loop: {}", strerror(errno));
		}

		if (_multi && _mcast_interface) {
			if (this->_nametoindex())
				return this->_log.fail(EINVAL, "Failed to get interface list");

			this->_log.info("Set multicast interface to {}: index {}", *_mcast_interface, _mcast_ifindex);
			if (_addr->sa_family == AF_INET6) {
				if (setsockoptT<int>(this->fd(), IPPROTO_IPV6, IPV6_MULTICAST_IF, _mcast_ifindex))
					return this->_log.fail(EINVAL, "Failed to set IPv6 multicast index to {} ({}): {}", _mcast_ifindex, *_mcast_interface, strerror(errno));
			} else {
				ip_mreqn mreq = {};
				mreq.imr_ifindex = _mcast_ifindex;
				if (setsockoptT(this->fd(), IPPROTO_IP, IP_MULTICAST_IF, mreq))
					return this->_log.fail(EINVAL, "Failed to set IP multicast index to {} ({}): {}", _mcast_ifindex, *_mcast_interface, strerror(errno));
			}
		}

		if ((this->internal.caps & caps::InOut) != caps::Output)
			this->_update_dcaps(dcaps::Process | dcaps::CPOLLIN);
		return 0;
	}

	int _close()
	{
		auto fd = this->_update_fd(-1);
		if (fd != -1)
			::close(fd);
		_mcast_ifindex = 0;
		return 0;
	}

	/// Override data hook
	int _on_data(const tll::network::sockaddr_any &from, tll_msg_t &msg)
	{
		this->_callback_data(&msg);
		return 0;
	}

	int _process(long timeout, int flags)
	{
		iovec iov = {_buf.data(), _buf.size()};
		msghdr mhdr = {};
		mhdr.msg_name = _peer.buf;
		mhdr.msg_namelen = sizeof(_peer.buf);
		mhdr.msg_iov = &iov;
		mhdr.msg_iovlen = 1;
		mhdr.msg_control = _buf_control.data();
		mhdr.msg_controllen = _buf_control.size();
		auto r = recvmsg(this->fd(), &mhdr, MSG_NOSIGNAL);
		if (r < 0) {
			if (errno == EAGAIN)
				return _process_errqueue(&mhdr);
			return this->_log.fail(EINVAL, "Failed to receive data: {}", strerror(errno));
		}
		_peer.size = mhdr.msg_namelen;
		this->_log.trace("Got data from {} {}", _peer, _peer->sa_family);

		tll_msg_t msg = { TLL_MESSAGE_DATA };
		msg.size = r;
		msg.data = _buf.data();

		if (_timestamping)
			msg.time = _cmsg_timestamp(&mhdr).count();

		return this->channelT()->_on_data(_peer, msg);
	}

	int _sendv(long long seq, const iovec *iov, size_t iovlen, const tll::network::sockaddr_any &addr)
	{
		size_t size = 0;
		for (auto i = 0u; i < iovlen; i++)
			size += iov[i].iov_len;
		this->_log.trace("Post {} bytes of data", size);

		msghdr m = {};
		m.msg_name = (void *) static_cast<const sockaddr *>(addr);
		m.msg_namelen = addr.size;
		m.msg_iov = (iovec *) iov;
		m.msg_iovlen = iovlen;
		_tx_seq[++_tx_idx % _tx_seq.size()] = seq;

		auto r = sendmsg(this->fd(), &m, MSG_NOSIGNAL);
		if (r < 0) {
			if (errno == EAGAIN)
				return EAGAIN;
			return this->_log.fail(errno, "Failed to post data: {}", strerror(errno));
		} else if ((size_t) r != size)
			return this->_log.fail(errno, "Failed to post data (truncated): {}", strerror(errno));
		r = _process_errqueue();
		if (r == EAGAIN)
			return 0;
		return r;
	}
};

} // namespace tll::channel::udp

#endif//_TLL_CHANNEL_UDP_H
