#include <dlfcn.h>
#include <errno.h>
#include <Python.h>

#include "tll/channel/module.h"
#include "tll/config.h"

#include "context_api.h"

static int pyref = 0;
static PyThreadState * pystate = NULL;

static int pyinit(struct tll_channel_module_t * m, tll_channel_context_t * ctx, const tll_config_t * cfg)
{
	tll_logger_t * log = tll_logger_new("tll.python", -1);

	if (!Py_IsInitialized()) {
		tll_logger_printf(log, TLL_LOGGER_INFO, "Initialize embedded Python interpreter");
		Py_InitializeEx(0);

#if PY_VERSION_HEX < 0x03070000
		PyEval_InitThreads();
#endif

		/*
		 * After initialization GIL is locked by current thread.
		 * Without releasing current state this leads to deadlock when other thread tries to take GIL
		 */
		pystate = PyEval_SaveThread();
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
	tll_logger_t * log = tll_logger_new("tll.python", -1);
	if (--pyref == 0) {
		tll_logger_printf(log, TLL_LOGGER_INFO, "Finalize embedded Python interpreter");
		PyEval_RestoreThread(pystate);
		pystate = NULL;
		Py_FinalizeEx();
	}

	tll_logger_free(log);
	return 0;
}

static int pychannel_init(tll_channel_t * c, const tll_config_t * url, tll_channel_t * parent, tll_channel_context_t * ctx)
{
	tll_logger_t * log = tll_logger_new("tll.python", -1);

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
static tll_channel_impl_t prefix_impl = {.init = pychannel_init, .name = "python+" };

static tll_channel_impl_t *channels[] = { &python_impl, &prefix_impl, NULL };

struct tll_channel_module_t mod = {
	.version = TLL_CHANNEL_MODULE_VERSION,
	.impl = channels,
	.flags = TLL_CHANNEL_MODULE_DLOPEN_GLOBAL,
	.init = pyinit,
	.free = pyfree,
};

tll_channel_module_t * tll_channel_module()
{
	return &mod;
}
