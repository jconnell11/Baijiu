// flr_test.cpp : test of pseudo-range inference from color images
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

#pragma comment(lib, "winmm.lib")      // for timeGetTime

#include <windows.h>                   // needed for timeGetTime
#include <stdio.h>
#include <conio.h>

#include "vid_ocv.h"                   
#include "rng_flr.h"

//= Test of pseudo-range inference from color images.
// hardwired for ESP32Cam wifi streaming at VGA 
// shows floor area image and grayscale version of depth
// takes final IP number as argument, e.g. 240 or 155
// NOTE: needs ocv_vid, opencv_world4100, and opencv_videoio_ffmpeg4100_64 DLLs!

int main (int argc, char *argv[])
{
  char ipnam[40] = "http://192.168.0.200:81/stream";
  const unsigned char *vbuf;
  unsigned char *gbuf = NULL, *nbuf = NULL;
  unsigned long start;
  double secs, ht = 5.5, tilt = 4.2;
  int rc, cnt = 0;

  // announce default camera pose
  printf("Assuming power-on camera pose:\n");
  printf("  ht = %3.1f\", tilt = %3.1f degs\n", ht, tilt);

  // build camera URL using argument to exec then try to connect
  if (argc > 1)
    sprintf_s(ipnam, "http://192.168.0.%s:81/stream", argv[1]);
  printf("Opening %s ...\n", ipnam);                                 
  if ((rc = ocv_open(ipnam, 1)) <= 0)            // has OpenCV DLLs?
  {
    printf("Failed to open video source -> %d\n", rc);
    return 0;
  }

  // set standard geometric correction and create two display windows
  ocv_warp(0.14, -0.13, 0.024, 219, 1, 313, 242);  // wide lens
  rng_init(219, 640, 480);          
  ocv_win(0, "Floor", 0, 0);     
  ocv_win(1, "Depth", 650, 0);     

  // create buffers on heap for debugging images
  gbuf = new unsigned char [640 * 480 * 3];
  nbuf = new unsigned char [640 * 480 * 3];

  // continuously framegrab
  printf("Streaming video (hit any key to exit) ...\n");
  start = timeGetTime();
  while (!_kbhit())
  {
    // get next video frame (blocks)
    if (ocv_get(&vbuf, 1) <= 0)      
    {
      printf("Video connection lost!\n");
      break;
    }

    // perform depth inference and wait for completion
    rng_est(vbuf, ht, tilt);
    if (rng_rdy(200) <= 0)
      continue;
    rng_d16(NULL, NULL);               // to reset flag
    rng_gnd(gbuf);
    rng_nite(nbuf);

    // show newest images
    ocv_queue(0, gbuf, 640, 480);
    ocv_queue(1, nbuf, 640, 480);    
    ocv_show(); 
    cnt++;
    printf("\r  %d ", cnt);
    fflush(stdout);
  }

  // report speed 
  secs = 0.001 * (timeGetTime() - start);
  if (cnt > 0)
    printf("frames in %3.1f secs = %3.1f fps\n", secs, cnt / secs);
  else
    printf("  0 frames in 0.0 secs = 0.0 fps\n");

  // clean up
  ocv_close();
  delete [] nbuf;
  delete [] gbuf;

  // keep terminal window visible
  while (_kbhit())
    rc = _getch();
  printf("Hit any key to exit ...\n");
  rc = _getch();
  return 1;
}

