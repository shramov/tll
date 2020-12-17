/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "processor/processor.h"
#include "processor/scheme.h"

#include "tll/logger/prefix.h"
#include "tll/util/conv-fmt.h"
#include "tll/util/result.h"

#include <set>

TLL_DECLARE_IMPL(tll::processor::_::Worker);

namespace tll {
template <>
struct CallbackT<tll::processor::_::Processor>
{
	template <typename ... Args> static int call(tll::processor::_::Processor *ptr, Args ... args) { return ptr->cb(args...); }
};
}

namespace {
tll::result_t<tll::Channel::Url> get_url(const tll::Config &cfg, std::string_view name)
{
	auto sub = cfg.sub(name);
	if (!sub)
		return tll::error(fmt::format("Url not found at '{}'", name));
	auto str = sub->get();
	tll::Channel::Url result;
	if (str) {
		auto r = tll::Channel::Url::parse(*str);
		if (!r)
			return tll::error(r.error());
		result.merge(*r);
	}
	auto copy = sub->copy();
	result.merge(copy, true);
	result.unset();
	return result;
}
}

using namespace tll::processor::_;

std::optional<tll::Channel::Url> Processor::parse_common(std::string_view type, std::string_view name, const Config &cfg)
{
	auto n = std::string(cfg.get("name").value_or(name));
	auto url = get_url(cfg, "url");
	if (!url)
		return _log.fail(std::nullopt, "Failed to load url for {}: {}", name, url.error());
	_log.debug("Create {} {}: {}", type, name, *url);
	if (url->has("name"))
		return _log.fail(std::nullopt, "Duplicate name parameter for {} {}", type, name);
	url->set("name", n);
	return *url;
}

void Processor::decay(Object * obj, bool root)
{
	if (obj->decay)
		return;
	if (!root)
		obj->decay = true;
	if (obj->state == state::Closed && !obj->opening) {
		_log.debug("Object {} is already closed, decay not needed", obj->name());
		return;
	}
	_log.debug("Decay object {}", obj->name());
	for (auto & o : obj->rdepends)
		decay(o);
	if (obj->rdepends.empty() || obj->ready_close()) {
		_log.debug("Deactivate decayed leaf object {}", obj->name());
		post<scheme::Deactivate>(obj, { obj });
	}
}

int Processor::parse_deps(Object &obj, const Config &cfg)
{
	return 0;
}

int Processor::_init(const tll::Channel::Url &url, Channel * master)
{
	_log = { fmt::format("tll.processor.context.{}", name) };
	loop._log = { fmt::format("tll.processor.context.{}.loop", name) };

	auto sub = url.sub("processor");
	if (!sub)
		return _log.fail(EINVAL, "Empty processor config");
	_cfg = sub->copy();

	_ipc = context().channel(fmt::format("ipc://;mode=server;name={}/ipc;dump=no;tll.internal=yes", name));
	if (!_ipc.get())
		return _log.fail(EINVAL, "Failed to create IPC link for processor");
	_ipc->callback_add(this, TLL_MESSAGE_MASK_DATA);
	_child_add(_ipc.get(), "ipc");
	loop.add(_ipc.get());

	//static_cast<TChannel *>(_ipc.get())->callback_addT(this, TLL_MESSAGE_MASK_DATA);

	if (init_depends())
		return _log.fail(EINVAL, "Failed to init objects");

	if (build_rdepends())
		return _log.fail(EINVAL, "Failed to build dependency graph");
	if (!_objects.size())
		return _log.fail(EINVAL, "Empty object list");
	_log.debug("Processor initialized");
	return 0;
}

int Processor::_open(const tll::PropsView &)
{
	loop.stop = false;
	if (_ipc->open())
		return _log.fail(EINVAL, "Failed to open IPC link");
	return 0;
}

Worker * Processor::init_worker(std::string_view name)
{
	auto it = _workers.find(name);
	if (it != _workers.end())
		return it->second;
	tll::Channel::Url url;
	url.proto("worker");
	url.set("name", fmt::format("{}/worker/{}", this->name, name));
	url.set("worker-name", name);
	url.set("tll.internal", "yes");
	url.set("dump", "no");

	auto ptr = context().channel(url, self(), &Worker::impl);
	if (!ptr)
		return _log.fail(nullptr, "Failed to create worker {}", name);
	auto w = tll::channel_cast<Worker>(ptr.get());
	if (!w)
		return _log.fail(nullptr, "Created invalid worker channel {}", name);
	_workers_ptr.emplace_back(std::move(ptr));
	_workers[std::string(name)] = w;
	_log.debug("New worker {}: {}", name, (void *) w);
	_child_add(w->self(), fmt::format("worker/{}", name));
	return w;
}

