/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class org_ogre4j_LogManager */

#ifndef _Included_org_ogre4j_LogManager
#define _Included_org_ogre4j_LogManager
#ifdef __cplusplus
extern "C" {
#endif
/* Inaccessible static: pInstance */
/* Inaccessible static: singleton */
/*
 * Class:     org_ogre4j_LogManager
 * Method:    createInstance
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_ogre4j_LogManager_createInstance
  (JNIEnv *, jclass);

/*
 * Class:     org_ogre4j_LogManager
 * Method:    _getSingleton
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_ogre4j_LogManager__1getSingleton
  (JNIEnv *, jclass);

/*
 * Class:     org_ogre4j_LogManager
 * Method:    dispose
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_org_ogre4j_LogManager_dispose
  (JNIEnv *, jclass, jint);

/*
 * Class:     org_ogre4j_LogManager
 * Method:    _logMessage
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_ogre4j_LogManager__1logMessage
  (JNIEnv *, jclass, jstring);

#ifdef __cplusplus
}
#endif
#endif
