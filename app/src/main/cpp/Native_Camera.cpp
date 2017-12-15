#include "Native_Camera.h"

class StaticInfo
{
public:
    explicit StaticInfo(ACameraMetadata *chars) : mChars(chars)
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

    bool set_metadata_control_mode(ACaptureRequest *request, acamera_metadata_enum_android_control_mode_t t_control_mode)
    {
        // t_control_mode : ACAMERA_CONTROL_MODE_OFF, ACAMERA_CONTROL_MODE_AUTO
        ACameraMetadata_const_entry entry;
        cameraStatus = ACaptureRequest_getConstEntry(request, ACAMERA_CONTROL_MODE, &entry);
        ASSERT(cameraStatus == ACAMERA_OK, "Failed to ACAMERA_CONTROL_MODE (reason: %d)", cameraStatus);
        ASSERT(entry.tag == ACAMERA_CONTROL_MODE && entry.type == ACAMERA_TYPE_BYTE && entry.count != 1,
               "ACameraMetadata_const_entry failed");

        uint8_t control_mode = t_control_mode;
        cameraStatus = ACaptureRequest_setEntry_u8( request, ACAMERA_CONTROL_MODE, /*count*/ 1, &control_mode);
        ASSERT(cameraStatus == ACAMERA_OK, "ACaptureRequest_setEntry_u8 (reason: %d)", cameraStatus);

        // read and confirm
        cameraStatus = ACaptureRequest_getConstEntry(request, ACAMERA_CONTROL_MODE, &entry);
        ASSERT(cameraStatus == ACAMERA_OK, "Failed to ACAMERA_CONTROL_MODE (reason: %d)", cameraStatus);
        ASSERT(entry.data.u8[0] == control_mode, "E mode key is not updated");

        return true;
    }

    bool set_metadata_ae_mode(ACaptureRequest *request, acamera_metadata_enum_android_control_ae_mode_t t_ae_mode)
    {
        // t_aeMode : ACAMERA_CONTROL_AE_MODE_OFF, ACAMERA_CONTROL_AE_MODE_ON
        ACameraMetadata_const_entry entry;
        cameraStatus = ACaptureRequest_getConstEntry(request, ACAMERA_CONTROL_AE_MODE, &entry);
        ASSERT(cameraStatus == ACAMERA_OK, "Failed to ACAMERA_CONTROL_AE_MODE (reason: %d)", cameraStatus);
        ASSERT(entry.tag == ACAMERA_CONTROL_AE_MODE && entry.type == ACAMERA_TYPE_BYTE && entry.count != 1,
               "ACameraMetadata_const_entry failed");

        // try set AE_MODE_ON
        uint8_t ae_mode = t_ae_mode;
        cameraStatus = ACaptureRequest_setEntry_u8( request, ACAMERA_CONTROL_AE_MODE, /*count*/ 1, &ae_mode);
        ASSERT(cameraStatus == ACAMERA_OK, "ACaptureRequest_setEntry_u8 (reason: %d)", cameraStatus);

        // read and confirm
        cameraStatus = ACaptureRequest_getConstEntry(request, ACAMERA_CONTROL_AE_MODE, &entry);
        ASSERT(cameraStatus == ACAMERA_OK, "Failed to ACAMERA_CONTROL_AE_MODE (reason: %d)", cameraStatus);
        ASSERT(entry.data.u8[0] == ae_mode, "E mode key is not updated");

        return true;
    }

