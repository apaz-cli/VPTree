// clang vptree_VPTree.c -shared -fPIC -lpthread -lm -I/usr/lib/jvm/java-11-openjdk-amd64/include -I/usr/lib/jvm/java-11-openjdk-amd64/include/linux
#include "vptree_VPTree.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LONG_SIG "J"

/**********/
/* VPTree */
/**********/
#define vpt_t jobject
#include "../vpt.h"

typedef struct JVPTree {
    VPTree vpt;
    JNIEnv* env;
    jobject dist_fn;
};
typedef struct JVPTree JVPTree;

double JVPTree_dist_fn(void* extra_data, jobject first, jobject second) {
    JVPTree dist_fn = *((JVPTree*)extra_data);
}

/******************/
/* Helper Methods */
/******************/

static inline jint
throwException(JNIEnv* env, const char* classpath, const char* message) {
    jclass E_class = (*env)->FindClass(env, classpath);
    if (E_class == NULL) {
        return throwNoClassDefError(env, classpath);
    }
    return (*env)->ThrowNew(env, E_class, message);
}

static inline jint
throwIllegalState(JNIEnv* env, const char* message) {
    return throwException(env, "java/lang/IllegalStateException", message);
}

static inline jint
throwIllegalArgument(JNIEnv* env, const char* message) {
    return throwException(env, "java/lang/IllegalArgumentException", message);
}

static inline jint
throwOOM(JNIEnv* env, const char* message) {
    return throwException(env, "java/lang/OutOfMemoryError", message);
}

// 1 if null or failure, 0 if not null
static inline jint
assertNotNull(JNIEnv* env, jobject obj, const char* message) {
    if (obj == NULL) {
        throwIllegalArgument(env, message);
        return 1;
    }
    return 0;
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

    // Allocate space for a tree. The java object takes ownership of this pointer.
    JVPTree* jvpt = (JVPTree*)malloc(sizeof(JVPTree));
    jobject* datapoint_space = (vpt_t*)malloc(sizeof(vpt_t));
    if (!jvpt || !datapoint_space) {
        throwOOM(env, "Ran out of memory allocating the Vantage Point Tree.");
        return;
    }

    // Fix the distance function in memory, such that if GC runs between the
    // evaluation of this native method and the other methods, we still have a valid reference.
    // Then store the safe reference in the structure.
    jvpt->dist_fn = (*env)->NewGlobalRef(env, dist_fn);

    // Consume the array, fixing the object locations in memory, and put them in the scratch space
    jsize num_datapoints = (*env)->GetArrayLength(env, datapoints);
    for (jsize i = 0; i < num_datapoints; i++) {
        jobject obj = (*env)->GetObjectArrayElement(env, datapoints, i);
        datapoint_space[i] = (*env)->NewGlobalRef(env, obj);
    }

    // Create the tree
    VPT_build(&(jvpt->vpt), datapoint_space, num_datapoints, JVPTree_dist_fn, dist_fn);

    // Check for tree creation errors

    // Figure out where to put the pointer in the VPTree<T> object, and put it there.
    jclass this_class = (*env)->GetObjectClass(env, this);
    jfieldID ptr_fid = (*env)->GetFieldID(env, this_class, "vpt_ptr", LONG_SIG);
    (*env)->SetLongField(env, this, ptr_fid, (jlong)jvpt);
}

/*
 * Class:     vptree_VPTree
 * Method:    nn
 * Signature: (Ljava/lang/Object;)Lvptree/VPEntry;
 */
JNIEXPORT jobject JNICALL
Java_vptree_VPTree_nn(JNIEnv* env, jobject this, jobject datapoint) {
    // Get owned pointer
    jclass this_class = (*env)->GetObjectClass(env, this);
    jfieldID ptr_fid = (*env)->GetFieldID(env, this_class, "vpt_ptr", LONG_SIG);
    VPTree* vpt = (VPTree*)(*env)->GetLongField(env, this, ptr_fid);

    if (!vpt) {
        throwIllegalState(env, "Cannot be used after closed.");
        return NULL;
    }
}

/*
 * Class:     vptree_VPTree
 * Method:    knn
 * Signature: (Ljava/lang/Object;)Ljava/util/List;
 */
JNIEXPORT jobject JNICALL Java_vptree_VPTree_knn(JNIEnv* env, jobject this, jobject datapoint);

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
