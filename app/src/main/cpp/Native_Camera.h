#ifndef OPENCV_NDK_NATIVE_CAMERA_H
#define OPENCV_NDK_NATIVE_CAMERA_H

#include "Util.h"



#include <string>
#include <map>
#include <mutex>
#include <unistd.h>
#include <assert.h>
#include <jni.h>
#include <stdio.h>
#include <string.h>
#include <android/native_window_jni.h>
#include <android/native_window.h>

#include <camera/NdkCameraError.h>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraCaptureSession.h>

#include <media/NdkImage.h>
#include <media/NdkImageReader.h>
#include <cstdlib>
#include <camera/NdkCameraMetadata.h>


#define LOG_ERROR(buf, ...) sprintf(buf, __VA_ARGS__);                             LOGI("%s", buf);

enum camera_type
{
    BACK_CAMERA, FRONT_CAMERA
};

namespace
{
    const int MAX_ERROR_STRING_LEN = 512;
    char errorString[MAX_ERROR_STRING_LEN];
}

class CameraServiceListener
{
public:
    static void onAvailable(void *obj, const char *cameraId)
    {
        LOGI("Camera %s onAvailable", cameraId);
        if (obj == nullptr)
        {
            return;
        }
        CameraServiceListener *thiz = reinterpret_cast<CameraServiceListener *>(obj);

        std::lock_guard<std::mutex> lock(thiz->mMutex);
        thiz->mOnAvailableCount++;
        thiz->mAvailableMap[cameraId] = true;
        return;
    }

    static void onUnavailable(void *obj, const char *cameraId)
    {
        LOGI("Camera %s onUnavailable", cameraId);
        if (obj == nullptr)
        {
            return;
        }
        CameraServiceListener *thiz = reinterpret_cast<CameraServiceListener *>(obj);
        std::lock_guard<std::mutex> lock(thiz->mMutex);
        thiz->mOnUnavailableCount++;
        thiz->mAvailableMap[cameraId] = false;
        return;
    }

    void resetCount()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mOnAvailableCount = 0;
        mOnUnavailableCount = 0;
        return;
    }

    int getAvailableCount()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mOnAvailableCount;
    }

    int getUnavailableCount()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mOnUnavailableCount;
    }

    bool isAvailable(const char *cameraId)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mAvailableMap.count(cameraId) == 0)
        {
            return false;
        }
        return mAvailableMap[cameraId];
    }

private:
    std::mutex mMutex;
    int mOnAvailableCount = 0;
    int mOnUnavailableCount = 0;
    std::map<std::string, bool> mAvailableMap;
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
        thiz->mOnActive += 1 ;
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
#ifndef MAX
#define MAX(a, b)           \
  ({                        \
    __typeof__(a) _a = (a); \
    __typeof__(b) _b = (b); \
    _a > _b ? _a : _b;      \
  })
#define MIN(a, b)           \
  ({                        \
    __typeof__(a) _a = (a); \
    __typeof__(b) _b = (b); \
    _a < _b ? _a : _b;      \
  })
#endif

// This value is 2 ^ 18 - 1, and is used to clamp the RGB values before their
// ranges
// are normalized to eight bits.
static const int kMaxChannelValue = 262143;

static inline uint32_t YUV2RGB(int nY, int nU, int nV)
{
    nY -= 16;
    nU -= 128;
    nV -= 128;
    if (nY < 0) nY = 0;

    // This is the floating point equivalent. We do the conversion in integer
    // because some Android devices do not have floating point in hardware.
    // nR = (int)(1.164 * nY + 1.596 * nV);
    // nG = (int)(1.164 * nY - 0.813 * nV - 0.391 * nU);
    // nB = (int)(1.164 * nY + 2.018 * nU);

    int nR = (int) (1192 * nY + 1634 * nV);
    int nG = (int) (1192 * nY - 833 * nV - 400 * nU);
    int nB = (int) (1192 * nY + 2066 * nU);

    nR = MIN(kMaxChannelValue, MAX(0, nR));
    nG = MIN(kMaxChannelValue, MAX(0, nG));
    nB = MIN(kMaxChannelValue, MAX(0, nB));

    nR = (nR >> 10) & 0xff;
    nG = (nG >> 10) & 0xff;
    nB = (nB >> 10) & 0xff;

    return 0xff000000 | (nB << 16) | (nG << 8) | nR;
}

class ImageReaderListener
{
public:
    // count, acquire, validate, and delete AImage when a new image is available



