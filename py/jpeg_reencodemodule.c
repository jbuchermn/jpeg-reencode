#include <Python.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

#include "jpeg.h"

static PyObject* jpeg_reencode_reencode(PyObject* self, PyObject* args){
    PyObject* result = NULL;
    unsigned char* output_buffer = NULL;
    Py_BEGIN_ALLOW_THREADS;

    PyBytesObject* buffer;
    double factor;
    if(!PyArg_ParseTuple(args, "Sd", &buffer, &factor)){
        PyErr_SetString(PyExc_TypeError, "Invalid parameters");

        goto Return;
    }

    long size = PyBytes_Size(buffer);

    struct jpeg jpeg;
    int status = jpeg_init(&jpeg, size, PyBytes_AsString(buffer));
    if(status){
        PyErr_SetString(PyExc_TypeError, "Could not parse header");

        goto Return;
    }

    for(int i=0; i<jpeg.n_quantisation_tables; i++){
        jpeg_quantisation_table_init_recompress(jpeg.quantisation_tables[i], factor);
    }

    output_buffer = malloc(size);
    memset(output_buffer, 0, size);
    long bytes_header = jpeg_write_recompress_header(&jpeg, output_buffer, size);
    long bytes_scan = jpeg_reencode_huffman(&jpeg, output_buffer + bytes_header, size - bytes_header);

    result = PyBytes_FromStringAndSize(output_buffer, bytes_header + bytes_scan);
    if(!result){
        PyErr_SetString(PyExc_TypeError, "Could not create bytes");

        goto Return;
    }
    jpeg_destroy(&jpeg);

Return:
    Py_END_ALLOW_THREADS;
    free(output_buffer);
    return result;
}


static PyMethodDef jpeg_reencode_methods[] = {
    { "reencode",          &jpeg_reencode_reencode,     METH_VARARGS,   "" },
    { NULL, NULL, 0, NULL }
};

static struct PyModuleDef jpeg_reencode = {
    PyModuleDef_HEAD_INIT,
    "jpeg_reencode",
    "",
    -1,
    jpeg_reencode_methods
};

PyMODINIT_FUNC PyInit_jpeg_reencode(void){
    return PyModule_Create(&jpeg_reencode);
}
