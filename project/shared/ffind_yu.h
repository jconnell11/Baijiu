// ffind_yu.h : OpenCV YuNet face finder in background thread
//
// Written by Jonathan H. Connell, jconnell@alum.mit.edu
//
///////////////////////////////////////////////////////////////////////////
//
// Copyright 2026 Etaoin Systems
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

// NOTE: needs opencv_world4100.dll 

#pragma once

// define DEXP_Y (uses other DLLs whose header files define DEXP)

#ifdef __linux__
  #define DEXP_Y             // nothing special needed for Linux shared lib
#else 

  // function declarations
  #ifndef DEXP_Y
    #ifdef FFINDYU_EXPORTS
      #define DEXP_Y __declspec(dllexport)
    #else
      #define DEXP_Y __declspec(dllimport)
    #endif
  #endif

  // link to library stub
  #ifndef FFINDYU_EXPORTS
    #pragma comment(lib, "ffind_yu.lib")
  #endif

#endif


///////////////////////////////////////////////////////////////////////////
//                           Main Functions                              //
///////////////////////////////////////////////////////////////////////////

//= Set focal length, image size, and detection threshold for system.
// also specifies whether input image should be subsampled by 2 for speed
// returns 1 if okay, 0 or negative for problem

extern "C" DEXP_Y int ffind_init (double f, int w =640, int h =480, int sub =0, double th =0.75);


//= Start looking for faces in the supplied image with the given tilt angle.
// image is BGR left-to-right bottom-up, assumes buffer is big enough
// returns 1 if okay, 0 or negative for problem

extern "C" DEXP_Y int ffind_start (const unsigned char *src, double tilt =0.0);


//= Determine if face finder is ready with detection results.
// can automatically block for up to "ms" milleseconds if not ready
// returns -1 if still working, else number of faces found (could be zero)

extern "C" DEXP_Y int ffind_rdy (int ms =0);


//= Shut down processing and clean up neural net.

extern "C" DEXP_Y void ffind_done ();


///////////////////////////////////////////////////////////////////////////
//                          Detection Browsing                           //
///////////////////////////////////////////////////////////////////////////

//= Find the index of the biggest face in the submitted image.
// returns index of winner, negative if no faces or other problem

extern "C" DEXP_Y int ffind_king ();


//= For specified detection instance give bounding box in submitted image.
// (xc yc) is box center, yc adjusted for bottom-up input buffer
// returns score for this detection, negative if bad index or problem

extern "C" DEXP_Y double ffind_box (int& xc, int& yc, int& w, int& h, int i =0);


//= For specified detection find head gaze angle relative to camera direction.
// returns offset in degs, -180 if bad index or problem

extern "C" DEXP_Y double ffind_gaze (int i =0);


//= For specified detection give approximate position in global coord system.
// assumes camera at origin looking along y axis (x to right, z up)
// returns radial distance from camera, negative if bad index or problem

extern "C" DEXP_Y double ffind_pos (double& wx, double& wy, double& wz, int i =0);


