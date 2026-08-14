#ifndef _TLL_KEYRING_H
#define _TLL_KEYRING_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Special keyring constants, same as in keyutils.h
enum tll_keyring_id_t
{
	TLL_KEYRING_THREAD = -1,
	TLL_KEYRING_PROCESS = -2,
	TLL_KEYRING_SESSION = -3,
	TLL_KEYRING_USER = -4,
	TLL_KEYRING_USER_SESSION = -5,
};

struct tll_channel_context_t;
int tll_channel_context_keyring(const struct tll_channel_context_t *);

/** Read one key from the keyring, wrapper around request_key and keyctl_read_alloc
 * @param name name of the key
 * @param buf output buffer, should be deallocated with `free`
 * @param keyring target keyring where key should be linked, @see request_key(2)
 * @return size of key on success (may be zero), negative errno on error
 */
int tll_keyring_read(const char * name, char ** buf, int keyring);

/** Write one key into the keyring, wrapper around add_key
 * @param keyring target keyring where key will be added, @see add_key(2)
 * @param name name of the key
 * @param body body of the key
 * @param len length of the body, if -1 then strlen(body) is used
 * @return positive key id on success, -errno on error
 */
int tll_keyring_write(int keyring, const char * name, const char * body, int len);

/// Load keys from the file into keyring
int tll_keyring_load(int, const char * filename);

/** Create new keyring
 *
 * @param name name of the keyring
 * @param parent parent keyring
 * @return positive keyring id on success, -errno on error
 */
int tll_keyring_new(const char * name, int parent);

/** Unlink keyring from parent
 *
 * @param keyring keyring to unlink
 * @param parent parent keyring id
 * @return 0 on success, -errno on error
 */
int tll_keyring_unlink(int key, int parent);

/** Parse and read key ref
 * @param keyref reference string in format method:data, method may be one of `data` or `key`
 * @param len keyref length, if -1 then strlen(keyref) is used
 * @param buf output buffer, should be deallocated with `free`
 * @return length of the key on success, -errno on error
 */
int tll_keyring_read_ref(const char * keyref, int len, char ** buf);

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

#include <string>

#include "tll/compat/expected.h"
#include "tll/util/secret.h"
#include "tll/util/errno.h"

namespace tll::keyring {

using tll::compat::expected;
using tll::compat::unexpected;
using tll::util::Secret;
using tll::util::Errno;

static constexpr auto Thread = TLL_KEYRING_THREAD;
static constexpr auto Process = TLL_KEYRING_PROCESS;
static constexpr auto Session = TLL_KEYRING_SESSION;
static constexpr auto User = TLL_KEYRING_USER;

inline expected<Secret, Errno> read(const std::string &name, int keyring = 0)
{
	char * buf = nullptr;
	auto r = tll_keyring_read(name.c_str(), &buf, keyring);
	if (r < 0)
		return unexpected(Errno(-r));
	return Secret::consume(buf, r);
}

inline Errno write(int keyring, const std::string &name, std::string_view key)
{
	return -tll_keyring_write(keyring, name.c_str(), key.data(), key.size());
}

} // namespace tll::keyring

#endif//__cplusplus

#endif//_TLL_KEYRING_H
