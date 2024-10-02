// spio_win.cpp : online Azure speech recognition and local Windows TTS
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

// NOTE: must install NuGet package "Microsoft.CognitiveServices.Speech" as described at
// https://learn.microsoft.com/en-us/azure/ai-services/speech-service/quickstarts/setup-platform

#include <combaseapi.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <speechapi_cxx.h>
#include <sapi.h>

#include "spio_win.h"

using namespace Microsoft::CognitiveServices::Speech;


// Local function prototypes

BOOL init ();
BOOL shutdown ();
void next_chunk ();


///////////////////////////////////////////////////////////////////////////
//                          Global Variables                             //
///////////////////////////////////////////////////////////////////////////

//= Local Windows Text-to-Speech engine.

static ISpVoice *voice = NULL;


//= Microsoft Azure speech recognizer instance.

static std::shared_ptr<Microsoft::CognitiveServices::Speech::SpeechRecognizer> svc = NULL;


//= Special phrase list for adding people's names.

static std::shared_ptr<Microsoft::CognitiveServices::Speech::PhraseListGrammar> vocab = NULL;


//= Interface for determining if internet connection is working.

static std::shared_ptr<Microsoft::CognitiveServices::Speech::Connection> net = NULL;


//= COM object for controlling microphone muting.

static IAudioEndpointVolume *mic = NULL;


//= Whether to print partial recognition results.

static int show = 0;


//= Microphone muting status.

static int deaf = 0;


//= Most recent status from recognizer.

static int reco = 0;


//= Most recent recognition result.

static char heard[500];


//= Result duration plus endpoint silence (ms);

static int delay = 0;


//= Run-on utterance chunking variables.

static char blob[500] = "";
static const char *read = blob;


///////////////////////////////////////////////////////////////////////////
//                            Initialization                             //
///////////////////////////////////////////////////////////////////////////

//= DLL entry point.

BOOL APIENTRY DllMain (HANDLE hModule,
                       DWORD ul_reason_for_call, 
                       LPVOID lpReserved)
{
  if (ul_reason_for_call == DLL_PROCESS_DETACH)
    return shutdown();
  if (ul_reason_for_call != DLL_PROCESS_ATTACH)
    return TRUE;
  return init();
}


//= Do all system initializations.

BOOL init ()
{
  IMMDeviceEnumerator *list;
  IMMDevice *dev;

  // connect to COM
  CoInitialize(NULL);

  // get microphone control point
  CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator), (LPVOID *) &list);
  if (list != NULL)
  {
    list->GetDefaultAudioEndpoint(eCapture, eConsole, &dev);
    list->Release();
    if (dev != NULL)
    {
      dev->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, NULL, (LPVOID *) &mic);
      dev->Release();
    }
  }

  // get a TTS engine
  CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void **)(&voice));
  return TRUE;
}


//= Do all clean up activities.

BOOL shutdown ()
{
  // stop reco and talking
  spio_done();

  // get rid of mic control and TTS engine
  if (mic != NULL)
    mic->Release();
  if (voice != NULL)
    voice->Release();

  // release COM
  CoUninitialize();
  return TRUE;
}


///////////////////////////////////////////////////////////////////////////
//                             Main Functions                            //
///////////////////////////////////////////////////////////////////////////

//= Start recognizing speech and enable TTS output.
// path is home dir for files, prog > 0 prints partial recognitions
// reads required key and area from file:    config/spio_win.key
// optionally reads special names from file: config/all_names.txt
// returns 1 if successful, 0 if cannot connect, neg for bad credentials
// NOTE: meant to be called only once at beginning of program

