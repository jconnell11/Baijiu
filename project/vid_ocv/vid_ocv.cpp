// vid_ocv.cpp : simple reading and displaying of video using OpenCV 
//
// Written by Jonathan H. Connell, jconnell@alum.mit.edu
//
///////////////////////////////////////////////////////////////////////////
//
// Copyright 2024-2026 Etaoin Systems
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// 
///////////////////////////////////////////////////////////////////////////

#pragma comment(lib, "opencv_world4100.lib") 

#include <windows.h>

#include "opencv2/opencv.hpp"                 

#include "jhc_pthread.h"

#include "vid_ocv.h"


///////////////////////////////////////////////////////////////////////////
//                          Global Variables                             //
///////////////////////////////////////////////////////////////////////////

//= Video capture instance for frame grabbing.

static cv::VideoCapture vcap;
static cv::Mat raw;
static int invert, ok = -1;


//= Background receiver and coordination.

static pthread_t hoover;
static pthread_mutex_t data;
static int run = 0;     


//= Color images and status.

static cv::Mat c0, c1, c2;
static cv::Mat *fill, *done, *lock;
static int fresh;


//= Cached resampling positions and interpolation factors.

static unsigned long *base = NULL;
static unsigned short *mix = NULL;
static int npel = 0;


//= Display window names and corner positions.

static char name[6][40] = {"", "", "", "", "", ""};
static int wx[6], wy[6];


//= Mouse click information for each window.

static int id[6], but[6], mx[6], my[6];


///////////////////////////////////////////////////////////////////////////
//                             Initialization                            //
///////////////////////////////////////////////////////////////////////////

//= Clean up on exit.

BOOL APIENTRY DllMain (HANDLE hModule,
                       DWORD ul_reason_for_call, 
                       LPVOID lpReserved)
{
  if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    pthread_mutex_init(&data, NULL);
  else if (ul_reason_for_call == DLL_PROCESS_DETACH)
  {
    ocv_close();
    pthread_mutex_destroy(&data);
  }
  return TRUE;
}


///////////////////////////////////////////////////////////////////////////
//                        Background Acquisition                         //
///////////////////////////////////////////////////////////////////////////

//= Apply geometric tranform to image using pre-computed tables.
// assumes images are always color (3 bytes per pixel)
// returns 1 if successful, 0 or negative for problem
// NOTE: "base" and "mix" arrays set up by call to ocv_warp()

static int fixup (unsigned char *dest, const unsigned char *src)
{
  cv::Size sz = raw.size(); 
  int iw = sz.width, ih = sz.height, ln = 3 * iw;
  int x, y, fx, fy, cfx, cfy, lo, hi, val;
  const unsigned char *bot, *top;
  const unsigned long *b = base;
  const unsigned short *m = mix;
  unsigned char *d = dest;

  // make sure resonable transform exists
  if ((base == NULL) || (mix == NULL) || (npel != raw.total()))
    return 0; 

  // see if integer sampling position is valid
  for (y = 0; y < ih; y++)
    for (x = 0; x < iw; x++, d += 3, b++, m++)
    {
      // outside original -> black
      if (*b == 0xFFFFFFFF)
      {
        d[0] = 0;
        d[1] = 0;
        d[2] = 0;
        continue;
      }

      // base corner of pixel quartet and interpolation coefficients 
      bot = src + (*b);
      top = bot + ln;
      fx = (*m) >> 8;
      fy = (*m) & 0xFF;
      cfx = 256 - fx;
      cfy = 256 - fy;
  
      // interpolate blue pixel
      lo  = cfx * bot[0] + fx * bot[3];
      hi  = cfx * top[0] + fx * top[3];
      val = cfy * lo + fy * hi;
      d[0] = (unsigned char)(val >> 16);

      // interpolate green pixel
      lo  = cfx * bot[1] + fx * bot[4];
      hi  = cfx * top[1] + fx * top[4];
      val = cfy * lo + fy * hi;
      d[1] = (unsigned char)(val >> 16);

      // interpolate red pixel
      lo  = cfx * bot[2] + fx * bot[5];
      hi  = cfx * top[2] + fx * top[5];
      val = cfy * lo + fy * hi;
      d[2] = (unsigned char)(val >> 16);
    }
  return 1;
}


//= Continually receive frames from camera into best open buffer image.

static pthread_ret grab_loop (void *arg)
{
  cv::Mat bot;
  const unsigned char *src;

  while (run > 0) 
  {
    // attempt to read next frame (blocks)
    if (!vcap.read(raw))
      break;

    // OpenCV images are top-down
    src = raw.data;
    if (invert > 0)
    {
      cv::flip(raw, bot, 0);
      src = bot.data;
    }

    // apply geometric transform (if any)
    if (fixup(fill->data, src) <= 0)
      memcpy(fill->data, src, 3 * raw.total());  

    // shuffle output images
    pthread_mutex_lock(&data);
    done = fill;                                 // most recent complete
    fresh += 1;
    if (fill == &c0)
      fill = ((lock != &c1) ? &c1 : &c2);
    else if (fill == &c1)
      fill = ((lock != &c0) ? &c0 : &c2);
    else                                         // lock == c2             
      fill = ((lock != &c0) ? &c0 : &c1);
    pthread_mutex_unlock(&data);
  }
  ok = 0;                                        // stream ended
  return NULL;
}


