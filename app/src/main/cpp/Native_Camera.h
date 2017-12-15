#ifndef OPENCV_NDK_NATIVE_CAMERA_H
#define OPENCV_NDK_NATIVE_CAMERA_H

#include "Util.h"
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraManager.h>
#include <media/NdkImageReader.h>
#include <android/native_window.h>
#include <mutex>

enum camera_type
{
    BACK_CAMERA, FRONT_CAMERA
};

class CameraDeviceListener
{
public:
    static void onDisconnected(void *obj, ACameraDevice *device)
    {
        LOGI("Camera %s is disconnected!", ACameraDevice_getId(device));
        if (obj == nullptr)
        {
            return;
        }
        CameraDeviceListener *thiz = reinterpret_cast<CameraDeviceListener *>(obj);
        std::lock_guard<std::mutex> lock(thiz->mMutex);
        thiz->mOnDisconnect++;
        return;
    }

    static void onError(void *obj, ACameraDevice *device, int errorCode)
    {
        LOGI("Camera %s receive error %d!", ACameraDevice_getId(device), errorCode);
        if (obj == nullptr)
        {
            return;
        }
        CameraDeviceListener *thiz = reinterpret_cast<CameraDeviceListener *>(obj);
        std::lock_guard<std::mutex> lock(thiz->mMutex);
        thiz->mOnError++;
        thiz->mLatestError = errorCode;
        return;
    }

private:
    std::mutex mMutex;
    int mOnDisconnect = 0;
    int mOnError = 0;
    int mLatestError = 0;
};

class CaptureSessionListener
{
public:
    static void onClosed(void *obj, ACameraCaptureSession *session)
    {
        // TODO: might want an API to query cameraId even session is closed?
        LOGI("Session %p is closed!", session);
        if (obj == nullptr)
        {
            return;
        }
        CaptureSessionListener *thiz = reinterpret_cast<CaptureSessionListener *>(obj);
        std::lock_guard<std::mutex> lock(thiz->mMutex);
        thiz->mIsClosed = true;
        thiz->mOnClosed++; // Should never > 1
    }

    static void onReady(void *obj, ACameraCaptureSession *session)
    {
        LOGI("%s", __FUNCTION__);
        if (obj == nullptr)
        {
            return;
        }
        CaptureSessionListener *thiz = reinterpret_cast<CaptureSessionListener *>(obj);
        std::lock_guard<std::mutex> lock(thiz->mMutex);
        ACameraDevice *device = nullptr;
        camera_status_t ret = ACameraCaptureSession_getDevice(session, &device);
        // There will be one onReady fired after session closed
        if (ret != ACAMERA_OK && !thiz->mIsClosed)
        {
            LOGI("%s Getting camera device from session callback failed!",
                 __FUNCTION__);
            thiz->mInError = true;
        }
        LOGI("Session for camera %s is ready!", ACameraDevice_getId(device));
        thiz->mIsIdle = true;
        thiz->mOnReady++;
    }

    static void onActive(void *obj, ACameraCaptureSession *session)
    {
        LOGI("%s", __FUNCTION__);
        if (obj == nullptr)
        {
            return;
        }
        CaptureSessionListener *thiz = reinterpret_cast<CaptureSessionListener *>(obj);
        std::lock_guard<std::mutex> lock(thiz->mMutex);
        ACameraDevice *device = nullptr;
        camera_status_t ret = ACameraCaptureSession_getDevice(session, &device);
        if (ret != ACAMERA_OK)
        {
            LOGI("%s Getting camera device from session callback failed!",
                 __FUNCTION__);
            thiz->mInError = true;
        }
        LOGI("Session for camera %s is busy!", ACameraDevice_getId(device));
        thiz->mIsIdle = false;
//        thiz->mOnActive;
    }

    bool isClosed()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mIsClosed;
    }

    bool isIdle()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mIsIdle;
    }

    bool isInError()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mInError;
    }

    int onClosedCount()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mOnClosed;
    }

    int onReadyCount()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mOnReady;
    }

    int onActiveCount()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mOnActive;
    }

    void reset()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mIsClosed = false;
        mIsIdle = true;
        mInError = false;
        mOnClosed = 0;
        mOnReady = 0;
        mOnActive = 0;
    }