    static void validateImageCb(void *obj, AImageReader *reader)
    {
        /**
        * This callback is called when there is a new image available for in the image reader's queue.
        *
        * <p>The callback happens on one dedicated thread per {@link AImageReader} instance. It is okay
        * to use AImageReader_* and AImage_* methods within the callback. Note that it is possible that
        * calling {@link AImageReader_acquireNextImage} or {@link AImageReader_acquireLatestImage}
        * returns {@link AMEDIA_IMGREADER_NO_BUFFER_AVAILABLE} within this callback. For example, when
        * there are multiple images and callbacks queued, if application called
        * {@link AImageReader_acquireLatestImage}, some images will be returned to system before their
        * corresponding callback is executed.</p>
        */

        LOGI("%s", __FUNCTION__);
        if (obj == nullptr)
        {
            return;
        }
        ImageReaderListener *thiz = reinterpret_cast<ImageReaderListener *>(obj);
        std::lock_guard<std::mutex> lock(thiz->mMutex);
        thiz->mOnImageAvailableCount++;
        AImage *img = nullptr;
        media_status_t ret = AImageReader_acquireNextImage(reader, &img);
        if (ret != AMEDIA_OK || img == nullptr)
        {
            LOGI("%s: acquire image from reader %p failed! ret: %d, img %p",
                 __FUNCTION__, reader, ret, img);
            return;
        }
        // TODO: validate image content
        int32_t format = -1;
        ret = AImage_getFormat(img, &format);
        if (ret != AMEDIA_OK || format == -1)
        {
            LOGI("%s: get format for image %p failed! ret: %d, format %d",
                 __FUNCTION__, img, ret, format);
        }
        // Save jpeg to SD card AIMAGE_FORMAT_YUV_420_888
        //  if (thiz->mDumpFilePathBase && format == AIMAGE_FORMAT_JPEG)

        // To-do
        if (format == AIMAGE_FORMAT_JPEG)
        {
            int32_t numPlanes = 0;
            ret = AImage_getNumberOfPlanes(img, &numPlanes);
            if (ret != AMEDIA_OK || numPlanes != 1)
            {
                LOGI("%s: get numPlanes for image %p failed! ret: %d, numPlanes %d",
                     __FUNCTION__, img, ret, numPlanes);
                AImage_delete(img);
                return;
            }
            int32_t width = -1, height = -1;
            ret = AImage_getWidth(img, &width);
            if (ret != AMEDIA_OK || width <= 0)
            {
                LOGI("%s: get width for image %p failed! ret: %d, width %d",
                     __FUNCTION__, img, ret, width);
                AImage_delete(img);
                return;
            }
            ret = AImage_getHeight(img, &height);
            if (ret != AMEDIA_OK || height <= 0)
            {
                LOGI("%s: get height for image %p failed! ret: %d, height %d",
                     __FUNCTION__, img, ret, height);
                AImage_delete(img);
                return;
            }
            uint8_t *data = nullptr;
            int dataLength = 0;
            ret = AImage_getPlaneData(img, /*planeIdx*/0, &data, &dataLength);
            if (ret != AMEDIA_OK || data == nullptr || dataLength <= 0)
            {
                LOGI("%s: get jpeg data for image %p failed! ret: %d, data %p, len %d",
                     __FUNCTION__, img, ret, data, dataLength);
                AImage_delete(img);
                return;
            }

            char dumpFilePath[512];
            strcpy(dumpFilePath, thiz->filenamecapture );
            strcat(dumpFilePath,"jpg" );
            LOGI("Writing jpeg file to %s", dumpFilePath);
            FILE *file = fopen(dumpFilePath, "w+");
            if (file != nullptr)
            {
                fwrite(data, 1, dataLength, file);
                fflush(file);
                fclose(file);
            }

        }
        else if ( format == AIMAGE_FORMAT_YUV_420_888)
        {
            int32_t numPlanes = 0;
            ret = AImage_getNumberOfPlanes(img, &numPlanes);
            if (ret != AMEDIA_OK || numPlanes != 3)
            {
                LOGI("%s: get numPlanes for image %p failed! ret: %d, numPlanes %d",
                     __FUNCTION__, img, ret, numPlanes);
                AImage_delete(img);
                return;
            }
            int32_t width = -1, height = -1;
            ret = AImage_getWidth(img, &width);
            if (ret != AMEDIA_OK || width <= 0)
            {
                LOGI("%s: get width for image %p failed! ret: %d, width %d",
                     __FUNCTION__, img, ret, width);
                AImage_delete(img);
                return;
            }
            ret = AImage_getHeight(img, &height);
            if (ret != AMEDIA_OK || height <= 0)
            {
                LOGI("%s: get height for image %p failed! ret: %d, height %d",
                     __FUNCTION__, img, ret, height);
                AImage_delete(img);
                return;
            }

            int32_t yStride, uvStride, rgbStride, *rgbPixel;
            uint8_t *yPixel, *uPixel, *vPixel ;
            int32_t yLen, uLen, vLen;
            int32_t uvPixelStride;
            uint8_t *data = nullptr;
            int dataLength = 0;

            AImage_getPlaneRowStride(img, 0, &yStride);
            AImage_getPlaneRowStride(img, 1, &uvStride);

            thiz->RGBBuffer_ = (int32_t *) malloc(width * height * 4);
            ASSERT(thiz->RGBBuffer_ != nullptr, "Failed to allocate RGBBuffer_");
            rgbPixel = thiz->RGBBuffer_ ;
            rgbStride = width  ;

            uint8_t * imageBuffer_ = (uint8_t *) malloc(width * height * 4);
            ASSERT(imageBuffer_ != nullptr, "Failed to allocate imageBuffer_");

            yPixel = imageBuffer_;
            AImage_getPlaneData(img, 0, &yPixel, &yLen);

            vPixel = imageBuffer_ + yLen;
            AImage_getPlaneData(img, 1, &vPixel, &vLen);

            uPixel = imageBuffer_ + yLen + vLen;
            AImage_getPlaneData(img, 2, &uPixel, &uLen);

            AImage_getPlanePixelStride(img, 1, &uvPixelStride);

            // swap up to down for YUV format
            rgbPixel += rgbStride * ( height - 1 ) ;
            for (int32_t y = 0; y < height; y++)
            {
                const uint8_t *pY = yPixel + yStride * y ;
                int32_t uv_row_start = uvStride * (y  >> 1);
                const uint8_t *pU = uPixel + uv_row_start ;
                const uint8_t *pV = vPixel + uv_row_start ;

                for (int32_t x = 0; x < width; x++)
                {
                    const int32_t uv_offset = (x >> 1) * uvPixelStride;
                    rgbPixel[x] = YUV2RGB(pY[x], pU[uv_offset], pV[uv_offset]);
                }
                rgbPixel -= rgbStride;
            }


            char dumpFilePath[512];
            strcpy(dumpFilePath, thiz->filenamecapture );
            strcat(dumpFilePath,"bmp" );
            LOGI("Writing jpeg file to %s", dumpFilePath);
            FILE *file = fopen(dumpFilePath, "w+");
            if (file != nullptr)
            {
                int filesize = 54 + width * height * 4;  //w is your image width, h is image height, both int

                unsigned char bmpfileheader[14] = {'B','M', 0,0,0,0, 0,0, 0,0, 54,0,0,0};
                unsigned char bmpinfoheader[40] = {40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 32,0};

                bmpfileheader[ 2] = (unsigned char)(filesize    );
                bmpfileheader[ 3] = (unsigned char)(filesize>> 8);
                bmpfileheader[ 4] = (unsigned char)(filesize>>16);
                bmpfileheader[ 5] = (unsigned char)(filesize>>24);

                bmpinfoheader[ 4] = (unsigned char)(       width    );
                bmpinfoheader[ 5] = (unsigned char)(       width>> 8);
                bmpinfoheader[ 6] = (unsigned char)(       width>>16);
                bmpinfoheader[ 7] = (unsigned char)(       width>>24);
                bmpinfoheader[ 8] = (unsigned char)(       height    );
                bmpinfoheader[ 9] = (unsigned char)(       height>> 8);
                bmpinfoheader[10] = (unsigned char)(       height>>16);
                bmpinfoheader[11] = (unsigned char)(       height>>24);

                fwrite(bmpfileheader,1,14,file);
                fwrite(bmpinfoheader,1,40,file);
                fwrite(thiz->RGBBuffer_,1,width * height * 4,file);

                fflush(file);
                fclose(file);
            }
            free(imageBuffer_) ;
        }
        AImage_delete(img);
    }

