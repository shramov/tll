#ifndef _TLL_UTIL_MMSTRUCT_H
#define _TLL_UTIL_MMSTRUCT_H

#include <tll/compat/fallocate.h>
#include <tll/util/scoped_fd.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <string_view>

namespace tll::util {

class MMBase
{
	ScopedFd _fd;
	void * _ptr = nullptr;
	size_t _size = 0;
 public:
	enum Mode { ReadWrite = O_RDWR | O_CREAT, ReadOnly = O_RDONLY };

	struct Error
	{
		int err = 0;
		std::string_view message;

		constexpr operator bool () const { return err == 0; }
	};

	~MMBase() { reset(); }

	Error init(std::string_view filename, Mode mode, size_t size)
	{
		std::string fn(filename);
		_fd.reset(::open(fn.c_str(), mode, S_IRUSR | S_IWUSR));
		if (_fd == -1)
			return { errno, "Failed to open file" };

		struct stat stat = {};
		if (auto r = fstat(_fd, &stat); r)
			return { errno, "stat" };

		if (stat.st_size < (off_t) size) {
			if (mode == ReadOnly)
				return {EMSGSIZE, "File size too small"};
			return resize(size);
		}

		return _mmap(size, mode == ReadWrite);
	}

	void reset()
	{
		_munmap();
		_fd.reset();
	}

	Error resize(size_t size)
	{
		_munmap();
		if (auto r = ftruncate(_fd, size); r)
			return {errno, "Failed to truncate file"};
		if (auto r = posix_fallocate(_fd, 0, size); r)
			return {r, "Failed to allocate space"};
		return _mmap(size, true);
	}

	void * data() { return _ptr; }
	const void * data() const { return _ptr; }

	constexpr size_t size() const { return _size; }

	Error _mmap(size_t size, bool rw)
	{
		if (auto r = mmap(nullptr, size, rw ? PROT_READ | PROT_WRITE : PROT_READ, MAP_SHARED, _fd, 0); r == MAP_FAILED)
			return {errno, "Failed to mmap"};
		else
			_ptr = r;
		_size = size;
		return {};
	}

	void _munmap()
	{
		if (_ptr)
			munmap(_ptr, _size);
		_ptr = nullptr;
	}
};

template <typename T>
class MMStruct : public MMBase
{
 public:
	auto init(std::string_view filename, Mode mode = Mode::ReadWrite)
	{
		return MMBase::init(filename, mode, sizeof(T));
	}

	T * ptr() { return static_cast<T *>(data()); }
	const T * ptr() const { return static_cast<const T *>(data()); }

	T * operator -> () { return ptr(); }
	const T * operator -> () const { return ptr(); }

	T & operator * () { return *ptr(); }
	const T & operator * () const { return *ptr(); }
};

} // namespace tll::util

#endif//_TLL_UTIL_MMSTRUCT_H
