// vid_ocv.h : simple reading and displaying of video using OpenCV
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
// only a single source can be active at a time with this DLL
// returns positive if successful, 0 or negative for failure

extern "C" DEXP int ocv_open (const char *fname =NULL);


//= Tries to open a local camera for input and grabs a test frame.
// returns positive if successful, 0 or negative for failure

extern "C" DEXP int ocv_cam (int unit =0);


//= Returns dimensions and framerate of currently bound video source.

extern "C" DEXP int ocv_info (int *iw, int *ih, double *fps =NULL);


//= Set geometric manipulations to perform on raw image.
//   r2f = r^2 lens radial distortion x 10^6 (pixel coords)
//   r4f = r^4 lens radial distortion x 10^12 (pixel coords)
//   asp = width/length of individual pixel (if not square)
//   mag = overall magnification after correction
// NOTE: needs to know image size before building transform
 
extern "C" DEXP void ocv_warp (double r2f, double r4f =0.0, double mag =1.0, double asp =1.0);


//= Get next frame into supplied buffer (assumed to be big enough).
// images are left-to-right, bottom-up, with BGR color order
// can optionally flip image vertically so top becomes bottom
// returns 1 if successful, 0 or negative for problem
// NOTE: initiates framegrab and BLOCKS until fully decoded

extern "C" DEXP int ocv_get (unsigned char *buf, int vflip =0);


//= Disconnect from current video source (automatically called on exit).

extern "C" DEXP void ocv_close ();


///////////////////////////////////////////////////////////////////////////
//                           Display Functions                           //
///////////////////////////////////////////////////////////////////////////

//= Create a display window with given title and corner position.
// win is between 0 and 5, titles must be unique, can be moved later
// returns 1 if successful, 0 or negative for problem

extern "C" DEXP int ocv_win (int win, const char *title =NULL, int cx =-1, int cy =0);


//= Send an image to some window for display (must call ocv_show() later).
// buffer is left-to-right, bottom-up, BGR color order and size iw x ih
// returns 1 if successful, 0 or negative for problem

extern "C" DEXP int ocv_queue (int win, const unsigned char *buf, int iw =640, int ih =480);


//= Update all display windows with queued buffers (blocks for 1 ms).

extern "C" DEXP void ocv_show ();
