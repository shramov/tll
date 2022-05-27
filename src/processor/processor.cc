/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
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

#include "tll/scheme/channel/timer.h"

TLL_DECLARE_IMPL(tll::processor::_::Worker);

namespace tll {
template <>
struct CallbackT<tll::processor::_::Processor>
{
	template <typename ... Args> static int call(tll::processor::_::Processor *ptr, Args ... args) { return ptr->cb(args...); }
};
}

using namespace tll::processor::_;

void Processor::decay(Object * obj, bool root)
{
	if (obj->decay)
		return;
	if (!root)
		obj->decay = true;

	if (obj->reopen.next != tll::time::epoch) {
		_log.debug("Disable pending reopen of object {}", obj->name());
		pending_del(obj->reopen.next, obj);
		obj->reopen.next = {};
	}

	if (obj->state == state::Closed && !obj->opening && obj->ready_close()) {
		_log.debug("Object {} is already closed, decay not needed", obj->name());
		return;
	}

	_log.debug("Decay subtree of object {}", obj->name());
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

	tll::Config lcfg;
	lcfg.set("name", fmt::format("tll.processor.context.{}.loop", name));
	if (loop.init(lcfg))
		return _log.fail(EINVAL, "Failed to init processor loop");

	_root = url.copy();
	_root.set("sys", context().config());
	auto sub = _root.sub("processor");
	if (!sub)
		return _log.fail(EINVAL, "Empty processor config");
	_cfg = *sub;

	_ipc = context().channel(fmt::format("ipc://;mode=server;name={}/ipc;dump=no;tll.internal=yes", name));
	if (!_ipc.get())
		return _log.fail(EINVAL, "Failed to create IPC channel for processor");
	_ipc->callback_add(this, TLL_MESSAGE_MASK_DATA);
	_child_add(_ipc.get(), "ipc");
	loop.add(_ipc.get());

	_timer = context().channel(fmt::format("timer://;clock=realtime;name={}/timer;dump=no;tll.internal=yes", name));
	if (!_timer.get())
		return _log.fail(EINVAL, "Failed to create timer channel for processor");
	_timer->callback_add(pending_process, this, TLL_MESSAGE_MASK_DATA);
	_child_add(_timer.get(), "timer");
	loop.add(_timer.get());

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

int Processor::_open(const tll::ConstConfig &)
{
	loop.stop = false;
	if (_ipc->open())
		return _log.fail(EINVAL, "Failed to open IPC channel");
	if (_timer->open())
		return _log.fail(EINVAL, "Failed to open timer channel");
	return 0;
}

Worker * Processor::init_worker(std::string_view name)
{
	auto it = _workers.find(name);
	if (it != _workers.end())
		return it->second;

	tll::Channel::Url url;

	auto wcfg = _cfg.sub("worker");
	if (wcfg)
		wcfg = wcfg->sub(name);
	if (wcfg)
		url = wcfg->copy();

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

int Processor::init_one(PreObject &obj)
{
	auto & name = obj.name;
	auto log = _log.prefix("{} {}:", "object", std::string_view(name)); // TODO: name is moved and left empty without std::string_view wrapper
	log.debug("Init");

	auto wname = std::string(obj.config.get("worker").value_or("default"));
	auto w = init_worker(wname);
	if (!w)
		return log.fail(EINVAL, "Failed to init worker {}", wname);

	if (!obj.url.has("fd") && !w->loop._poll_enable)
		obj.url.set("fd", "no");

	auto channel = context().channel(obj.url);
	if (!channel)
		return log.fail(EINVAL, "Failed to create channel {}", conv::to_string(obj.url));
	_objects.emplace_back(std::move(channel));
	auto o = &_objects.back();
	o->worker = w;

	auto open = obj.config.sub("open");
	if (open)
		o->reopen.open_params = ConstConfig(*open);

	o->depends_names = {obj.depends_open.list.begin(), obj.depends_open.list.end()};
	if (o->init(obj.url))
		return log.fail(EINVAL, "Failed to init extra parameters");
	return 0;
}

std::optional<Processor::PreObject> Processor::init_pre(std::string_view extname, tll::ConstConfig &cfg)
{
	auto name = std::string(cfg.get("name").value_or(extname));
	auto log = _log.prefix("object {}:", std::string_view(name)); // TODO: name is moved and left empty without std::string_view wrapper
	log.debug("Parse dependencies (external name {})", extname);

	auto disable = cfg.getT("disable", false);
	if (!disable || *disable) {
		log.debug("Object is disabled");
		PreObject obj;
		obj.disabled = true;
		return obj;
	}

	auto url = cfg.getT<tll::Channel::Url>("url");
	if (!url)
		return log.fail(std::nullopt, "Failed to load url: {}", url.error());

	if (url->has("name"))
		return log.fail(std::nullopt, "Duplicate name parameter");
	url->set("name", name);

	for (auto & [k, c] : cfg.browse("channels.**")) {
		auto key = "tll.channel." + k.substr(strlen("channels."));
		if (url->has(key))
			return log.fail(std::nullopt, "Duplicate channel group '{}': in config and in url", key);
		url->set(key, *c.get());
	}

	PreObject obj = { *url, cfg, name };

	auto deps = cfg.get("depends");
	if (deps || !deps->empty()) {
		for (auto d : split<','>(*deps)) {
			auto n = tll::util::strip(d);
			if (n.empty())
				return log.fail(std::nullopt, "Empty dependency: '{}'", *deps);
			obj.depends_open.list.emplace(n);
		}
	}

	for (auto & [k, c] : obj.url.browse("tll.channel.**")) {
		auto deps = c.get();
		if (!deps || deps->empty())
			continue;
		for (auto d : split<','>(*deps)) {
			auto n = tll::util::strip(d);
			if (n.size() == 0)
				return log.fail(std::nullopt, "Empty channel in {}: '{}'", k, deps);
			obj.depends_init.list.emplace(n);
		}
	}

	auto master = obj.url.get("master");
	if (master)
		obj.depends_init.list.insert(std::string(*master));

	{
		std::list<std::string_view> tmp(obj.depends_init.list.begin(), obj.depends_init.list.end());
		log.debug("Init dependencies: {}", tmp);
	}
	{
		std::list<std::string_view> tmp(obj.depends_open.list.begin(), obj.depends_open.list.end());
		log.debug("Open dependencies: {}", tmp);
	}

	return obj;
}

int Processor::object_depth(std::map<std::string, PreObject, std::less<>> &map, PreObject &o, std::list<std::string_view> & path, bool init)
{
	std::string_view stage = init ? "Init" : "Open";

	auto obj = o.depends(init);
	if (obj.depth != -1)
		return obj.depth;

	auto it = path.begin();
	for (; it != path.end() && *it != o.name; it++) {}
	if (it != path.end()) {
		std::list<std::string_view> cycle = { it, path.end() };
		cycle.push_back(o.name);
		return _log.fail(-1, "{} dependency cycle detected: {}", stage, cycle);
	}

	path.push_back(o.name);

	int depth = 0;
	for (auto & d : obj.list) {
		auto di = map.find(d);
		if (di == map.end())
			return _log.fail(-1, "{} dependency for '{}' missing: '{}'", stage, o.name, d);
		auto dd = object_depth(map, di->second, path, init);
		if (dd == -1)
			return -1;
		depth = std::max(depth, dd + 1);
	}

	path.pop_back();
	obj.depth = depth;
	return depth;
}

int Processor::init_depends()
{
	std::map<std::string, PreObject, std::less<>> objects;

	std::list<std::string_view> order;

	for (auto & p : _cfg.browse("objects.*", true)) {
		auto obj = init_pre(p.first.substr(strlen("objects.")), p.second);
		if (!obj)
			return EINVAL;
		if (obj->disabled)
			continue;
		objects.emplace(obj->name, std::move(*obj));
	}

	int max_depth = 0;
	for (auto & [_, o] : objects) {
		std::list<std::string_view> path;
		if (o.depends_open.depth == -1)
			o.depends_open.depth = object_depth(objects, o, path, false);
		if (o.depends_open.depth == -1)
			return EINVAL;
		path.clear();
		if (o.depends_init.depth == -1)
			o.depends_init.depth = object_depth(objects, o, path, true);
		if (o.depends_init.depth == -1)
			return EINVAL;
		_log.debug("Object {} depth: init {}, open {}", o.name, o.depends_init.depth, o.depends_open.depth);
		max_depth = std::max(max_depth, o.depends_init.depth);
	}

	for (auto i = 0; i < max_depth + 1; i++) {
		for (auto & [k, o] : objects) {
			if (o.depends_init.depth == i)
				order.push_back(k);
		}
	}
	_log.debug("Init order: {}", order);

	for (auto & n : order) {
		if (init_one(objects[std::string(n)]))
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

	if (_timer) {
		loop.del(_timer.get());
		_timer.reset();
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
	o->state_prev = o->state;
	o->state = s;

	if (o->verbose)
		_log.info("Object {} state {}", c->name(), tll_state_str(s));
	o->on_state(s);
	switch (s) {
	case state::Opening:
		break;
	case state::Active:
		for (auto & d : o->rdepends) {
			if (d->ready_open()) {
				activate(*d);
			}
		}
		return;
	case state::Error:
		_log.debug("Deactivate failed object {}", o->name());
		post<scheme::Deactivate>( o, { o });
		break;
	case state::Closed:
		if (o->decay) {
			_log.debug("Clear decay of {}", o->name());
			o->decay = false;
		}

		if (!o->ready_close())
			decay(o, true);

		for (auto & d : o->depends) {
			_log.debug("Check dependency {} of {}", d->name(), o->name());
			if (d->decay && d->ready_close())
				post<scheme::Deactivate>(d, { d });
			else if (d->state == tll::state::Closed && !d->opening && d->ready_open())
				activate(*d);
		}

		if (state() != tll::state::Active)
			break;

		if (o->ready_open()) {
			using namespace std::chrono;
			if (o->reopen.next > tll::time::now()) {
				_log.info("Next open in {}", duration_cast<std::chrono::duration<double, std::ratio<1>>>(o->reopen.timeout()));
				pending_add(o->reopen.next, o);
			} else
				activate(*o);
		}
		break;
	default:
		break;
	}

	if (s == state::Closed && (state() == state::Closing || state() == state::Closed)) {
		_close_workers();
	}
}

void Processor::activate()
{
	state(state::Active);
	for (auto & o : _objects) {
		if (o.depends.size()) continue;
		activate(o);
	}
}

void Processor::activate(Object &o)
{
	_log.debug("Activate object {}", o->name());
	o.opening = true;
	o.reopen.next = {};
	post(&o, scheme::Activate { &o });
}

void Processor::_close_workers()
{
	if (std::all_of(_workers.begin(), _workers.end(), [](auto & w) { return w.second->proc.state == state::Closed; }))
		return;
	if (std::all_of(_objects.begin(), _objects.end(), [](auto & o) { return o.state == state::Closed; })) {
		_log.info("All objects closed, signal workers");
		for (auto & w : _workers)
			post(w.second->proc.addr, scheme::Exit {});
	}
}

int Processor::_close(bool force)
{
	_log.info("Close processor");
	for (auto & o : _objects) {
		decay(&o);
	}

	_close_workers();

	if (!force) return 0;

	_log.info("Close objects");
	for (auto c = _objects.rbegin(); c != _objects.rend(); c++)
		(*c)->close();

	_log.info("Close workers");
	for (auto & w : _workers_ptr)
		w->close();

	_ipc->close();
	_timer->close();
	loop.stop = true;

	return Base<Processor>::_close();
}

void Processor::pending_add(tll::time_point ts, Object * o)
{
	if (pending_has(ts, o))
		return;

	bool rearm = true;
	if (!_pending.empty())
		rearm = _pending.begin()->first > ts;

	_pending.insert(std::make_pair(ts, o));

	if (rearm) {
		_log.debug("New first element in pending list, rearm timer");
		pending_rearm(ts);
	}
}

void Processor::pending_del(const tll::time_point &ts, const Object * o)
{
	auto it = _pending.find(ts);
	if (it == _pending.end())
		return;
	while (it->first == ts) {
		if (it->second == o)
			it = _pending.erase(it);
		else
			it++;
	}

	if (_pending.empty()) {
		_log.debug("Pending list empty, disable timer");
		pending_rearm({});
	} else if (_pending.begin()->first > ts) {
		_log.debug("First element of pending list removed, shift timer");
		pending_rearm(_pending.begin()->first);
	}
}

int Processor::pending_rearm(const tll::time_point &ts)
{
	timer_scheme::absolute m = { ts };
	tll_msg_t msg = {};
	msg.type = TLL_MESSAGE_DATA;
	msg.msgid = m.id;
	msg.data = &m;
	msg.size = sizeof(m);
	if (_timer->post(&msg))
		return _log.fail(EINVAL, "Failed to rearm timer");
	return 0;
}

int Processor::pending_process(const tll_msg_t * msg)
{
	auto now = tll::time::now();
	for (auto it = _pending.begin(); it != _pending.end() && it->first < now; it = _pending.erase(it)) {
		auto o  = it->second;
		_log.debug("Pending action on {}", o->name());
		if (o->state == tll::state::Closed) {
			activate(*o);
		}
	}

	if (!_pending.empty()) {
		_log.debug("Shift timer");
		pending_rearm(_pending.begin()->first);
	}
	return 0;
}
