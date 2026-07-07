// rng_flr.h : makes pseudo-range image from non-textured floor areas
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

#pragma once

#include <stddef.h>           // for NULL


// function declarations 

#ifdef RNGFLR_EXPORTS
  #define DEXP __declspec(dllexport)
#else
  #define DEXP __declspec(dllimport)
#endif


// link to library stub

#ifndef RNGFLR_EXPORTS
  #pragma comment(lib, "rng_flr.lib")
#endif


///////////////////////////////////////////////////////////////////////////
//                            Main Functions                             //
///////////////////////////////////////////////////////////////////////////

//= Configure system for a certain input image size and clear all point data.

extern "C" DEXP void rng_init (double flen, int w =640, int h =480);


//= Start building depth map from color image and best-guess odometry.
// pixel data is BGR triples scanned left-to-right and bottom-up
// camera is up ht inches from floor, and tipped tilt degrees relative to level
// returns 1 if new image accepted, 0 if not (typically because busy)

extern "C" DEXP int rng_est (const unsigned char *rgb, double ht, double tilt);


//= Tell if range image ready (1), still processing (0), or never started (-1).
// can optionally wait for ms milliseconds if still processing

extern "C" DEXP int rng_rdy (int ms =0);


//= Binds aligned depth map and cached input image to supplied pointers.
// rng is 16 bit depth values from camera in 0.25mm steps orthogonal to image plane
// resets state to -1, output image remains valid until next rng_est() call
// returns 1 if images and odometry bound, 0 if not ready yet (busy)

extern "C" DEXP int rng_d16 (const unsigned char **rng, const unsigned char **col =NULL);


///////////////////////////////////////////////////////////////////////////
//                          Debugging Images                             //
///////////////////////////////////////////////////////////////////////////

//= Show floor area as green in supplied color image.
// image buffer filled BGR bottom-up, assumes large enough = iw * ih * 3

extern "C" DEXP void rng_gnd (unsigned char *buf);


//= Show range fading with distance in supplied color image.
// image buffer filled BGR bottom-up, assumes large enough = iw * ih * 3

extern "C" DEXP void rng_nite (unsigned char *buf);

