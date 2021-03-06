/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class sage_miniclient_OpenGLVideoRenderer */

#ifndef _Included_sage_miniclient_OpenGLVideoRenderer
#define _Included_sage_miniclient_OpenGLVideoRenderer
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     sage_miniclient_OpenGLVideoRenderer
 * Method:    initVideoServer
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_sage_miniclient_OpenGLVideoRenderer_initVideoServer
  (JNIEnv *, jobject);

/*
 * Class:     sage_miniclient_OpenGLVideoRenderer
 * Method:    deinitVideoServer
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_sage_miniclient_OpenGLVideoRenderer_deinitVideoServer
  (JNIEnv *, jobject);

/*
 * Class:     sage_miniclient_OpenGLVideoRenderer
 * Method:    processVideoCommand
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_sage_miniclient_OpenGLVideoRenderer_processVideoCommand
  (JNIEnv *, jobject);

/*
 * Class:     sage_miniclient_OpenGLVideoRenderer
 * Method:    getServerVideoOutParams
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_sage_miniclient_OpenGLVideoRenderer_getServerVideoOutParams
  (JNIEnv *, jobject);

/*
 * Class:     sage_miniclient_OpenGLVideoRenderer
 * Method:    isGLSLHardware
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_sage_miniclient_OpenGLVideoRenderer_isGLSLHardware
  (JNIEnv *, jobject);

#ifdef __cplusplus
}
#endif
#endif
