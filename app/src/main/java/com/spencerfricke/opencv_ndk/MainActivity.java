package com.spencerfricke.opencv_ndk;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.os.Environment;
import android.support.v4.app.ActivityCompat;
import android.os.Bundle;
import android.support.v7.app.ActionBarActivity;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.Button;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintWriter;

public class MainActivity extends ActionBarActivity
{

    static
    {
        System.loadLibrary("nativeStillCapture");
        System.loadLibrary("opencv_java");
    }

    static final String TAG = "OpenCV-NDK-Java";
    static final String DirSave = Environment.getExternalStorageDirectory() + File.separator + "Pictures" + File.separator + "OPENCV_NDK";

    private static final int PERMISSION_REQUEST_CODE_CAMERA = 1;

    Button mScanButton;
    Button mFlipCameraButton;
    Button mCaptureCameraButton ;
    SurfaceView mSurfaceView;
    SurfaceHolder mSurfaceHolder;

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // file create and delete to make dir .
        try
        {
            File dir = new File(DirSave);
            if(!dir.exists())
            {
                dir.mkdirs();
            }
            File file = new File(DirSave + File.separator + "test.txt");
            file.createNewFile();
            byte[] data1={1,1,0,0};
            if(file.exists())
            {
                OutputStream fo = new FileOutputStream(file);
                fo.write(data1);
                fo.close();
            }

            file.delete();
            Log.e(TAG, "Success to file delete") ;
        }
        catch (IOException ex)
        {
            Log.e(TAG, ex.getMessage()) ;
        }



        // Asking for permissions
        String[] accessPermissions = new String[]{
                Manifest.permission.CAMERA,
                Manifest.permission.INTERNET,
                Manifest.permission.WRITE_EXTERNAL_STORAGE,
                Manifest.permission.READ_EXTERNAL_STORAGE,
        };
        boolean needRequire = false;
        for (String access : accessPermissions)
        {
            int curPermission = ActivityCompat.checkSelfPermission(this, access);
            if (curPermission != PackageManager.PERMISSION_GRANTED)
            {
                needRequire = true;
                break;
            }
        }
        if (needRequire)
        {
            ActivityCompat.requestPermissions(
                    this,
                    accessPermissions,
                    PERMISSION_REQUEST_CODE_CAMERA);
            return;
        }

        // send class activity and assest fd to native code
        onCreateJNI(this, getAssets());

        // set up the Surface to display images too
        mSurfaceView = (SurfaceView) findViewById(R.id.surfaceView);
        mSurfaceHolder = mSurfaceView.getHolder();

        mScanButton = (Button) findViewById(R.id.scan_button);
        mScanButton.setOnClickListener(scanListener);

        mFlipCameraButton = (Button) findViewById(R.id.flip_button);
        mFlipCameraButton.setOnClickListener(flipCameraListener);

        mCaptureCameraButton = (Button) findViewById(R.id.capture_button);
        mCaptureCameraButton.setOnClickListener(captureCameraListener);



        mSurfaceHolder.addCallback(new SurfaceHolder.Callback()
        {
            @Override
            public void surfaceCreated(SurfaceHolder holder)
            {
                Log.v(TAG, "surfaceCreated");
                openCamera(holder.getSurface(), DirSave);
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder)
            {
                Log.v(TAG, "surfaceDestroyed");
                closeCamera();
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height)
            {
                Log.v(TAG, "surfaceChanged format=" + format + ", width=" + width + ", height=" + height);
            }

        });
    }

    private View.OnClickListener scanListener = new View.OnClickListener()
    {
        @Override
        public void onClick(View view)
        {
            scan();
        }
    };

    private View.OnClickListener captureCameraListener = new View.OnClickListener()
    {
        @Override
        public void onClick(View view)
        {
            captureCamera();
        }
    };

    private View.OnClickListener flipCameraListener = new View.OnClickListener()
    {
        @Override
        public void onClick(View view)
        {
            flipCamera(mSurfaceHolder.getSurface(), DirSave);
        }
    };

    public native void onCreateJNI(Activity callerActivity, AssetManager assetManager);

    // Sends surface buffer to NDK
    public native void openCamera(Surface surface, String pathbase );
    public native void flipCamera(Surface surface, String pathbase);
    public native void captureCamera();
    public native void closeCamera();

    public native void scan();
}
