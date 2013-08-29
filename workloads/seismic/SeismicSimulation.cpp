/*
    Copyright 2005-2010 Intel Corporation.  All Rights Reserved.

    This file is part of Threading Building Blocks.

    Threading Building Blocks is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.

    Threading Building Blocks is distributed in the hope that it will be
    useful, but WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Threading Building Blocks; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    As a special exception, you may use this file as part of a free software
    library without restriction.  Specifically, if other files instantiate
    templates or use macros or inline functions from this file, or you compile
    this file and link it with other files to produce an executable, this
    file does not by itself cause the resulting executable to be covered by
    the GNU General Public License.  This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
*/

//#define _CRT_SECURE_NO_DEPRECATE
//#define VIDEO_WINMAIN_ARGS
#include "video.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <cassert>
#include <math.h>
#include <new>
#ifdef CONCORD
#include "../headers/win_hetero.h"
#include "../headers/malloc_shared.h" 
#include "../headers/win_hetero_runtime.h"
#endif
#if defined TBB_STRESS_BODY || !defined CONCORD  
#include "tbb/task_scheduler_init.h"
#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"
#include "tbb/tick_count.h"
/** Affinity is an argument to parallel_for to hint that an iteration of a loop 
    is best replayed on the same processor for each execution of the loop. 
    It is a global object because it must remember where the iterations happened
    in previous executions. */
static tbb::affinity_partitioner Affinity;
#endif

using namespace std;

#ifdef _MSC_VER
// warning C4068: unknown pragma
#pragma warning(disable: 4068)
#endif

// Uncomment to verify the results using a sequential run
//#define VERIFY_RESULTS

#ifdef VERIFY_RESULTS
#define TEST
#else
#undef TEST
#endif /* VERIFY_RESULTS */


#define DEFAULT_NUMBER_OF_FRAMES 100
int number_of_frames = -1;
const size_t MAX_WIDTH = 1326;
const size_t MAX_HEIGHT = 1950;
#define UNROLL 1

//#define POWER_STUDY 1
#ifdef POWER_STUDY
#define MULTIPLIER 10
#endif

int UniverseHeight=MAX_HEIGHT;
int UniverseWidth=MAX_WIDTH;

typedef float value;

#ifdef CONCORD
//! Velocity at each grid point
value **V;

//! Horizontal stress
value **S;

//! Vertical stress
value **T;

//! Coefficient related to modulus
value **M;

//! Coefficient related to lightness
value **L;

//! Damping coefficients
value **D;
#else
//! Velocity at each grid point
static value V[MAX_HEIGHT][MAX_WIDTH];

//! Horizontal stress
static value S[MAX_HEIGHT][MAX_WIDTH];

//! Vertical stress
static value T[MAX_HEIGHT][MAX_WIDTH];

//! Coefficient related to modulus
static value M[MAX_HEIGHT][MAX_WIDTH];

//! Coefficient related to lightness
static value L[MAX_HEIGHT][MAX_WIDTH];

//! Damping coefficients
static value D[MAX_HEIGHT][MAX_WIDTH];
#endif

#ifdef TEST
//! Velocity at each grid point
static value V_s[MAX_HEIGHT][MAX_WIDTH];

//! Horizontal stress
static value S_s[MAX_HEIGHT][MAX_WIDTH];

//! Vertical stress
static value T_s[MAX_HEIGHT][MAX_WIDTH];

//! Coefficient related to modulus
static value M_s[MAX_HEIGHT][MAX_WIDTH];

//! Coefficient related to lightness
static value L_s[MAX_HEIGHT][MAX_WIDTH];

//! Damping coefficients
static value D_s[MAX_HEIGHT][MAX_WIDTH];
#endif

enum MaterialType {
    WATER=0,
    SANDSTONE=1,
    SHALE=2
};

//! Values are MaterialType, cast to an unsigned char to save space.
static unsigned char Material[MAX_HEIGHT][MAX_WIDTH];

static const colorcomp_t MaterialColor[4][3] = { // BGR
    {96,0,0},     // WATER
    {0,48,48},    // SANDSTONE
    {32,32,23}    // SHALE
};

static const int DamperSize = 32;

static const int ColorMapSize = 1024;
static color_t ColorMap[4][ColorMapSize];

