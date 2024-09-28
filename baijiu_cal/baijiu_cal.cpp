// baijiu_cal.cpp : camera calibration for Qtruck used with pc_blulink.py 
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

#ifndef DEXP
 #define DEXP __declspec(dllexport)
#endif

#include <windows.h>
#include <stdio.h>

#include "jhcQtCamCal.h"


///////////////////////////////////////////////////////////////////////////
//                      Initialization and Locking                       //
///////////////////////////////////////////////////////////////////////////

//= Defines the entry point for the DLL application.

BOOL APIENTRY DllMain (HANDLE hModule,
                       DWORD ul_reason_for_call, 
                       LPVOID lpReserved)
{
  return TRUE;
}


///////////////////////////////////////////////////////////////////////////
//                          Global Variables                             //
///////////////////////////////////////////////////////////////////////////

//= An instance of the main computational class.

static jhcQtCamCal qt;


//= Decimal coded command string for Qtruck (null terminated).

static char cmd[14] = "4949090755510";


//= How many respond() calls have been made so far.

static int cnt = 0;


///////////////////////////////////////////////////////////////////////////
//                           Main Functions                              //
///////////////////////////////////////////////////////////////////////////

//= Initializes external system before attempting robot connection.
// returns positive if successful, 0 or negative for failure

extern "C" DEXP int ext_init ()
{
  return qt.Setup();
}


//= Resets processing state at the start of a run given robot ID.
// at this point robot should be connected and responsive
// returns positive if successful, 0 or negative for failure

extern "C" DEXP int ext_start (const char *id)
{
  return qt.BluStart(id);
}


//= Takes a Qtruck sensor string and returns a command string.
// sensor data is hex coded = CC:TT:RR:DD:L:V      (10 chars)
// command is decimal coded = LL:RR:BBB:FF:GG:C:M  (13 chars)
// Note: call rate varies from 16-32 Hz, return NULL or "" to exit

extern "C" DEXP const char *ext_swap (const char *data)
{
  return qt.BluSwap(data);
}


//= Releases any allocated resources (call at end of run).

extern "C" DEXP void ext_done ()
{
  qt.BluDone();
}


