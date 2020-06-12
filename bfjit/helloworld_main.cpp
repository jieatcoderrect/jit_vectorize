#include <iostream>
#include <dlfcn.h>

constexpr size_t TAPE_SIZE = 30000;

int main() {
    uint8_t *tape = new uint8_t[TAPE_SIZE];
    char* error;
    void (*fn)(uint8_t *);

    // load library
    void *lib_handle = dlopen("jitfunc_1804289383.so", RTLD_LAZY);
    if (!lib_handle)
    {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }

    // obtain the function handler
    *(void **)(&fn) = dlsym(lib_handle, "jitfunc_1804289383");
    if ((error = dlerror()) != NULL)
    {
        fprintf(stderr, "%s\n", error);
        exit(1);
    }

    (*fn)(tape);

    delete []tape;

    // release library
    dlclose(lib_handle);

    return 0;
}