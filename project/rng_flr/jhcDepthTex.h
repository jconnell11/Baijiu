// jhcDepthTex.h : builds approx depth map from color image in background
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

#include "jhc_pthread.h"

#include "Environ/jhcPlainFloor.h"


//= Builds synthetic depth map from tracking 2D points.
// also generates most likely camera pose as a byproduct 

class jhcDepthTex : private jhcPlainFloor
{
// PRIVATE MEMBER VARIABLES
private:
  // input and output image plus partial pose
  jhcImg col, rng;
  double cz, ct;

  // background thread
  pthread_t bg;
  int done;

  // debugging image wrappers
  jhcImg gwrap, nwrap;
  

// PUBLIC MEMBER VARIABLES
public:


// PUBLIC MEMBER FUNCTIONS
public:
  // creation and initialization
  ~jhcDepthTex ();
  jhcDepthTex ();
 
  // main functions
  void Init (double flen, int iw =640, int ih =480);
  int Estimate (const unsigned char *rgb, double ht, double tilt);
  int Ready (int ms =0) const;
  int Depth (const unsigned char **d16, const unsigned char **rgb =NULL);

  // debugging images
  void Ground (unsigned char *buf);
  void Night (unsigned char *buf);


// PRIVATE MEMBER FUNCTIONS
private:
  // main functions
  void analyze ();

  // background thread
  static pthread_ret build_d16 (void *inst)
    {jhcDepthTex *me = (jhcDepthTex *) inst; me->analyze(); return 0;}
             
};
