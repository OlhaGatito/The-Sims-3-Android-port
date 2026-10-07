// Sims 3 stub library - missing S3E extensions
// Compiles into: lib/s3eAndroidJNI.so, lib/s3eVFS.so

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

// Stub for S3E Android JNI
int s3eAndroidJNIInitialize() {
    printf("STUB: s3eAndroidJNIInitialize()\n");
    return 1; // Success
}

void s3eAndroidJNITerminate() {
    printf("STUB: s3eAndroidJNITerminate()\n");
}

// Stub for S3E VFS (Virtual File System)
int s3eVFSInit() {
    printf("STUB: s3eVFSInit() - Using standard file operations\n");
    return 1; // Success
}

void s3eVFSTerm() {
    printf("STUB: s3eVFSTerm()\n");
}

// Common stubs
int s3eGetSystemProperty(int property) {
    switch(property) {
        case 0: // S3E_DEVICE_NAME
            return 1; // Simulate ARM device
        case 1: // S3_DEVICE_OS_VERSION
            return 1; // Android
        default:
            return 0;
    }
}

void s3eDeviceYield(int ms) {
    // Just yield to system
    usleep(ms * 1000);
}

// Export table for .so files
__attribute__((visibility("default"))) void* dlsym(void* handle, const char* symbol) {
    static void* (*original_dlsym)(void*, const char*) = NULL;
    
    if (!original_dlsym) {
        original_dlsym = dlsym(RTLD_NEXT, "dlsym");
    }
    
    // Stub specific symbols
    if (strcmp(symbol, "s3eAndroidJNIInitialize") == 0) {
        return (void*)s3eAndroidJNIInitialize;
    }
    if (strcmp(symbol, "s3eAndroidJNITerminate") == 0) {
        return (void*)s3eAndroidJNITerminate;
    }
    if (strcmp(symbol, "s3eVFSInit") == 0) {
        return (void*)s3eVFSInit;
    }
    if (strcmp(symbol, "s3eVFSTerm") == 0) {
        return (void*)s3eVFSTerm;
    }
    if (strcmp(symbol, "s3eGetSystemProperty") == 0) {
        return (void*)s3eGetSystemProperty;
    }
    if (strcmp(symbol, "s3eDeviceYield") == 0) {
        return (void*)s3eDeviceYield;
    }
    
    // Fall back to original dlsym
    return original_dlsym(handle, symbol);
}

__attribute__((visibility("default"))) int dlclose(void* handle) {
    return 0; // Success
}

__attribute__((visibility("default"))) void* dlopen(const char* filename, int flag) {
    static void* (*original_dlopen)(const char*, int) = NULL;
    
    if (!original_dlopen) {
        original_dlopen = dlsym(RTLD_NEXT, "dlopen");
    }
    
    // Intercept specific libraries
    if (filename && strstr(filename, "s3eAndroidJNI")) {
        printf("STUB: Loading s3eAndroidJNI stub\n");
        return original_dlopen(filename, flag);
    }
    if (filename && strstr(filename, "s3eVFS")) {
        printf("STUB: Loading s3eVFS stub\n");
        return original_dlopen(filename, flag);
    }
    
    // Fall back to original dlopen
    return original_dlopen(filename, flag);
}