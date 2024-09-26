// vid_ocv.cpp : simple reading and displaying of video using OpenCV 
//
// Written by Jonathan H. Connell, jconnell@alum.mit.edu
//
///////////////////////////////////////////////////////////////////////////
//
// Copyright 2024 Etaoin Systems
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

#pragma comment(lib, "opencv_world4100.lib")  // under opencv/build/x64/vc16

#include <windows.h>

#include "opencv2/opencv.hpp"                 // under opencv/build/include

#include "vid_ocv.h"


///////////////////////////////////////////////////////////////////////////
//                          Global Variables                             //
///////////////////////////////////////////////////////////////////////////

//= Video capture instance and image for frame grabbing.

static cv::VideoCapture vcap;
static cv::Mat img;


//= Display window names and corner positions.

static char name[6][40] = {"", "", "", "", "", ""};
static int wx[6], wy[6];


//= Cached resampling positions and interpolation factors.

unsigned long *base = NULL;
unsigned short *mix = NULL;
int npel = 0;


///////////////////////////////////////////////////////////////////////////
//                             Initialization                            //
///////////////////////////////////////////////////////////////////////////

//= Clean up on exit.

BOOL APIENTRY DllMain (HANDLE hModule,
                       DWORD ul_reason_for_call, 
                       LPVOID lpReserved)
{
  if (ul_reason_for_call == DLL_PROCESS_DETACH)
    ocv_close();
  return TRUE;
}


///////////////////////////////////////////////////////////////////////////
//                           Video Functions                             //
///////////////////////////////////////////////////////////////////////////

//= Tries to open a video source (file or stream) and grabs a test frame.
// only a single source can be active at a time with this DLL
// returns positive if successful, 0 or negative for failure

extern "C" DEXP int ocv_open (const char *fname)
{
  if ((fname == NULL) || (*fname == '\0'))
    return -2;
  if (!vcap.open(fname))
    return -1;
  if (!vcap.read(img))
    return 0;
  return 1;
}


//= Tries to open a local camera for input and grabs a test frame.
// use unit = -1 to get first working camera
// returns positive if successful, 0 or negative for failure

extern "C" DEXP int ocv_cam (int unit)
{
  if (!vcap.open(unit))
    return -1;
  if (!vcap.read(img))
    return 0;
  return 1;
}


//= Returns dimensions and framerate of currently bound video source.

extern "C" DEXP int ocv_info (int *iw, int *ih, double *fps)
{
  cv::Size sz = img.size();

  if (!vcap.isOpened())
    return 0;
  if (iw != NULL)
    *iw = sz.width;
  if (ih != NULL)
    *ih = sz.height;
  if (fps != NULL)
    *fps = vcap.get(cv::CAP_PROP_FPS);
  return 1;
}


//= Set geometric manipulations to perform on raw image.
//   r2f = r^2 lens radial distortion x 10^6 (pixel coords)
//   r4f = r^4 lens radial distortion x 10^12 (pixel coords)
//   mag = overall magnification after correction
//   asp = width/length of individual pixel (if not square)
// NOTE: needs to know image size before building transform
 
extern "C" DEXP void ocv_warp (double r2f, double r4f, double mag, double asp)
{
  cv::Size sz = img.size(); 
  int iw = sz.width, ih = sz.height, xlim = iw - 1, ylim = ih - 1, ln = 3 * iw;
  int x, y, ix, iy, fx, fy;
  double f2 = r2f * 1e-6, f4 = r4f * 1e-12, ysc = 1.0 / mag, xsc = asp * ysc;
  double dx, dy, dy2, r2, r4, warp, wx, wy, x0 = 0.5 * xlim, y0 = 0.5 * ylim;
  unsigned long *b;
  unsigned short *m;

  // get rid of any old transform
  delete [] mix;
  delete [] base;
  mix  = NULL;
  base = NULL;
  npel = 0;

  // make new cached value arrays if needed (and possible)
  if ((mag <= 0.0) || (iw <= 0) || (ih <= 0) ||
      ((mag == 1.0) && (r2f == 0.0) && (r4f == 0.0)))
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
    dy = ysc * (y - y0);
    dy2 = dy * dy;
    for (x = 0; x < iw; x++, b++, m++)
    {
      // compute radial offset from center
      dx = xsc * (x - x0);
      r2 = dx * dx + dy2;
      r4 = r2 * r2;

      // determine lens warped coordinates
      warp = 1.0 + f2 * r2 + f4 * r4;
      wx = x0 + warp * dx;
      wy = y0 + warp * dy;

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


//= Apply geometric tranform to image using pre-computed tables.
// assumes images are always color (3 bytes per pixel)
// returns 1 if successful, 0 or negative for problem

int fixup (unsigned char *dest, const unsigned char *src)
{
  cv::Size sz = img.size(); 
  int iw = sz.width, ih = sz.height, ln = 3 * iw;
  int x, y, fx, fy, cfx, cfy, lo, hi, val;
  const unsigned char *bot, *top;
  const unsigned long *b = base;
  const unsigned short *m = mix;
  unsigned char *d = dest;

  // make sure resonable transform exists
  if ((base == NULL) || (mix == NULL) || (npel != img.total()))
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

      // base corner of pixel quartet and interpolation coeffients 
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


//= Get next frame into supplied buffer (assumed to be big enough).
// images are left-to-right, bottom-up, with BGR color order
// can optionally flip image vertically so top becomes bottom
// returns 1 if successful, 0 or negative for problem
// NOTE: initiates framegrab and BLOCKS until fully decoded

extern "C" DEXP int ocv_get (unsigned char *buf, int vflip)
{
  cv::Mat bot;
  const unsigned char *src;

  // wait for next image to arrive
  if (!vcap.read(img))
    return 0;

  // OpenCV images are top-down
  src = img.data;
  if (vflip <= 0)
  {
    cv::flip(img, bot, 0);
    src = bot.data;
  }

  // apply geometric transform (if any)
  if (fixup(buf, src) <= 0)
    memcpy(buf, src, 3 * img.total());  
  return 1;
}


//= Disconnect from current video source (automatically called on exit).

extern "C" DEXP void ocv_close ()
{
  ocv_warp(0.0);            // deallocates arrays
  vcap.release();
}


///////////////////////////////////////////////////////////////////////////
//                           Display Functions                           //
///////////////////////////////////////////////////////////////////////////

//= Create a display window with given title and corner position.
// win is between 0 and 5, titles must be unique, can be moved later
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
  return 1;
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

  // see if window needs to be moved
  if ((wx[win] >= 0) && (wy[win] >= 0))
  {
    cv::moveWindow(name[win], wx[win], wy[win]);
    wx[win] = -1;
  }
  return 1;
}


//= Update all display windows with queued buffers (blocks for 1 ms).

extern "C" DEXP void ocv_show ()
{
  cv::waitKey(1);
}

