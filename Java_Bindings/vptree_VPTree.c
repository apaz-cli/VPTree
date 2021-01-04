// clang vptree_VPTree.c -shared -fPIC -lpthread -lm -I/usr/lib/jvm/java-11-openjdk-amd64/include -I/usr/lib/jvm/java-11-openjdk-amd64/include/linux
// sudo clang -Wall -Wextra -g -fsanitize=address -lpthread -lm -Ofast -shared -fPIC -I/usr/lib/jvm/java-11-openjdk-amd64/include -I/usr/lib/jvm/java-11-openjdk-amd64/include/linux vptree_VPTree.c -o /usr/lib/libJVPTree.so

#include "vptree_VPTree.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#define EXT_DEBUG 0
#if EXT_DEBUG

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
ext_printf(const char* fmt, ...) {(void) fmt; }

#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ANSI_TERMINAL 0
#define PRINT_MEMALLOCS 0
#define MEMDEBUG 1
#include "../memdebug.h/memdebug.h"

#define DEBUG 0
/**********/
/* VPTree */
/**********/
#define vpt_t jint
#include "../vpt.h"

struct JVPTree {
    // The jni environment for this call. Replace this with the new one on every method call.
    JNIEnv* env;

    // The actual data structure. Destroyed with VPT_destroy() or VPT_reclaim().
    VPTree vpt;

    // Global references. Will be freed on close.
    jobject this;
    jobject dist_fn;
    jobjectArray datapoints;
    jobject currently_comparing;
    jclass entry_class;
    jclass arrays_class;

    // The following are cached methods IDs.
    // They need not be freed.
    jmethodID dist_fn_ID;
    jmethodID unwrap_double_ID;
    jmethodID entry_constructor;
    jmethodID to_list_ID;
};
typedef struct JVPTree JVPTree;

