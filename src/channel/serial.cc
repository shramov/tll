/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "channel/serial.h"

#include "tll/util/size.h"

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

using namespace tll;

TLL_DEFINE_IMPL(ChSerial);

namespace {
std::string_view speed2str(speed_t speed)
{
	switch (speed) {
	case B4800: return "4800";
	case B9600: return "9600";
	case B19200: return "19200";
	case B38400: return "38400";
	case B57600: return "57600";
	default:
		return "unknown";
	}
}
}

int ChSerial::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	auto reader = channel_props_reader(url);
	_speed = reader.getT("speed", B9600, {{"9600", B9600}, {"19200", B19200}});
	_parity = reader.getT("parity", Parity::None, {{"none", Parity::None}, {"even", Parity::Even}, {"odd", Parity::Odd}});
	_stop_bits = reader.getT("stop", 1u);
	_data_bits = reader.getT("data", 8u);
	_flow_control = reader.getT("flow-control", false);
	auto size = reader.getT("size", tll::util::Size(64 * 1024));
	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());
	if (_data_bits != 8 && _data_bits != 7)
		return _log.fail(EINVAL, "Invalid data bits: {}, must be 7 or 8", _data_bits);
	if (_stop_bits != 1 && _stop_bits != 2)
		return _log.fail(EINVAL, "Invalid stop bits: {}, must be 1 or 2", _stop_bits);

	_filename = url.host();
	if (!_filename.size())
		return _log.fail(EINVAL, "Empty serial port name");

	_buf.resize(size);

	return channel::Base<ChSerial>::_init(url, master);
}

int ChSerial::_open(const PropsView &url)
{
	auto fd = ::open(_filename.c_str(), O_RDWR | O_NONBLOCK);
	if (fd == -1)
		return _log.fail(EINVAL, "Failed open serial port {}: {}", _filename, strerror(errno));
	_update_fd(fd);

	struct termios tty = {};
	if (tcgetattr(fd, &tty))
		return _log.fail(EINVAL, "Failed to get serial parameters: {}", strerror(errno));

	cfsetispeed(&tty, _speed);
	cfsetospeed(&tty, _speed);

	tty.c_iflag = 0;
	tty.c_oflag = 0;
	tty.c_lflag = 0;
	tty.c_cflag &= ~(PARODD | PARENB | CRTSCTS | CSTOPB | CSIZE);

	switch (_data_bits) {
	case 8u: tty.c_cflag |= CS8; break;
	case 7u: tty.c_cflag |= CS7; break;
	default:
		return _log.fail(EINVAL, "Invalid data bits: {}", _data_bits);
	}

	switch (_parity) {
	case Parity::Odd:
		tty.c_cflag |= PARODD;
	case Parity::Even:
		tty.c_cflag |= PARENB;
		break;
	case Parity::None:
		break;
	}

	if (_flow_control)
		tty.c_cflag |= CRTSCTS;

	if (_stop_bits == 2u)
		tty.c_cflag |= CSTOPB;
	_log.info("Set serial parameters {}{}{} ({:x})", speed2str(_speed), _parity == Parity::None ? "n" : (_parity == Parity::Odd ? "o" : "e"), _stop_bits, tty.c_cflag);
	if (tcsetattr(fd, 0, &tty))
		return _log.fail(EINVAL, "Failed to set serial parameters: {}", strerror(errno));

	_dcaps_poll(dcaps::CPOLLIN);
	return channel::Base<ChSerial>::_open(url);
}

int ChSerial::_close()
{
	auto fd = _update_fd(-1);
	if (fd != -1)
		::close(fd);
	return 0;
}

int ChSerial::_post(const tll_msg_t *msg, int flags)
{
	if (msg->type != TLL_MESSAGE_DATA)
		return 0;
	auto r = ::write(fd(), msg->data, msg->size);
	if (r < 0)
		return _log.fail(EINVAL, "Failed to write data: {}", strerror(errno));
	if ((size_t ) r != msg->size)
		return _log.fail(EINVAL, "Truncated write");
	return 0;
}

int ChSerial::_process(long timeout, int flags)
{
	tll_msg_t msg = { TLL_MESSAGE_DATA };
	auto r = ::read(fd(), _buf.data(), _buf.size());
	if (r < 0) {
		if (errno == EAGAIN)
			return EAGAIN;
		return _log.fail(EINVAL, "Failed to read from serial: {}", strerror(errno));
	}
	msg.data = _buf.data();
	msg.size = r;
	_callback_data(&msg);
	return 0;
}
