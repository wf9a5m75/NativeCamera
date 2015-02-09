package com.blogspot.mesai0;


import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.opengl.GLSurfaceView.Renderer;
import android.util.Log;

import org.opencv.android.Utils;
import org.opencv.core.Core;
import org.opencv.core.Mat;
import org.opencv.core.Size;
import org.opencv.imgproc.Imgproc;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;


public class CameraRenderer implements Renderer {

  private Size size;
  private Context context;
  private static final int FPS_STEPS = 20;
  private int mFpsCounter;
  private double mFpsFrequency;
  private long mPrevFrameTime;
  private double mPreviousFps;

  public CameraRenderer(Context c, Size size) {
    super();
    context = c;
    this.size = size;
  }

  public void onSurfaceCreated(GL10 gl, EGLConfig config) {

    Bitmap bmp = BitmapFactory.decodeResource(context.getResources(), R.drawable.dummy_background);
    Bitmap backgroundBmp = Bitmap.createBitmap((int)size.width, (int)size.height, Bitmap.Config.ARGB_8888);
    Canvas canvas = new Canvas(backgroundBmp);
    canvas.drawBitmap(bmp, 0, 0, null);

    Mat backgroundMat = new Mat();
    Utils.bitmapToMat(backgroundBmp, backgroundMat);
    Imgproc.cvtColor(backgroundMat, backgroundMat, Imgproc.COLOR_RGBA2RGB);



    // Initalize the fps counter
    mFpsFrequency = Core.getTickFrequency();
    mPrevFrameTime = Core.getTickCount();
    mFpsCounter = 0;

    Thread.currentThread().setPriority(Thread.MAX_PRIORITY);
    Native.initCamera((int) size.width, (int) size.height, backgroundMat.getNativeObjAddr());
  }

  public void onDrawFrame(GL10 gl) {
    //  long startTime   = System.currentTimeMillis();
    Native.renderBackground();

    // Measuring the fps
    mPreviousFps = measureFps();
    //  long endTime   = System.currentTimeMillis();
    //  if(30-(endTime-startTime)>0){
    //   try {
    //    Thread.sleep(30-(endTime-startTime));
    //   } catch (InterruptedException e) {}
    //  }
    //  endTime   = System.currentTimeMillis();
    //System.out.println(endTime-startTime+" ms");
  }

  public void onSurfaceChanged(GL10 gl, int width, int height) {
    Native.surfaceChanged(width, height, context.getResources().getConfiguration().orientation);
  }

  public double measureFps() {
    mFpsCounter++;
    if (mFpsCounter % FPS_STEPS == 0) {
      long time = Core.getTickCount();
      double fps = FPS_STEPS * mFpsFrequency / (time - mPrevFrameTime);
      Log.d("MainActivity", "fps = " + fps);
      mPrevFrameTime = time;
      mPreviousFps = fps;
      mFpsCounter = 0;
    }
    return mPreviousFps;
  }
}