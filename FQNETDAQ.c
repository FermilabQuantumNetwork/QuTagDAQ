/*******************************************************************************
 *
 *  Project:        quTAG User Library
 *
 *  Filename:       FQNETDAQ.c
 *
 *  Purpose:        Collect data for FQNET using tdcbase lib
 *                  timestamps, counters
 *
 *  Author:         C.Pena, S.Xie; based on NHands GmbH & Co KG
 *
 *******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>              /* for exit() */
#include <math.h>
#include "tdcbase.h"
#include "tdcstartstop.h"


#ifdef unix
#include <unistd.h>
#define SLEEP(x) usleep(x)
#else
/* windows.h for Sleep */
#include <windows.h>
#define SLEEP(x) Sleep(x)
#endif

#define HIST_BINCOUNT        500  /* Histogram size */
#define HIST_BINWIDTH    1000  /* Histogram bin width .ps */
#define TIMESTAMP_COUNT   100000  /* Timestamp buffer size */
#define EXP_TIME         1  /* Exposure Time for coincidences [ms] */


/* Check return code and exit on error */
static void checkRc( const char * fctname, int rc )
{
  if ( rc ) {
    printf( ">>> %s: %s\n", fctname, TDC_perror( rc ) );
    /* Don't quit on "not connected" to allow for demo mode */
    if ( rc != TDC_NotConnected ) {
      TDC_deInit();
      exit( 1 );
    }
  }
}

float rate(int ch_rate)
{
  return ((float)ch_rate/(float)EXP_TIME)*1.e3/1.e6;//convert to Hz then to MHz
}

int get_max_collection_time( float rate )
{
  return (int)((float)TIMESTAMP_COUNT*0.90/rate);//rate in MHz so time is in us
}

unsigned int get_coincidence_index(int ch0, int ch1, int ch2, int ch3, int ch4);

/*
 * Initialize and start TDC, wait and get some data
 * and retrieve start stop histograms
 * selftest: generate data with hardware selftest
 * flatgen:  generate data by software, uniform distribution
 * nomrgen:  generate data by software, normal distribution
 */