    // count, acquire image but not delete the image
    static void acquireImageCb(void *obj, AImageReader *reader)
    {
        LOGI("%s", __FUNCTION__);
        if (obj == nullptr)
        {
            return;
        }
        ImageReaderListener *thiz = reinterpret_cast<ImageReaderListener *>(obj);
        std::lock_guard<std::mutex> lock(thiz->mMutex);
        thiz->mOnImageAvailableCount++;
        // Acquire, but not closing.
        AImage *img = nullptr;
        media_status_t ret = AImageReader_acquireNextImage(reader, &img);
        if (ret != AMEDIA_OK || img == nullptr)
        {
            LOGI("%s: acquire image from reader %p failed! ret: %d, img %p",
                 __FUNCTION__, reader, ret, img);
            return;
        }
        return;
    }

    int onImageAvailableCount()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mOnImageAvailableCount;
    }

    void setDumpFilePathBase(const char *path)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        strcpy(mDumpFilePathBase, path) ;
    }

    void setFilenameCapture( char *pfilenamecapture)
    {
        std::lock_guard<std::mutex> lock(mMutex);

        std::string ss = std::string(mDumpFilePathBase) + "/" + pfilenamecapture + "." ;
        strcpy(filenamecapture, ss.c_str()) ;

    }
    ~ImageReaderListener()
    {
        if (RGBBuffer_) free(RGBBuffer_) ;
    }

private:
    // TODO: add mReader to make sure each listener is associated to one reader?
    std::mutex mMutex;
    int mOnImageAvailableCount = 0;
    char mDumpFilePathBase[512];
    char filenamecapture [512] ;
    int32_t* RGBBuffer_ = nullptr;
};

class CameraMetaDataInfo
{
public:
    explicit CameraMetaDataInfo(const ACameraMetadata *chars) : mChars(chars)
    {
    }

    bool isColorOutputSupported()
    {
        return isCapabilitySupported(ACAMERA_REQUEST_AVAILABLE_CAPABILITIES_BACKWARD_COMPATIBLE);
    }

    bool isCapabilitySupported(acamera_metadata_enum_android_request_available_capabilities_t cap)
    {
        ACameraMetadata_const_entry entry;
        ACameraMetadata_getConstEntry(mChars, ACAMERA_REQUEST_AVAILABLE_CAPABILITIES, &entry);
        for (uint32_t i = 0; i < entry.count; i++)
        {
            if (entry.data.u8[i] == cap)
            {
                return true;
            }
        }
        return false;
    }
    uint8_t get_metadata_af_state()
    {
        ACameraMetadata_const_entry entry;

        uint8_t af_state;
        cameraStatus = ACameraMetadata_getConstEntry(mChars, ACAMERA_CONTROL_AF_STATE, &entry);
        ASSERT(cameraStatus == ACAMERA_OK, "Failed to get_metadata_af_state (reason: %d)", cameraStatus);
        return entry.data.u8[0] ;

        /*
         * ACAMERA_CONTROL_AF_STATE_INACTIVE                                = 0,
         * ACAMERA_CONTROL_AF_STATE_PASSIVE_SCAN                            = 1,
         * ACAMERA_CONTROL_AF_STATE_PASSIVE_FOCUSED                         = 2,
         * ACAMERA_CONTROL_AF_STATE_ACTIVE_SCAN                             = 3,
         * ACAMERA_CONTROL_AF_STATE_FOCUSED_LOCKED                          = 4,
         * ACAMERA_CONTROL_AF_STATE_NOT_FOCUSED_LOCKED                      = 5,
         * ACAMERA_CONTROL_AF_STATE_PASSIVE_UNFOCUSED                       = 6,
         */

    }

private:
    const ACameraMetadata *mChars;
    camera_status_t cameraStatus;
};