extern "C" DEXP int spio_start (const char *path, int prog)
{
  std::shared_ptr<Microsoft::CognitiveServices::Speech::SpeechConfig> cfg;
  char fn[200], name[80], key[80] = "", reg[40] = "";
  FILE *in;
  int i, n;

  // can only be called once
  if (svc != NULL)
    return -4;
  show = prog;

  // read Azure web credentials from file and check format
  sprintf_s(fn, "%s/config/spio_win.key", ((path == NULL) ? "." : path));
  if (fopen_s(&in, fn, "r") != 0)
    return -3;                                                 // bad file
  fscanf_s(in, "%s %s", key, 80, reg, 40);
  fclose(in);
  if (((int) strlen(key) != 32) || (*reg == '\0'))             
    return -2;                                                 // bad format
  for (i = 0; i < 32; i++)
    if (isxdigit(key[i]) == 0)
      return -2;                                               // bad format

  // create instance of recognizer based on credentials
  if ((cfg = SpeechConfig::FromSubscription(key, reg)) == NULL)
    return -1;                                                 // invalid credentials
  cfg->SetProfanity(ProfanityOption::Raw);
  cfg->SetProperty(PropertyId::Speech_SegmentationSilenceTimeoutMs, "500");
  if ((svc = SpeechRecognizer::FromConfig(cfg)) == NULL)
    return 0;                                                  // no internet?

  // add proper spellings of names from file: config/all_names.txt
  vocab = PhraseListGrammar::FromRecognizer(svc);       
  sprintf_s(fn, "%s/config/all_names.txt", ((path == NULL) ? "." : path));
  if (fopen_s(&in, fn, "r") == 0)
  {
    while (fgets(name, 80, in) != NULL)
      if ((n = (int) strlen(name)) > 0)
      {
        name[n - 1] = '\0';
        vocab->AddPhrase(name);                        // limit of 1024
      }
    fclose(in);
  }

  // ---------------------------------------------------------------------------
  // CALLBACK: for partial result
  svc->Recognizing.Connect([] (const SpeechRecognitionEventArgs& e)
  {
    if (show > 0)
      printf("  %s ...\n", (e.Result->Text).data());
    reco = 1;                                                  // speaking
  });

  // ---------------------------------------------------------------------------
  // CALLBACK: for final result
  svc->Recognized.Connect([] (const SpeechRecognitionEventArgs& e)
  {
    if (e.Result->Reason == ResultReason::RecognizedSpeech)
    {
      const char *res = (e.Result->Text).data(); 
      if ((*res != '\0') && (strcmp(res, "Hey, Cortana.") != 0))      // quirk
      {
        delay = (int)(0.0001 * e.Result->Duration()) + 500;
        strcpy_s(blob, res); 
        read = blob;
        reco = 2;
      }
    }
    else if (e.Result->Reason == ResultReason::NoMatch)
      reco = -1;                                               // unintelligible
  });

  // ---------------------------------------------------------------------------
  // CALLBACK: for network monitoring
  if ((net = Connection::FromRecognizer(svc)) != NULL)
    net->Disconnected.Connect([] (const ConnectionEventArgs& e)
    {
      reco = -2;                                               // lost network
    });

  // start processing speech input right now
  svc->StartContinuousRecognitionAsync().get();
  return 1;
}


//= Add a particular name to grammar to increase likelihood of correct spelling.
// can be called even when recognition is actively running

extern "C" DEXP int reco_name (const char *name)
{
  if ((svc == NULL) || (vocab == NULL))
    return -1;
  if ((name == NULL) || (*name == '\0'))
    return 0;
  vocab->AddPhrase(name);                        // limit of 1024
  return 1;
}


//= Turn microphone on and off (e.g. to prevent TTS transcription).

extern "C" DEXP void reco_mute (int doit)
{
  int prev = deaf;

  deaf = ((doit > 0) ? 1 : 0);
  if (deaf != prev)
    mic->SetMute((deaf > 0), NULL);
}


//= Check to see if any utterances are ready for harvesting.
// return: 2 new result, 1 speaking, 0 silence, -1 unintelligible, -2 lost connection 

extern "C" DEXP int reco_status ()
{
  return reco;
}


//= Gives text string of last full recognition result (changes status).

