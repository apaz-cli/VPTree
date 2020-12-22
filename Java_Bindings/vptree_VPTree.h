/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class vptree_VPTree */

#ifndef _Included_vptree_VPTree
#define _Included_vptree_VPTree
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     vptree_VPTree
 * Method:    VPT_build
 * Signature: ([Ljava/lang/Object;Ljava/util/function/BiFunction;)V
 */
JNIEXPORT void JNICALL Java_vptree_VPTree_VPT_1build
  (JNIEnv *, jobject, jobjectArray, jobject);

/*
 * Class:     vptree_VPTree
 * Method:    nn
 * Signature: (Ljava/lang/Object;)Lvptree/VPEntry;
 */
JNIEXPORT jobject JNICALL Java_vptree_VPTree_nn
  (JNIEnv *, jobject, jobject);

/*
 * Class:     vptree_VPTree
 * Method:    knn
 * Signature: (Ljava/lang/Object;J)Ljava/util/List;
 */
JNIEXPORT jobject JNICALL Java_vptree_VPTree_knn
  (JNIEnv *, jobject, jobject, jlong);

/*
 * Class:     vptree_VPTree
 * Method:    size
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_vptree_VPTree_size
  (JNIEnv *, jobject);

/*
 * Class:     vptree_VPTree
 * Method:    close
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_vptree_VPTree_close
  (JNIEnv *, jobject);

#ifdef __cplusplus
}
#endif
#endif
