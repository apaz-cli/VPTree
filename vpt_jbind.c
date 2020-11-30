#include <jni.h>

#define vpt_t jobject
#include "vpt.h"

typedef struct JVPTree {
    VPTree vpt;

    JNIEnv* env;
    jobject this;
    jmethodID dist_fn_ID;
};
typedef struct JVPTree JVPTree;

double jdistfn(void* extra_data, jobject first, jobject second) {
    JNIEnv* env = ((JVPTree*)extra_data)->env;
    jmethodID comparatorID = ((JVPTree*)extra_data)->dist_fn_ID;
    jobject this = ((JVPTree*)extra_data)->this;

    (*env)->CallDoubleMethod(env, this, comparatorID, first, second);
}

JNIEXPORT void JNICALL
Java_VPTree_VPT_build(JNIEnv* env, jobject this, jobjectArray jobjarr, jobject dist_fn) {
    // Grab all the elements from the collection
    size_t num_items = (*env)->GetArrayLength(env, (jarray)jobjarr);
    jobject* collection_items = malloc(sizeof(jobject) * num_items);
    for (jsize i = 0; i < num_items; ++i) {
        collection_items[i] = (*env)->GetObjectArrayElement(env, jobjarr, i);
    }

    // Set JVPTree elements
    JVPTree* jvpt = (JVPTree*)malloc(sizeof(JVPTree));

    // Build the tree
    VPT_build(&(jvpt->vpt), collection_items, num_items, jdistfn, env);
}

JNIEXPORT void JNICALL
Java_VPTree_VPT_destroy(JNIEnv* env, jobject this) {
}