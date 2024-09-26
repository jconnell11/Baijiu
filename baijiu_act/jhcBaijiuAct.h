// jhcBaijiuAct.h : coordinate Qtruck with ALIA variables
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

#pragma once

#include "jhcQtruck.h"


//= Coordinate Qtruck with ALIA variables.

class jhcBaijiuAct : public jhcQtruck
{
// PRIVATE MEMBER VARIABLES
private:
  // putative map coordinates
  double mapx, mapy, ang;

  // mood-based speed factor
  double sf;


// PUBLIC MEMBER FUNCTIONS
public:
  // creation and initialization
  int Setup ();


// PROTECTED MEMBER FUNCTIONS
protected:
  // primary loop
  int Launch ();
  int Respond ();
  void Cleanup ();


// PRIVATE MEMBER FUNCTIONS
private:
  // primary loop 
  void get_sensors ();
  void set_commands ();

  // speech
  void reco_update ();
  void tts_issue ();

  // body
  void body_update ();
  void body_issue ();

  // neck
  void neck_update ();

  // arm 
  void arm_update ();
  void arm_issue ();

  // base
  void base_update ();
  void base_issue ();

};