static void run( int selftest, int flatgen, int normgen,
		 float master_rate, int coincidenceWindow, int collect_time, int collect_rounds,
		 char* outputfilename
		 )
{
  Int32 rc, count, tooSmall, tooBig, tsValid, eventsA, eventsB, i, j, it, ch;
  Int64 expTime, lastTimestamp = 0;
  Int32 hist1[HIST_BINCOUNT], hist2[HIST_BINCOUNT];
  Int64 timestamps[TIMESTAMP_COUNT];
  Int8  channels[TIMESTAMP_COUNT];
  int   coincCnt[TDC_COINC_CHANNELS];
  double bin2ns = 0, timeBase = 0.;
  double simPara[2] = { 1000., 1000. };                /* center,width = 81ns */
  
  float COINC_WINDOW = 10000;

  /* Initialize & start
   */
  rc = TDC_init( -1 );                                 /* Accept every device */
  checkRc( "TDC_init", rc );
  rc = TDC_getTimebase( &timeBase );
  checkRc( "TDC_getTimebase", rc );
  //printf( ">>> timebase: %g ps\n", timeBase * 1.e12 );
  bin2ns = HIST_BINWIDTH * timeBase * 1.e9;            /* Width of a bin in nanoseconds */
  rc = TDC_enableChannels( 0xff );                     /* Use all channels */
  checkRc( "TDC_enableChannels", rc );
  rc = TDC_enableStartStop( 1 );                       /* Enable start stop histograms */
  checkRc( "TDC_enableStartStop", rc );
  rc = TDC_addHistogram( 1, 2, 1 );
  checkRc( "TDC_addHistogram", rc );
  rc = TDC_addHistogram( 3, 2, 1 );
  checkRc( "TDC_addHistogram", rc );
  rc = TDC_setHistogramParams( HIST_BINWIDTH, HIST_BINCOUNT );
  checkRc( "TDC_setHistogramParams", rc );
  rc = TDC_setTimestampBufferSize( TIMESTAMP_COUNT );
  checkRc( "TDC_setTimestampBufferSize", rc );
  rc = TDC_setExposureTime( EXP_TIME );
  checkRc( "TDC_setExposureTime", rc );
  rc = TDC_setCoincidenceWindow(  COINC_WINDOW ); /* units in ps */
  checkRc( "TDC_setCoincidenceWindow", rc );

  if ( selftest ) {
    /* Configure internal signal generator:
     * Ch. 1 + 2, signal period 80ns, bursts of 3 Periods, distance betw. bursts 800ns
     * Expecting peaks at 80ns and 800 - 2*80 = 640ns
     */
    rc = TDC_configureSelftest( 6, 4, 3, 10 );
    checkRc( "TDC_configureSelftest", rc );
  }

  rc = TDC_clearAllHistograms();
  checkRc( "TDC_clearAllHistograms", rc );

  ///****************************************************************************
  //Reset Coincidence Counters and collect some DAQ parameters
  //****************************************************************************
  SLEEP( 1e3 );
  TDC_getCoincCounters( coincCnt, NULL );
  SLEEP( 1e3 );
  TDC_getCoincCounters( coincCnt, NULL );
  SLEEP(1e6);
  TDC_getCoincCounters( coincCnt, NULL );

  int clock_channel = 1;
  int max_tag_for_clock = TIMESTAMP_COUNT;
  
  float TOTAL_RATE     = rate(coincCnt[0]+coincCnt[1]+coincCnt[2]+coincCnt[3]+coincCnt[4]);
  int COLLECT_TIME   = get_max_collection_time(TOTAL_RATE);/* Time [us] for data acquisition per round */
  if(collect_time > 0 && collect_time < COLLECT_TIME) COLLECT_TIME = collect_time;
  int COLLECT_ROUNDS = collect_rounds+2;/* Number of data acquisition rounds */
  
  printf( "**********************************************************\n");
  printf( "Printing DAQ Parameters\n");
  printf( "**********************************************************\n");
  printf("COINC WINDOW = %f [ps]\n", COINC_WINDOW);
  printf("TOTAL RATE: %.3f MHz \n", TOTAL_RATE);
  printf("COLLECT TIME = %d [us]\n", COLLECT_TIME);
  printf("COLLECT ROUDNS = %d\n", COLLECT_ROUNDS-2);
  printf("TOTAL ACQUISITION TIME= %.2f [s]\n", (float)(COLLECT_TIME*(COLLECT_ROUNDS-2))/1.e6);
  printf("tdc coin channels = %d\n", TDC_COINC_CHANNELS);
  printf("tdc qutag channels = %d\n", TDC_QUTAG_CHANNELS);

  if (TOTAL_RATE == 0) {
    printf("\n\nNo signals detected. Exiting...\n\n");
    TDC_deInit();
    return;
  }
  /* wait some seconds, collect data, and check samples for errors that should never occur*/
  count = 0;
  FILE *data;

  char outfname[1024];
  strcat(outfname, outputfilename);
  strcat(outfname, ".txt");
  printf("Output Data File: %s",outfname);
  data = fopen(outfname,"w");
  fprintf(data,"#timestamp  channel\n");
  int ii = 0;
  Int64 last_time = 0;
  Int64 evt_time[4];
  //int evt_ch[4];
  int last_ch = -666666;
  int max_delta_time_arr[1000];
  int event_coinc_window = 0;
  unsigned int i_coin = 0;
  int new_event = 0;
  int good_event = 0;
  Int64 NEventsCollected = 0;
  Int64 NEventsWritten = 0;
  time_t StartTime;    
  
  for( ii = 0; ii < 4; ii++) evt_time[ii] = -666;
  
  //**************************************************************
  //LOOP for each buffer readout iteration 
  //**************************************************************
  for ( i = 0; i < COLLECT_ROUNDS; ++i ) {
    
    //TDC_getCoincCounters( coincCnt, NULL );
    
    //if( coincCnt[get_coincidence_index(1,1,0,0,0)] > -1 ){
    SLEEP( COLLECT_TIME );
    TDC_getLastTimestamps( 1, timestamps, channels, &tsValid );

    //Print out DAQ rates periodically
    if(i > 2 && i % 100 == 0 ) {
      //printf("valid entries  = %d\n", tsValid);
      time_t timeNow;
      time(&timeNow);
      printf("Time Since Start of Run : %d[s]\n", timeNow - StartTime);
      //printf("Number of events collected: %d ; Rate: %4.2f [MHz]\n", NEventsCollected, (float)NEventsCollected/(float)(timeNow - StartTime)/1000000);
      printf("Number of events triggered and written: %d ; Rate: %4.2f [MHz]\n", NEventsWritten, (float)NEventsWritten/(float)(timeNow - StartTime)/1000000);
      printf("\n");
    }
    if (!tsValid){
      printf("[WARNING] NO TAGs!,make sure collection time is large enough. Now using %d [us]\n",
	     COLLECT_TIME);
      continue;
    }
   
    //******************************************************
    //drop the first 2 rounds of data to flush possible corrupted buffers
    //******************************************************
    if( i < 2 ) continue;

    //******************************************************
    //Use the first round of data collected to 
    //calculate a reasonable coincidence time window
    //event_coinc_window is calculated automatically
    //
    //There is also the option to set a fixed coincidence
    //window, if you set the coincidenceWindow parameter.
    //******************************************************  
    if (coincidenceWindow > 0) {
      event_coinc_window = coincidenceWindow;   
    } else {
      if (i==2) {
	//find first reasonable channel
	int slow_index = -1;
	int index = -1;
	for(ii = 0; ii < TIMESTAMP_COUNT; ii++){
	  if( clock_channel >=0 &&  clock_channel != channels[ii] ) continue;
	  if ( channels[ii] < 5 ){
	    slow_index = ii;
	    break;
	  }
	}
	//printf("first index: %d %d\n", slow_index, index);
	//int sit;
	while( slow_index < max_tag_for_clock ){
	  if( slow_index >= tsValid ) break;
	  int max_delta_time = 0;
	  for ( it = slow_index+1; it < max_tag_for_clock; it++ ) {
	    if( it >= tsValid ) break;
	    if( channels[it] > 4 ) continue;
	    if (abs(channels[it]-channels[slow_index])) continue;
	    int current_delta_time = timestamps[it]-timestamps[slow_index];
	    if( current_delta_time > max_delta_time ){
	      max_delta_time = current_delta_time;
	    }
	    //find next reasonable channel
	    for(ii = slow_index+1; ii < TIMESTAMP_COUNT; ii++){
	      if( clock_channel >= 0 && clock_channel != channels[ii] ) continue;
	      if(i_coin > 3 )
		{
		  slow_index = TIMESTAMP_COUNT;
		  break;
		}
	      if ( channels[ii] < 5 )
		{
		  slow_index = ii;
		  break;
		}
	    }
	    break;//found next identical channel, move to next slow index;
	  }
	  if(max_delta_time > 0)
	    {
	      max_delta_time_arr[i_coin] = max_delta_time;
	      i_coin++; 
	    }
	}
      
	float mean_delta_time = 0;
	float rms_delta_time = 0;
	for ( ii = 0; ii < i_coin-1; ii++ ){
	  mean_delta_time += max_delta_time_arr[ii]/(float)(i_coin-1);
	}      
	event_coinc_window = (int)( mean_delta_time/2.0);      
      } 
    }

    if (i==2) {      
      printf( "Event Coincidence Window : %d [ps]\n", event_coinc_window);
      printf( "\n\n");
      printf( "**********************************************************\n");
      printf( "Starting Data Acquisition\n");
      printf( "**********************************************************\n");
      time(&StartTime); //start the time counter
      //printf("Start Time : %d \n", StartTime);
    }

    //******************************************************
    // Start the full readout here
    //******************************************************
    for ( it = 0; it < TIMESTAMP_COUNT; ++it ) {
      for ( ch = 0; ch < TDC_QUTAG_CHANNELS; ch++ ) {
	int index = TDC_QUTAG_CHANNELS*it + ch;
	if( index >= tsValid ) break;
	if(channels[index]>4) continue;
	int current_delta_time = timestamps[index]-last_time;
	int current_delta_ch = abs(channels[index]-last_ch);
	last_time = timestamps[index];
	last_ch = channels[index];
	
	if(current_delta_time > event_coinc_window  && current_delta_ch < 5 ){
	  new_event = 1;
	}
	else{
	  new_event = 0;
	}
	
	if(new_event){
	  NEventsCollected = NEventsCollected + 1;
	  //printf("==new event==\n");
	  int n_ch = 0;
	  for( ii = 0; ii < 4; ii++){
	    if(evt_time[ii] != -666) n_ch++;
	  }
	  //printf("%d %d\n", n_ch, good_event);
	  if( n_ch <=1 ) good_event = 0;
	  
	  if( good_event ){
	    NEventsWritten = NEventsWritten + 1;
	    //printf("==new event==\n");
	    for( ii = 0; ii < 4; ii++){
	      if( evt_time[ii] != -666 ){
		//printf("%d:%"LLDFORMAT";", ii+1, evt_time[ii]);	
		if( ii < 3 ) fprintf(data,"%"LLDFORMAT" ", evt_time[ii]);
		else fprintf(data,"%"LLDFORMAT"\n", evt_time[ii]);
	      }
	    }
	    //printf("\n");
	  }
	  
	  good_event = 1;
	  for( ii = 0; ii < 4; ii++) evt_time[ii] = -666;
	  if( evt_time[channels[index]-1] == -666 ) evt_time[channels[index]-1] = timestamps[index];
	}
	else{
	  if( evt_time[channels[index]-1] == -666){
	    evt_time[channels[index]-1] = timestamps[index];
	  }
	  else
	    {
	      good_event = 0;
	    }
	}
	

      }
    }
    
    if ( flatgen ) {
      rc = TDC_generateTimestamps( SIM_FLAT, simPara, 100 );
      checkRc( "TDC_generateTimestamps", rc );
    }
    if ( normgen ) {
      rc = TDC_generateTimestamps( SIM_NORMAL, simPara, 100 );
      checkRc( "TDC_generateTimestamps", rc );
    }
  }

  //Close the output file
  fclose(data);


  /* Stop it and exit */
  TDC_deInit();
}


