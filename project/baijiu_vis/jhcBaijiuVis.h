// jhcBaijiuVis.h : coordinate Qtruck with ALIA variables and camera
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

#pragma once

#include "jhcQtruck.h"


//= Coordinate Qtruck with ALIA variables and camera.

class jhcBaijiuVis : public jhcQtruck
{
// PRIVATE MEMBER VARIABLES
private:
  // color image, debugging images, and I/O status
  const unsigned char *snap;
  unsigned char *view, *map;
  int vok, got, show;

// video performance
unsigned long ms;
int vcnt, fcnt;

  // cumulative odometry and interim adjustements
  double mapx, mapy, trav, wind, imapx, imapy, itrav, iwind;

  // voice modifications and mood-based speed factor
  int p0, r0;
  double sf;


// PUBLIC MEMBER FUNCTIONS
public:
  // creation and initialization
  ~jhcBaijiuVis ();
  jhcBaijiuVis ();


// PROTECTED MEMBER FUNCTIONS
protected:
  // primary loop
  int Launch (int dbg =0);
  int Respond ();
  void Cleanup ();


// PRIVATE MEMBER FUNCTIONS
private:
  // primary loop 
  void assemble_sensors ();
  void expand_commands ();
  int arm_mode ();

  // speech
  void reco_update ();
  void tts_issue ();
  void prosody (int& dp, int& dr, int bits) const;

  // body
  void body_update ();
  void body_issue ();

  // neck
  void neck_update ();
  void neck_issue ();

  // arm 
  void arm_update ();
  void arm_issue ();

  // base
  void base_update ();
  void base_issue ();

  // images
  void img_update ();
  void img_issue ();

};
