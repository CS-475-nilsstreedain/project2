#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <omp.h>

int     NowYear;        // 2025- 2030
int     NowMonth;       // 0 - 11

float   NowPrecip;      // inches of rain per month
float   NowTemp;        // temperature this month
float   NowHeight;      // grain height in inches
int     NowNumDeer;     // number of deer in the current population
float   NowFert;        // fertilizer level (our extra agent)

// barrier data:
omp_lock_t    Lock;
volatile int    NumInThreadTeam;
volatile int    NumAtBarrier;
volatile int    NumGone;

// simulation constants:
const float GRAIN_GROWS_PER_MONTH =     12.0;
const float ONE_DEER_EATS_PER_MONTH =   1.0;

const float AVG_PRECIP_PER_MONTH =      7.0;    // average
const float AMP_PRECIP_PER_MONTH =      6.0;    // plus or minus
const float RANDOM_PRECIP =             2.0;    // plus or minus noise

const float AVG_TEMP =                  60.0;   // average
const float AMP_TEMP =                  20.0;   // plus or minus
const float RANDOM_TEMP =               10.0;   // plus or minus noise

const float MIDTEMP =                   40.0;
const float MIDPRECIP =                 10.0;

const float FERT_DECAY_RATE =           0.9f;  // retains 90% each month
const float FERT_APPLY_HIGH =           2.0f;   // extra if grain low
const float FERT_APPLY_LOW =            0.5f;   // small top up if grain healthy
const float GRAIN_FERTILE_BOOST =       0.1f;  // 10% growth per unit fert

float Ranf( float low, float high ) {
    float r = (float) rand();               // 0 - RAND_MAX
    float t = r  /  (float) RAND_MAX;       // 0. - 1.

    return   low  +  t * ( high - low );
}

float SQR( float x ) {
    return x*x;
}

// specify how many threads will be in the barrier:
//    (also init's the Lock)

void InitBarrier( int n ) {
    NumInThreadTeam = n;
    NumAtBarrier = 0;
    omp_init_lock(&Lock);
}

// have the calling thread wait here until all the other threads catch up:

void WaitBarrier() {
    omp_set_lock( &Lock );
    NumAtBarrier++;
    if ( NumAtBarrier == NumInThreadTeam ) {
        NumGone = 0;
        NumAtBarrier = 0;
        // let all other threads get back to what they were doing
        // before this one unlocks, knowing that they might immediately
        // call WaitBarrier( ) again:
        while ( NumGone != NumInThreadTeam-1 ) {}
        omp_unset_lock( &Lock );
        return;
    }
    omp_unset_lock( &Lock );

    while ( NumAtBarrier != 0 ) {}    // this waits for the nth thread to arrive
    
    #pragma omp atomic
    NumGone++;            // this flags how many threads have returned
}

void Grain() {
    while (NowYear < 2031) {
        // 1. compute next height
        float tempFactor = exp(   -SQR(  ( NowTemp - MIDTEMP ) / 10.  )   );
        float precipFactor = exp(   -SQR(  ( NowPrecip - MIDPRECIP ) / 10.  )   );
        float grow = tempFactor * precipFactor * GRAIN_GROWS_PER_MONTH;
        
        // boost from fertilizer:
        grow *= (1.0f + NowFert * GRAIN_FERTILE_BOOST);

        float nextHeight = NowHeight;
        nextHeight += grow;
        nextHeight -= (float)NowNumDeer * ONE_DEER_EATS_PER_MONTH;
        
        if( nextHeight < 0. ) nextHeight = 0.;

        WaitBarrier();              // DoneComputing
        NowHeight = nextHeight;     // DoneAssigning
        WaitBarrier();
        WaitBarrier();
    }
}

void Deer() {
    while (NowYear < 2031) {
        int nextNumDeer = NowNumDeer;
        int carryingCapacity = (int)( NowHeight );
        if( nextNumDeer < carryingCapacity )
            nextNumDeer++;
        else if( nextNumDeer > carryingCapacity )
            nextNumDeer--;

        if( nextNumDeer < 0 )
            nextNumDeer = 0;

        WaitBarrier();              // DoneComputing
        NowNumDeer = nextNumDeer;   // DoneAssigning
        WaitBarrier();
        WaitBarrier();
    }
}

void Fertilizer() {
    while (NowYear < 2031) {
        float nextFert = NowFert * FERT_DECAY_RATE;
        if (NowHeight < 10.0f)
            nextFert += FERT_APPLY_HIGH;
        else
            nextFert += FERT_APPLY_LOW;

        WaitBarrier();          // DoneComputing
        NowFert = nextFert;     // DoneAssigning
        WaitBarrier();
        WaitBarrier();
    }
}

void Watcher() {
    printf("Month:, Temp:, Precip:, Grain:, Deer:, Fert:\n");
    while (NowYear < 2031) {
        // print current state
        printf("%d-%d, %6.2f, %5.2f, %6.2f, %2d, %4.2f\n", NowYear, NowMonth+1, NowTemp, NowPrecip, NowHeight, NowNumDeer, NowFert);

        // advance time
        NowMonth++;
        if (NowMonth == 12) {
            NowMonth = 0;
            NowYear++;
        }

        // update weather
        float ang = (  30.*(float)NowMonth + 15.  ) * ( M_PI / 180. );    // angle of earth around the sun

        float temp = AVG_TEMP - AMP_TEMP * cos( ang );
        NowTemp = temp + Ranf( -RANDOM_TEMP, RANDOM_TEMP );

        float precip = AVG_PRECIP_PER_MONTH + AMP_PRECIP_PER_MONTH * sin( ang );
        NowPrecip = precip + Ranf( -RANDOM_PRECIP, RANDOM_PRECIP );
        if( NowPrecip < 0. )
            NowPrecip = 0.;

        WaitBarrier();  // DoneComputing
        WaitBarrier();  // DoneAssigning
        WaitBarrier();  // DonePrinting
    }
}

int main() {
    // starting date and time:
    NowMonth =    0;
    NowYear  = 2025;

    // starting state (feel free to change this if you want):
    NowNumDeer = 2;
    NowHeight =  5.;
    
    NowFert = 5.;
    
    float ang = (  30.*(float)NowMonth + 15.  ) * ( M_PI / 180. );    // angle of earth around the sun

    float temp = AVG_TEMP - AMP_TEMP * cos( ang );
    NowTemp = temp + Ranf( -RANDOM_TEMP, RANDOM_TEMP );

    float precip = AVG_PRECIP_PER_MONTH + AMP_PRECIP_PER_MONTH * sin( ang );
    NowPrecip = precip + Ranf( -RANDOM_PRECIP, RANDOM_PRECIP );
    if( NowPrecip < 0. )
        NowPrecip = 0.;

    // set up threading
    omp_set_num_threads(4);
    InitBarrier(4);

    #pragma omp parallel sections
    {
        #pragma omp section
            Deer();
        #pragma omp section
            Grain();
        #pragma omp section
            Watcher();
        #pragma omp section
            Fertilizer();
    }

    return 0;
}