extern std::map<int, const char* > ACAMERA_metadata_tagnamemap ;
class CameraMetaDataRequest
{
public:

    void get_metadata_all_tags()
    {
        int32_t numTags = 0;
        const uint32_t *tags = nullptr;
        cameraStatus = ACaptureRequest_getAllTags(mRequest, &numTags, &tags);
        ASSERT(cameraStatus == ACAMERA_OK, "Failed to ACaptureRequest_getAllTags (reason: %d)", cameraStatus);

        ACameraMetadata_const_entry entry;
        for (int tid = 0; tid < numTags; tid++)
        {
            uint32_t tagId = tags[tid];
            LOGI("%s capture request contains key %x\n", __FUNCTION__, tagId);
            // uint32_t sectionId = tagId >> 16;

            cameraStatus = ACaptureRequest_getConstEntry(mRequest, tagId, &entry);
            ASSERT(cameraStatus == ACAMERA_OK, "Failed to ACaptureRequest_getConstEntry(reason: %d)", cameraStatus);
            LOGI("TagName : %s, typebyte:%d, count:%d \n", ACAMERA_metadata_tagnamemap[tagId], entry.type, entry.count);
            if ( entry.type == 0 )
            {
                for(int i = 0 ; i < entry.count ;i ++)
                    LOGI("%d ", entry.data.u8[i]);
            }
            else if ( entry.type == 1 )
            {
                for(int i = 0 ; i < entry.count ;i ++)
                    LOGI("%d ", entry.data.i32[i]);
            }
            else if ( entry.type == 2 )
            {
                for(int i = 0 ; i < entry.count ;i ++)
                    LOGI("%f ", entry.data.f[i]);
            }
//            else if ( entry.type == 3 )
//            {
//                for(int i = 0 ; i < entry.count ;i ++)
//                    LOGI("%I64d ", entry.data.i64[i]);
//            }
            else if ( entry.type == 4 )
            {
                for(int i = 0 ; i < entry.count ;i ++)
                    LOGI("%f ", entry.data.d[i]);
            }

        }
    }

    bool set_metadata_control_mode( acamera_metadata_enum_android_control_mode_t t_control_mode)
    {
        // t_control_mode : ACAMERA_CONTROL_MODE_OFF, ACAMERA_CONTROL_MODE_AUTO
        ACameraMetadata_const_entry entry;

        uint8_t control_mode = t_control_mode;
        cameraStatus = ACaptureRequest_setEntry_u8( mRequest, ACAMERA_CONTROL_MODE, /*count*/ 1, &control_mode);
        ASSERT(cameraStatus == ACAMERA_OK, "ACaptureRequest_setEntry_u8 (reason: %d)", cameraStatus);

        // read and confirm
//        cameraStatus = ACaptureRequest_getConstEntry(mRequest, ACAMERA_CONTROL_MODE, &entry);
//        ASSERT(cameraStatus == ACAMERA_OK, "Failed to ACAMERA_CONTROL_MODE (reason: %d)", cameraStatus);
//        ASSERT(entry.data.u8[0] == control_mode, "E mode key is not updated");

        return true;
    }

    bool set_metadata_ae_mode( acamera_metadata_enum_android_control_ae_mode_t t_ae_mode)
    {
        // t_aeMode : ACAMERA_CONTROL_AE_MODE_OFF, ACAMERA_CONTROL_AE_MODE_ON
        ACameraMetadata_const_entry entry;

        // try set AE_MODE_ON
        uint8_t ae_mode = t_ae_mode;
        cameraStatus = ACaptureRequest_setEntry_u8( mRequest, ACAMERA_CONTROL_AE_MODE, /*count*/ 1, &ae_mode);
        ASSERT(cameraStatus == ACAMERA_OK, "ACaptureRequest_setEntry_u8 (reason: %d)", cameraStatus);

        // read and confirm
//        cameraStatus = ACaptureRequest_getConstEntry(mRequest, ACAMERA_CONTROL_AE_MODE, &entry);
//        ASSERT(cameraStatus == ACAMERA_OK, "Failed to ACAMERA_CONTROL_AE_MODE (reason: %d)", cameraStatus);
//        ASSERT(entry.data.u8[0] == ae_mode, "E mode key is not updated");

        return true;
    }

    bool set_metadata_af_mode( acamera_metadata_enum_android_control_af_mode_t t_af_mode)
    {
        // t_aeMode : ACAMERA_CONTROL_AF_MODE_OFF, ACAMERA_CONTROL_AF_MODE_AUTO, ACAMERA_CONTROL_AF_MODE_CONTINUOUS_PICTURE
        ACameraMetadata_const_entry entry;

        // try set AE_MODE_ON
        uint8_t af_mode = t_af_mode;
        cameraStatus = ACaptureRequest_setEntry_u8( mRequest, ACAMERA_CONTROL_AF_MODE, /*count*/ 1, &af_mode);
        ASSERT(cameraStatus == ACAMERA_OK, "ACaptureRequest_setEntry_u8 (reason: %d)", cameraStatus);

        // read and confirm
//        cameraStatus = ACaptureRequest_getConstEntry(mRequest, ACAMERA_CONTROL_AF_MODE, &entry);
//        ASSERT(cameraStatus == ACAMERA_OK, "Failed to ACAMERA_CONTROL_AF_MODE (reason: %d)", cameraStatus);
//        ASSERT(entry.data.u8[0] == af_mode, "E mode key is not updated");

        return true;
    }

