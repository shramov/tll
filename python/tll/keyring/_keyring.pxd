# vim: sts=4 sw=4 et
# cython: language_level=3

cdef extern from "tll/keyring.h":
    cdef enum tll_keyring_id_t:
        TLL_KEYRING_THREAD
        TLL_KEYRING_PROCESS
        TLL_KEYRING_SESSION
        TLL_KEYRING_USER

    cdef int tll_keyring_read(const char * name, char ** buf, int keyring)
    cdef int tll_keyring_write(int keyring, const char * name, const char * body, int len)
    cdef int tll_keyring_load(int keyring, const char * filename)
    cdef int tll_keyring_new(const char * name, int parent)
    cdef int tll_keyring_unlink(int key, int parent)

    cdef int tll_keyring_read_ref(const char * keyref, int len, char ** buf, int compat)
