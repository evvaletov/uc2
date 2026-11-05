/* SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause */
/* Copyright (c) 2026 Eremey Valetov. See LICENSES/BSD-2-Clause.txt. */
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <uc2/uc2_writer.h>

typedef struct {
    PyObject_HEAD
    uc2_writer *writer;
    int busy, initialized;
} Writer;
static int available(Writer *self)
{
    if (self->busy) { PyErr_SetString(PyExc_RuntimeError,"writer is already in use"); return 0; }
    if (!self->writer) { PyErr_SetString(PyExc_ValueError,"writer is closed"); return 0; }
    return 1;
}
static void set_error(Writer *self, int result)
{
    if (PyErr_Occurred()) return;
    PyObject *type=PyExc_OSError;
    if (result==UC2_WRITER_LIMIT) {
        PyObject *module=PyType_GetModule(Py_TYPE(self));
        PyObject *exception=PyObject_GetAttrString(module,"ResourceLimitError");
        if (exception) { PyErr_SetString(exception,uc2_writer_error(self->writer)); Py_DECREF(exception); }
        return;
    }
    if (result==UC2_WRITER_INVALID || result==UC2_BadState) type=PyExc_ValueError;
    PyErr_SetString(type,uc2_writer_error(self->writer));
}
static int writer_init(Writer *self, PyObject *args, PyObject *kwargs)
{
    int level=4; unsigned maximum=100000; long long total=UINT32_MAX;
    static char *names[]={"level","max_entries","max_total_size",NULL};
    if (self->initialized) { PyErr_SetString(PyExc_RuntimeError,"writer cannot be initialized twice"); return -1; }
    if (!PyArg_ParseTupleAndKeywords(args,kwargs,"|iIL",names,&level,&maximum,&total)) return -1;
    if (level<2 || level>9 || maximum<1 || maximum>1000000 || total<1 || total>UINT32_MAX) {
        PyErr_SetString(PyExc_ValueError,"invalid writer level or limits"); return -1;
    }
    struct uc2_writer_options options={level,maximum,(uint64_t)total};
    self->initialized=1; self->writer=uc2_writer_create(&options);
    if (!self->writer) { PyErr_SetString(PyExc_OSError,"cannot allocate writer or create temporary input file"); return -1; }
    return 0;
}
static void writer_dealloc(Writer *self)
{
    PyTypeObject *type=Py_TYPE(self);
    uc2_writer_free(self->writer);
    type->tp_free((PyObject *)self); Py_DECREF(type);
}
static PyObject *writer_abort(Writer *self, PyObject *unused)
{
    if (self->busy) { PyErr_SetString(PyExc_RuntimeError,"writer is already in use"); return NULL; }
    uc2_writer_free(self->writer); self->writer=NULL; Py_RETURN_NONE;
}
static PyObject *writer_directory(Writer *self, PyObject *args)
{
    unsigned parent,time,attr,id; const char *name;
    if (!available(self)) return NULL;
    if (!PyArg_ParseTuple(args,"IsII",&parent,&name,&time,&attr)) return NULL;
    int r=uc2_writer_add_directory(self->writer,parent,name,time,attr,&id);
    if (r<0) { set_error(self,r); return NULL; }
    return PyLong_FromUnsignedLong(id);
}
static int read_input(void *ctx, void *data, unsigned len)
{
    if (PyErr_Occurred() || PyErr_CheckSignals()<0) return -1;
    PyObject *value=PyObject_CallMethod((PyObject *)ctx,"read","I",len);
    if (!value) return -1;
    if (!PyBytes_Check(value) || PyBytes_GET_SIZE(value)>(Py_ssize_t)len) {
        Py_DECREF(value); PyErr_SetString(PyExc_TypeError,"input read() must return bytes, at most the requested length"); return -1;
    }
    int n=(int)PyBytes_GET_SIZE(value);
    memcpy(data,PyBytes_AS_STRING(value),(size_t)n); Py_DECREF(value); return n;
}
static PyObject *writer_file(Writer *self, PyObject *args)
{
    unsigned parent,time,attr; const char *name; PyObject *size,*source;
    if (!available(self)) return NULL;
    if (!PyArg_ParseTuple(args,"IsIIOO",&parent,&name,&time,&attr,&size,&source)) return NULL;
    uint64_t length=UC2_WRITER_UNKNOWN_SIZE;
    if (size!=Py_None) {
        length=PyLong_AsUnsignedLongLong(size); if (PyErr_Occurred()) return NULL;
        if (length>UINT32_MAX) { PyErr_SetString(PyExc_ValueError,"file size exceeds UINT32_MAX"); return NULL; }
    }
    self->busy=1;
    int r=uc2_writer_add_file(self->writer,parent,name,time,attr,length,read_input,source);
    self->busy=0;
    if (r<0 || PyErr_Occurred()) { set_error(self,r); return NULL; }
    Py_RETURN_NONE;
}
static int write_output(void *ctx, const void *data, unsigned len)
{
    if (PyErr_Occurred() || PyErr_CheckSignals()<0) return -1;
    PyObject *value=PyObject_CallMethod((PyObject *)ctx,"write","y#",data,(Py_ssize_t)len);
    if (!value) return -1;
    Py_ssize_t n=PyLong_AsSsize_t(value); Py_DECREF(value);
    if (PyErr_Occurred()) return -1;
    if (n!=(Py_ssize_t)len) { PyErr_SetString(PyExc_OSError,"output write() did not accept the full buffer"); return -1; }
    return 0;
}
static PyObject *writer_finish(Writer *self, PyObject *destination)
{
    if (!available(self)) return NULL;
    self->busy=1;
    int r=uc2_writer_finish(self->writer,write_output,destination);
    self->busy=0;
    if (r<0 || PyErr_Occurred()) { set_error(self,r); return NULL; }
    struct uc2_writer_stats stats; uc2_writer_get_stats(self->writer,&stats);
    uc2_writer_free(self->writer); self->writer=NULL;
    return Py_BuildValue("{sI,sI,sI,sI}","files",stats.files,"directories",stats.directories,
                         "masters",stats.masters,"archive_size",stats.archive_size);
}
static PyMethodDef methods[]={
    {"abort",(PyCFunction)writer_abort,METH_NOARGS,NULL},
    {"add_directory",(PyCFunction)writer_directory,METH_VARARGS,NULL},
    {"add_file",(PyCFunction)writer_file,METH_VARARGS,NULL},
    {"finish",(PyCFunction)writer_finish,METH_O,NULL}, {NULL,NULL,0,NULL}
};
static PyType_Slot slots[]={
    {Py_tp_new,PyType_GenericNew},{Py_tp_init,writer_init},{Py_tp_dealloc,writer_dealloc},
    {Py_tp_methods,methods},{0,NULL}
};
static PyType_Spec spec={"uc2._native.Writer",sizeof(Writer),0,Py_TPFLAGS_DEFAULT,slots};
int uc2_python_writer_init(PyObject *module)
{
    PyObject *type=PyType_FromModuleAndSpec(module,&spec,NULL);
    if (!type) return -1;
    int r=PyModule_AddObject(module,"Writer",type);
    if (r<0) Py_DECREF(type);
    return r;
}
