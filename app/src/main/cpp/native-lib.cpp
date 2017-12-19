#include <android/asset_manager_jni.h>
#include <android/native_window_jni.h>
#include <thread>
#include "CV_Main.h"

static CV_Main cv_main;

#ifdef __cplusplus
extern "C" {
#endif

jint JNI_OnLoad(JavaVM *vm, void *)
{
    // We need to store a reference to the Java VM so that we can call back
    cv_main.SetJavaVM(vm);
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL
Java_com_spencerfricke_opencv_1ndk_MainActivity_onCreateJNI(
        JNIEnv *env, jobject clazz, jobject activity, jobject j_asset_manager)
{
    cv_main.OnCreate(env, activity);
    cv_main.SetAssetManager(AAssetManager_fromJava(env, j_asset_manager));
}



// Alot of stuff depends on the m_frame_buffer being loaded
// this is done in SetNativeWindow

JNIEXPORT void JNICALL
Java_com_spencerfricke_opencv_1ndk_MainActivity_openCamera(JNIEnv *env,
                                                           jclass clazz,
                                                           jobject surface, jstring jOutPath)
{

    cv_main.openCamera(env, clazz, surface,  jOutPath);

//    std::thread loopThread(&CV_Main::CameraLoop, &cv_main);
//    loopThread.detach();
}
JNIEXPORT void JNICALL Java_com_spencerfricke_opencv_1ndk_MainActivity_captureCamera(
        JNIEnv *env, jobject clazz)
{
    cv_main.captureCamera(env, clazz);
}

JNIEXPORT void JNICALL Java_com_spencerfricke_opencv_1ndk_MainActivity_closeCamera(
        JNIEnv *env, jobject clazz)
{
    cv_main.closeCamera(env, clazz);
}

JNIEXPORT void JNICALL Java_com_spencerfricke_opencv_1ndk_MainActivity_scan(
        JNIEnv *env, jobject clazz)
{
    cv_main.RunCV();
}

JNIEXPORT void JNICALL Java_com_spencerfricke_opencv_1ndk_MainActivity_flipCamera(
        JNIEnv *env,
        jclass clazz,
        jobject surface, jstring jOutPath)
{
    cv_main.FlipCamera(env, clazz, surface,  jOutPath);
}



#ifdef __cplusplus
}
#endif