///////////////////////////////////////////////////////////////////////////
//                           Video Functions                             //
///////////////////////////////////////////////////////////////////////////

//= Common part of opening a named file/stream or physical camera. 

static int fg_init (int vflip)
{ 
  // try reading a frame then resize buffer images (for memcpy)
  if (!vcap.read(raw))      
    return 0;
  c0.create(raw.size(), raw.type());
  c1.create(raw.size(), raw.type());
  c2.create(raw.size(), raw.type());

  // initialize rotating buffers 
  fill = &c0;
  done = NULL;
  lock = NULL;
  fresh = 0;     

  // launch receiver and pre-processor thread
  invert = vflip;
  run = 1;
  pthread_create(&hoover, NULL, grab_loop, NULL);
  ok = 1;
  return 1;
}


//= Tries to open a video source (file or stream) and grabs a test frame.
// can optionally flip all images vertically so top becomes bottom
// only a single source can be active at a time with this DLL
// returns positive if successful, 0 or negative for failure

extern "C" DEXP int ocv_open (const char *fname, int vflip)
{
  int unit;  

  ok = 0;
  if ((fname == NULL) || (*fname == '\0'))
    return -2;
  if (sscanf_s(fname, "%d", &unit) == 1)         // fname = "1"
    return ocv_cam(unit, vflip);
  if (!vcap.open(fname))
    return -1;
  return fg_init(vflip);
}
 

//= Tries to open a local camera for input and grabs a test frame.
// use unit = -1 to get first working camera
// can optionally flip all images vertically so top becomes bottom
// only a single source can be active at a time with this DLL
// returns positive if successful, 0 or negative for failure

extern "C" DEXP int ocv_cam (int unit, int vflip)
{
  ok = 0;
  if (!vcap.open(unit, cv::CAP_V4L2))
    return -1;
  return fg_init(vflip);
}


//= Tells whether more frames are available from the source.
// returns 1 if still running, 0 if stopped, -1 if never opened

extern "C" DEXP int ocv_live ()
{
  return ok;
}


//= Binds dimensions and framerate of currently active video source.
// returns 1 if info valid (even if stopped), 0 if no current source 

extern "C" DEXP int ocv_info (int& iw, int& ih, double& fps)
{
  cv::Size sz = raw.size();

  if (!vcap.isOpened())
    return 0;
  iw = sz.width;
  ih = sz.height;
  fps = vcap.get(cv::CAP_PROP_FPS);
  return 1;
}


//= Set geometric manipulations to perform on raw image.
// sets up "base" and "mix" arrays for use by fixup()
//   k1   = r^2 lens radial distortion (wrt flen)
//   k2   = r^4 lens radial distortion (wrt flen)
//   k3   = r^6 lens radial distortion (wrt flen)
//   flen = focal length (both x and y, in pels)
//   mag  = overall magnification after correction
//   cx   = lens center x coordinate (defaults to mid-x)
//   cy   = lens center y coordinate (defaults to mid-y)
// de-warped version will have optical center in middle of image
// NOTE: needs to know image size before building transform
 
extern "C" DEXP void ocv_warp (double k1, double k2, double k3, double flen, 
                               double mag, double cx, double cy)
{
  cv::Size sz = raw.size(); 
  int iw = sz.width, ih = sz.height, xlim = iw - 1, ylim = ih - 1, ln = 3 * iw;
  int x, y, ix, iy, fx, fy;
  double sc = 1.0 / mag, norm2 = 1.0 / (flen * flen), x0 = 0.5 * xlim, y0 = 0.5 * ylim;
  double dx, dy, dy2, r2, warp, wx, wy;
  unsigned long *b;
  unsigned short *m;

  // use image center if camera center not specified
  if ((cx <= 0.0) || (cy <= 0.0))
  {
    cx = x0;
    cy = y0;  
  }

  // get rid of any old transform
  delete [] mix;
  delete [] base;
  mix  = NULL;
  base = NULL;
  npel = 0;

  // make new cached value arrays if needed (and possible)
  if ((mag <= 0.0) || (iw <= 0) || (ih <= 0) ||
      ((mag == 1.0) && (k1 == 0.0) && (k2 == 0.0) && (k3 == 0.0)))
    return;
  npel = iw * ih;
  base = new unsigned long [4 * npel];
  mix  = new unsigned short [2 * npel];

  // build transform lookup tables
  b = base;
  m = mix;
  for (y = 0; y < ih; y++)
  {
    // get central offset adjusted for pixel aspect ratio
    dy = sc * (y - y0);
    dy2 = dy * dy;
    for (x = 0; x < iw; x++, b++, m++)
    {
      // compute radial offset from center
      dx = sc * (x - x0);
      r2 = norm2 * (dx * dx + dy2);

      // determine lens warped coordinates
      warp = 1.0 + (k1 + (k2 + k3 * r2) * r2) * r2;
      wx = cx + warp * dx;
      wy = cy + warp * dy;

      // check for valid input pixel location
      if ((wx < 0.0) || (wx >= xlim) || (wy < 0.0) || (wy >= ylim))
      {
        *b = 0xFFFFFFFF;
        continue;
      }

      // get integer part of color sampling location
      ix = (int) wx;
      iy = (int) wy;
      *b = (unsigned long)(iy * ln + 3 * ix);

      // save fractional interpolation coefficients
      fx = (int)(256.0 * (wx - ix) + 0.5);
      fx = __min(fx, 255);
      fy = (int)(256.0 * (wy - iy) + 0.5);
      fy = __min(fy, 255);
      *m = (unsigned short)((fx << 8) | fy);
    }
  }
}


