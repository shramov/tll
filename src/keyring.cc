#include "tll/config.h"
#include "tll/keyring.h"
#include "tll/util/keyref.h"

#include "build-config.h"

#undef WITH_KEYUTILS
#ifdef WITH_KEYUTILS
// Old versions lack extern "C" guard
extern "C" {
#include <keyutils.h>
}
#else
#include <atomic>
#include <map>
#include <memory>

using tll::util::Secret;
struct Keyring
{
	std::map<std::string, Secret, std::less<>> keys;
	std::map<int, Keyring> keyrings;

	const Secret * lookup(std::string_view name) const
	{
		if (auto it = keys.find(name); it != keys.end())
			return &it->second;
		for (auto &[_, kr]: keyrings) {
			if (auto r = kr.lookup(name); r)
				return r;
		}
		return nullptr;
	}

	Keyring * keyring(int id)
	{
		if (auto it = keyrings.find(id); it != keyrings.end())
			return &it->second;
		for (auto &[_, kr]: keyrings) {
			if (auto r = kr.keyring(id); r)
				return r;
		}
		return nullptr;
	}
};

std::atomic<int> _tll_keyring_id = 0;
static Keyring * _tll_keyrings()
{
	static std::unique_ptr<Keyring> _keyrings;
	if (!_keyrings) {
		_keyrings = std::make_unique<Keyring>();
		_keyrings->keyrings.emplace(tll::keyring::User, Keyring{});
		_keyrings->keyrings.emplace(tll::keyring::Session, Keyring{});
		_keyrings->keyrings.emplace(tll::keyring::Process, Keyring{});
		_keyrings->keyrings.emplace(tll::keyring::Thread, Keyring{});
	}
	return _keyrings.get();
}
#endif

int tll_keyring_read(const char * name, char ** buf, int kr)
{
	if (!name || !buf)
		return -EINVAL;
#ifdef WITH_KEYUTILS
	auto id = request_key("user", name, nullptr, kr);
	if (id < 0)
		return -errno;
	if (auto r = keyctl_read_alloc(id, (void **) buf); r < 0)
		return -errno;
	else
		return r;
#else
	if (auto r = _tll_keyrings()->lookup(name); r) {
		*buf = (char *) Secret(*r).release();
		return r->size();
	}
	return -ENOENT;
#endif
}

int tll_keyring_write(int kr, const char * name, const char * body, int len)
{
	if (!name || !body)
		return -EINVAL;
#ifdef WITH_KEYUTILS
	if (auto r = add_key("user", name, body, len < 0 ? strlen(body) : len, kr); r > 0)
		return r;
	return -errno;
#else
	if (auto r = _tll_keyrings()->keyring(kr); r) {
		r->keys.emplace(name, Secret(body, len));
		return ++_tll_keyring_id;
	}
	return -ENOENT;
#endif
}

int tll_keyring_new(const char * name, int parent)
{
	if (!name)
		return -EINVAL;
#ifdef WITH_KEYUTILS
	if (auto r = add_key("keyring", name, nullptr, 0, parent); r > 0)
		return r;
	return -errno;
#else
	if (auto r = _tll_keyrings()->keyring(parent); r) {
		auto id = ++_tll_keyring_id;
		r->keyrings.emplace(id, Keyring{});
		return id;
	}
	return -ENOENT;
#endif
}

int tll_keyring_unlink(int key, int parent)
{
#ifdef WITH_KEYUTILS
	if (keyctl_unlink(key, parent) < 0)
		return -errno;
	return 0;
#else
	if (auto r = _tll_keyrings()->keyring(parent); r) {
		if (auto it = r->keyrings.find(key); it != r->keyrings.end()) {
			r->keyrings.erase(it);
			return 0;
		}
		return -ENOENT;
	}
	return -ENOENT;
#endif
}

int tll_keyring_load(int keyring, const char * filename)
{
	// TODO: Replace with normal parser
	auto cfg = tll::Config::load("yaml", filename);
	if (!cfg)
		return -EINVAL;
	for (auto [k, c] : cfg->browse("**")) {
		auto v = c.get();
		if (!v)
			continue;
		if (auto r = tll_keyring_write(keyring, k.c_str(), v->data(), v->size()); r < 0)
			return r;
	}
	return 0;
}

int tll_keyring_read_ref(const char * name, int len, char ** buf, int compat)
{
	auto k = tll::util::KeyRef::parse({ name, (len < 0)?strlen(name):len }, compat);
	if (!k)
		return -EINVAL;
	auto r = k->read();
	if (!r)
		return -r.error();
	auto rlen = r->size();
	*buf = (char *) r->release();
	return rlen;
}
