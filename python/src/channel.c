#include <dlfcn.h>
#include <errno.h>
#include <Python.h>

#include "tll/channel/impl.h"
#include "tll/config.h"

#include "context_api.h"

static int pyref = 0;

static int pyinit(struct tll_channel_module_t * m, tll_channel_context_t * ctx)
{
	tll_logger_t * log = tll_logger_new("tll.channel.python", -1);

	if (!Py_IsInitialized()) {
		tll_logger_printf(log, TLL_LOGGER_INFO, "Initialize embedded Python interpreter");
		Py_InitializeEx(0);
#if PY_VERSION_HEX < 0x03070000
		PyEval_InitThreads();
#endif
	} else if (!pyref) {
		tll_logger_printf(log, TLL_LOGGER_INFO, "Loaded with external interpreter, disable finalization");
		pyref++;
	}

	if (!tll_pychannel_lookup) {
		PyGILState_STATE state = PyGILState_Ensure();
		tll_logger_printf(log, TLL_LOGGER_DEBUG, "Loading python channel implementation '%s'", "tll.channel.context");
		if (import_tll__channel__context()) {
			if (PyErr_Occurred()) {
				PyErr_PrintEx(0);
			}
			PyGILState_Release(state);
			tll_logger_printf(log, TLL_LOGGER_ERROR, "Failed to load tll.channel.context module");
			return EINVAL;
		}
		PyGILState_Release(state);
	}
	pyref++;

	tll_logger_free(log);
	return 0;
}

static int pyfree(struct tll_channel_module_t * m, tll_channel_context_t * ctx)
{
	tll_logger_t * log = tll_logger_new("tll.channel.python", -1);
	if (--pyref == 0) {
		tll_logger_printf(log, TLL_LOGGER_INFO, "Finalize embedded Python interpreter");
		Py_FinalizeEx();
	}

	tll_logger_free(log);
	return 0;
}

static int pychannel_init(tll_channel_t * c, const tll_config_t * url, tll_channel_t * parent, tll_channel_context_t * ctx)
{
	tll_logger_t * log = tll_logger_new("tll.channel.python", -1);

	const char * module = tll_config_get_copy(url, "python", -1, NULL);
	if (!module) {
		tll_logger_printf(log, TLL_LOGGER_ERROR, "Missing 'python' parameter");
		tll_logger_free(log);
		return ENOENT;
	}

	tll_logger_printf(log, TLL_LOGGER_DEBUG, "Loading python module '%s'", module);
	c->impl = (*tll_pychannel_lookup)(module);

	tll_config_value_free(module);
	tll_logger_free(log);

	if (c->impl)
		return EAGAIN;
	return ENOENT;
}

static tll_channel_impl_t python_impl = {.init = pychannel_init, .name = "python" };

static tll_channel_impl_t *channels[] = { &python_impl, NULL };

struct tll_channel_module_t channel_module = {
	.impl = channels,
	.flags = TLL_CHANNEL_MODULE_DLOPEN_GLOBAL,
	.init = pyinit,
	.free = pyfree,
};