    bool set_metadata_af_mode(ACaptureRequest *request, acamera_metadata_enum_android_control_af_mode_t t_af_mode)
    {
        // t_aeMode : ACAMERA_CONTROL_AF_MODE_OFF, ACAMERA_CONTROL_AF_MODE_AUTO
        ACameraMetadata_const_entry entry;
        cameraStatus = ACaptureRequest_getConstEntry(request, ACAMERA_CONTROL_AF_MODE, &entry);
        ASSERT(cameraStatus == ACAMERA_OK, "Failed to ACAMERA_CONTROL_AF_MODE (reason: %d)", cameraStatus);
        ASSERT(entry.tag == ACAMERA_CONTROL_AF_MODE && entry.type == ACAMERA_TYPE_BYTE && entry.count != 1,
               "ACameraMetadata_const_entry failed");

        // try set AE_MODE_ON
        uint8_t af_mode = t_af_mode;
        cameraStatus = ACaptureRequest_setEntry_u8( request, ACAMERA_CONTROL_AF_MODE, /*count*/ 1, &af_mode);
        ASSERT(cameraStatus == ACAMERA_OK, "ACaptureRequest_setEntry_u8 (reason: %d)", cameraStatus);

        // read and confirm
        cameraStatus = ACaptureRequest_getConstEntry(request, ACAMERA_CONTROL_AF_MODE, &entry);
        ASSERT(cameraStatus == ACAMERA_OK, "Failed to ACAMERA_CONTROL_AF_MODE (reason: %d)", cameraStatus);
        ASSERT(entry.data.u8[0] == af_mode, "E mode key is not updated");

        return true;
    }

private:
    const ACameraMetadata *mChars;
    camera_status_t cameraStatus;
};


Native_Camera::Native_Camera(camera_type type)
{

    camera_status_t cameraStatus = ACAMERA_OK;

    m_camera_manager = ACameraManager_create();

    cameraStatus =
            ACameraManager_getCameraIdList(m_camera_manager, &m_camera_id_list);
    ASSERT(cameraStatus == ACAMERA_OK,
           "Failed to get camera id list (reason: %d)", cameraStatus);
    ASSERT(m_camera_id_list->numCameras > 0, "No camera device detected");

    // ASSUMPTION: Back camera is index[0] and front is index[1]
    // TODO - why I need orientation as is below
    if (type == BACK_CAMERA)
    {
        m_selected_camera_id = m_camera_id_list->cameraIds[0];
        m_camera_orientation = 90;
    }
    else
    {
        ASSERT(m_camera_id_list->numCameras > 1, "No dual camera setup");
        m_selected_camera_id = m_camera_id_list->cameraIds[1];
        m_camera_orientation = 270;
    }

    cameraStatus = ACameraManager_getCameraCharacteristics(
            m_camera_manager, m_selected_camera_id, &cameraMetadata);
    ASSERT(cameraStatus == ACAMERA_OK, "Failed to get camera meta data of ID: %s",
           m_selected_camera_id);

    cameraStatus =
            ACameraManager_openCamera(m_camera_manager, m_selected_camera_id,
                                      &m_device_state_callbacks, &m_camera_device);
    ASSERT(cameraStatus == ACAMERA_OK, "Failed to open camera device (id: %s)",
           m_selected_camera_id);

    m_camera_ready = true;
}

Native_Camera::~Native_Camera()
{
    if (m_capture_request != nullptr)
    {
        ACaptureRequest_free(m_capture_request);
        m_capture_request = nullptr;
    }

    if (m_camera_output_target != nullptr)
    {
        ACameraOutputTarget_free(m_camera_output_target);
        m_camera_output_target = nullptr;
    }

    if (m_camera_device != nullptr)
    {
        ACameraDevice_close(m_camera_device);
        m_camera_device = nullptr;
    }

    ACaptureSessionOutputContainer_remove(m_capture_session_output_container,
                                          m_session_output);
    if (m_session_output != nullptr)
    {
        ACaptureSessionOutput_free(m_session_output);
        m_session_output = nullptr;
    }

    if (m_capture_session_output_container != nullptr)
    {
        ACaptureSessionOutputContainer_free(m_capture_session_output_container);
        m_capture_session_output_container = nullptr;
    }

    if (m_camera_id_list != nullptr)
    {
        ACameraManager_deleteCameraIdList(m_camera_id_list);
        m_camera_id_list = nullptr;
    }

    if (cameraMetadata)
    {
        ACameraMetadata_free(cameraMetadata);
        cameraMetadata = nullptr;
    }

    ACameraManager_delete(m_camera_manager);
}

