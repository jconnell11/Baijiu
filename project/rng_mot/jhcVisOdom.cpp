// jhcVisOdom.cpp : builds synthetic depth map from tracking 2D points
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

#include <windows.h>
#include <stdlib.h>                    // for NULL
#include <stdio.h>
#include <memory.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include "jhcVisOdom.h"

#include <wchar.h>
///////////////////////////////////////////////////////////////////////////
//                      Creation and Initialization                      //
///////////////////////////////////////////////////////////////////////////

//= Default destructor does necessary cleanup.

jhcVisOdom::~jhcVisOdom ()
{
  if (done == 0)
    Sleep(500);
  delete [] rng;
  delete [] col;
}


//= Default constructor initializes certain values.

jhcVisOdom::jhcVisOdom ()
{
  // degrees to radians
  D2R = M_PI / 180.0;
  R2D = 180.0 / M_PI;

  // no image arrays
  col = NULL;
  rng = NULL;
  iw = 0;
  ih = 0;

  // nothing analyzed yet
  done = -1;             
  first = 1;
}


///////////////////////////////////////////////////////////////////////////
//                              Main Functions                           //
///////////////////////////////////////////////////////////////////////////

//= Configure system for a certain input image size and clear all point data.

void jhcVisOdom::Init (int w, int h)
{
  // wait for background thread (if needed)
  if (done == 0)
    Sleep(500);

  // get rid of old images if wrong size
  if ((w == iw) && (h == ih))
    return;
  delete [] rng;
  delete [] col;

  // make new input and output images (default to blank)
  col = new unsigned char [w * h * 3];
  rng = new unsigned char [w * h * 2];
  iw = w;
  ih = h;

  // defaultto blank
  memset(col, 0, w * h * 3);
  memset(rng, 0, w * h * 2);
  done = 1;     

// tilting plane as default
wchar_t *d = (wchar_t *) rng;
for (int y = 0; y < h; y++, d += w)
  wmemset(d, 150 + 5 * y, w); 

  // clear global odometry (skip first cumulative update)
  mapx = 0.0;
  mapy = 0.0;
  head = 0.0;
  trav = 0.0;
  wind = 0.0;
  first = 1;                

  // default camera pose 
  cx = 0.0;
  cy = 0.0;
  cz = 0.0;
  cp = 0.0;
  ct = 0.0;
  cr = 0.0;

  // first submitted changes are always full values
  sx = 0.0;
  sy = 0.0;
  sz = 0.0;
  sp = 0.0;
  st = 0.0;
  sr = 0.0;
}


//= Start building depth map from color image and best-guess odometry.
// location of base in global map is (xbase ybase) and traveling along head (degs)
// camera is at (x y z) wrt base and rotated by (p t r) relative to forward and level
// assumes odometry and relative pose are contemporaneous with image acquisition
// returns 1 if new image accepted, 0 if not (typically because busy)

int jhcVisOdom::Estimate (const unsigned char *rgb, double xbase, double ybase, double head, 
                          double x, double y, double z, double p, double t, double r)
{
  double sx0 = sx, sy0 = sy, sz0 = sz, sp0 = sp, st0 = st, sr0 = sr;
  double rads = D2R * head, c = cos(rads), s = sin(rads);

  // barf if system active or not initialized
  if (done <= 0)
    return 0;

  // cache input image to return later
  memcpy(col, rgb, iw * ih * 3);

  // record camera planar offset and aim
  rx = x;
  ry = y;
  rp = p;

  // find submitted full camera pose
  sx = xbase + (-x * s + y * c); 
  sy = ybase + ( x * c + y * s);
  sz = z;
  sp = fmod(head, 360.0) + p;
  st = t;
  sr = r;

  // tweak camera pose based on differences of submitted over time
  ex = cx + (sx - sx0);  
  ey = cy + (sy - sy0);  
  ez = cz + (sz - sz0);  
  ep = cp + (sp - sp0);  
  et = ct + (st - st0);  
  er = cr + (sr - sr0);  

  // start background analysis thread - runs analyze() which sets done
  done = 0;
  pthread_create(&bg, NULL, build_d16, this);
  pthread_detach(bg);
  return 1; 
}


//= Tell if range image ready (1), still processing (0), or never started (-1).

int jhcVisOdom::Ready () const
{
  return done;
}


//= Binds cached input image and aligned depth map to supplied pointers.
// also binds improved estimate of base odometry at time of image acquisition
// location of base in global map is (mx my), total travel of tr, total turn of wd
// rng is 16 bit depth values from camera in 0.02" steps orthogonal to image plane
// output images remain valid until next Estimate() call
// returns 1 if images and odometry bound, 0 if not ready yet (busy)

int jhcVisOdom::Depth (const unsigned char **rgb, const unsigned char **d16, 
                       double& mx, double& my, double& tr, double& wd)
{
  // barf is system active or not initialized
  if (done <= 0) 
    return 0;

  // allow caller to see internal images (quiescent for now)
  if (rgb != NULL)
    *rgb = col;
  if (d16 != NULL)
    *d16 = rng;

  // copy odometry values
  mx = mapx;
  my = mapy;
  tr = trav;
  wd = wind;
  return 1;
}


///////////////////////////////////////////////////////////////////////////
//                           Image Processing                            //
///////////////////////////////////////////////////////////////////////////

//= Generate synthetic depth image and camera pose estimate for color input.

void jhcVisOdom::analyze ()
{
/*
  // project 3D anchors and match to new points
  find_points();
  view_anchors();
  match_points();

  // extract best camera pose from correspondences
  best_view();
  update_odom();

  // refine 3D anchors and interpolate dense depth
  adjust_anchors();
  depth_surface();
*/

  // accept submitted pose
  cx = sx;
  cy = sy;
  cz = sz;
  cp = sp;
  ct = st;
  cr = sr;

  // reprocess for odometry
  update_odom();
//  Sleep(10);

  // signal completion
  done = 1;
}


//= Update global odometry assuming camera pose has already been bound.

void jhcVisOdom::update_odom ()
{
  double rads, c, s, dx, dy, dh, mx0 = mapx, my0 = mapy, hd0 = head;

  // update vase heading based on camera offset
  head = cp - rp;
  rads = D2R * head;
  c = cos(rads); 
  s = sin(rads);

  // update base position based on camera offset
  mapx = cx - (-rx * s + ry * c);
  mapy = cy - ( rx * c + ry * s); 

  // skip rest if first call
  if (first > 0)
  {
    first = 0;
    return;
  }

  // update cumulative travel (straight line)
  dx = mapx - mx0;
  dy = mapy - my0;
  trav += sqrt(dx * dx + dy * dy);    

  // update cumulative turning
  dh = head - hd0;
  if (dh > 180.0)
    dh -= 360.0;
  else if (dh <= -180.0)
    dh += 360.0;
  wind += dh;
}


