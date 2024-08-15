// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#ifndef _TLL_CPPRING_H
#define _TLL_CPPRING_H

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <memory>

#include <tll/compat/align.h>

namespace tll {

template <bool HeadGen = false, bool TailGen = false>
struct RingT
{
	static_assert(!(TailGen && !HeadGen), "Tail generation is not available without head generation enabled");

	int32_t _magic = Magic;
	int32_t _version = 0;
	size_t _size = 0;

	template <bool Gen>
	struct Pointer
	{
		std::atomic<size_t> ptr;
		std::atomic<uint64_t> generation_pre;
		std::atomic<uint64_t> generation_post;
		bool enabled = false;

		auto load(std::memory_order order) { return ptr.load(order); }
		auto load(std::memory_order order) const { return ptr.load(order); }

		void store(size_t value, std::memory_order order)
		{
			if constexpr (Gen) {
				auto gen = generation_pre.load(std::memory_order_relaxed) + 1;
				generation_pre.store(gen, order);
				ptr.store(value, std::memory_order_relaxed); // Relaxed store is guarded by next one
				generation_post.store(gen, order);
			} else
				ptr.store(value, order);
		}

		void reset()
		{
			enabled = Gen;
			ptr = 0;
			if (Gen)
				generation_pre = generation_post = 0;
			else
				generation_pre = generation_post = -1;
		}
	};

	/// Head marker, only updated by reader
	Pointer<HeadGen> TLL_ALIGN(64) head;

	/// Tail marker, only updated by writer
	Pointer<TailGen> TLL_ALIGN(64) tail;

	char TLL_ALIGN(64) data[];

	using Size = int32_t;

	RingT() {}

	Size * _size_at(size_t off) { return (Size *) &data[off]; }
	const Size * _size_at(size_t off) const { return (const Size *) &data[off]; }

 public:
	static constexpr int32_t Magic = 0x72696e67; // 'ring'

	struct Memory { void * base; Size size; };

	auto magic() const { return _magic; }
	auto size() const { return _size; }

	static std::unique_ptr<RingT> allocate(size_t size)
	{
		std::unique_ptr<RingT> ptr { (RingT *) new char[sizeof(RingT) + size] };
		ptr->init(size);
		return ptr;
	}

	static void operator delete (void *ptr) { delete [] (char *) ptr; }

	static RingT * bind(void * ptr)
	{
		auto r = static_cast<RingT *>(ptr);
		if (!r->_validate())
			return nullptr;
		return r;
	}

	static const RingT * bind(const void * ptr)
	{
		auto r = static_cast<const RingT *>(ptr);
		if (!r->_validate())
			return nullptr;
		return r;
	}

	void init(size_t size)
	{
		_magic = Magic;
		_version = 0;
		_size = size;
		head.reset();
		tail.reset();
	}

	template <unsigned Align = 8>
	static constexpr size_t aligned(size_t x)
	{
	    return x + (-x & (Align - 1));
	}

	int write_begin(void ** data, size_t size)
	{
		size_t a = aligned(size + sizeof(Size));
		if (a > _size)
			return ERANGE;

		auto t = tail.load(std::memory_order_relaxed);
		auto h = head.load(std::memory_order_acquire);
		size_t free = _wrap_size(_size + h - t - 1, _size) + 1; // -1 + 1 is needed for head==tail

		if (free <= a)
			return EAGAIN;
		if (t + a > _size) {
			if (h <= a)
				return EAGAIN;
			*data = _size_at(0) + 1;
			return 0;
		}

		*data = _size_at(t) + 1;
		return 0;
	}

	int write_end(const void * data, size_t size)
	{
		size_t a = aligned(size + sizeof(Size));
		auto t = tail.load(std::memory_order_relaxed);
		if (data == _size_at(0) + 1) {
			// Wrap
			*_size_at(t) = -1;
			t = 0;
		}

		*_size_at(t) = size;
		tail.store(_wrap_size(t + a, _size), std::memory_order_release);
		return 0;
	}

	int read(const void **data, size_t *size) const
	{
	    return _read_at(head.load(std::memory_order_relaxed), data, size);
	}

	int shift()
	{
		auto t = tail.load(std::memory_order_acquire);
		auto h = head.load(std::memory_order_relaxed);
		if (h == t)
			return EAGAIN;
		auto off = _shift_offset(h);
		if (off < 0)
			return EAGAIN;

		head.store(off, std::memory_order_release);
		return 0;
	}

	struct Iterator
	{
		const RingT * ring = nullptr;
		size_t offset;
		uint64_t generation;

		bool valid() const
		{
			return ring->head.generation_pre.load(std::memory_order_acquire) <= generation;
		}

		int read(const void **data, size_t *size)
		{
			if (!valid())
				return EINVAL;
			if (auto r = ring->_read_at(offset, data, size); r)
				return r;
			if (!valid())
				return EINVAL; // Check that data and size are valid
			return 0;
		}

		int shift()
		{
			if (!valid())
				return EINVAL;
			auto t = ring->tail.load(std::memory_order_acquire);
			if (ring->head.load(std::memory_order_acquire) == t)
				return EAGAIN;
			if (offset == t)
				return EAGAIN;
			auto off = ring->_shift_offset(offset);
			if (off < 0)
				return EAGAIN;
			if (!valid())
				return EINVAL;

			generation++;
			offset = off;
			return 0;
		}
	};

	Iterator begin() const { return _iterator(head); }
	Iterator end() const { return _iterator(tail); }

 private:
	bool _validate() const
	{
		if (_magic != Magic)
			return false;
		if (HeadGen && !head.enabled)
			return false;
		if (TailGen && !tail.enabled)
			return false;
		return true;
	}

	template <typename Ptr>
	Iterator _iterator(Ptr &ptr) const
	{
		auto r = Iterator { this };
		if (!ptr.enabled)
			return r;
		r.generation = ptr.generation_post.load(std::memory_order_acquire);
		r.offset = ptr.load(std::memory_order_acquire);
		if (ptr.generation_pre.load(std::memory_order_acquire) != r.generation)
			return Iterator { this };
		return r;
	}

	size_t _wrap_size(size_t off, size_t size) const
	{
		//return off % size;
		if (off >= size)
			return off - size;
		return off;
	}

	ssize_t _shift_offset(size_t offset) const
	{
		auto size = *_size_at(offset);
		if (size < 0)
			return _shift_offset(0);
		size = aligned(size + sizeof(Size));
		return _wrap_size(offset + size, _size);
	}

	int _read_at(size_t offset, const void **data, size_t *size) const
	{
		if (offset == tail.load(std::memory_order_acquire))
			return EAGAIN;

		auto ptr = _size_at(offset);
		auto sz = *ptr;
		if (sz < 0)
			return _read_at(0, data, size);

		*size = sz;
		*data = ptr + 1;
		return 0;
	}
};

using Ring = RingT<false, false>;
using PubRing = RingT<true, true>;

} // namespace tll

#endif//_TLL_CPPRING_H
