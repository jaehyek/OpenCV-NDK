#include "Native_Camera.h"


extern const char* ACAMERA_metadata_tagnamelist[];

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
    Display_Dimension foundRes(400, 300); // max resolution for current gen phones

    // width/height = 4/3 인 것만 다룬다. 그리고 큰 것을 선택한다.

    for (int i = 0; i < entry.count; ++i)
    {
        int32_t input = entry.data.i32[i * 4 + 3];
        int32_t format = entry.data.i32[i * 4 + 0];
        if (input) continue;

        // 우리는 YUV만 취급한다.
        if (format == AIMAGE_FORMAT_YUV_420_888 )
        {
            Display_Dimension res(entry.data.i32[i * 4 + 1],
                                  entry.data.i32[i * 4 + 2]);

            LOGI("--- W -- H -- %d -- %d -- %f", res.width(), res.height(), (float)res.width()/res.height());


            if (!foundRes.IsSameRatio(res)) continue;
            if (res > foundRes)
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
            resView->width = 3000;
            resView->height = 4000;
        }
        else
        {
            resView->width = 4000;
            resView->height = 3000;
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
//    StaticInfo staticinfo (cameraMetadata) ;
//    staticinfo.set_metadata_control_mode(m_capture_request, ACAMERA_CONTROL_MODE_AUTO);
//    staticinfo.set_metadata_af_mode(m_capture_request, ACAMERA_CONTROL_AF_MODE_AUTO);
//    staticinfo.set_metadata_ae_mode(m_capture_request, ACAMERA_CONTROL_AE_MODE_ON);

    // 주어진 ANativeWindow 객체에 대한 참조를 얻습니다. 참조가 제거될 때까지 객체가 삭제되지 못하게 합니다
    ANativeWindow_acquire(window);

    //  destination인 navtiveWindows 에서 session 관리하는 객체를 만든다.
    ACaptureSessionOutput_create(window, &m_session_output);

    ACaptureSessionOutputContainer_create(&m_capture_session_output_container);
    ACaptureSessionOutputContainer_add(m_capture_session_output_container, m_session_output);

    ACameraDevice_createCaptureSession(
            m_camera_device, m_capture_session_output_container,
            &m_capture_session_state_callbacks, &m_capture_session);

    ACameraCaptureSession_setRepeatingRequest(m_capture_session, nullptr, 1,
                                              &m_capture_request, nullptr);

    return true;
}