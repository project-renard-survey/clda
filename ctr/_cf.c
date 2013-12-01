#include <Python.h>
#include <numpy/arrayobject.h>
#include "blas.h"

struct module_state {
    PyObject *error;
};

#if PY_MAJOR_VERSION >= 3
#define GETSTATE(m) ((struct module_state*)PyModule_GetState(m))
#else
#define GETSTATE(m) (&_state)
static struct module_state _state;
#endif

#define PARSE_ARRAY(o) (PyArrayObject*) PyArray_FROM_OTF(o, NPY_DOUBLE, \
        NPY_IN_ARRAY)

static PyObject *cf_update (PyObject *self, PyObject *args)
{
    PyObject *V_obj, *U_obj, *user_items;
    if (!PyArg_ParseTuple(args, "OOO", &U_obj, &V_obj, &user_items))
        return NULL;

    PyArrayObject *V_array = PARSE_ARRAY(V_obj),
                  *U_array = PARSE_ARRAY(U_obj);
    if (U_array == NULL || V_array == NULL) goto fail;

    int nusers = (int)PyArray_DIM(U_array, 0),
        nitems = (int)PyArray_DIM(V_array, 0),
        ntopics = (int)PyArray_DIM(V_array, 1);
    if (ntopics != (int)PyArray_DIM(U_array, 1) ||
        nusers != (int)PyList_Size(user_items)) {
        PyErr_SetString(PyExc_ValueError, "Dimension mismatch");
        goto fail;
    }

    double *U = PyArray_DATA(U_array),
           *V = PyArray_DATA(V_array),
           *UTU = malloc(ntopics*ntopics*sizeof(double)),
           *VTV = malloc(ntopics*ntopics*sizeof(double));
    char n = 'N', t = 'T';
    double one = 1.0, zero = 0.0;

    // Pre-compute U^T.U.
    dgemm_(&n, &t, &ntopics, &ntopics, &nusers, &one, U, &ntopics, U,
           &ntopics, &zero, UTU, &ntopics);

    int i, j, k, l, uid, ione = 1, *ipiv = malloc(ntopics*sizeof(int)), info;
    long el;
    double val, alpha = 40.0, l2u = 0.1;
    for (uid = 0; uid < nusers; ++uid) {
        PyObject *items = PyList_GetItem(user_items, uid);
        l = (int)PyList_Size(items);

        double *m = malloc(ntopics*ntopics*sizeof(double)),
               *b = malloc(ntopics*sizeof(double));
        for (i = 0; i < ntopics*ntopics; ++i) m[i] = V[i];
        for (i = 0; i < ntopics; ++i) b[i] = 0.0;

        for (i = 0; i < l; ++i) {
            el = PyInt_AsLong(PyList_GetItem(items, i));
            for (j = 0; j < ntopics; ++j) {
                b[j] += alpha*V[el*ntopics+j];
                m[j*ntopics+j] += alpha*V[el*ntopics+j]*V[el*ntopics+j]+l2u;
                for (k = j+1; j < ntopics; ++j) {
                    val = alpha * V[el*ntopics+j] * V[el*ntopics+k];
                    m[j*ntopics+k] += val;
                    m[k*ntopics+j] += val;
                }
            }
        }

        for (i = 0; i < ntopics; ++i) printf("%f ", b[i]);
        printf("\n");
        dgesv_(&ntopics, &ione, m, &ntopics, ipiv, b, &ntopics, &info);
        printf("info = %d\n", info);
        for (i = 0; i < ntopics; ++i) printf("%f ", b[i]);
        printf("\n");

        free(b);
        free(m);
    }

    free(ipiv);
    free(UTU);
    free(VTV);
    Py_DECREF(V_array);
    Py_DECREF(U_array);

    Py_INCREF(Py_None);
    return Py_None;

fail:

    Py_XDECREF(V_array);
    Py_XDECREF(U_array);

    return NULL;
}

static PyMethodDef cf_methods[] = {
    {"update", (PyCFunction)cf_update, METH_VARARGS, ""},
    {NULL, NULL, 0, NULL}
};

#if PY_MAJOR_VERSION >= 3

static int cf_traverse(PyObject *m, visitproc visit, void *arg) {
    Py_VISIT(GETSTATE(m)->error);
    return 0;
}

static int cf_clear(PyObject *m) {
    Py_CLEAR(GETSTATE(m)->error);
    return 0;
}

static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "_cf",
    NULL,
    sizeof(struct module_state),
    cf_methods,
    NULL,
    cf_traverse,
    cf_clear,
    NULL
};

#define INITERROR return NULL

PyObject *PyInit__cf(void)
#else
#define INITERROR return

void init_cf(void)
#endif
{
#if PY_MAJOR_VERSION >= 3
    PyObject *module = PyModule_Create(&moduledef);
#else
    PyObject *module = Py_InitModule("_cf", cf_methods);
#endif

    if (module == NULL)
        INITERROR;
    struct module_state *st = GETSTATE(module);

    st->error = PyErr_NewException("_cf.Error", NULL, NULL);
    if (st->error == NULL) {
        Py_DECREF(module);
        INITERROR;
    }

    import_array();

#if PY_MAJOR_VERSION >= 3
    return module;
#endif
}
