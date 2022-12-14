/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class org_ogre4j_TextureManager */

#ifndef _Included_org_ogre4j_TextureManager
#define _Included_org_ogre4j_TextureManager
#ifdef __cplusplus
extern "C" {
#endif
/* Inaccessible static: table */
/*
 * Class:     org_ogre4j_TextureManager
 * Method:    createCppInstance
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_ogre4j_TextureManager_createCppInstance
  (JNIEnv *, jclass);

/*
 * Class:     org_ogre4j_TextureManager
 * Method:    enable32BitTextures
 * Signature: (IZ)V
 */
JNIEXPORT void JNICALL Java_org_ogre4j_TextureManager_enable32BitTextures
  (JNIEnv *, jclass, jint, jboolean);

/*
 * Class:     org_ogre4j_TextureManager
 * Method:    getDefaultNumMipmaps
 * Signature: (I)J
 */
JNIEXPORT jlong JNICALL Java_org_ogre4j_TextureManager_getDefaultNumMipmaps
  (JNIEnv *, jclass, jint);

/*
 * Class:     org_ogre4j_TextureManager
 * Method:    getSingletonImpl
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_ogre4j_TextureManager_getSingletonImpl
  (JNIEnv *, jclass);

/*
 * Class:     org_ogre4j_TextureManager
 * Method:    setDefaultNumMipmaps
 * Signature: (IJ)V
 */
JNIEXPORT void JNICALL Java_org_ogre4j_TextureManager_setDefaultNumMipmaps
  (JNIEnv *, jclass, jint, jlong);

#ifdef __cplusplus
}
#endif
#endif
