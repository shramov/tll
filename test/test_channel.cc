/*
 * Copyright (c) 2019-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "gtest/gtest.h"

#include "tll/channel/base.h"

class Null : public tll::channel::Base<Null>
{
 public:
	static constexpr std::string_view param_prefix() { return "null"; }

	int _init(const tll::Channel::Url &, tll::Channel *master) { return 0; }

	int _process(long timeout, int flags) { return EAGAIN; }
	int _post(const tll_msg_t *msg, int flags) { return 0; }
};

TLL_DEFINE_IMPL(Null);

class Echo : public tll::channel::Base<Echo>
{
 public:
	static constexpr std::string_view param_prefix() { return "echo"; }
	static constexpr auto open_policy() { return OpenPolicy::Manual; }

	const tll_channel_impl_t * _init_replace(const tll::Channel::Url &url)
	{
		auto null = url.getT("null", false);
		if (null && *null)
			return &Null::impl;
		return nullptr;
	}

	int _open(const tll::PropsView &) { return 0; }
	int _post(const tll_msg_t *msg, int flags) { return _callback(msg); }
	int _process(long timeout, int flags)
	{
		if (state() == tll::state::Opening) {
			state(tll::state::Active);
			return 0;
		}
		return EAGAIN;
	}
};

TLL_DEFINE_IMPL(Echo);

TEST(Channel, Register)
{
	auto cfg = tll::Config();
	auto ctx = tll::channel::Context(cfg);

	ASSERT_EQ(ctx.channel("echo://;name=echo"), nullptr);
	ASSERT_EQ(ctx.reg(&Echo::impl), 0);
	ASSERT_NE(ctx.reg(&Echo::impl), 0);

	ASSERT_EQ(ctx.channel("alias://;name=alias"), nullptr);
	ASSERT_EQ(ctx.reg(&Echo::impl, "alias"), 0);
	auto c = ctx.channel("alias://;name=alias");
	ASSERT_NE(c.get(), nullptr);
	c.reset();

	ASSERT_EQ(ctx.unreg(&Echo::impl, "alias"), 0);
	ASSERT_EQ(ctx.channel("alias://;name=echo").get(), nullptr);
	c = ctx.channel("echo://;name=echo");
	ASSERT_NE(c.get(), nullptr);
	c.reset();

	ASSERT_EQ(ctx.unreg(&Echo::impl), 0);
	ASSERT_NE(ctx.unreg(&Echo::impl), 0);
}

TEST(Channel, New)
{
	auto ctx = tll::channel::Context(tll::Config());

	ASSERT_EQ(ctx.reg(&Echo::impl), 0);

	auto c = ctx.channel("echo://;name=echo");
	ASSERT_NE(c.get(), nullptr);
	ASSERT_EQ(c->impl, &Echo::impl);
	ASSERT_EQ(c->state(), tll::state::Closed);
	ASSERT_EQ(c->open(), 0);
	ASSERT_EQ(c->state(), tll::state::Opening);
	ASSERT_EQ(c->process(), 0);
	ASSERT_EQ(c->state(), tll::state::Active);
	ASSERT_EQ(c->process(), EAGAIN);

	auto cfg = c->config();
	ASSERT_EQ(std::string(cfg.get("state").value_or("")), "Active");
	ASSERT_EQ(tll::conv::to_string(tll::Channel::Url(*cfg.sub("url"))), "echo://;name=echo");

	tll_msg_t msg = { TLL_MESSAGE_DATA };
	msg.seq = 100;

	long long rseq = 0;
	//auto rlambda = [&rseq](const tll::Channel *c, const tll_msg_t *m, void *) -> int { rseq = m->seq; return 0; };
	auto rlambda = [](const tll_channel_t *, const tll_msg_t *m, void * user) -> int { *(long long *) user = m->seq; return 0; };

	c->callback_add(rlambda, &rseq);

	c->post(&msg);
	ASSERT_EQ(rseq, msg.seq);

	c = ctx.channel("echo://;name=echo-null;null=yes");
	ASSERT_NE(c.get(), nullptr);
	ASSERT_EQ(c->impl, &Null::impl);

	ASSERT_EQ(c->state(), tll::state::Closed);
	ASSERT_EQ(c->open(), 0);
	ASSERT_EQ(c->state(), tll::state::Active);
	ASSERT_EQ(c->process(), EAGAIN);
}
