#include <Python.h>

int C2PyFunc(char *funcname, const char* msg, ...)
{
    va_list	argptr;
    char	msg_cpy[1024];

    va_start(argptr, msg);
	vsprintf(msg_cpy, msg, argptr);
	va_end(argptr);

    PyObject *pName, *pModule, *pDict, *pFunc;

    // Initialize the Python Interpreter
    Py_Initialize();

    // Build the name object
    pName = PyUnicode_FromString(funcname);

    // Load the module object
    pModule = PyImport_Import(pName);

    // pDict is a borrowed reference 
    pDict = PyModule_GetDict(pModule);

    // pFunc is also a borrowed reference 
    pFunc = PyDict_GetItemString(pDict, msg_cpy);

    if (PyCallable_Check(pFunc)) 
    {
        PyObject_CallObject(pFunc, NULL);
    } else 
    {
        PyErr_Print();
    }

    // Clean up
    Py_DECREF(pModule);
    Py_DECREF(pName);

    // Finish the Python Interpreter
    Py_Finalize();

    return 0;
}