/* SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause */
/* Copyright (c) 2026 Eremey Valetov. See LICENSES/BSD-2-Clause.txt. */
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <limits.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <uc2/libuc2.h>
#include <uc2/uc2_path.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

struct module_state { PyObject *bad_archive, *resource_limit; };
typedef struct {
    PyObject_HEAD
    PyObject *source;
    uc2_handle handle;
    struct uc2_entry *entries;
    Py_ssize_t count, capacity, max_entries;
    size_t allocated, memory_limit;
    int busy, allocation_refused, initialized;
} Reader;

/* Record allocation sizes without reducing malloc's alignment. */
union allocation { size_t size; long double alignment; void *pointer; };
static void *allocate(void *ctx, unsigned size)
{
    Reader *self = ctx;
    if (size > self->memory_limit - self->allocated
#if SIZE_MAX <= UINT_MAX
        || (size_t)size > SIZE_MAX - sizeof(union allocation)
#endif
    ) {
        self->allocation_refused = 1;
        return NULL;
    }
    union allocation *p = malloc(sizeof *p + size);
    if (!p) { PyErr_NoMemory(); return NULL; }
    p->size = size; self->allocated += size;
    return p + 1;
}
static void release(void *ctx, void *data)
{
    if (!data) return;
    Reader *self = ctx;
    union allocation *p = (union allocation *)data - 1;
    self->allocated -= p->size;
    free(p);
}
static int read_at(void *ctx, unsigned pos, void *buffer, unsigned len)
{
    Reader *self = ctx;
    if (PyErr_Occurred() || PyErr_CheckSignals() < 0) return -1;
    if (len > INT_MAX) len = INT_MAX;
    PyObject *result = PyObject_CallMethod(self->source, "seek", "KI",
                                          (unsigned long long)pos, 0U);
    if (!result) return -1;
    Py_DECREF(result);
    result = PyObject_CallMethod(self->source, "read", "I", len);
    if (!result) return -1;
    if (!PyBytes_Check(result) || PyBytes_GET_SIZE(result) > (Py_ssize_t)len) {
        PyErr_SetString(PyExc_TypeError, "archive read() must return bytes, at most the requested length");
        Py_DECREF(result); return -1;
    }
    int got = (int)PyBytes_GET_SIZE(result);
    memcpy(buffer, PyBytes_AS_STRING(result), (size_t)got);
    Py_DECREF(result);
    return got;
}
static struct uc2_io io = { read_at, allocate, release, NULL };
static void set_error(Reader *self, int result)
{
    if (PyErr_Occurred()) return;
    struct module_state *state = PyType_GetModuleState(Py_TYPE(self));
    if (self->allocation_refused)
        PyErr_SetString(state->resource_limit, "decoder memory limit exceeded");
    else
        PyErr_SetString(state->bad_archive,
            self->handle ? uc2_message(self->handle, result) : "invalid UC2 archive");
}
static void dispose(Reader *self)
{
    if (self->handle) self->handle = uc2_close(self->handle);
    free(self->entries); self->entries = NULL;
    self->count = self->capacity = 0;
    Py_CLEAR(self->source);
}
static int reader_traverse(Reader *self, visitproc visit, void *arg)
{ Py_VISIT(self->source); Py_VISIT(Py_TYPE(self)); return 0; }
static int reader_clear(Reader *self) { dispose(self); return 0; }
static void reader_dealloc(Reader *self)
{
    PyTypeObject *type = Py_TYPE(self);
    PyObject_GC_UnTrack(self);
    dispose(self);
    type->tp_free((PyObject *)self);
    Py_DECREF(type);
}
static int reader_init(Reader *self, PyObject *args, PyObject *kwargs)
{
    PyObject *source;
    Py_ssize_t maximum = 100000;
    long long memory = 64 * 1024 * 1024;
    static char *names[] = {"source", "max_entries", "max_decoder_memory", NULL};
    if (self->initialized) {
        PyErr_SetString(PyExc_RuntimeError, "reader cannot be initialized twice"); return -1;
    }
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|nL", names, &source, &maximum, &memory))
        return -1;
    if (maximum < 1 || memory < 1 || (unsigned long long)memory > SIZE_MAX) {
        PyErr_SetString(PyExc_ValueError, "reader limits must be positive and fit this platform"); return -1;
    }
    self->initialized = 1; self->max_entries = maximum;
    self->memory_limit = (size_t)memory;
    Py_INCREF(source); self->source = source; self->busy = 1;
    self->handle = uc2_open(&io, self);
    if (!self->handle) { set_error(self, UC2_Damaged); goto fail; }
    for (;;) {
        struct uc2_entry entry;
        memset(&entry, 0, sizeof entry);
        int result = uc2_read_cdir(self->handle, &entry);
        if (result == UC2_End) break;
        if (result < 0) { set_error(self, result); goto fail; }
        while (result == UC2_TaggedEntry) {
            result = uc2_get_tag(self->handle, &entry, NULL, NULL, NULL);
            if (result < 0) { set_error(self, result); goto fail; }
        }
        if (self->count >= maximum) {
            struct module_state *state = PyType_GetModuleState(Py_TYPE(self));
            PyErr_SetString(state->resource_limit, "archive entry limit exceeded"); goto fail;
        }
        if (!memchr(entry.name, 0, sizeof entry.name)) { set_error(self, UC2_Damaged); goto fail; }
        if (self->count == self->capacity) {
            Py_ssize_t next = self->capacity ? self->capacity : 16;
            if (next > maximum / 2) next = maximum;
            else next *= 2;
            if ((size_t)next > SIZE_MAX / sizeof entry) { PyErr_NoMemory(); goto fail; }
            void *grown = realloc(self->entries, (size_t)next * sizeof entry);
            if (!grown) { PyErr_NoMemory(); goto fail; }
            self->entries = grown; self->capacity = next;
        }
        self->entries[self->count++] = entry;
    }
    int result = uc2_finish_cdir(self->handle, NULL);
    if (result < 0 || PyErr_Occurred()) { set_error(self, result); goto fail; }
    self->busy = 0;
    return 0;
fail:
    self->busy = 0; dispose(self); return -1;
}
static int ready(Reader *self)
{
    if (self->busy) { PyErr_SetString(PyExc_RuntimeError, "reader is already in use"); return 0; }
    if (!self->handle) { PyErr_SetString(PyExc_ValueError, "archive is closed"); return 0; }
    return 1;
}
static PyObject *reader_close(Reader *self, PyObject *unused)
{
    if (self->busy) { PyErr_SetString(PyExc_RuntimeError, "reader is already in use"); return NULL; }
    dispose(self); Py_RETURN_NONE;
}
static PyObject *reader_entries(Reader *self, PyObject *unused)
{
    if (!ready(self)) return NULL;
    PyObject *list = PyList_New(self->count);
    if (!list) return NULL;
    for (Py_ssize_t i = 0; i < self->count; i++) {
        struct uc2_entry *e = &self->entries[i];
        PyObject *name = PyUnicode_DecodeUTF8(e->name, (Py_ssize_t)strlen(e->name), "strict");
        if (!name) { Py_DECREF(list); return NULL; }
        PyObject *row = Py_BuildValue("(IIiNIIIII)", e->dirid, e->id, (int)e->is_dir,
            name, e->size, e->csize, e->dos_time, (unsigned)e->attr, (unsigned)e->xi.method);
        if (!row) { Py_DECREF(list); return NULL; }
        PyList_SET_ITEM(list, i, row);
    }
    return list;
}
struct output { PyObject *writer; char *buffer; size_t size, written; Reader *reader; };
static int write_data(void *ctx, const void *data, unsigned len)
{
    struct output *out = ctx;
    if (PyErr_Occurred() || PyErr_CheckSignals() < 0) return -1;
    if (len > out->size - out->written) {
        struct module_state *state = PyType_GetModuleState(Py_TYPE(out->reader));
        PyErr_SetString(state->bad_archive, "decoder exceeded the declared size"); return -1;
    }
    if (out->buffer) memcpy(out->buffer + out->written, data, len);
    if (out->writer && out->writer != Py_None) {
        PyObject *bytes = PyBytes_FromStringAndSize(data, len);
        if (!bytes) return -1;
        PyObject *result = PyObject_CallOneArg(out->writer, bytes);
        Py_DECREF(bytes);
        if (!result) return -1;
        Py_ssize_t count = PyLong_AsSsize_t(result);
        Py_DECREF(result);
        if (PyErr_Occurred()) return -1;
        if (count != (Py_ssize_t)len) {
            PyErr_SetString(PyExc_OSError, "output write() did not consume the full block"); return -1;
        }
    }
    out->written += len;
    return 0;
}
static struct uc2_entry *get_entry(Reader *self, Py_ssize_t index, unsigned long long limit)
{
    if (!ready(self)) return NULL;
    if (index < 0 || index >= self->count) {
        PyErr_SetString(PyExc_IndexError, "entry index out of range"); return NULL;
    }
    struct uc2_entry *entry = &self->entries[index];
    if (entry->is_dir) { PyErr_SetString(PyExc_IsADirectoryError, entry->name); return NULL; }
    if (entry->size > limit) {
        struct module_state *state = PyType_GetModuleState(Py_TYPE(self));
        PyErr_SetString(state->resource_limit, "entry size exceeds the configured limit"); return NULL;
    }
    return entry;
}
static int extract(Reader *self, struct uc2_entry *entry, struct output *out)
{
    self->busy = 1; self->allocation_refused = 0;
    int result = uc2_extract(self->handle, &entry->xi, entry->size, write_data, out);
    self->busy = 0;
    if (result < 0 || PyErr_Occurred()) { set_error(self, result); return -1; }
    if (out->written != entry->size) { set_error(self, UC2_Truncated); return -1; }
    return 0;
}
static PyObject *reader_read(Reader *self, PyObject *args)
{
    Py_ssize_t index;
    long long limit;
    if (!PyArg_ParseTuple(args, "nL", &index, &limit)) return NULL;
    if (limit < 1) { PyErr_SetString(PyExc_ValueError, "read limit must be positive"); return NULL; }
    struct uc2_entry *entry = get_entry(self, index, limit);
    if (!entry) return NULL;
#if SIZE_MAX <= UINT_MAX
    if (entry->size > (unsigned)PY_SSIZE_T_MAX) return PyErr_NoMemory();
#endif
    PyObject *bytes = PyBytes_FromStringAndSize(NULL, (Py_ssize_t)entry->size);
    if (!bytes) return NULL;
    struct output out = { NULL, PyBytes_AS_STRING(bytes), entry->size, 0, self };
    if (extract(self, entry, &out) < 0) { Py_DECREF(bytes); return NULL; }
    return bytes;
}
static PyObject *reader_copy(Reader *self, PyObject *args)
{
    Py_ssize_t index;
    PyObject *writer;
    long long limit;
    if (!PyArg_ParseTuple(args, "nOL", &index, &writer, &limit)) return NULL;
    if (limit < 1) { PyErr_SetString(PyExc_ValueError, "stream limit must be positive"); return NULL; }
    if (writer != Py_None && !PyCallable_Check(writer)) {
        PyErr_SetString(PyExc_TypeError, "writer must be callable or None"); return NULL;
    }
    struct uc2_entry *entry = get_entry(self, index, limit);
    if (!entry) return NULL;
    struct output out = { writer, NULL, entry->size, 0, self };
    if (extract(self, entry, &out) < 0) return NULL;
    return PyLong_FromSize_t(out.written);
}
static PyMethodDef methods[] = {
    {"close", (PyCFunction)reader_close, METH_NOARGS, NULL},
    {"entries", (PyCFunction)reader_entries, METH_NOARGS, NULL},
    {"read", (PyCFunction)reader_read, METH_VARARGS, NULL},
    {"copy_to", (PyCFunction)reader_copy, METH_VARARGS, NULL}, {NULL, NULL, 0, NULL}
};
static PyType_Slot slots[] = {
    {Py_tp_new, PyType_GenericNew}, {Py_tp_init, reader_init},
    {Py_tp_dealloc, reader_dealloc}, {Py_tp_traverse, reader_traverse},
    {Py_tp_clear, reader_clear}, {Py_tp_methods, methods}, {0, NULL}
};
static PyType_Spec spec = { "uc2._native.Reader", sizeof(Reader), 0,
                           Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC, slots };