    bool set_metadata_awb_mode( acamera_metadata_enum_acamera_control_awb_mode t_awb_mode)
    {
        // t_aeMode : ACAMERA_CONTROL_AWB_MODE_OFF, ACAMERA_CONTROL_AWB_MODE_AUTO
        ACameraMetadata_const_entry entry;

        // try set AE_MODE_ON
        uint8_t awb_mode = t_awb_mode;
        cameraStatus = ACaptureRequest_setEntry_u8( mRequest, ACAMERA_CONTROL_AWB_MODE,  1, &awb_mode);
        ASSERT(cameraStatus == ACAMERA_OK, "ACaptureRequest_setEntry_u8 (reason: %d)", cameraStatus);

        // read and confirm
//        cameraStatus = ACaptureRequest_getConstEntry(mRequest, ACAMERA_CONTROL_AWB_MODE, &entry);
//        ASSERT(cameraStatus == ACAMERA_OK, "Failed to ACAMERA_CONTROL_AWB_MODE (reason: %d)", cameraStatus);
//        ASSERT(entry.data.u8[0] == awb_mode, "E mode key is not updated");

        return true;
    }

    bool set_metadata_lens_focus_distance( float t_focus_distance)
    {
        ACameraMetadata_const_entry entry;

        // try set AE_MODE_ON
        float focus_distance = t_focus_distance;
        cameraStatus = ACaptureRequest_setEntry_float(mRequest, ACAMERA_LENS_FOCUS_DISTANCE, 1,
                                                      &focus_distance);
        ASSERT(cameraStatus == ACAMERA_OK, "ACaptureRequest_setEntry_float (reason: %d)",
               cameraStatus);

        // read and confirm
//        cameraStatus = ACaptureRequest_getConstEntry(mRequest, ACAMERA_LENS_FOCUS_DISTANCE, &entry);
//        ASSERT(cameraStatus == ACAMERA_OK, "Failed to ACAMERA_LENS_FOCUS_DISTANCE (reason: %d)", cameraStatus);
//        ASSERT(entry.data.f[0] == focus_distance, "E mode key is not updated");

        return true;
    }
public:

    camera_status_t cameraStatus;
    ACaptureRequest *mRequest;

};


class CameraCaptureListener
{
public:

    //2.  prototype : ACameraCaptureSession_captureCallback_start
    static void onCaptureStarted(void* context, ACameraCaptureSession* session,const ACaptureRequest* request, int64_t timestamp)
    {
        LOGI("onCaptureStarted");
    }

    //1. prototype : ACameraCaptureSession_captureCallback_result
    static void onCaptureProgressed(void* context, ACameraCaptureSession* session, ACaptureRequest* request, const ACameraMetadata* result)
    {
        CameraMetaDataInfo info(result);
        if(result)
            LOGI("onCaptureProgressed status: %d", info.get_metadata_af_state());
        else
            LOGI("onCaptureProgressed");
    }

    //3.  prototype : ACameraCaptureSession_captureCallback_result
    static void onCaptureCompleted(void* context, ACameraCaptureSession* session, ACaptureRequest* request, const ACameraMetadata* result)
    {
        CameraMetaDataInfo info(result);
        if(result)
            LOGI("onCaptureCompleted status: %d", info.get_metadata_af_state());
        else
            LOGI("onCaptureCompleted");
    }

    // prototype : ACameraCaptureSession_captureCallback_failed
    static void onCaptureFailed(void* context, ACameraCaptureSession* session, ACaptureRequest* request, ACameraCaptureFailure* failure)
    {
        LOGI("onCaptureFailed");
    }

    //4.  prototype : ACameraCaptureSession_captureCallback_sequenceEnd
    static void onCaptureSequenceCompleted(void* context, ACameraCaptureSession* session,int sequenceId, int64_t frameNumber)
    {
        LOGI("onCaptureSequenceCompleted");
    }

    // prototype : ACameraCaptureSession_captureCallback_sequenceAbort
    static void onCaptureSequenceAborted(void* context, ACameraCaptureSession* session,int sequenceId)
    {
        LOGI("onCaptureSequenceAborted");
    }

    // prototype : ACameraCaptureSession_captureCallback_bufferLost
    static void onCaptureBufferLost(void* context, ACameraCaptureSession* session,ACaptureRequest* request, ANativeWindow* window, int64_t frameNumber)
    {
        LOGI("onCaptureBufferLost");
    }

public:
};

class PreviewTestCase
{

public:
    ACameraManager *createManager()
    {
        if (!mCameraManager)
        {
            mCameraManager = ACameraManager_create();
        }
        return mCameraManager;
    }

