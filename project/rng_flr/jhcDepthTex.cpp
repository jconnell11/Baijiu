// jhcDepthTex.cpp : builds approx depth map from color image in background
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

#include "jhcDepthTex.h"


///////////////////////////////////////////////////////////////////////////
//                      Creation and Initialization                      //
///////////////////////////////////////////////////////////////////////////

//= Default destructor does necessary cleanup.

jhcDepthTex::~jhcDepthTex ()
{
  if (done == 0)
    Sleep(500);
}


//= Default constructor initializes certain values.

jhcDepthTex::jhcDepthTex ()
{
  done = -1;             
}


///////////////////////////////////////////////////////////////////////////
//                              Main Functions                           //
///////////////////////////////////////////////////////////////////////////

//= Configure system for given focal length and a certain input image size.

void jhcDepthTex::Init (double flen, int iw, int ih)
{
  // wait for background thread (if needed)
  if (done == 0)
    Sleep(500);
  done = -1;

  // set size all intermediate images, input, and output
  jhcPlainFloor::Init(flen, iw, ih);
  col.SetSize(iw, ih, 3);
  rng.SetSize(iw, ih, 2);
}


//= Start building depth map from color image and partial camera pose.
// pixel data is BGR triples scanned left-to-right and bottom-up
// camera is up ht inches from floor, and tipped tilt degrees relative to level
// returns 1 if new image accepted, 0 if not (typically because busy)

int jhcDepthTex::Estimate (const unsigned char *rgb, double ht, double tilt)
{
  // barf if system active, no input image, or not initialized
  if ((done == 0) || (rgb == NULL) || !col.Valid())
    return 0;

  // cache input values
  col.CopyArr(rgb);
  cz = ht;
  ct = tilt;

  // start background analysis thread - runs analyze() which sets done
  done = 0;
  pthread_create(&bg, NULL, build_d16, this);
  pthread_detach(bg);
  return 1; 
}


//= Generate synthetic depth image and camera pose estimate for color input.
// function called by background thread in build_16()

void jhcDepthTex::analyze ()
{
  Range(rng, col, cz, ct);   // use local output image and cached inputs
  done = 1;                  // signal completion
}


//= Tell if range image ready (1), still processing (0), or never started (-1).
// can optionally wait for ms milliseconds if still processing

int jhcDepthTex::Ready (int ms) const
{
  int i;

  if (done == 0)
    for (i = 0; i < ms; i++)
    {
      Sleep(1);
      if (done != 0)
        break;
    }
  return done;
}


//= Binds cached input image and aligned depth map to supplied pointers.
// also binds improved estimate of base odometry at time of image acquisition
// location of base in global map is (mx my), total travel of tr, total turn of wd
// rng is 16 bit depth values from camera in 0.25mm steps orthogonal to image plane
// resets state to -1, output images remain valid until next Estimate() call
// returns 1 if images and odometry bound, 0 if not ready yet (busy)

int jhcDepthTex::Depth (const unsigned char **d16, const unsigned char **rgb)
{
  // barf is system active or not initialized
  if (done <= 0) 
    return 0;
  done = -1;

  // allow caller to see internal images (quiescent for now)
  if (d16 != NULL)
    *d16 = rng.PxlSrc();
  if (rgb != NULL)
    *rgb = col.PxlSrc();
  return 1;
}


///////////////////////////////////////////////////////////////////////////
//                           Debugging Images                            //
///////////////////////////////////////////////////////////////////////////

//= Show floor area as green in supplied color image.
// image buffer filled BGR bottom-up, assumes large enough = iw * ih * 3

void jhcDepthTex::Ground (unsigned char *buf)
{
  if (buf == NULL)
    return;
  gwrap.Wrap(buf, iw, ih, 3);
  Grass(gwrap);
}


//= Show range fading with distance in supplied color image.
// image buffer filled BGR bottom-up, assumes large enough = iw * ih * 3

void jhcDepthTex::Night (unsigned char *buf)
{
  if (buf == NULL)
    return;
  nwrap.Wrap(buf, iw, ih, 3);
  NightSD(tmp, rng);
  CopyMono(nwrap, tmp);
}

