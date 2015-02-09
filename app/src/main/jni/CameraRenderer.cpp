#include <jni.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <android/log.h>
#include <pthread.h>
#include <time.h>
#include <Math.h>
#include <stdio.h>
#include <math.h>
#include "opencv/cv.h"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"

// Utility for logging:
#define LOG_TAG    "CAMERA_RENDERER"
#define LOG(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

GLuint texture;
cv::VideoCapture capture;
cv::Mat buffer[30];
cv::Mat rgbFrame;
cv::Mat inframe;
cv::Mat outframe;
int bufferIndex;
int rgbIndex;
int frameWidth;
int frameHeight;
int screenWidth;
int screenHeight;
int orientation;
pthread_mutex_t FGmutex;
pthread_t frameGrabber;
pthread_attr_t attr;
struct sched_param param;
cv::Mat mBitmap;

using namespace std;

GLfloat vertices[] = {
    -1.0f, -1.0f, 0.0f, // V1 - bottom left
    -1.0f,  1.0f, 0.0f, // V2 - top left
    1.0f, -1.0f, 0.0f, // V3 - bottom right
    1.0f,  1.0f, 0.0f // V4 - top right
};

GLfloat textures[8];

extern "C" {
    
    void drawBackground();
    void createTexture();
    void destroyTexture();
    void *frameRetriever(void*);
    void test(cv::Mat, cv::Mat&);
void upScale(cv::Mat src, cv::Mat& dst);
void downScale(cv::Mat src, cv::Mat& dst);
void colorEnhancement(cv::Mat src, cv::Mat& dst, double r, double g, double b);
void correctGamma(cv::Mat img, cv::Mat& dst, double gamma );

    
    JNIEXPORT void JNICALL Java_com_blogspot_mesai0_Native_initCamera(JNIEnv*, jobject,jint width,jint height, jlong addrBitmap)
    {
        mBitmap = *(cv::Mat*)addrBitmap;
        LOG("Camera Created");
        capture.open(CV_CAP_ANDROID + 1);
        capture.set(CV_CAP_PROP_FRAME_WIDTH, width);
        capture.set(CV_CAP_PROP_FRAME_HEIGHT, height);
        frameWidth =width;
        frameHeight = height;
        LOG("frameWidth = %d",frameWidth);
        LOG("frameHeight = %d",frameHeight);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glShadeModel(1.0f);
        glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
        memset(&param, 0, sizeof(param));
        param.sched_priority = 100;
        pthread_attr_setschedparam(&attr, &param);
        pthread_create(&frameGrabber, &attr, frameRetriever, NULL);
        pthread_attr_destroy(&attr);
        
    }
    
    JNIEXPORT void JNICALL Java_com_blogspot_mesai0_Native_surfaceChanged(JNIEnv*, jobject,jint width,jint height,jint orien)
    {
        LOG("Surface Changed");
        glViewport(0, 0, width,height);
        if(orien==1) {
            screenWidth = width;
            screenHeight = height;
            orientation = 1;
            } else {
            screenWidth = height;
            screenHeight = width;
            orientation = 2;
        }
        
        
        LOG("screenWidth = %d",screenWidth);
        LOG("screenHeight = %d",screenHeight);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        float aspect = screenWidth / screenHeight;
        float bt = (float) tan(45 / 2);
        float lr = bt * aspect;
        glFrustumf(-lr * 0.1f, lr * 0.1f, -bt * 0.1f, bt * 0.1f, 0.1f, 100.0f);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glEnable(GL_TEXTURE_2D);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClearDepthf(1.0f);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        createTexture();
    }
    
    JNIEXPORT void JNICALL Java_com_blogspot_mesai0_Native_releaseCamera(JNIEnv*, jobject)
    {
        LOG("Camera Released");
        capture.release();
        destroyTexture();
        
    }
    
    void createTexture() {
        textures[0] = ((1024.0f-frameWidth*1.0f)/2.0f)/1024.0f;
        textures[1] = ((1024.0f-frameHeight*1.0f)/2.0f)/1024.0f + (frameHeight*1.0f/1024.0f);
        textures[2] = ((1024.0f-frameWidth*1.0f)/2.0f)/1024.0f + (frameWidth*1.0f/1024.0f);
        textures[3] = ((1024.0f-frameHeight*1.0f)/2.0f)/1024.0f + (frameHeight*1.0f/1024.0f);
        textures[4] = ((1024.0f-frameWidth*1.0f)/2.0f)/1024.0f;
        textures[5] = ((1024.0f-frameHeight*1.0f)/2.0f)/1024.0f;
        textures[6] = ((1024.0f-frameWidth*1.0f)/2.0f)/1024.0f + (frameWidth*1.0f/1024.0f);
        textures[7] = ((1024.0f-frameHeight*1.0f)/2.0f)/1024.0f;
        LOG("Texture Created");
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1024,1024, 0, GL_RGB,
        GL_UNSIGNED_SHORT_5_6_5, NULL);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    
    void destroyTexture() {
        LOG("Texture destroyed");
        glDeleteTextures(1, &texture);
    }
    
    JNIEXPORT void JNICALL Java_com_blogspot_mesai0_Native_renderBackground(JNIEnv*, jobject) {
        drawBackground();
    }
    
    void drawBackground() {
        glClear (GL_COLOR_BUFFER_BIT);
        glBindTexture(GL_TEXTURE_2D, texture);
        if(bufferIndex>0){
            pthread_mutex_lock(&FGmutex);
            cvtColor(buffer[(bufferIndex - 1) % 30], outframe, CV_BGR2BGR565);
            pthread_mutex_unlock(&FGmutex);
            outframe.copyTo(rgbFrame);
            //cv::flip(outframe, rgbFrame, 1);
            
            if (texture != 0)
            glTexSubImage2D(GL_TEXTURE_2D, 0, (1024-frameWidth)/2, (1024-frameHeight)/2, frameWidth, frameHeight,
            GL_RGB, GL_UNSIGNED_SHORT_5_6_5, rgbFrame.ptr());
        }
        glEnableClientState (GL_VERTEX_ARRAY);
        glEnableClientState (GL_TEXTURE_COORD_ARRAY);
        glLoadIdentity();
        if(orientation!=1){
            glRotatef( 90,0,0,1);
        }
        // Set the face rotation
        glFrontFace (GL_CW);
        // Point to our vertex buffer
        glVertexPointer(3, GL_FLOAT, 0, vertices);
        glTexCoordPointer(2, GL_FLOAT, 0, textures);
        // Draw the vertices as triangle strip
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        //Disable the client state before leaving
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }
    
    void *frameRetriever(void*) {
        while (capture.isOpened()) {
            capture.read(inframe);
            if (!inframe.empty()) {
                pthread_mutex_lock(&FGmutex);
                test(inframe, buffer[(bufferIndex++) % 30]);
                //inframe.copyTo(buffer[(bufferIndex++) % 30]);
                pthread_mutex_unlock(&FGmutex);
            }
        }
        LOG("Camera Closed");
        pthread_exit (NULL);
    }


    void test(cv::Mat mRgb, cv::Mat& mResult) {
      cv::Mat mRgb2 = cv::Mat();
      cv::Mat mHsv = cv::Mat();

      //flip(mRgb, mRgb, 1);
      downScale(mRgb, mRgb2);
      upScale(mRgb2, mRgb2);


      colorEnhancement(mRgb2, mRgb2, 0.89, 0.96, 1.0);

      //--------------------------------------
      // Convert RGB -> HSV
      //--------------------------------------
      cv::cvtColor(mRgb2, mHsv, CV_RGB2HSV);


      //--------------------------------------
      // Create a mask image using inRange() function
      //--------------------------------------
      //__android_log_print(ANDROID_LOG_DEBUG, "MainActivity", "---mMaskInRange");
      cv::Mat mMaskInRange;
      cv::inRange(mHsv,
         cv::Scalar(30, 120, 36),
         cv::Scalar(96, 255, 255),
         mMaskInRange);

      cv::Mat dummy = cv::Mat();
      erode(mMaskInRange, mMaskInRange, dummy);
      erode(mMaskInRange, mMaskInRange, dummy);
      dummy.release();


      //--------------------------------------
      // Find contours
      //--------------------------------------
      vector<cv::Vec4i> hierarchy;
      vector<cv::vector<cv::Point> > contours;
      cv::Mat mMask = cv::Mat();
      mMaskInRange.copyTo(mMask);

      //blur(mMaskInRange, mMaskInRange, cv::Size(6, 6));

      findContours( mMask, contours, hierarchy, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE, cv::Point(0, 0) );

      int mMaskW = mMask.cols;
      int mMaskH = mMask.rows;
      // Draw the contours
      if (contours.size() > 0) {

       // Find the largest contour
       double area, maxArea, thresholdArea;
       maxArea = 0;
       for (int i = 0; i< contours.size(); i++ ) {
         area = contourArea(contours[i]);
         if (area > maxArea) {
           maxArea = area;
         }
       }

       // Fill the contour that over the size of the maxarea * 0.02 (2% of the maxarea)
       thresholdArea = maxArea * 0.02;
       cv::Scalar mWhiteScalar = cv::Scalar(255, 255, 255);
       cv::Scalar mBlackScalar = cv::Scalar(0, 0, 0);

       for( int i = 0; i< contours.size(); i++ ) {
         area = contourArea(contours[i]);
         if (area > thresholdArea) {
             //drawContours(mMask, contours, i, mWhiteScalar, 5);
             drawContours(mMaskInRange, contours, i, mWhiteScalar, 10);
             drawContours(mMask, contours, i, mWhiteScalar, CV_FILLED, 8, hierarchy);
         }
         contours[i].clear();
       }
       contours.clear();
       hierarchy.clear();


       //------------------------------
       // Generate the final mask
       //------------------------------
       cv::Mat maskXor = cv::Mat();
       bitwise_xor(mMaskInRange, mMask, maskXor);
       bitwise_or(mMaskInRange, maskXor, mMaskInRange);
       bitwise_or(mMaskInRange, mMask, mMask);
       maskXor.release();
      }



      //upScale(mMask, mMask);
      //cv::GaussianBlur(mMask,mMask,cv::Size(21,21),11.0);

      //--------------------------------------
      // color temperature
      //--------------------------------------
      colorEnhancement(mRgb, mRgb2, 0.96, 0.85, 0.81);

      correctGamma(mRgb2, mRgb2, 1.5);

      bitwise_not(mMask, mMask);
      mBitmap.copyTo(mResult);
      mRgb2.copyTo(mResult, mMask);

      mRgb.release();
      mRgb2.release();
      mMask.release();
      mMaskInRange.release();

    }



    void colorEnhancement(cv::Mat src, cv::Mat& dst, double r, double g, double b) {
      vector<cv::Mat> channels;
      split(src, channels);

      r = r < 1.4 ? r : 1.4;
      g = g < 1.4 ? g : 1.4;
      b = b < 1.4 ? b : 1.4;

      r = r > 0.5 ? r : 0.5;
      g = g > 0.5 ? g : 0.5;
      b = b > 0.5 ? b : 0.5;

      // Color enhancement as beach scene
      channels[0] *= r; //R
      channels[1] *= g; //G
      channels[2] *= b; //B

      merge(channels, dst);
    }

    void downScale(cv::Mat src, cv::Mat& dst) {
      //pyrDown(src, src, cv::Size(src.cols / 2, src.rows / 2));
      pyrDown(src, dst, cv::Size(src.cols / 2, src.rows / 2));
    }
    void upScale(cv::Mat src, cv::Mat& dst) {
      //pyrUp(src, src, cv::Size(src.cols * 2, src.rows * 2));
      pyrUp(src, dst, cv::Size(src.cols * 2, src.rows * 2));
    }


    /**
     * http://subokita.com/2013/06/18/simple-and-fast-gamma-correction-on-opencv/
     */
    void correctGamma(cv::Mat img, cv::Mat& dst, double gamma ) {
     double inverse_gamma = 1.0 / gamma;

     cv::Mat lut_matrix(1, 256, CV_8UC1 );
     uchar * ptr = lut_matrix.ptr();
     for( int i = 0; i < 256; i++ )
       ptr[i] = (int)( pow( (double) i / 255.0, inverse_gamma ) * 255.0 );

     cv::LUT( img, lut_matrix, dst );
    }
}