    CameraServiceListener mServiceListener;
    ACameraManager_AvailabilityCallbacks mServiceCb{
            &mServiceListener,
            CameraServiceListener::onAvailable,
            CameraServiceListener::onUnavailable
    };
    CameraDeviceListener mDeviceListener;
    ACameraDevice_StateCallbacks mDeviceCb{
            &mDeviceListener,
            CameraDeviceListener::onDisconnected,
            CameraDeviceListener::onError
    };
    CaptureSessionListener mSessionListener;
    ACameraCaptureSession_stateCallbacks mSessionCb{
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

    CameraMetaDataRequest mcamerametadata ;

    ACameraIdList *mCameraIdList = nullptr;
    ACameraDevice *mDevice = nullptr;
    AImageReader *mImgReader = nullptr;
    ANativeWindow *mImgReaderAnw = nullptr;
    ANativeWindow *mPreviewAnw = nullptr;
    ACameraManager *mCameraManager = nullptr;
    ACaptureSessionOutputContainer *mOutputs = nullptr;
    ACaptureSessionOutput *mPreviewOutput = nullptr;
    ACaptureSessionOutput *mImgReaderOutput = nullptr;
    ACameraCaptureSession *mSession = nullptr;
    ACaptureRequest *mPreviewRequest = nullptr;
    ACaptureRequest *mStillRequest = nullptr;
    ACameraOutputTarget *mReqPreviewOutput = nullptr;
    ACameraOutputTarget *mReqImgReaderOutput = nullptr;
    const char *mCameraId;
    bool mMgrInited = false; // cameraId, serviceListener
    bool mImgReaderInited = false;
    bool mPreviewInited = false;
public:
    ~PreviewTestCase()
    {
        resetCamera();
        deInit();
        if (mCameraManager)
        {
            ACameraManager_delete(mCameraManager);
            mCameraManager = nullptr;
        }
    }

    PreviewTestCase()
    {
        // create is guaranteed to succeed;
        createManager();
    }

    // Free all resources except camera manager
    void resetCamera()
    {
        mSessionListener.reset();
        if (mSession)
        {
            ACameraCaptureSession_close(mSession);
            mSession = nullptr;
        }
        if (mDevice)
        {
            ACameraDevice_close(mDevice);
            mDevice = nullptr;
        }
        if (mImgReader)
        {
            AImageReader_delete(mImgReader);
            // No need to call ANativeWindow_release on imageReaderAnw
            mImgReaderAnw = nullptr;
            mImgReader = nullptr;
        }
        if (mPreviewAnw)
        {
            ANativeWindow_release(mPreviewAnw);
            mPreviewAnw = nullptr;
        }
        if (mOutputs)
        {
            ACaptureSessionOutputContainer_free(mOutputs);
            mOutputs = nullptr;
        }
        if (mPreviewOutput)
        {
            ACaptureSessionOutput_free(mPreviewOutput);
            mPreviewOutput = nullptr;
        }
        if (mImgReaderOutput)
        {
            ACaptureSessionOutput_free(mImgReaderOutput);
            mImgReaderOutput = nullptr;
        }
        if (mPreviewRequest)
        {
            ACaptureRequest_free(mPreviewRequest);
            mPreviewRequest = nullptr;
        }
        if (mStillRequest)
        {
            ACaptureRequest_free(mStillRequest);
            mStillRequest = nullptr;
        }
        if (mReqPreviewOutput)
        {
            ACameraOutputTarget_free(mReqPreviewOutput);
            mReqPreviewOutput = nullptr;
        }
        if (mReqImgReaderOutput)
        {
            ACameraOutputTarget_free(mReqImgReaderOutput);
            mReqImgReaderOutput = nullptr;
        }
        mImgReaderInited = false;
        mPreviewInited = false;
    }

    camera_status_t initWithErrorLog()
    {
        camera_status_t ret = ACameraManager_getCameraIdList(
                mCameraManager, &mCameraIdList);
        if (ret != ACAMERA_OK)
        {
            LOG_ERROR(errorString, "Get camera id list failed: ret %d", ret);
            return ret;
        }
        ret = ACameraManager_registerAvailabilityCallback(mCameraManager, &mServiceCb);
        if (ret != ACAMERA_OK)
        {
            LOG_ERROR(errorString, "Register availability callback failed: ret %d", ret);
            return ret;
        }
        mMgrInited = true;
        return ACAMERA_OK;
    }

    camera_status_t deInit()
    {
        if (!mMgrInited)
        {
            return ACAMERA_OK;
        }
        camera_status_t ret = ACameraManager_unregisterAvailabilityCallback(
                mCameraManager, &mServiceCb);
        if (ret != ACAMERA_OK)
        {
            LOGI("Unregister availability callback failed: ret %d", ret);
            return ret;
        }
        if (mCameraIdList)
        {
            ACameraManager_deleteCameraIdList(mCameraIdList);
            mCameraIdList = nullptr;
        }
        mMgrInited = false;
        return ACAMERA_OK;
    }

    int getNumCameras()
    {
        if (!mMgrInited || !mCameraIdList)
        {
            return -1;
        }
        return mCameraIdList->numCameras;
    }

    const char *getCameraId(int idx)
    {
        if (!mMgrInited || !mCameraIdList || idx < 0 || idx >= mCameraIdList->numCameras)
        {
            return nullptr;
        }
        return mCameraIdList->cameraIds[idx];
    }

    camera_status_t openCamera(const char *cameraId)
    {
        if (mDevice)
        {
            LOGI("Cannot open camera before closing previously open one");
            return ACAMERA_ERROR_INVALID_PARAMETER;
        }
        mCameraId = cameraId;
        return ACameraManager_openCamera(mCameraManager, cameraId, &mDeviceCb, &mDevice);
    }

    camera_status_t closeCamera()
    {
        camera_status_t ret = ACameraDevice_close(mDevice);
        mDevice = nullptr;
        return ret;
    }

    bool isCameraAvailable(const char *cameraId)
    {
        if (!mMgrInited)
        {
            LOGI("Camera service listener has not been registered!");
        }
        return mServiceListener.isAvailable(cameraId);
    }

    media_status_t initImageReaderWithErrorLog(
            int32_t width, int32_t height, int32_t format, int32_t maxImages,
            AImageReader_ImageListener *listener)
    {
        if (mImgReader || mImgReaderAnw)
        {
            LOG_ERROR(errorString, "Cannot init image reader before closing existing one");
            return AMEDIA_ERROR_UNKNOWN;
        }
        media_status_t ret = AImageReader_new(
                width, height, format,
                maxImages, &mImgReader);
        if (ret != AMEDIA_OK)
        {
            LOG_ERROR(errorString, "Create image reader. ret %d", ret);
            return ret;
        }
        if (mImgReader == nullptr)
        {
            LOG_ERROR(errorString, "null image reader created");
            return AMEDIA_ERROR_UNKNOWN;
        }
        ret = AImageReader_setImageListener(mImgReader, listener);
        if (ret != AMEDIA_OK)
        {
            LOG_ERROR(errorString, "Set AImageReader listener failed. ret %d", ret);
            return ret;
        }
        ret = AImageReader_getWindow(mImgReader, &mImgReaderAnw);
        if (ret != AMEDIA_OK)
        {
            LOG_ERROR(errorString, "AImageReader_getWindow failed. ret %d", ret);
            return ret;
        }
        if (mImgReaderAnw == nullptr)
        {
            LOG_ERROR(errorString, "Null ANW from AImageReader!");
            return AMEDIA_ERROR_UNKNOWN;
        }
        mImgReaderInited = true;
        return AMEDIA_OK;
    }

    ANativeWindow *initPreviewAnw(JNIEnv *env, jobject jSurface)
    {
        if (mPreviewAnw)
        {
            LOGI("Cannot init preview twice!");
            return nullptr;
        }
        mPreviewAnw = ANativeWindow_fromSurface(env, jSurface);
        mPreviewInited = true;
        return mPreviewAnw;
    }

    camera_status_t createCaptureSessionWithLog()
    {
        if (mSession)
        {
            LOG_ERROR(errorString, "Cannot create session before closing existing one");
            return ACAMERA_ERROR_UNKNOWN;
        }
        if (!mMgrInited || (!mImgReaderInited && !mPreviewInited))
        {
            LOG_ERROR(errorString, "Cannot create session. mgrInit %d readerInit %d previewInit %d",
                      mMgrInited, mImgReaderInited, mPreviewInited);
            return ACAMERA_ERROR_UNKNOWN;
        }
        camera_status_t ret = ACaptureSessionOutputContainer_create(&mOutputs);
        if (ret != ACAMERA_OK)
        {
            LOG_ERROR(errorString, "Create capture session output container failed. ret %d", ret);
            return ret;
        }
        if (mImgReaderInited)
        {
            ret = ACaptureSessionOutput_create(mImgReaderAnw, &mImgReaderOutput);
            if (ret != ACAMERA_OK || mImgReaderOutput == nullptr)
            {
                LOG_ERROR(errorString,
                          "Sesssion image reader output create fail! ret %d output %p",
                          ret, mImgReaderOutput);
                if (ret == ACAMERA_OK)
                {
                    ret = ACAMERA_ERROR_UNKNOWN; // ret OK but output is null
                }
                return ret;
            }
            ret = ACaptureSessionOutputContainer_add(mOutputs, mImgReaderOutput);
            if (ret != ACAMERA_OK)
            {
                LOG_ERROR(errorString, "Sesssion image reader output add failed! ret %d", ret);
                return ret;
            }
        }
        if (mPreviewInited)
        {
            ret = ACaptureSessionOutput_create(mPreviewAnw, &mPreviewOutput);
            if (ret != ACAMERA_OK || mPreviewOutput == nullptr)
            {
                LOG_ERROR(errorString,
                          "Sesssion preview output create fail! ret %d output %p",
                          ret, mPreviewOutput);
                if (ret == ACAMERA_OK)
                {
                    ret = ACAMERA_ERROR_UNKNOWN; // ret OK but output is null
                }
                return ret;
            }
            ret = ACaptureSessionOutputContainer_add(mOutputs, mPreviewOutput);
            if (ret != ACAMERA_OK)
            {
                LOG_ERROR(errorString, "Sesssion preview output add failed! ret %d", ret);
                return ret;
            }
        }
        ret = ACameraDevice_createCaptureSession(
                mDevice, mOutputs, &mSessionCb, &mSession);
        if (ret != ACAMERA_OK || mSession == nullptr)
        {
            LOG_ERROR(errorString, "Create session for camera %s failed. ret %d session %p",
                      mCameraId, ret, mSession);
            if (ret == ACAMERA_OK)
            {
                ret = ACAMERA_ERROR_UNKNOWN; // ret OK but session is null
            }
            return ret;
        }
        return ACAMERA_OK;
    }

    void closeSession()
    {
        if (mSession != nullptr)
        {
            ACameraCaptureSession_close(mSession);
        }
        if (mOutputs)
        {
            ACaptureSessionOutputContainer_free(mOutputs);
            mOutputs = nullptr;
        }
        if (mPreviewOutput)
        {
            ACaptureSessionOutput_free(mPreviewOutput);
            mPreviewOutput = nullptr;
        }
        if (mImgReaderOutput)
        {
            ACaptureSessionOutput_free(mImgReaderOutput);
            mImgReaderOutput = nullptr;
        }
        mSession = nullptr;
    }

    camera_status_t createRequestsWithErrorLog()
    {
        if (mPreviewRequest || mStillRequest)
        {
            LOG_ERROR(errorString, "Cannot create requests before deleteing existing one");
            return ACAMERA_ERROR_UNKNOWN;
        }
        if (mDevice == nullptr || (!mPreviewInited && !mImgReaderInited))
        {
            LOG_ERROR(errorString,
                      "Cannot create request. device %p previewInit %d readeInit %d",
                      mDevice, mPreviewInited, mImgReaderInited);
            return ACAMERA_ERROR_UNKNOWN;
        }
        camera_status_t ret;
        if (mPreviewInited)
        {
            ret = ACameraDevice_createCaptureRequest(
                    mDevice, TEMPLATE_PREVIEW, &mPreviewRequest);
            if (ret != ACAMERA_OK)
            {
                LOG_ERROR(errorString, "Camera %s create preview request failed. ret %d",
                          mCameraId, ret);
                return ret;
            }
            ret = ACameraOutputTarget_create(mPreviewAnw, &mReqPreviewOutput);
            if (ret != ACAMERA_OK)
            {
                LOG_ERROR(errorString,
                          "Camera %s create request preview output target failed. ret %d",
                          mCameraId, ret);
                return ret;
            }
            ret = ACaptureRequest_addTarget(mPreviewRequest, mReqPreviewOutput);
            if (ret != ACAMERA_OK)
            {
                LOG_ERROR(errorString, "Camera %s add preview request output failed. ret %d",
                          mCameraId, ret);
                return ret;
            }

            // print out the camera characteric
            mcamerametadata.mRequest = mPreviewRequest;
            //mcamerametadata.get_metadata_all_tags();
            //mcamerametadata.set_metadata_control_mode(ACAMERA_CONTROL_MODE_AUTO) ;
            mcamerametadata.set_metadata_ae_mode(ACAMERA_CONTROL_AE_MODE_ON);
            mcamerametadata.set_metadata_awb_mode(ACAMERA_CONTROL_AWB_MODE_AUTO) ;
            mcamerametadata.set_metadata_af_mode(ACAMERA_CONTROL_AF_MODE_OFF);
            mcamerametadata.set_metadata_lens_focus_distance(0.0f) ;

        }
        else
        {
            LOGI("Preview not inited. Will not create preview request!");
        }
        if (mImgReaderInited)
        {
            ret = ACameraDevice_createCaptureRequest(
                    mDevice, TEMPLATE_STILL_CAPTURE, &mStillRequest);
            if (ret != ACAMERA_OK)
            {
                LOG_ERROR(errorString, "Camera %s create still request failed. ret %d",
                          mCameraId, ret);
                return ret;
            }
            ret = ACameraOutputTarget_create(mImgReaderAnw, &mReqImgReaderOutput);
            if (ret != ACAMERA_OK)
            {
                LOG_ERROR(errorString,
                          "Camera %s create request reader output target failed. ret %d",
                          mCameraId, ret);
                return ret;
            }
            ret = ACaptureRequest_addTarget(mStillRequest, mReqImgReaderOutput);
            if (ret != ACAMERA_OK)
            {
                LOG_ERROR(errorString, "Camera %s add still request output failed. ret %d",
                          mCameraId, ret);
                return ret;
            }
            if (mPreviewInited)
            {
                ret = ACaptureRequest_addTarget(mStillRequest, mReqPreviewOutput);
                if (ret != ACAMERA_OK)
                {
                    LOG_ERROR(errorString,
                              "Camera %s add still request preview output failed. ret %d",
                              mCameraId, ret);
                    return ret;
                }
            }
        }
        else
        {
            LOGI("AImageReader not inited. Will not create still request!");
        }
        return ACAMERA_OK;
    }

    camera_status_t startPreview()
    {
        if (mSession == nullptr || mPreviewRequest == nullptr)
        {
            LOGI("Testcase cannot start preview: session %p, preview request %p",
                 mSession, mPreviewRequest);
            return ACAMERA_ERROR_UNKNOWN;
        }
        int previewSeqId;
        return ACameraCaptureSession_setRepeatingRequest(
                mSession, nullptr, 1, &mPreviewRequest, &previewSeqId);
    }

    camera_status_t takePicture()
    {
        if (mSession == nullptr || mStillRequest == nullptr)
        {
            LOGI("Testcase cannot take picture: session %p, still request %p",
                 mSession, mStillRequest);
            return ACAMERA_ERROR_UNKNOWN;
        }
        int seqId;
        return ACameraCaptureSession_capture(
                mSession, &m_capture_session_capture_callbacks, 1, &mStillRequest, &seqId);
    }

    camera_status_t resetWithErrorLog()
    {
        camera_status_t ret;
        closeSession();
        for (int i = 0; i < 50; i++)
        {
            usleep(100000); // sleep 100ms
            if (mSessionListener.isClosed())
            {
                LOGI("Session take ~%d ms to close", i * 100);
                break;
            }
        }
        if (!mSessionListener.isClosed() || mSessionListener.onClosedCount() != 1)
        {
            LOG_ERROR(errorString,
                      "Session for camera %s close error. isClosde %d close count %d",
                      mCameraId, mSessionListener.isClosed(), mSessionListener.onClosedCount());
            return ACAMERA_ERROR_UNKNOWN;
        }
        mSessionListener.reset();
        ret = closeCamera();
        if (ret != ACAMERA_OK)
        {
            LOG_ERROR(errorString, "Close camera device %s failure. ret %d", mCameraId, ret);
            return ret;
        }
        resetCamera();
        return ACAMERA_OK;
    }

    CaptureSessionListener *getSessionListener()
    {
        return &mSessionListener;
    }

};

//jint throwAssertionError(JNIEnv *env, const char *message)
//{
//    jclass assertionClass;
//    const char *className = "junit/framework/AssertionFailedError";
//    assertionClass = env->FindClass(className);
//    if (assertionClass == nullptr)
//    {
//        LOGI("Native throw error: cannot find class %s", className);
//        return -1;
//    }
//    return env->ThrowNew(assertionClass, message);
//}



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



    const char *m_selected_camera_id = NULL;
    bool m_camera_ready;

    int32_t backup_width = 480;
    int32_t backup_height = 720;
};







#endif //OPENCV_NDK_NATIVE_CAMERA_H
