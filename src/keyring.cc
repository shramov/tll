#include "tll/config.h"
#include "tll/keyring.h"
#include "tll/util/keyref.h"

#include "scheme-config.h"

#ifdef WITH_KEYUTILS
// Old versions lack extern "C" guard
extern "C" {
#include <keyutils.h>
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
	return -ENOSYS;
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
	return -ENOSYS;
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
	return -ENOSYS;
#endif
}

int tll_keyring_unlink(int key, int parent)
{
#ifdef WITH_KEYUTILS
	if (keyctl_unlink(key, parent) < 0)
		return -errno;
	return 0;
#else
	return -ENOSYS;
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

int tll_keyring_read_ref(const char * name, int len, char ** buf)
{
	auto k = tll::util::KeyRef::parse({ name, (len < 0)?strlen(name):len });
	if (!k)
		return -EINVAL;
	auto r = k->read();
	if (!r)
		return -r.error();
	auto rlen = r->size();
	*buf = (char *) r->release();
	return rlen;
}