static int module_traverse(PyObject *module, visitproc visit, void *arg)
{
    struct module_state *state = PyModule_GetState(module);
    Py_VISIT(state->bad_archive); Py_VISIT(state->resource_limit); return 0;
}
static int module_clear(PyObject *module)
{
    struct module_state *state = PyModule_GetState(module);
    Py_CLEAR(state->bad_archive); Py_CLEAR(state->resource_limit); return 0;
}
int uc2_python_writer_init(PyObject *module);
/* A bounded buffer contains at least a maximum-sized chunk unless it is
 * the final buffer. Reuse the archive ingestion Gear boundaries exactly. */
#include "uc2/uc2_cdc.h"
static PyObject *chunk_length(PyObject *module, PyObject *args)
{
    (void)module;
    const char *data;
    Py_ssize_t length;
    if (!PyArg_ParseTuple(args, "y#", &data, &length)) return NULL;
    struct uc2_chunker chunker;
    size_t offset, size;
    uc2_chunker_init(&chunker, 13, 0, 0);
    uc2_chunker_next(&chunker, (const uint8_t *)data, (size_t)length, &offset, &size);
    return PyLong_FromSize_t(size);
}
static PyObject *open_path_impl(PyObject *module, const char *path)
{
    char error[256];
    FILE *stream;
    Py_BEGIN_ALLOW_THREADS
    stream=uc2_open_path(path,error,sizeof error);
    Py_END_ALLOW_THREADS
    if (!stream) {
        struct module_state *state=PyModule_GetState(module);
        PyErr_SetString(state->bad_archive,error);
        return NULL;
    }
#ifdef _WIN32
    /* The wheel uses a static CRT, whose descriptor table differs from
       CPython's CRT. Transfer a duplicated OS handle through Python's msvcrt. */
    PyObject *crt=PyImport_ImportModule("msvcrt");
    if (!crt) { fclose(stream);return NULL; }
    HANDLE duplicate=NULL;
    BOOL copied=DuplicateHandle(GetCurrentProcess(),(HANDLE)_get_osfhandle(_fileno(stream)),
                                GetCurrentProcess(),&duplicate,0,FALSE,DUPLICATE_SAME_ACCESS);
    DWORD error_code=copied ? 0 : GetLastError();
    fclose(stream);
    if (!copied) { Py_DECREF(crt);return PyErr_SetFromWindowsErr(error_code); }
    PyObject *handle=PyLong_FromVoidPtr(duplicate);
    PyObject *descriptor=handle ? PyObject_CallMethod(crt,"open_osfhandle","Oi",handle,
                                      _O_RDONLY|_O_BINARY|_O_NOINHERIT) : NULL;
    Py_XDECREF(handle);Py_DECREF(crt);
    if (!descriptor) CloseHandle(duplicate);
    return descriptor; /* Caller owns a CPython-CRT descriptor. */
#else
    int fd=dup(fileno(stream));
    int saved_errno=errno;
    fclose(stream);
    if (fd<0) { errno=saved_errno;return PyErr_SetFromErrno(PyExc_OSError); }
    PyObject *result=PyLong_FromLong(fd);
    if (!result) close(fd);
    return result;
#endif
}
static PyObject *open_path(PyObject *module, PyObject *args)
{
#ifdef _WIN32
    /* The Windows path opener explicitly converts UTF-8 to UTF-16. */
    const char *path;
    if (!PyArg_ParseTuple(args,"s",&path)) return NULL;
    return open_path_impl(module,path);
#else
    /* Preserve surrogateescaped filesystem bytes, including bytes paths. */
    PyObject *encoded=NULL;
    if (!PyArg_ParseTuple(args,"O&",PyUnicode_FSConverter,&encoded)) return NULL;
    PyObject *result=open_path_impl(module,PyBytes_AS_STRING(encoded));
    Py_DECREF(encoded);
    return result;
#endif
}
static PyMethodDef module_methods[] = {
    {"open_path", open_path, METH_VARARGS, "Return an owned OS/CPython-CRT descriptor for data-only archive input."},
    {"chunk_length", chunk_length, METH_VARARGS, "First Gear chunk length (8 KiB target)."},
    {NULL, NULL, 0, NULL}
};
static int module_exec(PyObject *module)
{
    struct module_state *state = PyModule_GetState(module);
    state->bad_archive = PyErr_NewException("uc2.BadArchiveError", PyExc_OSError, NULL);
    state->resource_limit = PyErr_NewException("uc2.ResourceLimitError", PyExc_ValueError, NULL);
    if (!state->bad_archive || !state->resource_limit) return -1;
    if (PyModule_AddObjectRef(module, "BadArchiveError", state->bad_archive) < 0 ||
        PyModule_AddObjectRef(module, "ResourceLimitError", state->resource_limit) < 0) return -1;
    PyObject *type = PyType_FromModuleAndSpec(module, &spec, NULL);
    if (!type) return -1;
    int result = PyModule_AddObject(module, "Reader", type);
    if (result < 0) Py_DECREF(type);
    return result < 0 ? result : uc2_python_writer_init(module);
}
static PyModuleDef_Slot module_slots[] = { {Py_mod_exec, module_exec}, {0, NULL} };
static struct PyModuleDef module_def = {
    PyModuleDef_HEAD_INIT, "_native", NULL, sizeof(struct module_state), module_methods,
    module_slots, module_traverse, module_clear, NULL
};
PyMODINIT_FUNC PyInit__native(void) { return PyModuleDef_Init(&module_def); }