unsigned int get_coincidence_index(int ch4, int ch3, int ch2, int ch1, int ch0)
{
  unsigned int global = 0xFFFF & (ch0 | (ch1 << 1) | (ch2 << 2) | (ch3 << 3) | (ch4 << 4)); 
  //return global;
  switch (global)
    {
    case 0:
      return -1;
      break; 
      
    case 1:
      return 0;
      break;
      
    case 2:
      return 1;
      break; 
      
    case 3:
      return 11;
      break;
      
    case 4:
      return 2;
      break; 
      
    case 5:
      return 12;
      break;
      
    case 6:
      return 5;
      break; 
      
    case 7:
      return 19;
      break;
      
    case 8:
      return 3;
      break; 
      
    case 9:
      return 13;
      break;
      
    case 10:
      return 6;
      break; 
      
    case 11:
      return 20;
      break;
      
    case 12:
      return 7;
      break; 
      
    case 13:
      return 21;
      break;
      
    case 14:
      return 15;
      break; 
      
    case 15:
      return 26;
      break;

    case 16:
      return 4;
      break; 
      
    case 17:
      return 14;
      break;
      
    case 18:
      return 8;
      break; 
      
    case 19:
      return 22;
      break;
      
    case 20:
      return 9;
      break; 
      
    case 21:
      return 23;
      break;
      
    case 22:
      return 16;
      break; 
      
    case 23:
      return 27;
      break;
      
    case 24:
      return 10;
      break; 
      
    case 25:
      return 24;
      break;
      
    case 26:
      return 15;
      break; 
      
    case 27:
      return 28;
      break;
      
    case 28:
      return 18;
      break; 
      
    case 29:
      return 29;
      break;
      
    case 30:
      return 25;
      break; 
      
    case 31:
      return 30;
      break;
      
    default:
      return -1;
      break;
    }
  
  return -1;
  //if(ch0 && ch1 && ch2 && ch)return 0;
  //if(ch1) return 1;
  //if(ch2) return 

}