static int PulseTime = 100;
static int PulseCounter;
static int PulseX = UniverseWidth/3;
static int PulseY = UniverseHeight/4;

static bool InitIsParallel = true;
const char *titles[2] = {"Seismic Simulation: Serial", "Seismic Simulation: Parallel"};
//! It is used for console mode for test with different number of threads and also has
//! meaning for gui: threads_low  - use separate event/updating loop thread (>0) or not (0).
//!                  threads_high - initialization value for scheduler
#ifdef CONCORD
int threads_low = 0, threads_high = 1;
#else
int threads_low = 0, threads_high = tbb::task_scheduler_init::automatic;
#endif

static void UpdatePulse() {
    if( PulseCounter>0 ) {
        value t = (PulseCounter-PulseTime/2)*0.05f;
        V[PulseY][PulseX] += 64*sqrt(M[PulseY][PulseX])*exp(-t*t);
        --PulseCounter;
    }
}

static void SerialUpdateStress() {
#ifndef CONCORD
    drawing_area drawing(0, 0, UniverseWidth, UniverseHeight);
#endif
    for( int i=1; i<UniverseHeight-1; ++i ) {
 //       drawing.set_pos(1, i);
#pragma ivdep
        for( int j=1; j<UniverseWidth-1; j+=UNROLL ) {

				S[i][j] += M[i][j]*(V[i][j+1]-V[i][j]);
                T[i][j] += M[i][j]*(V[i+1][j]-V[i][j]);
/*
				float *M_i=M[i];
				float M_ij = M_i[j];
				float M_ij1 = M_i[j+1];
				float M_ij2 = M_i[j+2];
				float M_ij3 = M_i[j+3];
				float *S_i = S[i];
				float *T_i = T[i];
				float **V_ = V;
				float *V_i = V_[i];
				float *V_i1= V_[i+1];
				float V_I_J= V_i[j];
				S_i[j] += M_ij*(V_i[j+1]-V_I_J);
                T_i[j] += M_ij*(V_i1[j]-V_I_J);
				S_i[j+1] += M_ij1*(V_i[j+2]-V_I_J);
                T_i[j+1] += M_ij1*(V_i1[j+1]-V_I_J);

				S_i[j+2] += M_ij2*(V_i[j+3]-V_I_J);
                T_i[j+2] += M_ij2*(V_i1[j+2]-V_I_J);
				S_i[j+3] += M_ij3*(V_i[j+4]-V_I_J);
                T_i[j+3] += M_ij3*(V_i1[j+3]-V_I_J);
*/
/*
				S_i[j+4] += M_i[j+4]*(V_i[j+5]-V_I_J);
                T_i[j+4] += M_i[j+4]*(V_[i+1][j+4]-V_I_J);
				S_i[j+5] += M_i[j+5]*(V_i[j+6]-V_I_J);
                T_i[j+5] += M_i[j+5]*(V_[i+1][j+5]-V_I_J);

				S_i[j+6] += M_i[j+6]*(V_i[j+7]-V_I_J);
                T_i[j+6] += M_i[j+6]*(V_[i+1][j+6]-V_I_J);
				S_i[j+7] += M_i[j+7]*(V_i[j+8]-V_I_J);
                T_i[j+7] += M_i[j+7]*(V_[i+1][j+7]-V_I_J);
*/

#ifndef CONCORD
            int index = (int)(V[i][j]*(ColorMapSize/2)) + ColorMapSize/2;
            if( index<0 ) index = 0;
            if( index>=ColorMapSize ) index = ColorMapSize-1;
            color_t* c = ColorMap[Material[i][j]];
            drawing.put_pixel(c[index]);
#endif
        }
    }
}

