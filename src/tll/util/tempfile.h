// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#ifndef _TLL_UTIL_TEMPFILE_H
#define _TLL_UTIL_TEMPFILE_H

#include <filesystem>
#include <cstdlib>
#include <cstring>
#include <utility>

#include <unistd.h>

namespace tll::util {

class TempFile {
	std::filesystem::path _filename;
	int _fd = -1;
	int _errno = 0;

public:
	static constexpr std::string_view suffix = ".XXXXXX"; ///< Suffix required for mkstemp (with additional dot)

	explicit TempFile(std::string_view tmpl)
	{
		std::string str;
		str.reserve(tmpl.size() + suffix.size());
		str += tmpl;
		str += suffix;
		_fd = mkstemp(str.data());
		if (_fd == -1)
			_errno = errno;
		else
			_filename = std::move(str);
	}

	TempFile(const TempFile &) = delete;
	TempFile(TempFile && rhs) noexcept
		: _filename(std::exchange(rhs._filename, {}))
		, _fd(std::exchange(rhs._fd, -1))
		, _errno(std::exchange(rhs._errno, 0))
	{
	}

	~TempFile() { reset(); }

	TempFile & operator = (const TempFile &) = delete;
	TempFile & operator = (TempFile && rhs) noexcept { swap(rhs); return *this; }

	void swap(TempFile & rhs)
	{
		std::swap(_filename, rhs._filename);
		std::swap(_fd, rhs._fd);
		std::swap(_errno, rhs._errno);
	}

	void reset()
	{
		if (!_filename.empty())
			unlink(_filename.c_str());
		_filename.clear();
		if (_fd != -1)
			close(_fd);
		_fd = -1;
	}

	void release() { _filename.clear(); }
	int release_fd() { return std::exchange(_fd, -1); }

	operator bool () const { return _fd != -1; }

	const std::filesystem::path & filename() const { return _filename; }
	int fd() const { return _fd; }
	int error() const { return _errno; }
	std::string_view strerror() const { return ::strerror(_errno); }
};

} // namespace tll::util

#endif//_TLL_UTIL_TEMPFILE_H