private:
    std::mutex mMutex;
    bool mIsClosed = false;
    bool mIsIdle = true;
    bool mInError = false; // should always stay false
    int mOnClosed = 0;
    int mOnReady = 0;
    int mOnActive = 0;
};

class CameraCaptureListener
{
public:
    static void onCaptureStarted(void* context, ACameraCaptureSession* session,const ACaptureRequest* request, int64_t timestamp)
    {
        LOGI("onCaptureStarted");
    }
    static void onCaptureProgressed(void* context, ACameraCaptureSession* session, ACaptureRequest* request, const ACameraMetadata* result)
    {
        LOGI("onCaptureProgressed");
    }
    static void onCaptureCompleted(void* context, ACameraCaptureSession* session, ACaptureRequest* request, const ACameraMetadata* result)
    {
        LOGI("onCaptureCompleted");
    }
    static void onCaptureFailed(void* context, ACameraCaptureSession* session, ACaptureRequest* request, ACameraCaptureFailure* failure)
    {
        LOGI("onCaptureFailed");
    }
    static void onCaptureSequenceCompleted(void* context, ACameraCaptureSession* session,int sequenceId, int64_t frameNumber)
    {
        LOGI("onCaptureSequenceCompleted");
    }
    static void onCaptureSequenceAborted(void* context, ACameraCaptureSession* session,int sequenceId)
    {
        LOGI("onCaptureSequenceAborted");
    }
    static void onCaptureBufferLost(void* context, ACameraCaptureSession* session,ACaptureRequest* request, ANativeWindow* window, int64_t frameNumber)
    {
        LOGI("onCaptureBufferLost");
    }

private:
};
// This class was created since if you want to alternate cameras
// you need to do a proper cleanup and setup and creating a
// separate class was the best OOP move
class Native_Camera
{
public:
    explicit Native_Camera(camera_type type);

    ~Native_Camera();

    bool MatchCaptureSizeRequest(ImageFormat *resView, int32_t width, int32_t height);

    bool CreateCaptureSession(ANativeWindow *window);

    int32_t GetCameraCount()
    {
        return m_camera_id_list->numCameras;
    }

    uint32_t GetOrientation()
    {
        return m_camera_orientation;
    };

private:

    // Camera variables
    ACameraDevice *m_camera_device;
    ACaptureRequest *m_capture_request;
    ACameraOutputTarget *m_camera_output_target;
    ACaptureSessionOutput *m_session_output;
    ACaptureSessionOutputContainer *m_capture_session_output_container;
    ACameraCaptureSession *m_capture_session;

    ACameraMetadata *cameraMetadata = nullptr;

    ACameraManager *m_camera_manager;
    uint32_t m_camera_orientation;
    ACameraIdList *m_camera_id_list = NULL;

    CameraDeviceListener deviceListener;
    ACameraDevice_StateCallbacks m_device_state_callbacks
    {
            &deviceListener,
            CameraDeviceListener::onDisconnected,
            CameraDeviceListener::onError
    };

    CaptureSessionListener mSessionListener;
    ACameraCaptureSession_stateCallbacks m_capture_session_state_callbacks
    {
            &mSessionListener,
            CaptureSessionListener::onClosed,
            CaptureSessionListener::onReady,
            CaptureSessionListener::onActive
    };

    CameraCaptureListener  cameracaptureListener;
    ACameraCaptureSession_captureCallbacks m_capture_session_capture_callbacks
    {
            &cameracaptureListener,
            CameraCaptureListener::onCaptureStarted,
            CameraCaptureListener::onCaptureProgressed,
            CameraCaptureListener::onCaptureCompleted,
            CameraCaptureListener::onCaptureFailed,
            CameraCaptureListener::onCaptureSequenceCompleted,
            CameraCaptureListener::onCaptureSequenceAborted,
            CameraCaptureListener::onCaptureBufferLost,
    };

    const char *m_selected_camera_id = NULL;
    bool m_camera_ready;

    int32_t backup_width = 480;
    int32_t backup_height = 720;
};

#endif //OPENCV_NDK_NATIVE_CAMERA_H
