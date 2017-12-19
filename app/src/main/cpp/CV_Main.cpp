#include "CV_Main.h"


CV_Main::CV_Main()
        : m_camera_ready(false), m_image(nullptr), m_image_reader(nullptr),
          m_native_camera(nullptr), scan_mode(false)
{

//  AAssetDir* assetDir = AAssetManager_openDir(m_aasset_manager, "");
//  const char* filename = (const char*)NULL;
//  while ((filename = AAssetDir_getNextFileName(assetDir)) != NULL) {
//    AAsset* asset = AAssetManager_open(m_aasset_manager, filename, AASSET_MODE_STREAMING);
//    char buf[BUFSIZ];
//    int nb_read = 0;
//    FILE* out = fopen(filename, "w");
//    while ((nb_read = AAsset_read(asset, buf, BUFSIZ)) > 0)
//      fwrite(buf, nb_read, 1, out);
//    fclose(out);
//    AAsset_close(asset);
//  }
//  AAssetDir_close(assetDir);

    if (!face_cascade.load(face_cascade_name))
    { LOGE("--(!)Error loading face cascade\n"); };
    if (!eyes_cascade.load(eyes_cascade_name))
    { LOGE("--(!)Error loading eyes cascade\n"); };
};

CV_Main::~CV_Main()
{
    // clean up VM and callback handles
    JNIEnv *env;
    java_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
    env->DeleteGlobalRef(calling_activity_obj);
    calling_activity_obj = nullptr;

    // ACameraCaptureSession_stopRepeating(m_capture_session);

    if (m_native_camera != nullptr)
    {
        delete m_native_camera;
        m_native_camera = nullptr;
    }

    // make sure we don't leak native windows
    if (m_native_window != nullptr)
    {
        ANativeWindow_release(m_native_window);
        m_native_window = nullptr;
    }


    if (m_image_reader != nullptr)
    {
        delete (m_image_reader);
        m_image_reader = nullptr;
    }
}

void CV_Main::OnCreate(JNIEnv *env, jobject caller_activity)
{
    // Need to create an instance of the Java activity
    calling_activity_obj = env->NewGlobalRef(caller_activity);

    // Need to enter package and class to find Java class
    jclass handler_class = env->GetObjectClass(caller_activity);

    // Create function pointeACameraManager_getCameraCharacteristicsr to use for
    // on_loaded callbacks
    // on_callback = env->GetMethodID(handler_class, "JAVA_FUNCTION", "()V");
}


void CV_Main::openCamera(JNIEnv *env, jclass clazz, jobject jPreviewSurface, jstring jOutPath)
{
    const char* poutPath = env->GetStringUTFChars(jOutPath, nullptr);
    strcpy(outPath, poutPath);
    env->ReleaseStringUTFChars(jOutPath, poutPath);

    // set saving path to Imagereader
    readerListener.setDumpFilePathBase(outPath);

    // CameraIdList 구하기
    camera_status_t ret = testCase.initWithErrorLog();
    ASSERT(ret == ACAMERA_OK, "testCase.initWithErrorLog() ==> error");
    ASSERT(testCase.getNumCameras() >= 1 , "the number of Camera  == 0");

    const char *cameraId = testCase.getCameraId(m_selected_camera_type);

    ret = testCase.openCamera(cameraId);
    ASSERT(ret == ACAMERA_OK, "testCase.openCamera() ==> error");

    usleep(100000); // sleep to give some time for callbacks to happen
    if (testCase.isCameraAvailable(cameraId))
    {
        LOG_ERROR(errorString, "Camera %s should be unavailable now", cameraId);
    }

    ANativeWindow *previewAnw = testCase.initPreviewAnw(env, jPreviewSurface);
    ASSERT(previewAnw != nullptr, "testCase.initPreviewAnw() ==> error");

    readerListener.setDumpFilePathBase(outPath);
    media_status_t mediaRet = AMEDIA_ERROR_UNKNOWN;

    mediaRet = testCase.initImageReaderWithErrorLog(
            ANativeWindow_getWidth(previewAnw),ANativeWindow_getHeight(previewAnw),
            AIMAGE_FORMAT_YUV_420_888, NUM_TEST_IMAGES,  &readerCb);
    // AIMAGE_FORMAT_JPEG, AIMAGE_FORMAT_YUV_420_888

    ASSERT(mediaRet == AMEDIA_OK, "testCase.initImageReaderWithErrorLog() ==> error");

    ret = testCase.createCaptureSessionWithLog();
    ASSERT(ret == ACAMERA_OK, "testCase.createCaptureSessionWithLog() ==> error");

    ret = testCase.createRequestsWithErrorLog();
    ASSERT(ret == ACAMERA_OK, "testCase.createRequestsWithErrorLog() ==> error");

    ret = testCase.startPreview();
    ASSERT(ret == ACAMERA_OK, "testCase.startPreview() ==> error");


}