extern "C" DEXP const char *reco_heard ()
{
  next_chunk();
  return heard;
}
 

//= Gives approximate time (ms) that utterance started before notification.

extern "C" DEXP int reco_delay ()
{
  return delay;
}


//= Start speaking some message, overriding any current one (never blocks).
// returns 1 if successful, 0 or negative for some error

extern "C" DEXP int tts_say (const char *msg)
{
  WCHAR wide[200];

  // stop any talking currently in progress
  voice->Speak(NULL, SPF_PURGEBEFORESPEAK, NULL);
  if ((msg == NULL) || (*msg == '\0'))
    return 1;

  // start speaking new message
  MultiByteToWideChar(CP_UTF8, 0, msg, -1, wide, 200);
  if (FAILED(voice->Speak(wide, SPF_ASYNC, NULL)))
    return 0;
  return 1;
}


//= Tells if system has completed emitting utterance yet.
// returns viseme+1 if talking, 0 if queue is empty, negative for some error

extern "C" DEXP int tts_status ()
{
  SPVOICESTATUS info;

  if (FAILED(voice->GetStatus(&info, NULL)))
    return -1;
  if (info.dwRunningState == SPRS_DONE)
    return 0;
  return(info.VisemeId + 1);
}


//= Stop talking and recognizing speech (automatically called at exit).

extern "C" DEXP void spio_done ()
{
  // stop talking and re-enable mic if needed
  tts_say();
  reco_mute(0);

  // stop online speech recognition and clear results
  if (svc != NULL)
    svc->StopContinuousRecognitionAsync().get();
  reco = 0;

  // smart pointer release
  vocab = NULL;    
  net = NULL;      
  svc = NULL;      
}


///////////////////////////////////////////////////////////////////////////
//                           Response Chunking                           //
///////////////////////////////////////////////////////////////////////////

//= Use Inverse-Text-Normalized form to help break up long lexical results.
// blob = "I saw you in London. I then went to France."
// call 1 --> heard = "I saw you in London."   + reco = 2 (still)
// call 2 --> heard = "I then went to France." + reco = 0 (done)

void next_chunk ()
{
  char abbr[6][10] = {"Dr", "Mr", "Ms", "Mrs", "Prof", "St"};
  const char *end, *scan = read;
  int i, n, bk;

  // clear output then sanity check
  *heard = '\0';
  if (reco < 2)
    return;

  // look for terminal punctuation
  while ((end = strpbrk(scan, ".!?")) != NULL)
  {
    // ? and ! end immediately but ignore . inside word (e.g. "3.14")
    if (*end != '.')
      break;
    if (end[1] != ' ')  
    {
      scan = end + 1;
      continue;
    }
    n = (int)(end - read);

    // possibly expand "Dr" to "drive" (not if "Dr. Jones")
    if ((n >= 2) && (strncmp(end - 2, "Dr", 2) == 0))   
      if ((end[1] != ' ') || !isupper(end[2]) || (strchr("FB", end[2]) != NULL))       
      {
        // copy string without "Dr" then add replacement
        strncat_s(heard, read, n - 2);  
        strcat_s(heard, "drive");
        if (end[1] == '\0')             // end of blob
          strcat_s(heard, ".");         

        // keep scanning after any trailing space
        read = end + 1;
        scan = read;
        continue;
      }

    // check for allowable abbreviations
    for (i = 0; i < 6; i++)
      if ((bk = (int) strlen(abbr[i])) <= n)
        if (strncmp(end - bk, abbr[i], bk) == 0)
          break;
    if (i >= 6)                         // true end found
      break;
    scan = end + 1; 
  }

  // copy rest of allowed portion of string
  if (end != NULL)
    n = (int)(end - read) + 1;
  else
    n = (int) strlen(read);
  strncat_s(heard, read, n);

  // set up for remainder of blob (if any)
  read += n;
  if (read[0] != '\0')
    read++;                    // skip space
  else
    reco = 0;                  
}