struct UpdateStressBody {

#if defined CONCORD && !defined TBB_STRESS_BODY
	UpdateStressBody(value **_V,value **_M,value **_S,value **_T,int UniverseWid){
		LP_V = _V;
		LP_M = _M;
		LP_S = _S;
		LP_T = _T;
		UnivWidth = UniverseWid;
//		Draw_Area = dw;
	}
#ifdef _GNUC_
__attribute__((noinline))
#endif
	void operator()( int range ) const {

//		int i = range;
		range*=UNROLL;
// CONCORD HACK!
#ifdef POWER_STUDY
        range /= MULTIPLIER;
#endif
        int i = range/UnivWidth;
		int j = range%UnivWidth;
		j++;
		i++;//increment as range starts from index 1
/*		Draw_Area[i].start_x=0;
		Draw_Area[i].start_y=i;
		Draw_Area[i].size_x=UnivWidth;
		Draw_Area[i].size_y=1;
*/

        //for( i=range; i!=i_end; ++i )
		{
//#pragma ivdep
            //for( int j=1; j<UnivWidth-1; ++j )
			{
				LP_S[i][j] += LP_M[i][j]*(LP_V[i][j+1]-LP_V[i][j]);
                LP_T[i][j] += LP_M[i][j]*(LP_V[i+1][j]-LP_V[i][j]);
/*
				float *LP_M_i = LP_M[i];
				float LP_M_ij = LP_M_i[j];
				float LP_M_ij1 = LP_M_i[j+1];
				float LP_M_ij2 = LP_M_i[j+2];
				float LP_M_ij3 = LP_M_i[j+3];
				float *LP_S_i = LP_S[i];
				float *LP_T_i = LP_T[i];
				float **LP_V_ = LP_V;
				float *LP_V_i = LP_V_[i];
				float *LP_V_i1 = LP_V_[i+1];
				float LP_V_I_J= LP_V_i[j];

				LP_S_i[j] += LP_M_ij*(LP_V_i[j+1]-LP_V_I_J);
                LP_T_i[j] += LP_M_ij*(LP_V_i1[j]-LP_V_I_J);
				LP_S_i[j+1] += LP_M_ij1*(LP_V_i[j+2]-LP_V_I_J);
                LP_T_i[j+1] += LP_M_ij1*(LP_V_i1[j+1]-LP_V_I_J);

				LP_S_i[j+2] += LP_M_ij2*(LP_V_i[j+3]-LP_V_I_J);
                LP_T_i[j+2] += LP_M_ij2*(LP_V_i1[j+2]-LP_V_I_J);
				LP_S_i[j+3] += LP_M_ij3*(LP_V_i[j+4]-LP_V_I_J);
                LP_T_i[j+3] += LP_M_ij3*(LP_V_i1[j+3]-LP_V_I_J);
*/
/*
				LP_S_i[j+4] += LP_M_i[j+4]*(LP_V_i[j+5]-LP_V_I_J);
                LP_T_i[j+4] += LP_M_i[j+4]*(LP_V_[i+1][j+4]-LP_V_I_J);
				LP_S_i[j+5] += LP_M_i[j+5]*(LP_V_i[j+6]-LP_V_I_J);
                LP_T_i[j+5] += LP_M_i[j+5]*(LP_V_[i+1][j+5]-LP_V_I_J);

				LP_S_i[j+6] += LP_M_i[j+6]*(LP_V_i[j+7]-LP_V_I_J);
                LP_T_i[j+6] += LP_M_i[j+6]*(LP_V_[i+1][j+6]-LP_V_I_J);
				LP_S_i[j+7] += LP_M_i[j+7]*(LP_V_i[j+8]-LP_V_I_J);
                LP_T_i[j+7] += LP_M_i[j+7]*(LP_V_[i+1][j+7]-LP_V_I_J);
*/

//				int index = (int)(LP_V[i][j]*(ColorMapSize/2)) + ColorMapSize/2;
//              if( index<0 ) index = 0;
//              if( index>=ColorMapSize ) index = ColorMapSize-1;
            }
        }
    }
#else
	void operator()( const tbb::blocked_range<int>& range ) const {

        drawing_area drawing(0, range.begin(), UniverseWidth, range.end()-range.begin());
		int i_end = range.end();

        for( int y = 0, i=range.begin(); i!=i_end; ++i,y++ ) {
			 drawing.set_pos(1, y);
#pragma ivdep
            for( int j=1; j<UniverseWidth-1; ++j ) {

				S[i][j] += M[i][j]*(V[i][j+1]-V[i][j]);
                T[i][j] += M[i][j]*(V[i+1][j]-V[i][j]);
                int index = (int)(V[i][j]*(ColorMapSize/2)) + ColorMapSize/2;
                if( index<0 ) index = 0;
                if( index>=ColorMapSize ) index = ColorMapSize-1;
				color_t* c = ColorMap[Material[i][j]];
                drawing.put_pixel(c[index]);
            }
        }
    }
#endif

#if defined CONCORD && !defined  PARALLEL_STRESS_BODY
	value **LP_V;
	value **LP_M;
	value **LP_S;
	value **LP_T;
	int UnivWidth;
//	drawing_area *Draw_Area;
#endif
};