//void CV_Main::openCamera()
//{
//    // 단순히 camera 그 자체를 생성
//    m_native_camera = new Native_Camera(m_selected_camera_type);
//
//    m_native_camera->MatchCaptureSizeRequest(&m_view,
//                                             ANativeWindow_getWidth(m_native_window),
//                                             ANativeWindow_getHeight(m_native_window));
//
////    ASSERT(m_view.width && m_view.height, "Could not find supportable resolution");
////
////    // Here we set the buffer to use RGBX_8888 as default might be; RGB_565
////    ANativeWindow_setBuffersGeometry(m_native_window, m_view.height, m_view.width,
////                                     WINDOW_FORMAT_RGBX_8888);
////
////    m_image_reader = new Image_Reader(&m_view, AIMAGE_FORMAT_YUV_420_888);
////    m_image_reader->SetPresentRotation(m_native_camera->GetOrientation());
////    ANativeWindow *image_reader_window = m_image_reader->GetNativeWindow();
//
//    // camera capture 하고 ,target 에 출력시  필요한 session들을 만든다.
////    m_camera_ready = m_native_camera->CreateCaptureSession(image_reader_window);
//    m_camera_ready = m_native_camera->CreateCaptureSession(m_native_window);
//}

void CV_Main::captureCamera(JNIEnv *env, jobject clazz)
{
    camera_status_t ret;

    // before capture, make filename to save jepg.
    time_t rawtime;
    struct tm * timeinfo;
    char buffer[256];

    time (&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(buffer,sizeof(buffer),"img%Y%m%d_%H%M%S",timeinfo);
    readerListener.setFilenameCapture(buffer);


    for (int capture = 0; capture < NUM_TEST_IMAGES; capture++)
    {
        ret = testCase.takePicture();
        ASSERT(ret == ACAMERA_OK, "testCase.takePicture() ==> error");
    }
    // wait until all capture finished
    for (int i = 0; i < 50; i++)
    {
        usleep(100000); // sleep 100ms
        if (readerListener.onImageAvailableCount() == NUM_TEST_IMAGES)
        {
            LOGI("Session take ~%d ms to capture %d images",
                 i * 100, NUM_TEST_IMAGES);
            break;
        }
    }
    ASSERT(readerListener.onImageAvailableCount() == NUM_TEST_IMAGES, "Not captured to NUM_TEST_IMAGES ==> error");

}

void CV_Main::closeCamera(JNIEnv *env, jobject clazz)
{
    camera_status_t ret;
    m_camera_thread_stopped = true;

    ret = testCase.resetWithErrorLog();
    ASSERT(ret == ACAMERA_OK, "testCase.resetWithErrorLog() ==> error");

    usleep(100000);
    ret = testCase.deInit();
    ASSERT(ret == ACAMERA_OK, "testCase.resetWithErrorLog() ==> error");

    //==================================================================
    if (m_native_camera != nullptr)
    {
        delete m_native_camera;
        m_native_camera = nullptr;
    }

}

//void CV_Main::closeCamera()
//{
//    m_camera_thread_stopped = true;
//
//    if (m_native_camera != nullptr)
//    {
//        delete m_native_camera;
//        m_native_camera = nullptr;
//    }
//
//
//}


void CV_Main::FlipCamera(JNIEnv *env, jclass clazz, jobject jPreviewSurface, jstring jOutPath)
{
    m_camera_thread_stopped = false;
    closeCamera(env,clazz) ;

    // reset info
    if (m_image_reader != nullptr)
    {
        delete (m_image_reader);
        m_image_reader = nullptr;
    }

    if (m_selected_camera_type == FRONT_CAMERA)
    {
        m_selected_camera_type = BACK_CAMERA;
    }
    else
    {
        m_selected_camera_type = FRONT_CAMERA;
    }

    openCamera(env, clazz, jPreviewSurface, jOutPath);

//    std::thread loopThread(&CV_Main::CameraLoop, this);
//    loopThread.detach();
}

//========================================================
void CV_Main::CameraLoop()
{
    bool buffer_printout = false;

    while (1)
    {
        if (m_camera_thread_stopped)
        { break; }
        if (!m_camera_ready || !m_image_reader)
        { continue; }
        m_image = m_image_reader->GetLatestImage();
        if (m_image == nullptr)
        { continue; }

        ANativeWindow_acquire(m_native_window);
        ANativeWindow_Buffer buffer;
        if (ANativeWindow_lock(m_native_window, &buffer, nullptr) < 0)
        {
            m_image_reader->DeleteImage(m_image);
            m_image = nullptr;
            continue;
        }

        if (false == buffer_printout)
        {
            buffer_printout = true;
            LOGI("/// H-W-S-F: %d, %d, %d, %d", buffer.height, buffer.width, buffer.stride,
                 buffer.format);
        }

        m_image_reader->DisplayImage(&buffer, m_image);

        display_mat = cv::Mat(buffer.height, buffer.stride, CV_8UC4, buffer.bits);

        if (true == scan_mode)
        {
            FaceDetect(display_mat);
        }

        ANativeWindow_unlockAndPost(m_native_window);
        ANativeWindow_release(m_native_window);
    }

}

// When scan button is hit
void CV_Main::RunCV()
{
    scan_mode = true;
    total_t = 0;
    start_t = clock();
}

void CV_Main::FaceDetect(cv::Mat &frame)
{

    std::vector<cv::Rect> faces;
    cv::Mat frame_gray;

    cv::cvtColor(frame, frame_gray, CV_RGBA2GRAY);

    // equalizeHist( frame_gray, frame_gray );

    //-- Detect faces
    face_cascade.detectMultiScale(frame_gray, faces, 1.18, 2, 0 | CV_HAAR_SCALE_IMAGE,
                                  cv::Size(70, 70));

    for (size_t i = 0; i < faces.size(); i++)
    {
        cv::Point center(faces[i].x + faces[i].width * 0.5, faces[i].y + faces[i].height * 0.5);

        ellipse(frame, center, cv::Size(faces[i].width * 0.5, faces[i].height * 0.5), 0, 0, 360,
                CV_PURPLE, 4, 8, 0);

        cv::Mat faceROI = frame_gray(faces[i]);
        std::vector<cv::Rect> eyes;

        //-- In each face, detect eyes
        eyes_cascade.detectMultiScale(faceROI, eyes, 1.2, 2, 0 | CV_HAAR_SCALE_IMAGE,
                                      cv::Size(45, 45));

        for (size_t j = 0; j < eyes.size(); j++)
        {
            cv::Point center(faces[i].x + eyes[j].x + eyes[j].width * 0.5,
                             faces[i].y + eyes[j].y + eyes[j].height * 0.5);
            int radius = cvRound((eyes[j].width + eyes[j].height) * 0.25);
            circle(frame, center, radius, CV_RED, 4, 8, 0);
        }
    }

    end_t = clock();
    total_t += (double) (end_t - start_t) / CLOCKS_PER_SEC;
    LOGI("Current Time: %f", total_t);
    if (total_t >= 20)
    {
        // stop after 20 seconds
        LOGI("DONE WITH 20 SECONDS");
        scan_mode = false;
    }
    start_t = clock();

}
