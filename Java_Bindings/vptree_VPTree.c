// clang vptree_VPTree.c -shared -fPIC -lpthread -lm -I/usr/lib/jvm/java-11-openjdk-amd64/include -I/usr/lib/jvm/java-11-openjdk-amd64/include/linux
// sudo clang -Wall -Wextra -g -fsanitize=address -lpthread -lm -Ofast -shared -fPIC -I/usr/lib/jvm/java-11-openjdk-amd64/include -I/usr/lib/jvm/java-11-openjdk-amd64/include/linux vptree_VPTree.c -o /usr/lib/libJVPTree.so

#include "vptree_VPTree.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#define EXT_DEBUG 1
#if EXT_DEBUG

#define VPENTRY_CLASSPATH "vptree/VPEntry"

static inline void
ext_printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
}

#else

static inline void
ext_printf(const char* fmt, ...) {}

#endif

#ifdef __cplusplus
extern "C" {
#endif

#define LONG_SIG "J"

#define MEMDEBUG 1
#define DEBUG 1
/**********/
/* VPTree */
/**********/
#define vpt_t jobject
#include "../vpt.h"

struct JVPTree {
    VPTree vpt;
    JNIEnv* env;
    jobject this;
    jobject dist_fn;

    jmethodID dist_fn_ID;
    jmethodID unwrap_double_ID;
    jfieldID entry_dist_ID;
    jfieldID entry_item_ID;
};
typedef struct JVPTree JVPTree;

double jdist_fn(void* extra_data, jobject first, jobject second) {
    JNIEnv* env = ((JVPTree*)extra_data)->env;
    jobject dist_fn = ((JVPTree*)extra_data)->dist_fn;
    jmethodID dist_fn_ID = ((JVPTree*)extra_data)->dist_fn_ID;
    jmethodID unwrap_double_ID = ((JVPTree*)extra_data)->unwrap_double_ID;

    jobject obj_result = (*env)->CallObjectMethod(env, dist_fn, dist_fn_ID, first, second);
    // We're not checking for null or exceptions.

    // Unwrap the result now. Get its
    jdouble double_result = (*env)->CallDoubleMethod(env, obj_result, unwrap_double_ID);
    return (double)double_result;
}

/******************/
/* Helper Methods */
/******************/

static inline jint
throwNoClassDefError(JNIEnv* env, char* message) {
    char* className = "java/lang/NoClassDefFoundError";
    jclass no_class_def_class = (*env)->FindClass(env, className);

    if (no_class_def_class == NULL) {
        printf("Something is very, very wrong on line: %d of vptree_VPTree.c, cannot load any classes.\n", __LINE__);
        fflush(stdout);
        exit(73);
    }

    return (*env)->ThrowNew(env, no_class_def_class, message);
}

static inline jint
throwException(JNIEnv* env, char* classpath, char* message) {
    jclass E_class = (*env)->FindClass(env, classpath);
    if (E_class == NULL) {
        return throwNoClassDefError(env, classpath);
    }
    return (*env)->ThrowNew(env, E_class, message);
}

static inline jint
throwIllegalState(JNIEnv* env, char* message) {
    return throwException(env, "java/lang/IllegalStateException", message);
}

static inline jint
throwIllegalArgument(JNIEnv* env, char* message) {
    return throwException(env, "java/lang/IllegalArgumentException", message);
}

static inline jint
throwOOM(JNIEnv* env, char* message) {
    return throwException(env, "java/lang/OutOfMemoryError", message);
}

// 1 if null or failure, 0 if not null
static inline jint
assertNotNull(JNIEnv* env, jobject obj, char* message) {
    if (obj == NULL) {
        throwIllegalArgument(env, message);
        return 1;
    }
    return 0;
}

static inline void
obj_print(JNIEnv* env, jobject obj) {
    // Get system class
    jclass system_class = (*env)->FindClass(env, "java/lang/System");
    // Look up the "out" field
    jfieldID fid = (*env)->GetStaticFieldID(env, system_class, "out", "Ljava/io/PrintStream;");
    jobject out = (*env)->GetStaticObjectField(env, system_class, fid);
    // Get PrintStream class
    jclass out_print_stream_class = (*env)->FindClass(env, "java/io/PrintStream");
    // Lookup printLn(String)
    jmethodID println_method_ID = (*env)->GetMethodID(env, out_print_stream_class, "println", "(Ljava/lang/String;)V");

    // Get the toString method ID of the object and execute it
    jclass obj_class = (*env)->GetObjectClass(env, obj);
    jmethodID toString_ID = (*env)->GetMethodID(env, obj_class, "toString", "()Ljava/lang/String;");
    jobject str = (*env)->CallObjectMethod(env, obj, toString_ID);

    // Invoke println()
    (*env)->CallVoidMethod(env, out, println_method_ID, str);
}

/******************/
/* Native Methods */
/******************/

/*
 * Class:     vptree_VPTree
 * Method:    VPT_build
 * Signature: ([Ljava/lang/Object;Ljava/util/function/BiFunction;)V
 */
JNIEXPORT void JNICALL Java_vptree_VPTree_VPT_1build(JNIEnv* env, jobject this, jobjectArray datapoints, jobject dist_fn) {
    // Validate args
    jint err;
    err = assertNotNull(env, datapoints, "Datapoints cannot be null.");
    if (err) return;
    err = assertNotNull(env, datapoints, "Distance function cannot be null.");
    if (err) return;
    ext_printf("Validated VPT_build arguments.\n");

    // Fix the distance function in memory so that it does not become invalidated between calls.
    // Later, the same will be done with the datapoints.
    dist_fn = (*env)->NewGlobalRef(env, dist_fn);
    if (!dist_fn) {
        throwOOM(env, "Ran out of memory creating a global reference to the distance function.");
        return;
    }

    // Allocate space for a tree. The java object takes ownership of this pointer.
    JVPTree* jvpt = (JVPTree*)malloc(sizeof(JVPTree));
    jvpt->dist_fn = dist_fn;
    jobject* datapoint_space = (vpt_t*)malloc(sizeof(vpt_t));
    if (!jvpt || !datapoint_space) {
        throwOOM(env, "Ran out of memory allocating the Vantage Point Tree.");
        return;
    }
    ext_printf("Space for tree allocated.\n");

    // Store Field IDs in the tree. These will come in handy later so that we don't have to keep looking them up.
    jclass entry_class = (*env)->FindClass(env, VPENTRY_CLASSPATH);
    if (!entry_class) {
        throwIllegalState(env, "Couldn't find the VPEntry class.");
        return;
    }
    ext_printf("Found the VPEntry class.\n");

    jvpt->entry_item_ID = (*env)->GetFieldID(env, entry_class, "item", "Ljava/lang/Object;");
    if (!(jvpt->entry_item_ID)) {
        throwIllegalState(env, "Couldn't find the VPEntry class's \"item\" member ID.");
        return;
    }
    jvpt->entry_dist_ID = (*env)->GetFieldID(env, entry_class, "distance", "D");
    if (!(jvpt->entry_dist_ID)) {
        throwIllegalState(env, "Couldn't find the VPEntry class's \"distance\" member ID.");
        return;
    }
    ext_printf("Found VPEntry Field IDs.\n");

    // Get the distance function's method id
    jclass dist_fn_class = (*env)->GetObjectClass(env, dist_fn);
    if (!dist_fn_class) {
        throwIllegalState(env, "Couldn't find the comparator's class.");
        return;
    }
    jvpt->dist_fn_ID = (*env)->GetMethodID(env, dist_fn_class, "apply", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    if (!(jvpt->dist_fn_ID)) {
        throwIllegalState(env, "Couldn't find the distance function's ID.");
        return;
    }
    ext_printf("Found distance function method ID.\n");

    // Get the unwrapping function from the Double class
    jclass double_class = (*env)->FindClass(env, "java/lang/Double");
    if (!double_class) {
        throwIllegalState(env, "Couldn't find the Double class.");
        return;
    }
    jvpt->unwrap_double_ID = (*env)->GetMethodID(env, double_class, "doubleValue", "()D");
    if (!(jvpt->unwrap_double_ID)) {
        throwIllegalState(env, "Couldn't find the doubleValue function's ID.");
        return;
    }
    ext_printf("Found the Double unwrapping function method ID.\n");

    // Consume the array, fixing the object locations in memory, and put them in the scratch space
    jsize num_datapoints = (*env)->GetArrayLength(env, datapoints);
    ext_printf("Found %d datapoints.\n", num_datapoints);
    for (jsize i = 0; i < num_datapoints; i++) {
        ext_printf("Pulling object %d out of the array.\n", i);
        jobject obj = (*env)->GetObjectArrayElement(env, datapoints, i);
        if (!obj) {
            throwIllegalArgument(env, "The collection cannot contain any null elements.");
            return;
        }
        // obj = (*env)->NewGlobalRef(env, obj);
        datapoint_space[i] = obj;
        obj_print(env, obj);
    }
    ext_printf("Copied the collection into a buffer.\n");

    // Create the tree
    ext_printf("Calling VPT_build.");
    bool success = VPT_build(&(jvpt->vpt), datapoint_space, num_datapoints, jdist_fn, dist_fn);
    if (!success) {
        throwOOM(env, "Ran out of memory building the Vantage Point Tree.");
        return;
    }
    ext_printf("Tree created out of data points successfully.\n");

    // Figure out where to put the pointer in the VPTree<T> object, and put it there.
    jclass this_class = (*env)->GetObjectClass(env, this);
    jfieldID ptr_fid = (*env)->GetFieldID(env, this_class, "vpt_ptr", LONG_SIG);
    (*env)->SetLongField(env, this, ptr_fid, (jlong)jvpt);

    free(datapoint_space);
}

/*
 * Class:     vptree_VPTree
 * Method:    nn
 * Signature: (Ljava/lang/Object;)Lvptree/VPEntry;
 */
JNIEXPORT jobject JNICALL
Java_vptree_VPTree_nn(JNIEnv* env, jobject this, jobject datapoint) {
    // Get owned pointer to jvptree struct
    jclass this_class = (*env)->GetObjectClass(env, this);
    jfieldID ptr_fid = (*env)->GetFieldID(env, this_class, "vpt_ptr", LONG_SIG);
    JVPTree* jvpt = (JVPTree*)(*env)->GetLongField(env, this, ptr_fid);
    if (!jvpt) {
        throwIllegalState(env, "Either the creation of the tree failed, or the tree has already been closed.");
        return NULL;
    }

    VPTree* vpt = &(jvpt->vpt);
    VPEntry* entry = VPT_nn(vpt, datapoint);
    if (!entry) return NULL;

    return this;
}

/*
 * Class:     vptree_VPTree
 * Method:    knn
 * Signature: (Ljava/lang/Object;J)Ljava/util/List;
 */
JNIEXPORT jobject JNICALL Java_vptree_VPTree_knn(JNIEnv*, jobject, jobject, jlong);

/*
 * Class:     vptree_VPTree
 * Method:    size
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_vptree_VPTree_size(JNIEnv* env, jobject this);

/*
 * Class:     vptree_VPTree
 * Method:    close
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_vptree_VPTree_close(JNIEnv* env, jobject this);

#ifdef __cplusplus
}
#endif