static void ParallelUpdateStress(UpdateStressBody *body) {
#if defined CONCORD && !defined TBB_STRESS_BODY
	int num_iters = ((UniverseHeight-2)*(UniverseWidth-2)/UNROLL);
#ifdef POWER_STUDY
    num_iters *= MULTIPLIER;
#endif
     parallel_for_hetero(num_iters , // Index space for loop
                         *body, GPU);                            // Body of loop
     // Affinity hint
#else
    tbb::parallel_for( tbb::blocked_range<int>( 1, UniverseHeight-1 ), // Index space for loop
                       UpdateStressBody(),                             // Body of loop
                       Affinity );                                     // Affinity hint
#endif
}

static void SerialUpdateVelocity() {
    for( int i=1; i<UniverseHeight-1; ++i ) 
#pragma ivdep
        for( int j=1; j<UniverseWidth-1; ++j ) 
            V[i][j] = D[i][j]*(V[i][j] + L[i][j]*(S[i][j] - S[i][j-1] + T[i][j] - T[i-1][j]));
}

struct UpdateVelocityBody {

#ifdef CONCORD
	UpdateVelocityBody(value **_V,value **_D,value **_L,value **_S,value **_T,int UniverseWid){
		LP_V = _V;
		LP_D = _D;
		LP_L = _L;
		LP_S = _S;
		LP_T = _T;
		UnivWidth = UniverseWid;
	}
#endif
#ifdef CONCORD
#ifdef _GNUC_
__attribute__((noinline))
#endif
    void operator()( int range) const {
        int i = range/UnivWidth;
		int j = range%UnivWidth;
		i++;
		j++;
        //for( int i=range.begin(); i!=i_end; ++i ) 
#pragma ivdep
            //for( int j=1; j<UnivWidth-1; ++j ) 
				LP_V[i][j] = LP_D[i][j]*(LP_V[i][j] + LP_L[i][j]*(LP_S[i][j] - LP_S[i][j-1] + LP_T[i][j] - LP_T[i-1][j]));

	}
#else
    void operator()( const tbb::blocked_range<int>& range ) const {
        int i_end = range.end();
        for( int i=range.begin(); i!=i_end; ++i ) 
#pragma ivdep
            for( int j=1; j<UniverseWidth-1; ++j ) 
                V[i][j] = D[i][j]*(V[i][j] + L[i][j]*(S[i][j] - S[i][j-1] + T[i][j] - T[i-1][j]));
	}
#endif
#ifdef CONCORD
	value **LP_V;
	value **LP_D;
	value **LP_L;
	value **LP_S;
	value **LP_T;
	int UnivWidth;
#endif
};

static void ParallelUpdateVelocity(UpdateVelocityBody *body1) {

#ifdef CONCORD
    parallel_for_hetero((UniverseHeight-2)*(UniverseWidth-2), // Index space for loop
						*body1                           // Body of loop
                       ,GPU);                                     // Affinity hint
#else
    tbb::parallel_for( tbb::blocked_range<int>( 1, UniverseHeight-1 ), // Index space for loop
                       UpdateVelocityBody(),                           // Body of loop
                       Affinity );                                     // Affinity hint
#endif
}

void SerialUpdateUniverse() {
    UpdatePulse();
    SerialUpdateStress();
    SerialUpdateVelocity();
}

void ParallelUpdateUniverse(UpdateStressBody *body,UpdateVelocityBody *body1) {

  UpdatePulse();
  ParallelUpdateStress(body);
  //SerialUpdateStress();
  //ParallelUpdateVelocity(body1);
  SerialUpdateVelocity();
}

