// spio_win.h : online Azure speech recognition and local Windows TTS
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

#include <stdlib.h>            // needed for NULL


// function declarations (possibly combined with other header files)

#ifndef DEXP
  #ifdef SPIOWIN_EXPORTS
    #define DEXP __declspec(dllexport)
  #else
    #define DEXP __declspec(dllimport)
  #endif
#endif


// link to library stub

#ifndef SPIOWIN_EXPORTS
  #pragma comment(lib, "spio_win.lib")
#endif


///////////////////////////////////////////////////////////////////////////
//                             Main Functions                            //
///////////////////////////////////////////////////////////////////////////

//= Start recognizing speech and enable TTS output.
// path is home dir for credentials, prog > 0 prints partial recognitions
// reads required key and area from file: <path>/config/spio_win.key
// returns 0 if successful, negative for some error
// NOTE: meant to be called only once at beginning of program

extern "C" DEXP int spio_start (const char *path =NULL, int prog =0);


//= Add proper spellings of names from file: <path>/config/all_names.txt.
// returns number of spellings added, negative for some error
// NOTE: meant to be called only once at beginning of program

extern "C" DEXP int reco_spell (const char *path =NULL);


//= Add a particular name to grammar to increase likelihood of correct spelling.
// can be called even when recognition is actively running

extern "C" DEXP int reco_name (const char *name);


//= Turn microphone on and off (e.g. to prevent TTS transcription).

extern "C" DEXP void reco_mute (int doit);


//= Check to see if any utterances are ready for harvesting.
// return: 2 new result, 1 speaking, 0 silence, -1 unintelligible, -2 lost connection 

extern "C" DEXP int reco_status ();


//= Gives text string of last full recognition result (changes status).

extern "C" DEXP const char *reco_heard ();


//= Gives approximate time (ms) that utterance started before notification.

extern "C" DEXP int reco_delay ();


//= Start speaking some message, overriding any current one (never blocks).
// can alter pitch and/or rate of voice, both in range -10 to +10
// returns 1 if successful, 0 or negative for some error

extern "C" DEXP int tts_say (const char *msg =NULL, int pitch =0, int rate =0);


//= Tells if system has completed emitting utterance yet.
// returns viseme+1 if talking, 0 if queue is empty, negative for some error

extern "C" DEXP int tts_status ();


//= Stop talking and recognizing speech (automatically called at exit).

extern "C" DEXP void spio_done ();







