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
    jobject this;

    jmethodID dist_fn_ID;
    jfieldID entry_dist_ID;
    jfieldID entry_item_ID;
};
typedef struct JVPTree JVPTree;

double JVPTree_dist_fn(void* extra_data, jobject first, jobject second) {
    JVPTree dist_fn = *((JVPTree*)extra_data);
}

double jdistfn(void* extra_data, jobject first, jobject second) {
    JNIEnv* env = ((JVPTree*)extra_data)->env;
    jmethodID comparatorID = ((JVPTree*)extra_data)->dist_fn_ID;
    jobject this = ((JVPTree*)extra_data)->this;

    jdouble result = (*env)->CallDoubleMethod(env, this, comparatorID, first, second);
    return (double)result;
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

    // Store method and field IDs in the tree. These will come in handy later so that we don't have to keep looking them up.
    jclass entry_class = (*env)->FindClass(env, "vptree/VPEntry");
    if (!entry_class) {
        throwIllegalState(env, "Couldn't find the VPEntry class.");
        return;
    }
    jvpt->dist_fn_ID = (*env)->GetMethodID(env, dist_fn, "apply", "(Ljava/lang/Object;Ljava/lang/Object;)D");
    if (!(jvpt->dist_fn_ID)) {
        throwIllegalState(env, "Couldn't find the distance function's ID.");
        return;
    }
    jvpt->entry_dist_ID = (*env)->GetFieldID(env, entry_class, "distance", "D");
    if (!(jvpt->entry_dist_ID)) {
        throwIllegalState(env, "Couldn't find the VPEntry class's \"distance\" member ID.");
        return;
    }
    jvpt->entry_item_ID = (*env)->GetFieldID(env, entry_class, "item", "Ljava/lang/Object");
    if (!(jvpt->entry_item_ID)) {
        throwIllegalState(env, "Couldn't find the VPEntry class's \"item\" member ID.");
        return;
    }

    // Now we know that we can maybe return results...
    // Consume the array, fixing the object locations in memory, and put them in the scratch space
    jsize num_datapoints = (*env)->GetArrayLength(env, datapoints);
    for (jsize i = 0; i < num_datapoints; i++) {
        jobject obj = (*env)->GetObjectArrayElement(env, datapoints, i);
        datapoint_space[i] = (*env)->NewGlobalRef(env, obj);
    }

    // Create the tree
    bool success = VPT_build(&(jvpt->vpt), datapoint_space, num_datapoints, JVPTree_dist_fn, dist_fn);
    if (!success) {
        throwOOM(env, "Ran out of memory building the Vantage Point Tree.");
        return;
    }

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