class seismic_video : public video
{
    void on_mouse(int x, int y, int key) {
        if(key == 1 && PulseCounter == 0) {
            PulseCounter = PulseTime;
            PulseX = x; PulseY = y;
        }
    }
    void on_key(int key) {
        key &= 0xff;
        if(char(key) == ' ') InitIsParallel = !InitIsParallel;
        else if(char(key) == 'p') InitIsParallel = true;
        else if(char(key) == 's') InitIsParallel = false;
        else if(char(key) == 'e') updating = true;
        else if(char(key) == 'd') updating = false;
        else if(key == 27) running = false;
        title = InitIsParallel?titles[1]:titles[0];
    }
    void on_process() {
#if !defined CONCORD || defined PARALLEL_STRESS_BODY
        tbb::task_scheduler_init Init(threads_high);
#endif
	UpdateStressBody *body=new (malloc_shared(sizeof(UpdateStressBody)))UpdateStressBody(V,M,S,T,UniverseWidth);
	UpdateVelocityBody *body1= new (malloc_shared(sizeof(UpdateVelocityBody))) UpdateVelocityBody(V,D,L,S,T,UniverseWidth);
        do {
            if( InitIsParallel )
                ParallelUpdateUniverse(body,body1);
            else
                SerialUpdateUniverse();
            if( number_of_frames > 0 ) --number_of_frames;
        } while(next_frame() && number_of_frames);
    }
} video;

void InitializeUniverse() {
    PulseCounter = PulseTime;

	//intialize the variables via malloc shared
#ifdef CONCORD
	V =(value**)malloc_shared(sizeof(value*)*MAX_HEIGHT);
	for(int i=0;i<MAX_HEIGHT;i++)
		V[i]=(value*)malloc_shared(sizeof(value)*MAX_WIDTH);

	S =(value**)malloc_shared(sizeof(value*)*MAX_HEIGHT);
	for(int i=0;i<MAX_HEIGHT;i++)
		S[i]=(value*)malloc_shared(sizeof(value)*MAX_WIDTH);

	T=(value**)malloc_shared(sizeof(value*)*MAX_HEIGHT);
	for(int i=0;i<MAX_HEIGHT;i++)
		T[i]=(value*)malloc_shared(sizeof(value)*MAX_WIDTH);

	M =(value**)malloc_shared(sizeof(value*)*MAX_HEIGHT);
	for(int i=0;i<MAX_HEIGHT;i++)
		M[i]=(value*)malloc_shared(sizeof(value)*MAX_WIDTH);

	L =(value**)malloc_shared(sizeof(value*)*MAX_HEIGHT);
	for(int i=0;i<MAX_HEIGHT;i++)
		L[i]=(value*)malloc_shared(sizeof(value)*MAX_WIDTH);

	D=(value**)malloc_shared(sizeof(value*)*MAX_HEIGHT);
	for(int i=0;i<MAX_HEIGHT;i++)
		D[i]=(value*)malloc_shared(sizeof(value)*MAX_WIDTH);

#endif

    // Initialize V, S, and T to slightly non-zero values, in order to avoid denormal waves.
    for( int i=0; i<UniverseHeight; ++i ) 
#pragma ivdep
        for( int j=0; j<UniverseWidth; ++j ) {
            T[i][j] = S[i][j] = V[i][j] = value(1.0E-6);
        }
    for( int i=1; i<UniverseHeight-1; ++i ) {
        for( int j=1; j<UniverseWidth-1; ++j ) {
            float x = float(j-UniverseWidth/2)/(UniverseWidth/2);
            value t = (value)i/UniverseHeight;
            MaterialType m;
            D[i][j] = 1.0;
            // Coefficient values are fictitious, and chosen to visually exaggerate 
            // physical effects such as Rayleigh waves.  The fabs/exp line generates
            // a shale layer with a gentle upwards slope and an anticline.
            if( t<0.3f ) {
                m = WATER;
                M[i][j] = 0.125;
                L[i][j] = 0.125;
            } else if( fabs(t-0.7+0.2*exp(-8*x*x)+0.025*x)<=0.1 ) {
                m = SHALE;
                M[i][j] = 0.5;
                L[i][j] = 0.6;
            } else {
                m = SANDSTONE;
                M[i][j] = 0.3;
                L[i][j] = 0.4;
            } 
            Material[i][j] = m;
        }
    }
    value scale = 2.0f/ColorMapSize;
    for( int k=0; k<4; ++k ) {
        for( int i=0; i<ColorMapSize; ++i ) {
            colorcomp_t c[3];
            value t = (i-ColorMapSize/2)*scale;
            value r = t>0 ? t : 0;
            value b = t<0 ? -t : 0;
            value g = 0.5f*fabs(t);
            memcpy(c, MaterialColor[k], sizeof(c));
            c[2] = colorcomp_t(r*(255-c[2])+c[2]);
            c[1] = colorcomp_t(g*(255-c[1])+c[1]);
            c[0] = colorcomp_t(b*(255-c[0])+c[0]);
            ColorMap[k][i] = video.get_color(c[2], c[1], c[0]);
        }
    }
    // Set damping coefficients around border to reduce reflections from boundaries.
    value d = 1.0;
    for( int k=DamperSize-1; k>0; --k ) {
        d *= 1-1.0f/(DamperSize*DamperSize);
        for( int j=1; j<UniverseWidth-1; ++j ) {
            D[k][j] *= d;
            D[UniverseHeight-1-k][j] *= d;
        }
        for( int i=1; i<UniverseHeight-1; ++i ) {
            D[i][k] *= d;
            D[i][UniverseWidth-1-k] *= d;
        }
    }
}

