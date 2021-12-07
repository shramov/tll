/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "gtest/gtest.h"

#include "tll/config.h"

#include <fmt/format.h>

TEST(Config, Get)
{
	tll::Config cfg;
	ASSERT_FALSE(cfg.has("a.b.c"));

	cfg.set("a.b.c", "");
	ASSERT_TRUE(cfg.has("a.b.c"));
	ASSERT_EQ(*cfg.get("a.b.c"), "");

	cfg.set("a.b.c", "1");
	ASSERT_TRUE(cfg.has("a.b.c"));
	ASSERT_EQ(*cfg.get("a.b.c"), "1");

	auto cstr = tll_config_get_copy(cfg, "a.b.c", -1, nullptr);
	EXPECT_NE(cstr, nullptr);
	if (cstr) {
		EXPECT_STREQ(cstr, "1");
	}
	tll_config_value_free(cstr);

	auto sub = cfg.sub("a.b");
	ASSERT_TRUE(sub);
	ASSERT_TRUE(sub->has("c"));
	ASSERT_EQ(*sub->get("c"), "1");

	auto csub = static_cast<const tll::Config *>(&cfg)->sub("a.b");
	ASSERT_TRUE(csub);
	ASSERT_TRUE(csub->has("c"));
	ASSERT_EQ(*csub->get("c"), "1");

	sub->set("c", "2");

	ASSERT_EQ(*sub->get("c"), "2");
	ASSERT_EQ(*cfg.get("a.b.c"), "2");

	int v = 10;
	cfg.set_ptr("a.b.d", &v);

	ASSERT_EQ(*sub->get("d"), "10");
	v = 20;
	ASSERT_EQ(*sub->get("d"), "20");

	sub = cfg.sub("a.b.c");
	ASSERT_TRUE(sub);
	tll_config_set(*sub, nullptr, -1, "3", -1);
	ASSERT_TRUE(sub->value());
	ASSERT_TRUE(tll_config_has(*sub, nullptr, -1));
	ASSERT_EQ(*sub->get(), "3");
}

template <typename T>
void compare_keys(const std::map<std::string, T> &m, std::list<std::string_view> l)
{
	std::list<std::string_view> r;
	for (auto & i : m) r.push_back(i.first);
	ASSERT_EQ(r, l);
}

TEST(Config, Browse)
{

	auto c = tll::Config::load("yamls://{a: 1, b: 2, c: [10, 20, 30], x: {y: {z: string}}}");
	ASSERT_TRUE(c);
	compare_keys<tll::Config>(c->browse("**"), {"a", "b", "c.0000", "c.0001", "c.0002", "x.y.z"});
	compare_keys<tll::Config>(c->list(), {"a", "b", "c", "x"});

	std::optional<tll::ConstConfig> s = c->sub("x");
	ASSERT_TRUE(s);

	compare_keys<tll::ConstConfig>(s->browse("**"), {"y.z"});
	compare_keys<tll::ConstConfig>(s->list(), {"y"});

	c = tll::Config::load("yamls://{a: 1, a: {b: 2, c: 3}}");
	ASSERT_TRUE(c);

	compare_keys<tll::Config>(c->browse("**"), {"a", "a.b", "a.c"});

	auto s1 = c->sub("a");
	ASSERT_TRUE(s1);
	ASSERT_TRUE(s1->has("b"));
	compare_keys<tll::Config>(s1->browse("**"), {"b", "c"});
}

TEST(Config, Copy)
{
	auto c = tll::Config::load("yamls://{a: 1, b: 2, c: [10, 20, 30], x: {y: {z: string}}}");
	ASSERT_TRUE(c);
	compare_keys<tll::Config>(c->browse("**"), {"a", "b", "c.0000", "c.0001", "c.0002", "x.y.z"});

	auto c1 = c->copy();
	compare_keys<tll::Config>(c->browse("**"), {"a", "b", "c.0000", "c.0001", "c.0002", "x.y.z"});

	c->set("a", "987");
	c->set("x.y.z", "str");
	ASSERT_EQ(*c1.get("a"), "1");
	ASSERT_EQ(*c1.get("x.y.z"), "string");
}