//= Bind filled framebuffer for next image to supplied pointer.
// images are left-to-right, bottom-up, with BGR color order
// can optionally block until brand new image becomes available
// returns 1 if buffer is new, 0 if not ready, negative for error

extern "C" DEXP int ocv_get (const unsigned char **buf, int block)
{
  int wait = 0;

  // check if source is operational and new frame is ready
  if (ok <= 0)  
    return -2;
  while (fresh <= 0)
  {
    if (block <= 0)                    // return immediately
      return 0;
    if (wait++ > 500)                  // barf after 0.5 sec
      return -1;
    Sleep(1);                          // 1 ms loop
  }

  // swap buffers to be sure output pointer remains valid
  pthread_mutex_lock(&data);
  lock = done;                         // mark as in-use
  fresh = 0;
  pthread_mutex_unlock(&data);
  *buf = lock->data;
  return 1;
}


//= Disconnect from current video source (automatically called on exit).

extern "C" DEXP void ocv_close ()
{
  abstime_t one_sec;

  // stop background thread (if needed)
  if (run > 0)
  {
    run = 0;     
    pthread_timedjoin_np(hoover, 0, abstime_wait(&one_sec, 1000));
  }

  // deallocate arrays and components
  ocv_warp(0.0);             
  if (vcap.isOpened())
    vcap.release();          
  ok = -1;
}


///////////////////////////////////////////////////////////////////////////
//                           Display Functions                           //
///////////////////////////////////////////////////////////////////////////

//= Create a display window with given title and corner position.
// win is between 0 and 5, titles must be unique
// returns 1 if successful, 0 or negative for problem

extern "C" DEXP int ocv_win (int win, const char *title, int cx, int cy)
{
  // create default label if none given
  if ((win < 0) || (win > 5))
    return 0;
  if (title == NULL)
    sprintf_s(name[win], "Window %d", win);
  else
    strcpy_s(name[win], title);

  // remember desired top left corner position
  wx[win] = cx;
  wy[win] = cy;

  // intitialize mouse click info
  id[win] = win;             
  but[win] = 0;
  return 1;
}


//= Capture position of mouse click to global variables.

static void cb_mouse (int evt, int x, int y, int flag, void *param)
{
  int win = *((int *) param);        

  if ((evt == cv::EVENT_LBUTTONDOWN) || (evt == cv::EVENT_RBUTTONDOWN))
  {
    but[win] = ((evt == cv::EVENT_LBUTTONDOWN) ? 1 : 3);
    mx[win] = x;
    my[win] = y;
  }
}


//= Send an image to some window for display (must call ocv_show() later).
// buffer is left-to-right, bottom-up, BGR color order and size iw x ih
// returns 1 if successful, 0 or negative for problem

extern "C" DEXP int ocv_queue (int win, const unsigned char *buf, int iw, int ih)
{
  cv::Mat bot, top;

  // sanity check
  if ((win < 0) || (win > 5))
    return -3;
  if (name[win][0] == '\0')
    return -2;
  if ((iw <= 0) || (ih <= 0))
    return -1;
  if (buf == NULL)
    return 0;

  // OpenCV images are top-down
  bot = cv::Mat(ih, iw, CV_8UC3, (void *) buf);
  cv::flip(bot, top, 0);
  cv::imshow(name[win], top);

  // see if window needs to be initialized
  if ((wx[win] >= 0) && (wy[win] >= 0))
  {
    cv::moveWindow(name[win], wx[win], wy[win]);
    cv::setMouseCallback(name[win], cb_mouse, (void *)(id + win));
    wx[win] = -1;
  }
  return 1;
}


//= Update all display windows with queued buffers (blocks for 1 ms).

extern "C" DEXP void ocv_show ()
{
  cv::waitKey(1);
}


//= Checks a particular window for position of most recent mouse click.
// coords wrt to displayed buffer, y is TOP DOWN, clears status during call
// returns 0 if nothing, 1 for left button, 3 for right button

extern "C" DEXP int ocv_click (int win, int& x, int& y)
{
  int clk;

  // sanity check
  if ((win < 0) || (win > 5))
    return 0;

  // check if any click and reset status
  clk = but[win];
  if (clk <= 0)
    return 0;
  but[win] = 0;

  // pass on requested information
  x = mx[win];
  y = my[win];
  return clk;
}