//////////////////////////////// Interface ////////////////////////////////////
#ifdef _WINDOWS
#include "msvs/resource.h"
#endif

#ifdef _WINDOWS

int main_(int, char *[]);


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, PSTR szCmdLine, int iCmdShow)
{
    //assert(false);
    video::win_hInstance = hInstance;  video::win_iCmdShow = iCmdShow;
    // threads number init
    return main_(__argc, __argv);
}


int main_(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
    if(argc > 1 && isdigit(argv[1][0])) {
        char* end; threads_high = threads_low = (int)strtol(argv[1],&end,0);
        switch( *end ) {
            case ':': threads_high = (int)strtol(end+1,0,0); break;
            case '\0': break;
            default: printf("unexpected character = %c\n",*end);
        }
    }
    if (argc > 2 && isdigit(argv[2][0])){
        number_of_frames = (int)strtol(argv[2],0,0);
    }
#ifdef CONCORD
    hetero_init();
#endif
    // video layer init
    video.title = InitIsParallel?titles[1]:titles[0];
#ifdef _WINDOWS
    #define MAX_LOADSTRING 100
    TCHAR szWindowClass[MAX_LOADSTRING];    // the main window class name
    LoadStringA(video::win_hInstance, IDC_SEISMICSIMULATION, szWindowClass, MAX_LOADSTRING);
    LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    WNDCLASSEX wcex; memset(&wcex, 0, sizeof(wcex));
    wcex.lpfnWndProc    = (WNDPROC)WndProc;
    wcex.hIcon          = LoadIcon(video::win_hInstance, MAKEINTRESOURCE(IDI_SEISMICSIMULATION));
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = LPCTSTR(IDC_SEISMICSIMULATION);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(video::win_hInstance, MAKEINTRESOURCE(IDI_SMALL));
    video.win_set_class(wcex); // ascii convention here
    video.win_load_accelerators(IDC_SEISMICSIMULATION);
#endif /* _WINDOWS */

    if (video.init_window(UniverseWidth, UniverseHeight)) {
        video.calc_fps = true;
        video.threaded = threads_low > 0;
        // video is ok, init universe
        InitializeUniverse();
        // main loop
        video.main_loop();
    } else if(video.init_console()) {
        // do console mode
        if (number_of_frames <= 0) {
            number_of_frames = DEFAULT_NUMBER_OF_FRAMES;
        }
#ifndef VERIFY_RESULTS
        threads_low = 1;
#endif  /* !VERIFY_RESULTS */
#ifndef CONCORD
        if (threads_high == tbb::task_scheduler_init::automatic) {
            threads_high = 2;
        }
#endif /* !CONCORD */
        if (threads_high < threads_low) {
            threads_high = threads_low;
        }

/*************
		threads_low = 0
	        threads_high =8;
		number_of_frames =  1000;
*************/

        for (int p = threads_low;  p <= threads_high;  ++p ) {
            InitializeUniverse();
#ifndef CONCORD
            tbb::task_scheduler_init init(tbb::task_scheduler_init::deferred);
            if( p > 0 )
                init.initialize( p );
            tbb::tick_count t0 = tbb::tick_count::now();
#else  /* CONCORD */
            long long t0;
#if 1
            LARGE_INTEGER qpcnt;
            QueryPerformanceCounter(&qpcnt);
            t0 = qpcnt.QuadPart;
#endif
#endif /* CONCORD */
            UpdateStressBody *body = new(malloc_shared(sizeof(UpdateStressBody)))UpdateStressBody(V,M,S,T,UniverseWidth-2);
            UpdateVelocityBody *body1 = new(malloc_shared(sizeof(UpdateVelocityBody))) UpdateVelocityBody(V,D,L,S,T,UniverseWidth-2);
            if ( p > 0 ) {
                for (int i=0; i < number_of_frames; ++i )
                    ParallelUpdateUniverse(body, body1);
            } else {
                for (int i=0; i < number_of_frames; ++i )
                    SerialUpdateUniverse();
            }
#ifndef CONCORD
            tbb::tick_count t1 = tbb::tick_count::now();
            printf("%.1f frame per sec", number_of_frames/(t1-t0).seconds());
#else  /* CONCORD */
            long long t1;
#if 1
            QueryPerformanceCounter(&qpcnt);
            t1 = qpcnt.QuadPart;
            LARGE_INTEGER qpfreq;
            QueryPerformanceFrequency(&qpfreq);
            double sec = (double)(t1-t0)/((double)qpfreq.QuadPart);
            if (p > 0) {
                printf("CONCORD:: Parallel done: %f seconds wall-clock time\n", sec);
            }
            printf("%.1f frame per sec", number_of_frames/sec);
#endif
#endif /* CONCORD */
            if ( p > 0 ) {
                printf(" with %d way parallelism\n",p);
            } else {
                printf(" with serial version\n"); 
#ifdef TEST
                for (int i=0; i<MAX_HEIGHT; i++){
                    for (int j=0; j<MAX_WIDTH; j++){
                        V_s[i][j]= V[i][j];
                        S_s[i][j]= S[i][j];
                        T_s[i][j]= T[i][j];
                        M_s[i][j]= M[i][j];
                        L_s[i][j]= L[i][j];
                        D_s[i][j]= D[i][j];
                    }
                }
#endif /* TEST */
            }
        }
    }

#ifdef TEST
    int flag=0;
    for(int i=0; i<MAX_HEIGHT; i++){
        for(int j=0; j<MAX_WIDTH; j++){
            if ((fabs(V_s[i][j]- V[i][j])) > 0.00001f) {
                //printf("ERROR VS%f VP%f at %d",V_s[i][j],V[i][j],i);
                flag=1;
                break;
            }
            if ((fabs(S_s[i][j] - S[i][j])) > 0.00001f) {
                //printf("ERROR SS%f SP%f at %d",S_s[i][j],S[i][j],i);
                flag=1;
                break;
            }
            if ((fabs(T_s[i][j] - T[i][j])) > 0.00001f) {
                //printf("ERROR SS%f SP%f at %d",S_s[i][j],S[i][j],i);
                flag=1;
                break;
            }
//			T_s[i][j]= T[i][j];
//			M_s[i][j]= M[i][j];
//			L_s[i][j]= L[i][j];
//			D_s[i][j]= D[i][j];
        }
        if (flag) break;
    }
    if (flag){
        printf("TEST FAILED.....\n");
    } else {
        printf("TEST PASSED.....\n");
    }
#endif /* TEST */

    video.terminate();
#ifdef CONCORD
    hetero_cleanup();
#endif /* CONCORD */
    return 0;
}

#ifdef _WINDOWS
//
//  FUNCTION: WndProc(HWND, unsigned, WORD, LONG)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG: return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return TRUE;
        }
        break;
    }
    return FALSE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId, wmEvent;
    switch (message) {
    case WM_COMMAND:
        wmId    = LOWORD(wParam); 
        wmEvent = HIWORD(wParam); 
        // Parse the menu selections:
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(video::win_hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, (DLGPROC)About);
            break;
        case IDM_EXIT:
            PostQuitMessage(0);
            break;
        case ID_FILE_PARALLEL:
            if( !InitIsParallel ) {
                InitIsParallel = true;
                video.title = titles[1];
            }
            break;
        case ID_FILE_SERIAL:
            if( InitIsParallel ) {
                InitIsParallel = false;
                video.title = titles[0];
            }
            break;
        case ID_FILE_ENABLEGUI:
            video.updating = true;
            break;
        case ID_FILE_DISABLEGUI:
            video.updating = false;
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

#endif