int Processor::init_one(std::string_view extname, const Config &cfg, bool logic)
{
	auto name = std::string(cfg.get("name").value_or(extname));
	auto log = _log.prefix("{} {}:", logic?"logic":"channel", std::string_view(name)); // TODO: name is moved and left empty without std::string_view wrapper
	log.debug("Init channel {} (external name {})", name, extname);

	auto disable = cfg.getT("disable", false);
	if (!disable || *disable) {
		log.debug("Object is disabled", name);
		return 0;
	}

	auto wname = std::string(cfg.get("worker").value_or("default"));
	auto w = init_worker(wname);
	if (!w)
		return log.fail(EINVAL, "Failed to init worker {}", wname);

	auto url = parse_common(logic?"logic":"channel", name, cfg);
	if (!url)
		return log.fail(EINVAL, "Failed to init channel");
	url->set("tll.worker", w->name);
	name = *url->getT<std::string>("name", "");

	if (logic) {
		url->proto("logic+" + url->proto());
		for (auto & c : cfg.browse("channels.**")) {
			auto key = "tll.channel." + c.first.substr(strlen("channels."));
			if (url->has(key))
				return log.fail(EINVAL, "Duplicate channel '{}'");
			url->set(key, *c.second.get());
		}
	}

	auto channel = context().channel(*url);
	if (!channel)
		return log.fail(EINVAL, "Failed to create channel {}", conv::to_string(*url));
	_objects.emplace_back(std::move(channel));
	auto o = &_objects.back();
	o->worker = w;

	auto deps = cfg.get("depends");
	if (deps || !deps->empty()) {
		for (auto d : split<','>(*deps)) {
			auto n = tll::util::strip(d);
			if (n.empty())
				return log.fail(EINVAL, "Empty dependency: '{}'", *deps);
			o->depends_names.emplace_back(n);
		}
	}

	if (o->init(*url))
		return log.fail(EINVAL, "Failed to init extra parameters");
	return 0;
}

int Processor::init_depends()
{
	for (auto & p : _cfg.browse("objects.*", true)) {
		if (p.second.get("type").value_or("") == "logic") continue;
		if (init_one(p.first.substr(strlen("objects.")), p.second, false))
			return EINVAL;
	}
	for (auto & p : _cfg.browse("objects.*", true)) {
		if (p.second.get("type").value_or("") != "logic") continue;
		if (init_one(p.first.substr(strlen("objects.")), p.second, true))
			return EINVAL;
	}
	return 0;
}

namespace tll::conv {
template <>
struct dump<Object *> : public to_string_from_string_buf<Object *>
{
	template <typename Buf>
	static std::string_view to_string_buf(const Object *v, Buf &buf)
	{
		if (!v) return "nullptr";
		return v->name();
	}
};
}

int Processor::build_rdepends()
{
	for (auto & o : _objects) {
		std::set<Object *> deps;
		for (auto & n : o.depends_names) {
			auto d = find(n);
			if (!d)
				return _log.fail(EINVAL, "Unknown dependency for {}: '{}'", o.name(), n);
			if (d == &o)
				return _log.fail(EINVAL, "Recursive dependency for {}", o.name());
			if (!deps.insert(d).second)
				return _log.fail(EINVAL, "Duplicate dependency {} -> {}", o.name(), d->name());
			o.depends.push_back(d);
			d->rdepends.push_back(&o);
		}
	}
	for (auto & o : _objects) {
		_log.debug("Object {}, depends [{}], rdepends [{}]", o->name(), o.depends, o.rdepends);
		o.worker->objects.push_back(&o);
	}
	return 0;
}

void Processor::_free()
{
	for (auto c = _objects.rbegin(); c != _objects.rend(); c++) {
		_log.debug("Destroy object {}", (*c)->name());
		c->channel.reset(nullptr);
	}
	_objects.clear();

	_workers.clear();
	_workers_ptr.clear();

	if (_ipc) {
		loop.del(_ipc.get());
		_ipc.reset();
	}
}