bool Native_Camera::MatchCaptureSizeRequest(ImageFormat *resView, int32_t width, int32_t height)
{
    Display_Dimension disp(width, height);
    if (m_camera_orientation == 90 || m_camera_orientation == 270)
    {
        disp.Flip();
    }

    ACameraMetadata_const_entry entry;
    ACameraMetadata_getConstEntry(
            cameraMetadata, ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, &entry);
    // format of the data: format, width, height, input?, type int32
    bool foundIt = false;
    Display_Dimension foundRes(1000, 1000); // max resolution for current gen phones

    for (int i = 0; i < entry.count; ++i)
    {
        int32_t input = entry.data.i32[i * 4 + 3];
        int32_t format = entry.data.i32[i * 4 + 0];
        if (input) continue;

        if (format == AIMAGE_FORMAT_YUV_420_888 || format == AIMAGE_FORMAT_JPEG)
        {
            Display_Dimension res(entry.data.i32[i * 4 + 1],
                                  entry.data.i32[i * 4 + 2]);
            if (!disp.IsSameRatio(res)) continue;
            if (foundRes > res)
            {
                foundIt = true;
                foundRes = res;
            }
        }
    }

    if (foundIt)
    {
        resView->width = foundRes.org_width();
        resView->height = foundRes.org_height();
    }
    else
    {
        if (disp.IsPortrait())
        {
            resView->width = 480;
            resView->height = 640;
        }
        else
        {
            resView->width = 640;
            resView->height = 480;
        }
    }
    resView->format = AIMAGE_FORMAT_YUV_420_888;
    LOGI("--- W -- H -- %d -- %d", resView->width, resView->height);
    return foundIt;
}

bool Native_Camera::CreateCaptureSession(ANativeWindow *window)
{

    camera_status_t cameraStatus = ACAMERA_OK;

    // TEMPLATE_RECORD because rather have post-processing quality for more
    // accureate CV algo
    // Frame rate should be good since all image buffers are being done from
    // native side

    // camera device에 요청할 capture_reauest을 만든다.
    cameraStatus = ACameraDevice_createCaptureRequest(m_camera_device,
                                                      TEMPLATE_PREVIEW, &m_capture_request);
    ASSERT(cameraStatus == ACAMERA_OK,
           "Failed to create preview capture request (id: %s)",
           m_selected_camera_id);

    // theNativeWindow 을 대변하는  cameraOutputTarget 을 만들고
    ACameraOutputTarget_create(window, &m_camera_output_target);
    // camera 에서 만든 captureRequest 을 output target(window)와 연결한다.
    ACaptureRequest_addTarget(m_capture_request, m_camera_output_target);

    // setting automode
    StaticInfo staticinfo (cameraMetadata) ;
    staticinfo.set_metadata_control_mode(m_capture_request, ACAMERA_CONTROL_MODE_AUTO);
    staticinfo.set_metadata_af_mode(m_capture_request, ACAMERA_CONTROL_AF_MODE_AUTO);
    staticinfo.set_metadata_ae_mode(m_capture_request, ACAMERA_CONTROL_AE_MODE_ON);

    // 주어진 ANativeWindow 객체에 대한 참조를 얻습니다. 참조가 제거될 때까지 객체가 삭제되지 못하게 합니다
    ANativeWindow_acquire(window);

    //  destination인 navtiveWindows 에서 session 관리하는 객체를 만든다.
    ACaptureSessionOutput_create(window, &m_session_output);

    ACaptureSessionOutputContainer_create(&m_capture_session_output_container);
    ACaptureSessionOutputContainer_add(m_capture_session_output_container, m_session_output);

    ACameraDevice_createCaptureSession(
            m_camera_device, m_capture_session_output_container,
            &m_capture_session_state_callbacks, &m_capture_session);

    ACameraCaptureSession_setRepeatingRequest(m_capture_session, &m_capture_session_capture_callbacks, 1,
                                              &m_capture_request, nullptr);

    return true;
}