double jdist_fn(void* extra_data, jint first_idx, jint second_idx) {
    JNIEnv* env = ((JVPTree*)extra_data)->env;
    jobject dist_fn = ((JVPTree*)extra_data)->dist_fn;
    jobjectArray datapoints = ((JVPTree*)extra_data)->datapoints;
    jmethodID dist_fn_ID = ((JVPTree*)extra_data)->dist_fn_ID;
    jmethodID unwrap_double_ID = ((JVPTree*)extra_data)->unwrap_double_ID;

    // Grab the actual objects
    jobject first = (*env)->GetObjectArrayElement(env, datapoints, first_idx);
    jobject second;
    if (second_idx == -1) {
        second = ((JVPTree*)extra_data)->currently_comparing;
    } else {
        second = (*env)->GetObjectArrayElement(env, datapoints, second_idx);
    }

#if EXT_DEBUG
    if (!(first && second)) {
        ext_printf("One of the objects in the Collection was null, despite the native function JVPT_build()'s initial check.\n");
        exit(2);
    }
#endif

    jobject obj_result = (*env)->CallObjectMethod(env, dist_fn, dist_fn_ID, first, second);
#if EXT_DEBUG
    if ((*env)->ExceptionCheck(env)) {
        ext_printf("The distance function threw an exception.\n");
        (*env)->ExceptionDescribe(env);
        exit(2);
    }
    if (!obj_result) {
        ext_printf("The result of the distance function was null.\n");
        exit(2);
    }
#endif

    // Unwrap the result
    jdouble double_result = (*env)->CallDoubleMethod(env, obj_result, unwrap_double_ID);
#if EXT_DEBUG
    if ((*env)->ExceptionCheck(env)) {
        ext_printf("Unwrapping the result of the distance function threw an exception.\n");
        (*env)->ExceptionDescribe(env);
        exit(2);
    }
    if (!obj_result) {
        ext_printf("Unwrapping the result of the distance function returned null.\n");
        ext_printf("The result of the distance function was null.\n");
        exit(2);
    }
#endif

    // Release the references
    (*env)->DeleteLocalRef(env, obj_result);
    (*env)->DeleteLocalRef(env, first);
    if (second_idx != -1) (*env)->DeleteLocalRef(env, second);

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

/*
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
*/

static inline void
set_owned_jvpt(JNIEnv* env, jobject this, JVPTree* jvpt) {
    ext_printf("Setting the owned JVPTree.\n");
    jclass this_class = (*env)->GetObjectClass(env, this);
    jfieldID ptr_fid = (*env)->GetFieldID(env, this_class, "vpt_ptr", "J");
#if EXT_DEBUG
    if (!ptr_fid) {
        ext_printf("Could not retrieve the field to store the poiter to the JVPTree* from the VPTree<T> Java class.\n");
        exit(2);
    }
#endif
    (*env)->SetLongField(env, this, ptr_fid, (jlong)jvpt);
    ext_printf("Set the owned JVPTree.\n");
}

static inline JVPTree*
get_owned_jvpt(JNIEnv* env, jobject this) {
    ext_printf("Getting the owned JVPTree.\n");
    // Get owned pointer to jvptree struct
    jclass this_class = (*env)->GetObjectClass(env, this);
    jfieldID ptr_fid = (*env)->GetFieldID(env, this_class, "vpt_ptr", "J");
    jlong longptr = (*env)->GetLongField(env, this, ptr_fid);
    JVPTree* jvpt = (JVPTree*)longptr;
    if (!jvpt) {
        throwIllegalState(env, "This VPTree has already been closed.");
        return NULL;
    }
    ext_printf("Got the owned JVPTree.\n");
    return jvpt;
}

/**********************/
/* Native JNI Methods */
/**********************/

/*
 * Class:     vptree_VPTree
 * Method:    VPT_build
 * Signature: ([Ljava/lang/Object;Ljava/util/function/BiFunction;)V
 */
JNIEXPORT void JNICALL
Java_vptree_VPTree_VPT_1build(JNIEnv* env, jobject this, jobjectArray datapoints, jobject dist_fn) {
    ext_printf("Starting JNI method: VPT_build.\n");

    // Validate args
    jint err;
    err = assertNotNull(env, datapoints, "Datapoints cannot be null.");
    if (err) return;
    err = assertNotNull(env, dist_fn, "Distance function cannot be null.");
    if (err) return;
    ext_printf("Validated VPT_build arguments not null.\n");

    // Validate that none of the array elements arguments are null.
    jsize obj_array_size = (*env)->GetArrayLength(env, datapoints);
    ext_printf("Found %d datapoints.\n", obj_array_size);
    if (!obj_array_size) {
        throwIllegalArgument(env, "Will not create a VPTree of size zero.");
        return;
    }
    for (jsize i = 0; i < obj_array_size; i++) {
        // ext_printf("Pulling object %d out of the array.\n", i);
        jobject obj = (*env)->GetObjectArrayElement(env, datapoints, i);
#if EXT_DEBUG
        if ((*env)->ExceptionCheck(env)) {
            return;
        }
#endif
        if (!obj) {
            throwIllegalArgument(env, "The Collection cannot contain any null elements.");
            return;
        }
        (*env)->DeleteLocalRef(env, obj);
    }
    ext_printf("Validated that none of the elements of the Collection are null.\n");

    // Allocate space for a tree. The java object takes ownership of this pointer.
    JVPTree* jvpt = (JVPTree*)malloc(sizeof(JVPTree));
    ext_printf("Malloced the tree.\n");

    vpt_t* datapoint_space = (vpt_t*)malloc(obj_array_size * sizeof(vpt_t));
    if (!jvpt || !datapoint_space) {
        throwOOM(env, "Ran out of memory allocating the Vantage Point Tree.");
        return;
    }
    // Fill it with the indexes;
    for (jsize i = 0; i < obj_array_size; i++) {
        datapoint_space[i] = i;
    }
    ext_printf("Malloced and filled the datapoint space.\n");

    // Cache information for other methods in the tree.
    jclass entry_class = (*env)->FindClass(env, "vptree/VPEntry");
#if EXT_DEBUG
    if (!entry_class) {
        throwIllegalState(env, "Couldn't find the VPEntry class.");
        return;
    }
#endif
    jvpt->entry_constructor = (*env)->GetMethodID(env, entry_class, "<init>", "(Ljava/lang/Object;D)V");
#if EXT_DEBUG
    if (!(jvpt->entry_constructor)) {
        throwIllegalState(env, "Couldn't find the VPEntry class's constructor method ID.");
        return;
    }
#endif

    // Cache distance function information in the tree. This will be useful for building the tree.
    jclass dist_fn_class = (*env)->GetObjectClass(env, dist_fn);
#if EXT_DEBUG
    if (!dist_fn_class) {
        throwIllegalState(env, "Couldn't find the distance function's class.");
        return;
    }
#endif
    jvpt->dist_fn_ID = (*env)->GetMethodID(env, dist_fn_class, "apply", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
#if EXT_DEBUG
    if (!(jvpt->dist_fn_ID)) {
        throwIllegalState(env, "Couldn't find the distance function's ID.");
        return;
    }
#endif
    jclass double_class = (*env)->FindClass(env, "java/lang/Double");
#if EXT_DEBUG
    if (!double_class) {
        throwIllegalState(env, "Couldn't find the Double class.");
        return;
    }
#endif
    jvpt->unwrap_double_ID = (*env)->GetMethodID(env, double_class, "doubleValue", "()D");
#if EXT_DEBUG
    if (!(jvpt->unwrap_double_ID)) {
        throwIllegalState(env, "Couldn't find the doubleValue function's ID.");
        return;
    }
    ext_printf("Found distance function method ID, as well as the function for unwrapping java.lang.Double.\n");
#endif

    // Cache the necessary information to call Arrays.toList()
    jclass arrays_class = (*env)->FindClass(env, "java/util/Arrays");
#if EXT_DEBUG
    if (!arrays_class) {
        throwIllegalState(env, "Couldn't find the Arrays class.");
        return;
    }
#endif
    jvpt->to_list_ID = (*env)->GetStaticMethodID(env, arrays_class, "asList", "([Ljava/lang/Object;)Ljava/util/List;");
#if EXT_DEBUG
    if (!(jvpt->to_list_ID)) {
        throwIllegalState(env, "Couldn't find the asList() method ID.");
        return;
    }
#endif

    // Create global references out of these values which will persist across JNI calls

    this = (*env)->NewGlobalRef(env, this);
#if EXT_DEBUG
    if (!this) {
        throwOOM(env, "Ran out of memory creating a global reference to the this VPTree.");
        return;
    }
    ext_printf("Created a global ref to this.\n");
#endif

    dist_fn = (*env)->NewGlobalRef(env, dist_fn);
#if EXT_DEBUG
    if (!dist_fn) {
        throwOOM(env, "Ran out of memory creating a global reference to the distance function.");
        return;
    }
    ext_printf("Created a global ref to the distance function.\n");
#endif

    datapoints = (*env)->NewGlobalRef(env, datapoints);
#if EXT_DEBUG
    if (!datapoints) {
        throwOOM(env, "Ran out of memory creating a global reference to the datapoints array.");
        return;
    }
    ext_printf("Created a global ref to datapoints.\n");
#endif

    arrays_class = (*env)->NewGlobalRef(env, arrays_class);
#if EXT_DEBUG
    if (!entry_class) {
        throwOOM(env, "Ran out of memory creating a global reference to the Arrays class.");
        return;
    }
    ext_printf("Created a global ref to the Arrays class.\n");
#endif

    entry_class = (*env)->NewGlobalRef(env, entry_class);
#if EXT_DEBUG
    if (!entry_class) {
        throwOOM(env, "Ran out of memory creating a global reference to the VPEntry<T> class.");
        return;
    }
    ext_printf("Created a global ref to the VPEntry class.\n");
#endif

    // Finally, after all that preprocessing, put it in the tree.
    jvpt->env = env;
    jvpt->this = this;
    jvpt->dist_fn = dist_fn;
    jvpt->datapoints = datapoints;
    jvpt->arrays_class = arrays_class;
    jvpt->entry_class = entry_class;
    ext_printf("References cached.\n");

    // Create the C tree
    bool success = VPT_build(&(jvpt->vpt), datapoint_space, obj_array_size, jdist_fn, jvpt);
    if (!success) {
        throwOOM(env, "Ran out of memory building the Vantage Point Tree.");
        return;
    }
    ext_printf("Tree created out of data points successfully.\n");

    // Transfer ownership of the jvpt C object to the VPTree<T> Java object.
    set_owned_jvpt(env, this, jvpt);
    free(datapoint_space);
}

static inline jobject
new_VPEntry_jobject(JNIEnv* env, JVPTree* jvpt, VPEntry nn) {
    jobject item = (*env)->GetObjectArrayElement(env, jvpt->datapoints, nn.item);
#if EXT_DEBUG
    if (!item) {
        ext_printf("Could not retrieve item %d from the array. Out of bounds?\n", nn.item);
        exit(2);
    }
    ext_printf("Got object from array to put into VPEntry object.\n");
#endif
    jobject ret = (*env)->NewObject(env, jvpt->entry_class, jvpt->entry_constructor, item, (jdouble)nn.distance);
#if EXT_DEBUG
    if (!ret) {
        ext_printf("The VPEntry constructor failed somehow.\n");
        exit(2);
    }
    ext_printf("Created a new VPEntry object.\n");
#endif
    (*env)->DeleteLocalRef(env, item);
    ext_printf("Deleted the local ref to the item.\n");
    return ret;
}

/*
 * Class:     vptree_VPTree
 * Method:    nn
 * Signature: (Ljava/lang/Object;)Lvptree/VPEntry;
 */
JNIEXPORT jobject JNICALL
Java_vptree_VPTree_nn(JNIEnv* env, jobject this, jobject datapoint) {
    ext_printf("Starting JNI method: VPT_nn.\n");

    if (!datapoint) {
        throwIllegalArgument(env, "The datapoint cannot be null.");
        return NULL;
    }

    JVPTree* jvpt = get_owned_jvpt(env, this);
    if (!jvpt) {
        throwIllegalState(env, "This VPTree has already been closed.");
        return NULL;
    }
    ext_printf("Setting currently comparing.\n");
    jvpt->currently_comparing = datapoint;

    ext_printf("Starting C nn.\n");
    VPEntry nn;
    VPT_nn(&(jvpt->vpt), -1, &nn);
    ext_printf("C nn complete.\n");

    return new_VPEntry_jobject(env, jvpt, nn);
}

/*
 * Class:     vptree_VPTree
 * Method:    knn
 * Signature: (Ljava/lang/Object;J)Ljava/util/List;
 */
JNIEXPORT jobject JNICALL
Java_vptree_VPTree_knn(JNIEnv* env, jobject this, jobject datapoint, jlong k) {
    ext_printf("Starting JNI method: VPT_knn.\n");
    if (!datapoint) {
        throwIllegalArgument(env, "The datapoint cannot be null.");
        return NULL;
    }
    if (k < 1) {
        throwIllegalArgument(env, "k cannot be less than 1.");
    }

    JVPTree* jvpt = get_owned_jvpt(env, this);
    jvpt->currently_comparing = datapoint;

    ext_printf("Starting C knn.\n");

    size_t num_found;
    VPEntry knns[k];
    VPTree* vpt = &(jvpt->vpt);
    VPT_knn(vpt, -1, (size_t)k, knns, &num_found);
    if (!num_found) return NULL;
    ext_printf("C knn completed. Found %zu knns.\n", num_found);

    jsize array_size = (jsize)num_found;
    jobjectArray knn_arr = (*env)->NewObjectArray(env, array_size, jvpt->entry_class, NULL);
    if (!knn_arr) {
        // already thrown
        return NULL;
    }
    ext_printf("Created new object array.\n");

    for (jsize i = 0; i < array_size; i++) {
        jobject new_entry = new_VPEntry_jobject(env, jvpt, knns[i]);
        (*env)->SetObjectArrayElement(env, knn_arr, (jsize)i, new_entry);
    }
    ext_printf("Filled the array with the entries that were found.\n");

    jobject knn_list = (*env)->CallStaticObjectMethod(env, jvpt->arrays_class, jvpt->to_list_ID, knn_arr);
#if EXT_DEBUG
    if (!knn_list) {
        // already thrown
        return NULL;
    }
#endif

    return knn_list;
}

/*
 * Class:     vptree_VPTree
 * Method:    size
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_vptree_VPTree_size(JNIEnv* env, jobject this) {
    ext_printf("Starting JNI method: VPT_size.\n");
    JVPTree* jvpt = get_owned_jvpt(env, this);
    return (jint)(jvpt->vpt.size);
}

/*
 * Class:     vptree_VPTree
 * Method:    close
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_vptree_VPTree_close(JNIEnv* env, jobject this) {
    ext_printf("Starting JNI method: VPT_close.\n");

    JVPTree* jvpt = get_owned_jvpt(env, this);
    set_owned_jvpt(env, this, NULL);

    (*env)->DeleteGlobalRef(env, jvpt->this);
    (*env)->DeleteGlobalRef(env, jvpt->dist_fn);
    (*env)->DeleteGlobalRef(env, jvpt->datapoints);
    (*env)->DeleteGlobalRef(env, jvpt->this);
    (*env)->DeleteGlobalRef(env, jvpt->entry_class);
    (*env)->DeleteGlobalRef(env, jvpt->arrays_class);

    VPT_destroy(&(jvpt->vpt));
    free(jvpt);
    print_heap();
}

#ifdef __cplusplus
}
#endif
