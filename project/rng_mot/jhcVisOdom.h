// jhcVisOdom.h : builds synthetic depth map from tracking 2D points
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


//= Builds synthetic depth map from tracking 2D points.
// also generates most likely camera pose as a byproduct 

class jhcVisOdom
{
// PRIVATE MEMBER VARIABLES
private:
  // useful constants
  double D2R, R2D;

  // images and dimensions
  unsigned char *col, *rng;
  int iw, ih;

  // submitted camera offset and full pose
  double rx, ry, rp, sx, sy, sz, sp, st, sr; 

  // full camera pose: estimated and computed
  double ex, ey, ez, ep, et, er;
  double cx, cy, cz, cp, ct, cr;

  // computed global odometry
  double mapx, mapy, head, trav, wind;

  // background thread
  pthread_t bg;
  int done, first;


// PUBLIC MEMBER VARIABLES
public:


// PUBLIC MEMBER FUNCTIONS
public:
  // creation and initialization
  ~jhcVisOdom ();
  jhcVisOdom ();
 
  // main functions
  void Init (int w, int h);
  int Estimate (const unsigned char *rgb, double mx, double my, double hd, 
                double x, double y, double z, double p, double t, double r);
  int Ready () const;
  int Depth (const unsigned char **col, const unsigned char **rng, 
             double& mx, double& my, double& tr, double& wd);


// PRIVATE MEMBER FUNCTIONS
private:
  // range map construction
  void analyze ();
  void update_odom ();

  // background thread
  static pthread_ret build_d16 (void *inst)
    {jhcVisOdom *me = (jhcVisOdom *) inst; me->analyze(); return 0;}
             
};