template <typename T> int Processor::post(tll_addr_t addr, T body)
{
	tll_msg_t msg = { TLL_MESSAGE_DATA };
	msg.msgid = T::id;
	msg.data = &body;
	msg.size = sizeof(body);
	msg.addr = addr;
	return _ipc->post(&msg);
}

int Processor::cb(const Channel * c, const tll_msg_t * msg)
{
	if (msg->type != TLL_MESSAGE_DATA) return 0;
	switch (msg->msgid) {
	case scheme::Exit::id: {
		auto data = (const scheme::Exit *) msg->data;
		if (state() == state::Closing) return 0;
		if (data->channel)
			_log.info("Shutdown requested by channel {}", data->channel->name());
		else
			_log.info("Shutdown");
		close();
		break;
	}
	case scheme::State::id: {
		auto data = (const scheme::State *) msg->data;
		update(data->channel, data->state);
		break;
	}
	case scheme::WorkerState::id: {
		auto data = (const scheme::WorkerState *) msg->data;
		_log.info("Worker {} state {}", data->worker->name, tll_state_str(data->state));
		data->worker->proc.state = data->state;
		data->worker->proc.addr = msg->addr;
		if (data->state == state::Closed) {
			//if (state() != state::Closing)
			//	return 0;
			if (!std::all_of(_workers.begin(), _workers.end(), [](auto & w) { return w.second->proc.state == state::Closed; }))
				return 0;
			_log.info("Workers finished");
			_log.info("Container exiting");
			_close(true);
			return 0;
		} else if (data->state != state::Active)
			return 0;
		if (!std::all_of(_workers.begin(), _workers.end(), [](auto & w) { return w.second->proc.state == state::Active; }))
			return 0;
		_log.info("All workers ready");
		activate();
		break;
	}
	default:
		_log.debug("Unknown message {}", msg->msgid);
	}
	return 0;
}

void Processor::update(const Channel *c, tll_state_t s)
{
	_log.info("Update channel {} state {}", c->name(), tll_state_str(s));
	auto o = find(c);
	if (!o)
		return _log.error("Channel {} not found", c->name());
	_log.info("Channel {} state {} -> {}", c->name(), tll_state_str(o->state), tll_state_str(s));
	o->state = s;

	if (o->verbose)
		_log.info("Object {} state {}", c->name(), tll_state_str(s));
	switch (s) {
	case state::Opening:
		o->opening = false;
		break;
	case state::Active:
		for (auto & d : o->rdepends) {
			if (d->ready_open()) {
				_log.debug("Activate object {}", d->name());
				post<scheme::Activate>(d, { d });
			}
		}
		return;
	case state::Error:
		_log.debug("Deactivate failed object {}", o->name());
		post<scheme::Deactivate>( o, { o });
		break;
	case state::Closed:
		if (!o->ready_close())
			decay(o, true);
		for (auto & d : o->depends) {
			if (o->decay)
				post<scheme::Deactivate>(d, { d });
		}
		break;
	default:
		break;
	}

	if (s == state::Closed && (state() == state::Closing || state() == state::Closed)) {
		if (!std::all_of(_objects.begin(), _objects.end(), [](auto & o) { return o.state == state::Closed; }))
			return;
		_log.info("All objects closed, signal workers");
		for (auto & w : _workers)
			post(w.second->proc.addr, scheme::Exit {});
		return;
	}
}

void Processor::activate()
{
	state(state::Active);
	for (auto & o : _objects) {
		if (o.depends.size()) continue;
		_log.debug("Activate object {}", o->name());
		post(&o, scheme::Activate { &o });
	}
}

int Processor::_close(bool force)
{
	_log.info("Close processor");
	for (auto & o : _objects) {
		decay(&o);
	}
	if (!force) return 0;

	_log.info("Close objects");
	for (auto c = _objects.rbegin(); c != _objects.rend(); c++)
		(*c)->close();

	_log.info("Close workers");
	for (auto & w : _workers_ptr)
		w->close();

	_ipc->close();
	loop.stop = true;

	return Base<Processor>::_close();
}
