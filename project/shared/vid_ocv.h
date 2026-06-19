// vid_ocv.h : simple reading and displaying of video using OpenCV
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

// Modified for faster background framegrabbing based on jhcMpiCam class
// NOTE: needs opencv_world4100.dll and opencv_videoio_ffmpeg4100_64.dll

#pragma once

#include <stddef.h>           // for NULL


// function declarations 

#ifdef VIDOCV_EXPORTS
  #define DEXP __declspec(dllexport)
#else
  #define DEXP __declspec(dllimport)
#endif


// link to library stub

#ifndef VIDOCV_EXPORTS
  #pragma comment(lib, "vid_ocv.lib")
#endif


///////////////////////////////////////////////////////////////////////////
//                           Video Functions                             //
///////////////////////////////////////////////////////////////////////////

//= Tries to open a video source (file or stream) and grabs a test frame.
// can optionally flip all images vertically so top becomes bottom
// only a single source can be active at a time with this DLL
// returns positive if successful, 0 or negative for failure

extern "C" DEXP int ocv_open (const char *fname =NULL, int vflip =0);


//= Tries to open a local camera for input and grabs a test frame.
// use unit = -1 to get first working camera
// can optionally flip all images vertically so top becomes bottom
// only a single source can be active at a time with this DLL
// returns positive if successful, 0 or negative for failure

extern "C" DEXP int ocv_cam (int unit =-1, int vflip =0);


//= Tells whether more frames are available from the source.
// returns 1 if still running, 0 if stopped, -1 if never opened

extern "C" DEXP int ocv_live ();


//= Binds dimensions and framerate of currently active video source.
// returns 1 if info valid (even if stopped), 0 if no current source 

extern "C" DEXP int ocv_info (int& iw, int& ih, double& fps);


//= Set geometric manipulations to perform on raw image.
// sets up "base" and "mix" arrays for use by fixup()
// OpenCV distortion conversion: r2f = 1e6 * k1 / flen^2
//   r2f = r^2 lens radial distortion x 10^6 (pixel coords)
//   r4f = r^4 lens radial distortion x 10^12 (pixel coords)
//   asp = width/length of individual pixel (if not square)
//   mag = overall magnification after correction
//   cx  = lens center x coordinate (defaults to mid-x)
//   cy  = lens center y coordinate (defaults to mid-y)
// NOTE: needs to know image size before building transform
 
extern "C" DEXP void ocv_warp (double r2f, double r4f =0.0, double r6f =0.0, double mag =1.0, 
                               double asp =1.0, double cx =0.0, double cy =0.0);
                               

//= Bind filled framebuffer for next image to supplied pointer.
// images are left-to-right, bottom-up, with BGR color order
// can optionally block until brand new image becomes available
// returns 1 if buffer is new, 0 if not ready, negative for error

extern "C" DEXP int ocv_get (const unsigned char **buf, int block =0);


//= Disconnect from current video source (automatically called on exit).

extern "C" DEXP void ocv_close ();


///////////////////////////////////////////////////////////////////////////
//                           Display Functions                           //
///////////////////////////////////////////////////////////////////////////

//= Create a display window with given title and corner position.
// win is between 0 and 5, titles must be unique
// returns 1 if successful, 0 or negative for problem

extern "C" DEXP int ocv_win (int win, const char *title =NULL, int cx =-1, int cy =0);


//= Send an image to some window for display (must call ocv_show() later).
// buffer is left-to-right, bottom-up, BGR color order and size iw x ih
// returns 1 if successful, 0 or negative for problem

extern "C" DEXP int ocv_queue (int win, const unsigned char *buf, int iw =640, int ih =480);


//= Update all display windows with queued buffers (blocks for 1 ms).

extern "C" DEXP void ocv_show ();


//= Checks a particular window for position of most recent mouse click.
// coords wrt to displayed buffer, y is TOP DOWN, clears status during call
// returns 0 if nothing, 1 for left button, 3 for right button

extern "C" DEXP int ocv_click (int win, int& x, int& y);