TEST(Config, Merge)
{

	auto c = tll::Config::load("yamls://{a: 1, b.c: 1}");
	ASSERT_TRUE(c);
	auto c1 = tll::Config::load("yamls://b.d: 2");
	ASSERT_TRUE(c1);

	ASSERT_EQ(c->merge(*c1), 0);

	compare_keys(c->browse("**"), {"a", "b.c", "b.d"});
}

TEST(Config, Imports)
{

	auto c = tll::Config::load(R"(yamls://
import:
 - 'yamls://{a: 1, b.c: 2}'
 - 'yamls://{a: 2, b.d: 3}'
b.c: 10
)");
	ASSERT_TRUE(c);

	compare_keys(c->browse("**"), {"b.c", "import.0000", "import.0001"});
	ASSERT_EQ(*c->get("b.c"), "10");

	ASSERT_EQ(c->process_imports("import"), 0);

	compare_keys(c->browse("**"), {"a", "b.c", "b.d", "import.0000", "import.0001"});
	ASSERT_EQ(*c->get("a"), "2");
	ASSERT_EQ(*c->get("b.c"), "10");
	ASSERT_EQ(*c->get("b.d"), "3");
}

TEST(Config, GetUrl)
{

	auto c = tll::Config::load(R"(yamls://
tcp:
  url: tcp://*:8080;dump=yes
  url:
    stat: yes
)");
	ASSERT_TRUE(c);

	auto r = c->getT<tll::ConfigUrl>("tcp.url");
	ASSERT_TRUE(r);
	ASSERT_EQ(std::string("tcp://*:8080;dump=yes;stat=yes"), tll::conv::to_string(*r));

	r = c->getT<tll::ConfigUrl>("tcp.url", tll::ConfigUrl());
	ASSERT_TRUE(r);
	ASSERT_EQ(std::string("tcp://*:8080;dump=yes;stat=yes"), tll::conv::to_string(*r));

	c->set("tcp.url.dump", "no");

	ASSERT_FALSE(c->getT<tll::ConfigUrl>("tcp.url"));
}

TEST(Config, Link)
{
	auto c = tll::Config::load(R"(yamls://{a: {a: 100, b: 200}})");
	ASSERT_TRUE(c);

	compare_keys(c->browse("**"), {"a.a", "a.b"});
	ASSERT_EQ(*c->get("a.a"), "100");

	ASSERT_EQ(c->link("b", "/a/a"), 0);

	compare_keys(c->browse("**"), {"a.a", "a.b", "b"});

	ASSERT_EQ(*c->get("b"), "100");
	ASSERT_EQ(c->set("a.a", "300"), 0);
	ASSERT_EQ(*c->get("b"), "300");

	ASSERT_EQ(c->link("b", "../a/b"), 0);

	ASSERT_EQ(*c->get("b"), "200");

	ASSERT_EQ(c->link("c", "b/../../a"), 0);

	compare_keys(c->browse("**"), {"a.a", "a.b", "b", "c.a", "c.b"});

	ASSERT_EQ(*c->get("c.a"), "300");
	ASSERT_EQ(*c->get("c.b"), "200");

	auto copy = c->copy();

	ASSERT_EQ(*copy.get("c.a"), "300");
	ASSERT_EQ(copy.set("a.a", "400"), 0);
	ASSERT_EQ(*copy.get("c.a"), "400");
	ASSERT_EQ(*c->get("c.a"), "300");

	ASSERT_EQ(c->link("d.a", "/a"), 0);
	ASSERT_EQ(c->set("d.b", "d.b"), 0);

	copy = c->sub("d")->copy();

	compare_keys(c->sub("d")->browse("**"), {"a.a", "a.b", "b"});
	compare_keys(copy.browse("**"), {"a.a", "a.b", "b"});
}