int main( int argc, char ** argv )
{
  int signal = 0, selftest = 0, flatgen = 0, normgen = 0;
  if ( argc > 1 ) {
    signal   = !strcmp( argv[1], "signal" );
    selftest = !strcmp( argv[1], "selftest" );
    flatgen  = !strcmp( argv[1], "flatgen" );
    normgen  = !strcmp( argv[1], "normgen" );
  }
  if ( !(signal || selftest || normgen || flatgen) ) {
    printf( "usage: %s [signal | selftest | flatgen | normgen]\n", argv[0] );
    return 1;
  }
  
  //define master rate for coincidence window;
  float master_rate  = -666;//in kHz
  float coinc_window = -666;//in ps
  int collect_time   = 100;//in ms
  int collect_rounds = 100;//iterations-> total aquisition time is collect_time*collect_rounds;
  //define time for  coincidence window, will override coin windows by rate;
  if ( argc > 2 ) coinc_window    = atof(argv[2]);
  if ( argc > 3 ) master_rate     = atof(argv[3]);
  if ( argc > 4 ) collect_time    = atoi(argv[4]);
  if ( argc > 5 ) collect_rounds  = atoi(argv[5]);
  
  run( selftest, flatgen, normgen, master_rate, coinc_window, collect_time, collect_rounds, "data_test" );
  return 0;
}
