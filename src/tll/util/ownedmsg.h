/*
 * Copyright (c) 2019 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_UTIL_OWNEDMSG_H
#define _TLL_UTIL_OWNEDMSG_H

#include "tll/channel.h"

namespace tll::util {
class OwnedMessage : public tll_msg_t
{
 public:
	OwnedMessage()
	{
		*static_cast<tll_msg_t *>(this) = {};
	}

	OwnedMessage(const tll_msg_t * rhs)
	{
		copy_noreset(rhs);
	}

	OwnedMessage(const OwnedMessage & rhs)
	{
		copy_noreset(&rhs);
	}

	OwnedMessage(OwnedMessage && rhs)
	{
		*static_cast<tll_msg_t *>(this) = rhs;
		rhs.data = nullptr;
		rhs.size = 0;
	}

	~OwnedMessage()
	{
		reset();
	}

	OwnedMessage & operator = (OwnedMessage rhs)
	{
		tll_msg_copy_info(this, rhs);
		std::swap(data, rhs.data);
		std::swap(size, rhs.size);
		return *this;
	}

	void reset()
	{
		if (data)
			delete [] (char *) data;
		data = nullptr;
		size = 0;
	}

	void resize(size_t size)
	{
		reset();
		this->data = new char[size];
		this->size = size;
	}

	static OwnedMessage * allocate(size_t size)
	{
		auto m = new OwnedMessage;
		m->resize(size);
		return m;
	}

	void copy(const tll_msg_t *rhs)
	{
		if (data)
			delete [] (char *) data;
		copy_noreset(rhs);
	}

	void copy_noreset(const tll_msg_t *rhs)
	{
		*(tll_msg_t *) this = *rhs;

		if (size) {
			data = new char[size];
			memcpy((void *) data, rhs->data, size);
		} else
			data = nullptr;
	}

	operator tll_msg_t * () { return this; }
	operator const tll_msg_t * () const { return this; }
};

} // namespace tll::util

#endif//_TLL_UTIL_OWNEDMSG_H
