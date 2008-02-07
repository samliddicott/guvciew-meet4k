/*******************************************************************************#
#	    guvcview              http://guvcview.berlios.de			#
#     Paulo Assis <pj.assis@gmail.com>						#
#										#
# This program is free software; you can redistribute it and/or modify         	#
# it under the terms of the GNU General Public License as published by   	#
# the Free Software Foundation; either version 2 of the License, or           	#
# (at your option) any later version.                                          	#
#                                                                              	#
# This program is distributed in the hope that it will be useful,              	#
# but WITHOUT ANY WARRANTY; without even the implied warranty of             	#
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  		#
# GNU General Public License for more details.                                 	#
#                                                                              	#
# You should have received a copy of the GNU General Public License           	#
# along with this program; if not, write to the Free Software                  	#
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA	#
#                                                                              	#
********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <string.h>
#include <pthread.h>
#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>
#include <SDL/SDL_audio.h>
#include <SDL/SDL_timer.h>
#include <linux/videodev.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <X11/Xlib.h>
#include <SDL/SDL_syswm.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <portaudio.h>

#include "v4l2uvc.h"



static const char version[] = VERSION;
struct vdIn *videoIn;
char confPath[80];
int AVIFormat=1; /*0-"MJPG"  1-"YUY2" 2-"DIB "(rgb32)*/
VidState * s;

/* The main window*/
GtkWidget *mainwin;
/* A restart Dialog */
GtkWidget *restartdialog;

/* Must set this as global so they */
/* can be set from any callback.   */
/* When AVI is in capture mode we  */
/* can't change settings           */
GtkWidget *AVIComp;
GtkWidget *SndEnable; 
GtkWidget *SndSampleRate;
GtkWidget *SndDevice;
GtkWidget *SndNumChan;
/*must be called from main loop if capture timer enabled*/
GtkWidget *CapAVIButt;
GtkWidget *AVIFNameEntry;

/*thread definitions*/
pthread_t mythread;
pthread_attr_t attr;

pthread_t sndthread;
pthread_attr_t sndattr;

pthread_t diskIOthread;
pthread_t diskIOattr;

/* parameters passed when restarting*/
//~int ARG_C;
//~char **ARG_V;
const char *EXEC_CALL;

/*the main SDL surface*/
SDL_Surface *pscreen = NULL;

SDL_Surface *ImageSurf = NULL;
SDL_Overlay *overlay=NULL;
SDL_Rect drect;
SDL_Event sdlevent;

Uint32 snd_begintime;/*begin time for audio capture*/
Uint32 currtime;
Uint32 lasttime;
Uint32 AVIstarttime;
Uint32 AVIstoptime;
Uint32 framecount=0;
avi_t *AviOut;
BYTE *p = NULL;
BYTE * pim= NULL;
BYTE * pavi=NULL;
//Uint32 * aviIm;
//unsigned char *pixeldata = (unsigned char *)&aviIm;

unsigned char frmrate;


char *sndfile=NULL; /*temporary snd filename*/
char *avifile=NULL; /*avi filename passed through argument options with -n */
int Capture_time=0; /*avi capture time passed through argument options with -t */
char *capt=NULL;
int Sound_enable=1; /*Enable Sound by Default*/
int Sound_SampRate=SAMPLE_RATE;
int Sound_SampRateInd=0;
int Sound_numInputDev=0;
sndDev Sound_IndexDev[20]; /*up to 20 input devices (should be alocated dinamicly)*/
int Sound_DefDev=0; 
int Sound_UseDev=0;
int Sound_NumChan=NUM_CHANNELS;
int Sound_NumChanInd=0;


int fps = DEFAULT_FPS;
int fps_num = DEFAULT_FPS_NUM;
int bpp = 0; //current bytes per pixel
int hwaccel = 1; //use hardware acceleration
int grabmethod = 1;//default mmap(1) or read(0)
int width = DEFAULT_WIDTH;
int height = DEFAULT_HEIGHT;
int winwidth=WINSIZEX;
int winheight=WINSIZEY;
const char *mode="jpg"; /*jpg (default) or yuv*/
int format = V4L2_PIX_FMT_MJPEG;
int formind = 0; /*0-MJPG 1-YUYV*/
//int freq=50;


static Uint32 SDL_VIDEO_Flags =
    SDL_ANYFORMAT | SDL_DOUBLEBUF | SDL_RESIZABLE;


double
ms_time (void)
{
  static struct timeval tod;
  gettimeofday (&tod, NULL);
  return ((double) tod.tv_sec * 1000.0 + (double) tod.tv_usec / 1000.0);

}

static
int writeConf(const char *confpath) {
	int ret=1;
	FILE *fp;
	if ((fp = fopen(confpath,"w"))!=NULL) {
   		fprintf(fp,"# guvcview configuration file\n\n");
   		fprintf(fp,"# video resolution - hardware supported (logitech) 320x240 640x480\n");
   		fprintf(fp,"resolution=%ix%i\n",width,height);
   		fprintf(fp,"# control window size: default %ix%i\n",WINSIZEX,WINSIZEY);
   		fprintf(fp,"windowsize=%ix%i\n",winwidth,winheight);
   		fprintf(fp,"# mode video format 'yuv' or 'jpg'(default)\n");
   		fprintf(fp,"mode=%s\n",mode);
   		fprintf(fp,"# frames per sec. - hardware supported - default( %i )\n",DEFAULT_FPS);
   		fprintf(fp,"fps=%d/%d\n",fps_num,fps);
   		fprintf(fp,"# bytes per pixel: default (0 - current)\n");
   		fprintf(fp,"bpp=%i\n",bpp);
   		fprintf(fp,"# hardware accelaration: 0 1 (default - 1)\n");
   		fprintf(fp,"hwaccel=%i\n",hwaccel);
   		fprintf(fp,"# video grab method: 0 -read 1 -mmap (default - 1)\n");
   		fprintf(fp,"grabmethod=%i\n",grabmethod);
		//fprintf(fp,"frequency=%i\n",freq);
		fprintf(fp,"# sound 0 - disable 1 - enable\n");
		fprintf(fp,"sound=%i\n",Sound_enable);   		
   		printf("write %s OK\n",confpath);
   		fclose(fp);
	} else {
	printf("Could not write file %s \n Please check file permissions\n",confpath);
	ret=0;
	exit(0);
	}
	return ret;
}

static
int readConf(const char *confpath) {
	int ret=1;
	char variable[20];
	char value[20];

	int i=0;
	int j=0;


	FILE *fp;

	if((fp = fopen(confpath,"r"))!=NULL) {
		char line[80];

  		while (fgets(line, 80, fp) != NULL) {
			j++;
			if ((line[0]=='#') || (line[0]==' ') || (line[0]=='\n')) {
			} else if ((i=sscanf(line,"%[^#=]=%[^#\n ]",variable,value))==2){
					/* set variables */
					if (strcmp(variable,"resolution")==0) {
						if ((i=sscanf(value,"%ix%i",&width,&height))==2)
							printf("resolution: %i x %i\n",width,height); 			
					} else if (strcmp(variable,"windowsize")==0) {
						if ((i=sscanf(value,"%ix%i",&winwidth,&winheight))==2)
							printf("windowsize: %i x %i\n",winwidth,winheight);
						} else if 	(strcmp(variable,"mode")==0) {
							mode=strdup(value);
							printf("mode: %s\n",mode);
						} else if 	(strcmp(variable,"fps")==0) {
								if ((i=sscanf(value,"%i/%i",&fps_num,&fps))==1)
									printf("fps: %i/%i\n",fps_num,fps);
						} else if 	(strcmp(variable,"bpp")==0) {
								if ((i=sscanf(value,"%i",&bpp))==1)
									printf("bpp: %i\n",bpp);
						} else if 	(strcmp(variable,"hwaccel")==0) {
								if ((i=sscanf(value,"%i",&hwaccel))==1)
									printf("hwaccel: %i\n",hwaccel);
						} else if 	(strcmp(variable,"grabmethod")==0) {
								if ((i=sscanf(value,"%i",&grabmethod))==1)
									printf("grabmethod: %i\n",grabmethod);
						} else if 	(strcmp(variable,"sound")==0) {
								if ((i=sscanf(value,"%i",&Sound_enable))==1)
									printf("sound: %i\n",Sound_enable);
						}
			}
		}
		fclose(fp);
	} else {
   		printf("Could not open %s for read,\n will try to create it\n",confpath);
   		ret=writeConf(confpath);
	}
	return ret;
}


/********************** sound threaded loop ************************************/
void *sound_capture(void *data)
{
    PaStreamParameters inputParameters, outputParameters;
    PaStream *stream;
    PaError err;
	//paCapData  snd_data;
    SAMPLE *recordedSamples=NULL;
    int i;
    int totalFrames;
    int numSamples;
    int numBytes;
	
	FILE  *fid;
    fid = fopen(sndfile, "wb+");
    if( fid == NULL )
    {
       printf("Could not open file.");
    }
	
	err = Pa_Initialize();
    if( err != paNoError ) goto error;
	/* Record for a few seconds. */
	
	if(Sound_SampRateInd==0)
		Sound_SampRate=Sound_IndexDev[Sound_UseDev].samprate;/*using default*/
	
	if(Sound_NumChanInd==0) {
		/*using default if channels <3 or stereo(2) otherwise*/
		Sound_NumChan=(Sound_IndexDev[Sound_UseDev].chan<3)?Sound_IndexDev[Sound_UseDev].chan:2;
	}
	//printf("dev:%d SampleRate:%d Chanels:%d\n",Sound_IndexDev[Sound_UseDev].id,Sound_SampRate,Sound_NumChan);
	
	/* setting maximum buffer size*/
	totalFrames = NUM_SECONDS * Sound_SampRate;
    numSamples = totalFrames * Sound_NumChan;
    numBytes = numSamples * sizeof(SAMPLE);
	recordedSamples = (SAMPLE *) malloc( numBytes );
	
    if( recordedSamples == NULL )
    {
        printf("Could not allocate record array.\n");
        exit(1);
    }
    for( i=0; i<numSamples; i++ ) recordedSamples[i] = 0;

    inputParameters.device = Sound_IndexDev[Sound_UseDev].id; /* input device */
    inputParameters.channelCount = Sound_NumChan;
    inputParameters.sampleFormat = PA_SAMPLE_TYPE;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL; 
	
	 /*---------------------- Record some audio. ----------------------------- */
    err = Pa_OpenStream(
              &stream,
              &inputParameters,
              NULL,                  /* &outputParameters, */
              Sound_SampRate,
              totalFrames,/*FRAMES_PER_BUFFER can cause input overflow*/
              paNoFlag,      /* PaNoFlag - clip and dhiter*/
              NULL, /* sound callback - using blocking API*/
              NULL ); /* callback userData -no callback no data */
    if( err != paNoError ) goto error;
  
    err = Pa_StartStream( stream );
    if( err != paNoError ) goto error;
    /*------------------------- capture loop ----------------------------------*/
	//snd_begintime=SDL_GetTicks();
	snd_begintime = ms_time();
	do {
	   //Pa_Sleep(SND_WAIT_TIME);
       err = Pa_ReadStream( stream, recordedSamples, totalFrames );
       //if( err != paNoError ) break; /*can break with input overflow*/
	   /* Write recorded data to a file. */  
          fwrite( recordedSamples, Sound_NumChan * sizeof(SAMPLE), totalFrames, fid );
    } while (videoIn->capAVI);   
	
    fclose( fid );
    //printf("Wrote sound data to '%s'\n",sndfile);
    err = Pa_CloseStream( stream );
    if( err != paNoError ) goto error; 
	
    if(recordedSamples!=NULL) free( recordedSamples  );
    recordedSamples=NULL;
    Pa_Terminate();
    return(0);

error:
	fclose(fid);
	free( recordedSamples );
    Pa_Terminate();
    fprintf( stderr, "An error occured while using the portaudio stream\n" );
    fprintf( stderr, "Error number: %d\n", err );
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
    return(-1);
	
}


/*------------------------------ Event handlers -------------------------------*/
gint
delete_event (GtkWidget *widget, GdkEventConfigure *event)
{
	shutd(0);//shutDown
	
	return 0;
}

void *AVIAudioAdd(void *data) {
	
	SAMPLE *recordedSamples=NULL;
    int i;  
    int totalFrames;
    int numSamples;
    long numBytes;
		
	totalFrames = NUM_SECONDS * Sound_SampRate;
    numSamples = totalFrames * Sound_NumChan;
 
	numBytes = numSamples * sizeof(SAMPLE);

    recordedSamples = (SAMPLE *) malloc( numBytes );

    if( recordedSamples == NULL )
    {
       	printf("Could not allocate record array.\n");
		/*must enable avi capture button*/
    	gtk_widget_set_sensitive (CapAVIButt, TRUE);
       	return (1);
    }
    for( i=0; i<numSamples; i++ ) recordedSamples[i] = 0;/*init to zero - silence*/	
	AVI_set_audio(AviOut, Sound_NumChan, Sound_SampRate, sizeof(SAMPLE)*8,WAVE_FORMAT_PCM);
	printf("sample size: %i bits\n",sizeof(SAMPLE)*8);
	
	/* Audio Capture allways starts first*/
	Uint32 synctime= snd_begintime - AVIstarttime; /*time diff for audio-video*/
	if(synctime) {
		/*shift sound by synctime*/
		Uint32 shiftFrames=(synctime*Sound_SampRate/1000);
		/*make sur its even if stereo*/
		//~ if (Sound_NumChan>1) /*it is a stereo imput*/
			//~ if ((shiftFrames & 0x00000001)>0) shiftFrames++; /*its odd so make it even*/
		int shiftSamples=shiftFrames*Sound_NumChan;
		printf("shift sound forward by %d ms - %d frames\n",synctime,shiftSamples);
		SAMPLE EmptySamp[shiftSamples];
		for(i=0; i<shiftSamples; i++) EmptySamp[i]=0;/*init to zero - silence*/
		AVI_write_audio(AviOut,&EmptySamp,shiftSamples*sizeof(SAMPLE)); 
	}
	FILE *fip;
	fip=fopen(sndfile,"rb");
	if( fip == NULL )
    {
    	printf("Could not open snd data file.\n");
	} else {
		while(fread( recordedSamples, Sound_NumChan * sizeof(SAMPLE), totalFrames, fip )!=0){  
	    	AVI_write_audio(AviOut,(BYTE *) recordedSamples,numBytes);
		}
	}
	fclose(fip);
	/*remove audio file*/
	unlink(sndfile);
	if (recordedSamples!=NULL) free( recordedSamples );
	recordedSamples=NULL;
	
	AVI_close (AviOut);
    printf ("close avi\n");
    AviOut = NULL;
	framecount = 0;
	/*must enable avi capture button*/
    gtk_widget_set_sensitive (CapAVIButt, TRUE);
	return (0);
	
}

void
aviClose (void)
{
  double tottime = -1;
  int tstatus;
	
  if (AviOut)
    {
      printf ("AVI close asked \n");
      tottime = AVIstoptime - AVIstarttime;
	  printf("AVI: %i in %lf\n",framecount,tottime);
      
	  if (tottime > 0) {
	    /*try to find the real frame rate*/
		AviOut->fps = (double) framecount *1000 / tottime;
	  }
      else {
		/*set the hardware frame rate*/   
		AviOut->fps=videoIn->fps;
	  }
	  /******************* write audio to avi if Sound Enable*******************/
	  if (Sound_enable > 0) {
		  
		printf("waiting for thread to finish\n");
	  	/* Free attribute and wait for the thread */
      	pthread_attr_destroy(&sndattr);
	
	  	int sndrc = pthread_join(sndthread, (void **)&tstatus);
      	if (sndrc)
      	{
         	printf("ERROR; return code from pthread_join() is %d\n", sndrc);
         	exit(-1);
      	}
      	printf("Completed join with thread status= %d\n", tstatus);
	  	/*run it in a new thread to make it non-blocking*/
		/*must disable avi capture button*/
    	gtk_widget_set_sensitive (CapAVIButt, FALSE);
		/* Initialize and set snd thread detached attribute */
		pthread_attr_init(&diskIOattr);
    	pthread_attr_setdetachstate(&diskIOattr, PTHREAD_CREATE_JOINABLE);
		int rio = pthread_create(&diskIOthread, &diskIOattr, AVIAudioAdd, NULL); 
        if (rio)
          {
             printf("ERROR; return code from IO pthread_create() is %d\n", rio);
          }  
		//AVIAudioAdd(NULL);
	  } else { /******* Sound Disable *********/
		
      	AVI_close (AviOut);
      	printf ("close avi\n");
      	AviOut = NULL;
	  	framecount = 0;
	  }
    }
}

static int
num_chars (int n)
{
    int i = 0;

    if (n <= 0) {
        i++;
        n = -n;
    }

    while (n != 0) {
        n /= 10;
        i++;
    }
    return i;
}



/*-------------------------Controls--------------------------------------------*/
static void
set_slider_label (GtkRange * range)
{
    ControlInfo * ci = g_object_get_data (G_OBJECT (range), "control_info");
    if (ci->labelval) {
        char str[12];
        sprintf (str, "%*d", ci->maxchars, (int) gtk_range_get_value (range));
        gtk_label_set_text (GTK_LABEL (ci->labelval), str);
    }
}

static void
slider_changed (GtkRange * range, VidState * s)
{
  
    ControlInfo * ci = g_object_get_data (G_OBJECT (range), "control_info");
    InputControl * c = s->control + ci->idx;
    int val = (int) gtk_range_get_value (range);
    
    if (input_set_control (videoIn, c, val) == 0) {
        set_slider_label (range);
        printf ("changed to %d\n", val);
    }
    else {
        printf ("changed to %d, but set failed\n", val);
    }
    if (input_get_control (videoIn, c, &val) == 0) {
        printf ("hardware value is %d\n", val);
    }
    else {
        printf ("hardware get failed\n");
    }
	
}

static void
check_changed (GtkToggleButton * toggle, VidState * s)
{
    
    ControlInfo * ci = g_object_get_data (G_OBJECT (toggle), "control_info");
    InputControl * c = s->control + ci->idx;
    int val;
    
    if (c->id == V4L2_CID_EXPOSURE_AUTO) {
    val = gtk_toggle_button_get_active (toggle) ? AUTO_EXP : MAN_EXP;
	} 
    else val = gtk_toggle_button_get_active (toggle) ? 1 : 0;
    
	
    if (input_set_control (videoIn, c, val) == 0) {
        printf ("changed to %d\n", val);
    }
    else {
        printf ("changed to %d, but set failed\n", val);
    }
    if (input_get_control (videoIn, c, &val) == 0) {
        printf ("hardware value is %d\n", val);
    }
    else {
        printf ("hardware get failed\n");
    }
	
}


static void
combo_changed (GtkComboBox * combo, VidState * s)
{
    
    ControlInfo * ci = g_object_get_data (G_OBJECT (combo), "control_info");
    InputControl * c = s->control + ci->idx;
    int val = gtk_combo_box_get_active (combo);
    
	
    if (input_set_control (videoIn, c, val) == 0) {
        printf ("changed to %d\n", val);
    }
    else {
        printf ("changed to %d, but set failed\n", val);
    }
    if (input_get_control (videoIn, c, &val) == 0) {
        printf ("hardware value is %d\n", val);
    }
    else {
        printf ("hardware get failed\n");
    }
	
}

static void
resolution_changed (GtkComboBox * Resolution, void *data)
{
	/* The new resolution is writen to conf file at exit             */
	/* then is read back at start. This means that for changing */
	/* resolution we must restart the application                    */
	
	int index = gtk_combo_box_get_active(Resolution);
	width=listVidCap[formind][index].width;
	height=listVidCap[formind][index].height;
	
	/*check if frame rate is available at the new resolution*/
	int i=0;
	int deffps=0;
	for(i=0;i<listVidCap[formind][index].numb_frates;i++) {
		if ((listVidCap[formind][index].framerate_num[i]==fps_num) && 
		       (listVidCap[formind][index].framerate_denom[i]==fps)) deffps=i;	
	}
	/*frame rate is not available so set to minimum*/
	if (deffps==0) {
		fps_num=listVidCap[formind][index].framerate_num[0];
		fps=listVidCap[formind][index].framerate_denom[0];		
	}


	
	restartdialog = gtk_dialog_new_with_buttons ("Program Restart",
                                                  GTK_WINDOW(mainwin),
                                                  GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  "now",
                                                  GTK_RESPONSE_ACCEPT,
                                                  "Later",
                                                  GTK_RESPONSE_REJECT,
                                                  NULL);
	
	GtkWidget *message = gtk_label_new ("Changes will only take effect after guvcview restart.\n\n\nRestart now?\n");
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(restartdialog)->vbox), message);
	gtk_widget_show_all(GTK_WIDGET(GTK_CONTAINER (GTK_DIALOG(restartdialog)->vbox)));
	
  gint result = gtk_dialog_run (GTK_DIALOG (restartdialog));
  switch (result)
    {
      case GTK_RESPONSE_ACCEPT:
         /*restart app*/
		 shutd(1);
         break;
      default:
         /* do nothing since Restart rejected*/		
         break;
    }
  
  gtk_widget_destroy (restartdialog);
		
}

static void
FrameRate_changed (GtkComboBox * FrameRate,GtkComboBox * Resolution)
{
	int resind = gtk_combo_box_get_active(Resolution);
	
	int index = gtk_combo_box_get_active (FrameRate);
      	
	fps=listVidCap[formind][resind].framerate_denom[index];
	fps_num=listVidCap[formind][resind].framerate_num[index];
 
	input_set_framerate (videoIn, fps, fps_num);
	
	input_get_framerate(videoIn);
	fps=videoIn->fps;
	fps_num=videoIn->fps_num;
	printf("hardware fps is %d/%d ,%i/%i\n",fps,fps_num,
	            videoIn->streamparm.parm.capture.timeperframe.numerator,
	            videoIn->streamparm.parm.capture.timeperframe.denominator);
	
}

static void
SndSampleRate_changed (GtkComboBox * SampleRate, void *data)
{
	Sound_SampRateInd = gtk_combo_box_get_active (SampleRate);
   	Sound_SampRate=stdSampleRates[Sound_SampRateInd];
	
	
}

static void
ImageType_changed (GtkComboBox * ImageType,GtkEntry *ImageFNameEntry) 
{
	gchar *filename;
	gchar *basename;
	videoIn->Imgtype=gtk_combo_box_get_active (ImageType);
	if(videoIn->formatIn==V4L2_PIX_FMT_YUYV) videoIn->Imgtype++; /*disable jpg*/
	
	switch(videoIn->Imgtype){
		case 0:
			filename=gtk_entry_get_text(ImageFNameEntry);
			sscanf(filename,"%[^.]",basename);
			sprintf(filename,"%s.jpg",basename);
			break;
		case 1:
			filename=gtk_entry_get_text(ImageFNameEntry);
			sscanf(filename,"%[^.]",basename);
			sprintf(filename,"%s.bmp",basename);
			break;
		case 2:
			filename=gtk_entry_get_text(ImageFNameEntry);
			sscanf(filename,"%[^.]",basename);
			sprintf(filename,"%s.png",basename);
			break;
		default:
			filename=DEFAULT_IMAGE_FNAME;
	}
	
	printf("set filename to:%s\n",filename);
	gtk_entry_set_text(ImageFNameEntry,filename);/*doesn't seem to change 
													until text selected*/
	//gtk_widget_show(ImageFNameEntry);
	videoIn->ImageFName = filename;
}

static void
SndDevice_changed (GtkComboBox * SoundDevice, void *data)
{
 
	Sound_UseDev=gtk_combo_box_get_active (SoundDevice);
	
	printf("using device id:%d\n",Sound_IndexDev[Sound_UseDev].id);
	
}

static void
SndNumChan_changed (GtkComboBox * SoundChan, void *data)
{
	Sound_NumChanInd = gtk_combo_box_get_active (SoundChan);
	Sound_NumChan=Sound_NumChanInd; /*0-device default 1-mono 2-stereo*/	
}

static void
AVIComp_changed (GtkComboBox * AVIComp, void *data)
{
	int index = gtk_combo_box_get_active (AVIComp);
      	
	if (videoIn->formatIn== V4L2_PIX_FMT_MJPEG){
		AVIFormat=index;
	} else {
		AVIFormat=index+1; /*disable MJPG (AVIFormat=0)*/
	}

}


static void
capture_image (GtkButton * CapImageButt, GtkWidget * ImageFNameEntry)
{
  
  	const char *filename = gtk_entry_get_text(GTK_ENTRY(ImageFNameEntry));
	
	videoIn->ImageFName = filename;
	videoIn->capImage = TRUE;
}

static void
capture_avi (GtkButton * CapAVIButt, GtkWidget * AVIFNameEntry)
{
	const char *filename = gtk_entry_get_text(GTK_ENTRY(AVIFNameEntry));
	char *compression="MJPG";

	switch (AVIFormat) {
	 case 0:
		compression="MJPG";
		break;
	 case 1:
		compression="YUY2";
		break;
	 case 2:
		compression="DIB ";
		break;
	 default:
		compression="MJPG";
	}	
	if(videoIn->capAVI) {
	 printf("stoping AVI capture\n");
	 gtk_button_set_label(CapAVIButt,"Capture");
	 AVIstoptime = ms_time();
	 printf("AVI stop time:%d\n",AVIstoptime);	
	 videoIn->capAVI = FALSE;
	 aviClose();
	 /*enabling sound and avi compression controls*/
	 gtk_widget_set_sensitive (AVIComp, TRUE);
	 gtk_widget_set_sensitive (SndEnable,TRUE);	
	 if(Sound_enable > 0) {	 
 	 	gtk_widget_set_sensitive (SndSampleRate,TRUE);
 	 	gtk_widget_set_sensitive (SndDevice,TRUE);
 	 	gtk_widget_set_sensitive (SndNumChan,TRUE);
	 }
	} 
	else {
	 if (!(videoIn->signalquit)) {/*should not happen*/
		/*thread exited while in AVI capture mode*/
        /* we have to close AVI                  */
     printf("close AVI since thread as exited\n");
	 gtk_button_set_label(CapAVIButt,"Capture");
	 AVIstoptime = ms_time();
	 printf("AVI stop time:%d\n",AVIstoptime);	
	 videoIn->capAVI = FALSE;
	 aviClose();
	 /*enabling sound and avi compression controls*/
	 gtk_widget_set_sensitive (AVIComp, TRUE);
	 gtk_widget_set_sensitive (SndEnable,TRUE); 
 	 if(Sound_enable > 0) {	 
 	 	gtk_widget_set_sensitive (SndSampleRate,TRUE);
 	 	gtk_widget_set_sensitive (SndDevice,TRUE);
 	 	gtk_widget_set_sensitive (SndNumChan,TRUE);
	 }
	 
	 } 
	 else {
	   /* thread is running so start AVI capture*/	 
	   printf("starting AVI capture to %s\n",filename);
	   gtk_button_set_label(CapAVIButt,"Stop");  
	   AviOut = AVI_open_output_file(filename);
	   /*4CC compression "YUY2" (YUV) or "DIB " (RGB24)  or  "MJPG"*/	
	   
	   AVI_set_video(AviOut, videoIn->width, videoIn->height, videoIn->fps,compression);		
	   /* audio will be set in aviClose - if enabled*/
	   videoIn->AVIFName = filename;
	   AVIstarttime = ms_time();
       //printf("AVI start time:%d\n",AVIstarttime);		
	   videoIn->capAVI = TRUE;
	   /*disabling sound and avi compression controls*/
	   gtk_widget_set_sensitive (AVIComp, FALSE);
	   gtk_widget_set_sensitive (SndEnable,FALSE); 
 	   gtk_widget_set_sensitive (SndSampleRate,FALSE);
 	   gtk_widget_set_sensitive (SndDevice,FALSE);
 	   gtk_widget_set_sensitive (SndNumChan,FALSE);
	   /* Creating the sound capture loop thread if Sound Enable*/ 
	   if(Sound_enable > 0) { 
	      int rsnd = pthread_create(&sndthread, &sndattr, sound_capture, NULL); 
          if (rsnd)
          {
             printf("ERROR; return code from snd pthread_create() is %d\n", rsnd);
          }
		}
		 
	 }
	}	
}

static void
SndEnable_changed (GtkToggleButton * toggle, VidState * s)
{
		Sound_enable = gtk_toggle_button_get_active (toggle) ? 1 : 0;
	    if (!Sound_enable) {
			gtk_widget_set_sensitive (SndSampleRate,FALSE);
 	   		gtk_widget_set_sensitive (SndDevice,FALSE);
 	   		gtk_widget_set_sensitive (SndNumChan,FALSE);	
		} else { 
 	 		gtk_widget_set_sensitive (SndSampleRate,TRUE);
 	 		gtk_widget_set_sensitive (SndDevice,TRUE);
 	 		gtk_widget_set_sensitive (SndNumChan,TRUE);
		}
}

static void
draw_controls (VidState *s)
{
    int i;
	
	
    if (s->control) {
        for (i = 0; i < s->num_controls; i++) {
            ControlInfo * ci = s->control_info + i;
            if (ci->widget)
                gtk_widget_destroy (ci->widget);
            if (ci->label)
                gtk_widget_destroy (ci->label);
            if (ci->labelval)
                gtk_widget_destroy (ci->labelval);
        }
        free (s->control_info);
        s->control_info = NULL;
        input_free_controls (s->control, s->num_controls);
        s->control = NULL;
    }
    
    s->control = input_enum_controls (videoIn, &s->num_controls);
    //fprintf(stderr,"V4L2_CID_BASE=0x%x\n",V4L2_CID_BASE);
	//fprintf(stderr,"V4L2_CID_PRIVATE_BASE=0x%x\n",V4L2_CID_PRIVATE_BASE);
	//fprintf(stderr,"V4L2_CID_PRIVATE_LAST=0x%x\n",V4L2_CID_PRIVATE_LAST);
    fprintf(stderr,"Controls:\n");
    for (i = 0; i < s->num_controls; i++) {
		fprintf(stderr,"control[%d]: 0x%x",i,s->control[i].id);
        fprintf (stderr,"  %s, %d:%d:%d, default %d\n", s->control[i].name,
                s->control[i].min, s->control[i].step, s->control[i].max,
                s->control[i].default_val);
    }

   if((s->control_info = malloc (s->num_controls * sizeof (ControlInfo)))==NULL){
   			printf("couldn't allocate memory for: s->control_info\n");
			exit(1); 
   }

    for (i = 0; i < s->num_controls; i++) {
        ControlInfo * ci = s->control_info + i;
        InputControl * c = s->control + i;

        ci->idx = i;
        ci->widget = NULL;
        ci->label = NULL;
        ci->labelval = NULL;
        
        if (c->id == V4L2_CID_EXPOSURE_AUTO) {
            int val;
            ci->widget = gtk_check_button_new_with_label (c->name);
            g_object_set_data (G_OBJECT (ci->widget), "control_info", ci);
            gtk_widget_show (ci->widget);
            gtk_table_attach (GTK_TABLE (s->table), ci->widget, 1, 3, 3+i, 4+i,
                    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
			

            if (input_get_control (videoIn, c, &val) == 0) {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ci->widget),
                        val==AUTO_EXP ? TRUE : FALSE);
            }
            else {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ci->widget),
                        c->default_val==AUTO_EXP ? TRUE : FALSE);
                gtk_widget_set_sensitive (ci->widget, FALSE);
            }

            if (!c->enabled) {
                gtk_widget_set_sensitive (ci->widget, FALSE);
            }
            
            g_signal_connect (G_OBJECT (ci->widget), "toggled",
                    G_CALLBACK (check_changed), s);
			
	        
	    } else if (c->type == INPUT_CONTROL_TYPE_INTEGER) {
            PangoFontDescription * desc;
            int val;

            if (c->step == 0)
                c->step = 1;
            ci->widget = gtk_hscale_new_with_range (c->min, c->max, c->step);
            gtk_scale_set_draw_value (GTK_SCALE (ci->widget), FALSE);

            /* This is a hack to use always round the HScale to integer
             * values.  Strangely, this functionality is normally only
             * available when draw_value is TRUE. */
            GTK_RANGE (ci->widget)->round_digits = 0;

            gtk_table_attach (GTK_TABLE (s->table), ci->widget, 1, 2, 3+i, 4+i,
                    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
            g_object_set_data (G_OBJECT (ci->widget), "control_info", ci);
            ci->maxchars = MAX (num_chars (c->min), num_chars (c->max));
            gtk_widget_show (ci->widget);
            
            ci->labelval = gtk_label_new (NULL);
            gtk_table_attach (GTK_TABLE (s->table), ci->labelval, 2, 3,
                    3+i, 4+i, GTK_FILL, 0, 0, 0);
            
            desc = pango_font_description_new ();
            pango_font_description_set_family_static (desc, "monospace");
            gtk_widget_modify_font (ci->labelval, desc);
            gtk_misc_set_alignment (GTK_MISC (ci->labelval), 1, 0.5);

            if (input_get_control (videoIn, c, &val) == 0) {
                gtk_range_set_value (GTK_RANGE (ci->widget), val);
            }
            else {
                gtk_range_set_value (GTK_RANGE (ci->widget), c->default_val);
                gtk_widget_set_sensitive (ci->widget, FALSE);
                gtk_widget_set_sensitive (ci->labelval, FALSE);
            }

            if (!c->enabled) {
                gtk_widget_set_sensitive (ci->widget, FALSE);
                gtk_widget_set_sensitive (ci->labelval, FALSE);
            }
            
            set_slider_label (GTK_RANGE (ci->widget));
            g_signal_connect (G_OBJECT (ci->widget), "value-changed",
                    G_CALLBACK (slider_changed), s);

            gtk_widget_show (ci->labelval);

            ci->label = gtk_label_new (g_strdup_printf ("%s:", c->name));
        }
        else if (c->type == INPUT_CONTROL_TYPE_BOOLEAN) {
            int val;
            ci->widget = gtk_check_button_new_with_label (c->name);
            g_object_set_data (G_OBJECT (ci->widget), "control_info", ci);
            gtk_widget_show (ci->widget);
            gtk_table_attach (GTK_TABLE (s->table), ci->widget, 1, 3, 3+i, 4+i,
                    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);

            if (input_get_control (videoIn, c, &val) == 0) {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ci->widget),
                        val ? TRUE : FALSE);
            }
            else {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ci->widget),
                        c->default_val ? TRUE : FALSE);
                gtk_widget_set_sensitive (ci->widget, FALSE);
            }

            if (!c->enabled) {
                gtk_widget_set_sensitive (ci->widget, FALSE);
            }
            
            g_signal_connect (G_OBJECT (ci->widget), "toggled",
                    G_CALLBACK (check_changed), s);
        }
        else if (c->type == INPUT_CONTROL_TYPE_MENU) {
            int val, j;

            ci->widget = gtk_combo_box_new_text ();
            for (j = 0; j <= c->max; j++) {
                gtk_combo_box_append_text (GTK_COMBO_BOX (ci->widget), c->entries[j]);
            }

            gtk_table_attach (GTK_TABLE (s->table), ci->widget, 1, 2, 3+i, 4+i,
                    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
            g_object_set_data (G_OBJECT (ci->widget), "control_info", ci);
            gtk_widget_show (ci->widget);

            if (input_get_control (videoIn, c, &val) == 0) {
                gtk_combo_box_set_active (GTK_COMBO_BOX (ci->widget), val);
            }
            else {
                gtk_combo_box_set_active (GTK_COMBO_BOX (ci->widget), c->default_val);
                gtk_widget_set_sensitive (ci->widget, FALSE);
            }

            if (!c->enabled) {
                gtk_widget_set_sensitive (ci->widget, FALSE);
            }
            
            g_signal_connect (G_OBJECT (ci->widget), "changed",
                    G_CALLBACK (combo_changed), s);

            ci->label = gtk_label_new (g_strdup_printf ("%s:", c->name));
        }
        else {
            fprintf (stderr, "TODO: implement menu and button\n");
            continue;
        }

        if (ci->label) {
            gtk_misc_set_alignment (GTK_MISC (ci->label), 1, 0.5);

            gtk_table_attach (GTK_TABLE (s->table), ci->label, 0, 1, 3+i, 4+i,
                    GTK_FILL, 0, 0, 0);

            gtk_widget_show (ci->label);
        }
    }

}

void *
yuyv2rgb (BYTE *pyuv, BYTE *prgb){
	int l=0;
	int SizeYUV=videoIn->height * videoIn->width * 2; /* 2 bytes per pixel*/
	for(l=0;l<SizeYUV;l=l+4) { /*iterate every 4 bytes*/
		/* b = y0 + 1.772 (u-128) */
		*prgb++=CLIP(pyuv[l] + 1.772 *( pyuv[l+1]-128)); 
		/* g = y0 - 0.34414 (u-128) - 0.71414 (v-128)*/
		*prgb++=CLIP(pyuv[l] - 0.34414 * (pyuv[l+1]-128) -0.71414*(pyuv[l+3]-128));
		/* r =y0 + 1.402 (v-128) */
		*prgb++=CLIP(pyuv[l] + 1.402 * (pyuv[l+3]-128));                                                        
		/* b1 = y1 + 1.772 (u-128) */
		*prgb++=CLIP(pyuv[l+2] + 1.772*(pyuv[l+1]-128));
		/* g1 = y1 - 0.34414 (u-128) - 0.71414 (v-128)*/
		*prgb++=CLIP(pyuv[l+2] - 0.34414 * (pyuv[l+1]-128) -0.71414 * (pyuv[l+3]-128)); 
		/* r1 =y1 + 1.402 (v-128) */
		*prgb++=CLIP(pyuv[l+2] + 1.402 * (pyuv[l+3]-128));
	}
	
}

void *
yuyv2bgr (BYTE *pyuv, BYTE *pbgr){

	int l=0;
	int k=0;
	BYTE *preverse;
	int bytesUsed;
	int SizeBGR=videoIn->height * videoIn->width * 3; /* 3 bytes per pixel*/
	/* BMP byte order is bgr and the lines start from last to first*/
	preverse=pbgr+SizeBGR;/*start at the end and decrement*/
	//printf("preverse addr:%d | pbgr addr:%d | diff:%d\n",preverse,pbgr,preverse-pbgr);
	for(l=0;l<videoIn->height;l++) { /*iterate every 1 line*/
		preverse-=videoIn->width*3;/*put pointer at begin of unprocessed line*/
		bytesUsed=l*videoIn->width*2;
		for (k=0;k<((videoIn->width)*2);k=k+4)/*iterate every 4 bytes in the line*/
		{                              
		/* b = y0 + 1.772 (u-128) */
		*preverse++=CLIP(pyuv[k+bytesUsed] + 1.772 *( pyuv[k+1+bytesUsed]-128)); 
		/* g = y0 - 0.34414 (u-128) - 0.71414 (v-128)*/
		*preverse++=CLIP(pyuv[k+bytesUsed] - 0.34414 * (pyuv[k+1+bytesUsed]-128) -0.71414*(pyuv[k+3+bytesUsed]-128));
		/* r =y0 + 1.402 (v-128) */
		*preverse++=CLIP(pyuv[k+bytesUsed] + 1.402 * (pyuv[k+3+bytesUsed]-128));                                                        
		/* b1 = y1 + 1.772 (u-128) */
		*preverse++=CLIP(pyuv[k+2+bytesUsed] + 1.772*(pyuv[k+1+bytesUsed]-128));
		/* g1 = y1 - 0.34414 (u-128) - 0.71414 (v-128)*/
		*preverse++=CLIP(pyuv[k+2+bytesUsed] - 0.34414 * (pyuv[k+1+bytesUsed]-128) -0.71414 * (pyuv[k+3+bytesUsed]-128)); 
		/* r1 =y1 + 1.402 (v-128) */
		*preverse++=CLIP(pyuv[k+2+bytesUsed] + 1.402 * (pyuv[k+3+bytesUsed]-128));
		}
		preverse-=videoIn->width*3;/*get it back at the begin of processed line*/
	}
	//printf("preverse addr:%d | pbgr addr:%d | diff:%d\n",preverse,pbgr,preverse-pbgr);
	preverse=NULL;

}


void *main_loop(void *data)
{
	int ret=0;
	while (videoIn->signalquit) {
	 //currtime = SDL_GetTicks();
	 currtime = ms_time();
	 /*gets real frame rate*/
	 //~ if (currtime - lasttime > 0) {
		//~ frmrate = 1000/(currtime - lasttime);
	 //~ }
	 //~ lasttime = currtime;
		
	 /*sets timer if Capture_time enable*/ 
	 if (Capture_time) {
		 if((AVIstarttime+(Capture_time*1000))<currtime) {
		 	/*stop avi capture*/
			printf("timer alarme - stoping avi\n");
			capture_avi(CapAVIButt,AVIFNameEntry);
			Capture_time=0; /*disables capture timer*/
		 }
	 }
	
	 /*-------------------------- Grab Frame ----------------------------------*/
	 if (uvcGrab(videoIn) < 0) {
	    printf("Error grabbing=> Frame Rate is %d\n",frmrate);
	    videoIn->signalquit=0;
		ret = 2;
	 }
	
	 /*------------------------- Display Frame --------------------------------*/
	 SDL_LockYUVOverlay(overlay);
	 memcpy(p, videoIn->framebuffer,
	       videoIn->width * (videoIn->height) * 2);
	 SDL_UnlockYUVOverlay(overlay);
	 SDL_DisplayYUVOverlay(overlay, &drect);
	
	 /*-------------------------capture Image----------------------------------*/
	 char fbasename[20];
	 if (videoIn->capImage){
		 switch(videoIn->Imgtype) {
		 case 0:/*jpg*/
			if(SaveJPG(videoIn->ImageFName,videoIn->buf.bytesused,
					                       videoIn->tmpbuffer)) {
	             fprintf (stderr,"Error: Couldn't capture Image to %s \n",
			     videoIn->ImageFName);		
			} else {
				printf ("Capture jpg Image to %s \n",videoIn->ImageFName);
			}
			break;
		 case 1:/*bmp*/
         	if(pim==NULL) {  
				 /*24 bits -> 3bytes     32 bits ->4 bytes*/
		  		if((pim= malloc((pscreen->w)*(pscreen->h)*3))==NULL){
		 			printf("Couldn't allocate memory for: pim\n");
	     			videoIn->signalquit=0;
				ret = 3;		
		  		}
			}
			yuyv2bgr(videoIn->framebuffer,pim);

		    if(SaveBPM(videoIn->ImageFName, width, height, 24, pim)) {
			      fprintf (stderr,"Error: Couldn't capture Image to %s \n",
				  videoIn->ImageFName);
	    	} 
	    	else {	  
          		printf ("Capture BMP Image to %s \n",videoIn->ImageFName);
        	}
			break;
	     case 2:/*png*/
			if(pim==NULL) {  
				 /*24 bits -> 3bytes     32 bits ->4 bytes*/
		  		if((pim= malloc((pscreen->w)*(pscreen->h)*3))==NULL){
		 			printf("Couldn't allocate memory for: pim\n");
	     			videoIn->signalquit=0;
				ret = 3;		
		  		}
			}
			 yuyv2rgb(videoIn->framebuffer,pim);
			 write_png(videoIn->ImageFName, width, height,pim);
		 }
	     videoIn->capImage=FALSE;	
	  }
	  
	  /*---------------------------capture AVI---------------------------------*/
	  if (videoIn->capAVI && videoIn->signalquit){
	   long framesize;		
	   switch (AVIFormat) {
		   
		case 0: /*MJPG*/
			   if (AVI_write_frame (AviOut,
			       videoIn->tmpbuffer, videoIn->buf.bytesused) < 0)
	                printf ("write error on avi out \n");
			 break;
		case 1:
	  	   framesize=(pscreen->w)*(pscreen->h)*2; /*YUY2 -> 2 bytes per pixel */
	           if (AVI_write_frame (AviOut,
			       p, framesize) < 0)
	                printf ("write error on avi out \n");
		   break;
		case 2:
		    framesize=(pscreen->w)*(pscreen->h)*3; /*DIB 24/32 -> 3/4 bytes per pixel*/ 
		    if(pavi==NULL){
		      if((pavi= malloc(framesize))==NULL){
				printf("Couldn't allocate memory for: pim\n");
	     		videoIn->signalquit=0;
				ret = 4;
			  }
			}
		    yuyv2bgr(videoIn->framebuffer,pavi); 
		    if (AVI_write_frame (AviOut,pavi, framesize) < 0)
	        	printf ("write error on avi out \n");
		    break;

		} 
	   framecount++;	   
	  } 
	  //SDL_Delay(SDL_WAIT_TIME);
	
  }
  
   /*check if thread exited while AVI in capture mode*/
  if (videoIn->capAVI) {
  	AVIstoptime = ms_time();
	videoIn->capAVI = FALSE;   
  }	   
  printf("Thread terminated...\n");
  //~ if(pix2!=NULL) free (pix2);
  //~ pix2=NULL;
  if(pim!=NULL) free(pim);
  pim=NULL;
  if(pavi!=NULL)	free(pavi);
  pavi=NULL;
  printf("cleanig Thread allocations 100%%\n");
  fflush(NULL);//flush all output buffers
  //return (ret);	
  pthread_exit((void *) 0);
}

/******************************** MAIN *****************************************/
int main(int argc, char *argv[])
{
	int i;
	sndfile= tempnam (NULL, "gsnd_");/*generates a temporary file name*/
	
	if((EXEC_CALL=malloc(strlen(argv[0])*sizeof(char)))==NULL) {
		printf("couldn't allocate memory for: EXEC_CALL)\n");
		exit(1);
	}
	strcpy(EXEC_CALL,argv[0]);/*stores argv[0] - program call string*/
	//printf("EXEC_CALL=%s argv[0]=%s\n",EXEC_CALL,argv[0]);
	
	const SDL_VideoInfo *info;
    char driver[128];
    GtkWidget * boxh;
	GtkWidget *Resolution;
	GtkWidget *FrameRate;
	GtkWidget *label_FPS;
	GtkWidget *table2;
	GtkWidget *labelResol;
	GtkWidget *CapImageButt;
	GtkWidget *ImageFNameEntry;
	GtkWidget *ImageType;
	GtkWidget *label_AVIComp;
	GtkWidget *label_SndSampRate;
	GtkWidget *label_SndDevice;
	GtkWidget *label_SndNumChan;
	//VidState * s;
	if ((s = malloc (sizeof (VidState)))==NULL){
		printf("couldn't allocate memory for: s\n");
		exit(1); 
	}
	
	/* Initialize and set thread detached attribute */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	
	/* Initialize and set snd thread detached attribute */
    pthread_attr_init(&sndattr);
    pthread_attr_setdetachstate(&sndattr, PTHREAD_CREATE_JOINABLE);
   
    if((capt = (char *) calloc(1, 100 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for: capt\n");
		exit(1);
	}
       
    const char *home;
    const char *videodevice = NULL;
    
    
    char *separateur;
    char *sizestring = NULL;
	
	home = getenv("HOME");
	
	sprintf(confPath,"%s%s", home,"/.guvcviewrc");
    
    readConf(confPath);

    printf("guvcview version %s \n", version);
	
    
    for (i = 1; i < argc; i++) {
	
	/* skip bad arguments */
	if (argv[i] == NULL || *argv[i] == 0 || *argv[i] != '-') {
	    continue;
	}
	if (strcmp(argv[i], "-d") == 0) {
	    if (i + 1 >= argc) {
		printf("No parameter specified with -d, aborting.\n");
		exit(1);
	    }
	    videodevice = strdup(argv[i + 1]);
	}
	if (strcmp(argv[i], "-g") == 0) {
	    /* Ask for read instead default  mmap */
	    grabmethod = 0;
	}
	if (strcmp(argv[i], "-w") == 0) {
	    /* disable hw acceleration */
	    hwaccel = 0;
	}
	if (strcmp(argv[i], "-f") == 0) {
	    if (i + 1 >= argc) {
		printf("No parameter specified with -f, aborting.\n");	
	    } else {
	    mode = strdup(argv[i + 1]);
		}
	}
	
	if (strcmp(argv[i], "-s") == 0) {
	    if (i + 1 >= argc) {
		printf("No parameter specified with -s, aborting.\n");
		exit(1);
	    }

	    sizestring = strdup(argv[i + 1]);

	    width = strtoul(sizestring, &separateur, 10);
	    if (*separateur != 'x') {
		printf("Error in size use -s widthxheight \n");
		exit(1);
	    } else {
		++separateur;
		height = strtoul(separateur, &separateur, 10);
		if (*separateur != 0)
		    printf("hmm.. dont like that!! trying this height \n");
		printf(" size width: %d height: %d \n", width, height);
	    }
		printf(" size width: %d height: %d \n", width, height);
	}
	if (strcmp(argv[i], "-n") == 0) {
	    if (i + 1 >= argc) {
			printf("No parameter specified with -n. Ignoring option.\n");	
	    } else {
	    	avifile = strdup(argv[i + 1]); 
		}
	}
	if ((strcmp(argv[i], "-t") == 0) && (avifile)) {
		if (i + 1 >= argc) {
			printf("No parameter specified with -t.Ignoring option.\n");	
	    } else {
			char *timestr = strdup(argv[i + 1]);
			Capture_time= strtoul(timestr, &separateur, 10);
			//sscanf(timestr,"%i",Capture_time);
			printf("capturing avi for %i seconds",Capture_time);
		}
	}

	if (strcmp(argv[i], "-h") == 0) {
	    printf("usage: guvcview [-h -d -g -f -s -c -C -S] \n");
	    printf("-h	print this message \n");
	    printf("-d	/dev/videoX       use videoX device\n");
	    printf("-g	use read method for grab instead mmap \n");
	    printf("-w	disable SDL hardware accel. \n");
	    printf
		("-f	video format  default jpg  others options are yuv jpg \n");
	    printf("-s	widthxheight      use specified input size \n");
	    printf("-n	avi_file_name   if avi_file_name set enable avi capture from start \n");
	    printf("-t  capture_time  used with -n option, sets the capture time in seconds\n");
		exit(0);
	  }
    }
    
    if (strncmp(mode, "yuv", 3) == 0) {
		format = V4L2_PIX_FMT_YUYV;
		formind = 1;
		printf("Format is yuyv\n");
	} else if (strncmp(mode, "jpg", 3) == 0) {
		format = V4L2_PIX_FMT_MJPEG;
		formind = 0;
		printf("Format is MJPEG\n");
	} else {
		format = V4L2_PIX_FMT_MJPEG;
		formind = 0;
		printf("Format is Default MJPEG\n");
	}
	
	
	
	/*---------------------------- GTK init -----------------------------------*/
	
	gtk_init(&argc, &argv);
	

	/* Create a main window */
	mainwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (mainwin), "GUVCViewer Controls");
	//gtk_widget_set_usize(mainwin, winwidth, winheight);
	gtk_window_resize(GTK_WINDOW(mainwin),winwidth,winheight);
	/* Add event handlers */
	gtk_signal_connect(GTK_OBJECT(mainwin), "delete_event", GTK_SIGNAL_FUNC(delete_event), 0);
	
	
    /*----------------------------- Test SDL capabilities ---------------------*/
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0) {
	fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
	exit(1);
    }
	
    /* For this version, we will use hardware acceleration as default*/
    if(hwaccel) {
     	if ( ! getenv("SDL_VIDEO_YUV_HWACCEL") ) {
        	putenv("SDL_VIDEO_YUV_HWACCEL=1");
     	}
	 	if ( ! getenv("SDL_VIDEO_YUV_DIRECT") ) {
			putenv("SDL_VIDEO_YUV_DIRECT=1"); 
	 	}
	 } else {
	 	if ( ! getenv("SDL_VIDEO_YUV_HWACCEL") ) {
        	putenv("SDL_VIDEO_YUV_HWACCEL=0");
     	}
	 	if ( ! getenv("SDL_VIDEO_YUV_DIRECT") ) {
			putenv("SDL_VIDEO_YUV_DIRECT=0"); 
	 	}
	 }
     
    if (SDL_VideoDriverName(driver, sizeof(driver))) {
	printf("Video driver: %s\n", driver);
    }
    info = SDL_GetVideoInfo();

    if (info->wm_available) {
	printf("A window manager is available\n");
    }
    if (info->hw_available) {
	printf("Hardware surfaces are available (%dK video memory)\n",
	       info->video_mem);
	SDL_VIDEO_Flags |= SDL_HWSURFACE;
    }
    if (info->blit_hw) {
	printf("Copy blits between hardware surfaces are accelerated\n");
	SDL_VIDEO_Flags |= SDL_ASYNCBLIT;
    }
    if (info->blit_hw_CC) {
	printf
	    ("Colorkey blits between hardware surfaces are accelerated\n");
    }
    if (info->blit_hw_A) {
	printf("Alpha blits between hardware surfaces are accelerated\n");
    }
    if (info->blit_sw) {
	printf
	    ("Copy blits from software surfaces to hardware surfaces are accelerated\n");
    }
    if (info->blit_sw_CC) {
	printf
	    ("Colorkey blits from software surfaces to hardware surfaces are accelerated\n");
    }
    if (info->blit_sw_A) {
	printf
	    ("Alpha blits from software surfaces to hardware surfaces are accelerated\n");
    }
    if (info->blit_fill) {
	printf("Color fills on hardware surfaces are accelerated\n");
    }

    if (!(SDL_VIDEO_Flags & SDL_HWSURFACE))
	SDL_VIDEO_Flags |= SDL_SWSURFACE;

    if (videodevice == NULL || *videodevice == 0) {
	videodevice = "/dev/video0";
    }
    /*----------------------- init videoIn structure --------------------------*/	
    if((videoIn = (struct vdIn *) calloc(1, sizeof(struct vdIn)))==NULL){
   		printf("couldn't allocate memory for: videoIn\n");
		exit(1); 
    }
    if (init_videoIn
	(videoIn, (char *) videodevice, width, height, format,
	 grabmethod, fps, fps_num) < 0)
	exit(1);
	/* populate video capabilities structure array*/ 
	check_videoIn(videoIn);

   //~  SDL_WM_SetCaption("GUVCVideo", NULL);

    
	/*-----------------------------GTK widgets---------------------------------*/
	s->table = gtk_table_new (1, 3, FALSE);
   	gtk_table_set_row_spacings (GTK_TABLE (s->table), 10);
    gtk_table_set_col_spacings (GTK_TABLE (s->table), 10);
    gtk_container_set_border_width (GTK_CONTAINER (s->table), 10);
    gtk_widget_set_size_request (s->table, 440, -1);
	
	s->control = NULL;
	draw_controls(s);
	
	boxh = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (boxh), s->table, FALSE, FALSE, 0);
	gtk_widget_show (s->table);
	
	table2 = gtk_table_new(1,3,FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table2), 10);
    gtk_table_set_col_spacings (GTK_TABLE (table2), 10);
    gtk_container_set_border_width (GTK_CONTAINER (table2), 10);
    gtk_widget_set_size_request (table2, 350, -1);
	
	/* Resolution*/
	Resolution = gtk_combo_box_new_text ();
	char temp_str[9];
	int defres=0;
	for(i=0;i<videoIn->numb_resol;i++) {
		if (listVidCap[formind][i].width>0){
			sprintf(temp_str,"%ix%i",listVidCap[formind][i].width,
				             listVidCap[formind][i].height);
			gtk_combo_box_append_text(Resolution,temp_str);
			if ((width==listVidCap[formind][i].width) && 
				                   (height==listVidCap[formind][i].height)){
				defres=i;/*set selected*/
			}
		}
	}
	gtk_combo_box_set_active(Resolution,defres);
	if (defres==0) {
		width=listVidCap[formind][0].width;
		height=listVidCap[formind][0].height;
		videoIn->width=width;
		videoIn->height=height;
	}
	gtk_table_attach(GTK_TABLE(table2), Resolution, 1, 3, 3, 4,
                    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (Resolution);
	
	gtk_widget_set_sensitive (Resolution, TRUE);
	g_signal_connect (Resolution, "changed",
    		G_CALLBACK (resolution_changed), NULL);
	
	labelResol = gtk_label_new("Resolution:");
	gtk_misc_set_alignment (GTK_MISC (labelResol), 1, 0.5);

    gtk_table_attach (GTK_TABLE(table2), labelResol, 0, 1, 3, 4,
                    GTK_FILL, 0, 0, 0);

    gtk_widget_show (labelResol);
	
	/* Frame Rate */
	FrameRate = gtk_combo_box_new_text ();
	int deffps=0;
	for(i=0;i<listVidCap[formind][defres].numb_frates;i++) {
		sprintf(temp_str,"%i/%i fps",listVidCap[formind][defres].framerate_num[i],
				             listVidCap[formind][defres].framerate_denom[i]);
		gtk_combo_box_append_text(FrameRate,temp_str);
		if ((videoIn->fps_num==listVidCap[formind][defres].framerate_num[i]) && 
			      (videoIn->fps==listVidCap[formind][defres].framerate_denom[i])){
				deffps=i;/*set selected*/
		}
	}
	
	gtk_table_attach(GTK_TABLE(table2), FrameRate, 1, 3, 2, 3,
                    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (FrameRate);
	
	
	gtk_combo_box_set_active(FrameRate,deffps);
	if (deffps==0) {
		fps=listVidCap[formind][defres].framerate_denom[0];
		fps_num=listVidCap[formind][0].framerate_num[0];
		videoIn->fps=fps;
		videoIn->fps_num=fps_num;
	}
	    
	gtk_widget_set_sensitive (FrameRate, TRUE);
	g_signal_connect (GTK_COMBO_BOX(FrameRate), "changed",
    	G_CALLBACK (FrameRate_changed), Resolution);
	
	label_FPS = gtk_label_new("Frame Rate:");
	gtk_misc_set_alignment (GTK_MISC (label_FPS), 1, 0.5);

    gtk_table_attach (GTK_TABLE(table2), label_FPS, 0, 1, 2, 3,
                    GTK_FILL, 0, 0, 0);

    gtk_widget_show (label_FPS);
	
	/* Image Capture*/
	CapImageButt = gtk_button_new_with_label("Capture");
	ImageFNameEntry = gtk_entry_new();
	
	if (videoIn->formatIn== V4L2_PIX_FMT_MJPEG) {
		gtk_entry_set_text(GTK_ENTRY(ImageFNameEntry),DEFAULT_IMAGE_FNAME);
	} else { /*disable jpg*/
		gtk_entry_set_text(GTK_ENTRY(ImageFNameEntry),DEFAULT_BMP_FNAME);
	}
	gtk_table_attach(GTK_TABLE(table2), CapImageButt, 0, 1, 4, 5,
                    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table2), ImageFNameEntry, 1, 2, 4, 5,
                    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	ImageType=gtk_combo_box_new_text ();
	/*if YUYV on input disable jpg capture*/
	if (videoIn->formatIn== V4L2_PIX_FMT_MJPEG) {
		gtk_combo_box_append_text(GTK_COMBO_BOX(ImageType),"JPG");
	} else {
		videoIn->Imgtype=1;
	}
	gtk_combo_box_append_text(GTK_COMBO_BOX(ImageType),"BMP");
	gtk_combo_box_append_text(GTK_COMBO_BOX(ImageType),"PNG");
	gtk_combo_box_set_active(GTK_COMBO_BOX(ImageType),0);
	gtk_table_attach(GTK_TABLE(table2), ImageType, 2, 3, 4, 5,
                    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (ImageType);
	
	gtk_widget_show (CapImageButt);
	gtk_widget_show (ImageFNameEntry);
	gtk_widget_show (ImageType);
	g_signal_connect (GTK_COMBO_BOX(ImageType), "changed",
    	G_CALLBACK (ImageType_changed), ImageFNameEntry);
	g_signal_connect (GTK_BUTTON(CapImageButt), "clicked",
         G_CALLBACK (capture_image), ImageFNameEntry);
	
	
	/*AVI Capture*/
	AVIFNameEntry = gtk_entry_new();
	if (avifile) {	/*avi capture enabled from start*/
		CapAVIButt = gtk_button_new_with_label("Stop");
		gtk_entry_set_text(GTK_ENTRY(AVIFNameEntry),avifile);
	} else {
		CapAVIButt = gtk_button_new_with_label("Capture");
		videoIn->capAVI = FALSE;
		gtk_entry_set_text(GTK_ENTRY(AVIFNameEntry),DEFAULT_AVI_FNAME);
	}
	
	gtk_table_attach(GTK_TABLE(table2), CapAVIButt, 0, 1, 5, 6,
                    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table2), AVIFNameEntry, 1, 3, 5, 6,
                    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_widget_show (CapAVIButt);
	gtk_widget_show (AVIFNameEntry);
	g_signal_connect (GTK_BUTTON(CapAVIButt), "clicked",
         G_CALLBACK (capture_avi), AVIFNameEntry);
	
	gtk_box_pack_start ( GTK_BOX (boxh), table2, FALSE, FALSE, 0);
	gtk_widget_show (table2);
	
	/* AVI Compressor */
	AVIComp = gtk_combo_box_new_text ();
	if (videoIn->formatIn== V4L2_PIX_FMT_MJPEG) {
		/* disable MJP if V4L2_PIX_FMT_YUYV*/
		gtk_combo_box_append_text(GTK_COMBO_BOX(AVIComp),"MJPG - compressed");
		AVIFormat=0;/*set MJPG as default*/
	} else {
		AVIFormat=1; /*set YUY2 as default*/
	}
	gtk_combo_box_append_text(GTK_COMBO_BOX(AVIComp),"YUY2 - uncomp YUV");
	gtk_combo_box_append_text(GTK_COMBO_BOX(AVIComp),"RGB - uncomp BMP");
	
	gtk_table_attach(GTK_TABLE(table2), AVIComp, 1, 3, 6, 7,
                    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (AVIComp);
	gtk_combo_box_set_active(GTK_COMBO_BOX(AVIComp),0);
 	
	gtk_widget_set_sensitive (AVIComp, TRUE);
	g_signal_connect (GTK_COMBO_BOX(AVIComp), "changed",
    	G_CALLBACK (AVIComp_changed), NULL);
	
	label_AVIComp = gtk_label_new("AVI Format:");
	gtk_misc_set_alignment (GTK_MISC (label_AVIComp), 1, 0.5);

    gtk_table_attach (GTK_TABLE(table2), label_AVIComp, 0, 1, 6, 7,
                    GTK_FILL, 0, 0, 0);

    gtk_widget_show (label_AVIComp);

    /* sound interface ------------------------------------------------------*/
	
	/*get sound device list and info-----------------------------------------*/
	
	SndDevice = gtk_combo_box_new_text ();
		
	int     it, numDevices, defaultDisplayed;
    const   PaDeviceInfo *deviceInfo;
    PaStreamParameters inputParameters, outputParameters;
    PaError err;
	
	Pa_Initialize();
	
	numDevices = Pa_GetDeviceCount();
    if( numDevices < 0 )
    {
        printf( "SOUND DISABLE: Pa_CountDevices returned 0x%x\n", numDevices );
        err = numDevices;
        Pa_Terminate();
		Sound_enable=0;
    } else {
	
	 	for( it=0; it<numDevices; it++ )
    	{
        	deviceInfo = Pa_GetDeviceInfo( it );
        	printf( "--------------------------------------- device #%d\n", it );
                
    		/* Mark global and API specific default devices */
        	defaultDisplayed = 0;
		    /* Default Input will save the ALSA default device index*/
			/* since ALSA lists after OSS							*/
        	if( it == Pa_GetDefaultInputDevice() )
        	{
            	printf( "[ Default Input" );
            	defaultDisplayed = 1;
				Sound_DefDev=Sound_numInputDev;/*default index in array of input devs*/
        	}
        	else if( it == Pa_GetHostApiInfo( deviceInfo->hostApi )->defaultInputDevice )
        	{
            	const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo( deviceInfo->hostApi );
            	printf( "[ Default %s Input", hostInfo->name );
            	defaultDisplayed = 2;
				Sound_DefDev=Sound_numInputDev;/*index in array of input devs*/
        	}
        /* Output device doesn't matter for capture*/
        	if( it == Pa_GetDefaultOutputDevice() )
        	{
            	printf( (defaultDisplayed ? "," : "[") );
            	printf( " Default Output" );
            	defaultDisplayed = 3;
        	}
        	else if( it == Pa_GetHostApiInfo( deviceInfo->hostApi )->defaultOutputDevice )
        	{
            	const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo( deviceInfo->hostApi );
            	printf( (defaultDisplayed ? "," : "[") );                
           		printf( " Default %s Output", hostInfo->name );/* OSS ALSA etc*/
            	defaultDisplayed = 4;
        	}

        	if( defaultDisplayed!=0 )
            	printf( " ]\n" );

    		/* print device info fields */
        	printf( "Name                        = %s\n", deviceInfo->name );
        	printf( "Host API                    = %s\n",  Pa_GetHostApiInfo( deviceInfo->hostApi )->name );
        	
			printf( "Max inputs = %d", deviceInfo->maxInputChannels  );
			/* if it as input channels it's a capture device*/
			if (deviceInfo->maxInputChannels >0) { 
				Sound_IndexDev[Sound_numInputDev].id=it; /*saves dev id*/
				Sound_IndexDev[Sound_numInputDev].chan=deviceInfo->maxInputChannels;
				Sound_IndexDev[Sound_numInputDev].samprate=deviceInfo->defaultSampleRate;
				//Sound_IndexDev[Sound_numInputDev].Hlatency=deviceInfo->defaultHighInputLatency;
				//Sound_IndexDev[Sound_numInputDev].Llatency=deviceInfo->defaultLowInputLatency;
				Sound_numInputDev++;
				gtk_combo_box_append_text(GTK_COMBO_BOX(SndDevice),deviceInfo->name);		
			}
			
        	printf( ", Max outputs = %d\n", deviceInfo->maxOutputChannels  );

        	printf( "Default low input latency   = %8.3f\n", deviceInfo->defaultLowInputLatency  );
        	printf( "Default low output latency  = %8.3f\n", deviceInfo->defaultLowOutputLatency  );
        	printf( "Default high input latency  = %8.3f\n", deviceInfo->defaultHighInputLatency  );
        	printf( "Default high output latency = %8.3f\n", deviceInfo->defaultHighOutputLatency  );
			printf( "Default sample rate         = %8.2f\n", deviceInfo->defaultSampleRate );
			
		}
		Pa_Terminate();

    	printf("----------------------------------------------\n");
	
	}
	
	gtk_table_attach(GTK_TABLE(table2), SndDevice, 1, 3, 8, 9,
                    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (SndDevice);
	gtk_combo_box_set_active(GTK_COMBO_BOX(SndDevice),Sound_DefDev);
	Sound_UseDev=Sound_DefDev;/* using default device*/
	
	if (Sound_enable) gtk_widget_set_sensitive (SndDevice, TRUE);
	else  gtk_widget_set_sensitive (SndDevice, FALSE);
	g_signal_connect (GTK_COMBO_BOX(SndDevice), "changed",
    	G_CALLBACK (SndDevice_changed), NULL);
	
	label_SndDevice = gtk_label_new("Imput Device:");
	gtk_misc_set_alignment (GTK_MISC (label_SndDevice), 1, 0.5);

    gtk_table_attach (GTK_TABLE(table2), label_SndDevice, 0, 1, 8, 9,
                    GTK_FILL, 0, 0, 0);

    gtk_widget_show (label_SndDevice);
	
	
	//~ if (Sound_numInputDev == 0) Sound_enable=0;
	//~ printf("SOUND DISABLE: no imput devices detected\n");
	
	SndEnable=gtk_check_button_new_with_label (" Sound");
	gtk_table_attach(GTK_TABLE(table2), SndEnable, 0, 1, 7, 8,
                    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_toggle_button_set_active(GTK_CHECK_BUTTON(SndEnable),(Sound_enable > 0));
	gtk_widget_show (SndEnable);
	g_signal_connect (GTK_CHECK_BUTTON(SndEnable), "toggled",
    	G_CALLBACK (SndEnable_changed), NULL);
	
	SndSampleRate= gtk_combo_box_new_text ();
	gtk_combo_box_append_text(GTK_COMBO_BOX(SndSampleRate),"Dev. Default");
	for( i=1; stdSampleRates[i] > 0; i++ )
    {
		char dst[8];
		sprintf(dst,"%d",stdSampleRates[i]);
		gtk_combo_box_append_text(GTK_COMBO_BOX(SndSampleRate),dst);
	}
	
	gtk_table_attach(GTK_TABLE(table2), SndSampleRate, 1, 3, 9, 10,
                    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (SndSampleRate);
	
	//gtk_combo_box_set_active(GTK_COMBO_BOX(SndSampleRate),Sound_IndexDev[Sound_DefDev]);
	printf("device:%d default sample rate:%d\n",Sound_IndexDev[Sound_UseDev].id,
		                              Sound_IndexDev[Sound_UseDev].samprate);
	
	gtk_combo_box_set_active(GTK_COMBO_BOX(SndSampleRate),0); /*device default*/
	//~ for( i=0; stdSampleRates[i] > 0; i++ )
            //~ {
				//~ if (Sound_IndexDev[Sound_UseDev].samprate==stdSampleRates[i]){ 
					//~ gtk_combo_box_set_active(GTK_COMBO_BOX(SndSampleRate),i);
					//~ Sound_SampRate=Sound_IndexDev[Sound_UseDev].samprate;
					//~ printf("SampleRate initial:%d\n",Sound_SampRate);
					//~ break;
				//~ }
			//~ }
	
 	
	
	if (Sound_enable) gtk_widget_set_sensitive (SndSampleRate, TRUE);
	else  gtk_widget_set_sensitive (SndSampleRate, FALSE);
	g_signal_connect (GTK_COMBO_BOX(SndSampleRate), "changed",
    	G_CALLBACK (SndSampleRate_changed), NULL);
	
	label_SndSampRate = gtk_label_new("Sample Rate:");
	gtk_misc_set_alignment (GTK_MISC (label_SndSampRate), 1, 0.5);

    gtk_table_attach (GTK_TABLE(table2), label_SndSampRate, 0, 1, 9, 10,
                    GTK_FILL, 0, 0, 0);

    gtk_widget_show (label_SndSampRate);
	
	SndNumChan= gtk_combo_box_new_text ();
	gtk_combo_box_append_text(GTK_COMBO_BOX(SndNumChan),"Dev. Default");
	gtk_combo_box_append_text(GTK_COMBO_BOX(SndNumChan),"1 - mono");
	gtk_combo_box_append_text(GTK_COMBO_BOX(SndNumChan),"2 - stereo");
	
	gtk_table_attach(GTK_TABLE(table2), SndNumChan, 1, 3, 10, 11,
                    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (SndNumChan);
	switch (Sound_NumChan) {
	   case 0:/*device default*/
			gtk_combo_box_set_active(GTK_COMBO_BOX(SndNumChan),0);
	    break;
	   case 1:/*mono*/	
	    	gtk_combo_box_set_active(GTK_COMBO_BOX(SndNumChan),1);
	    break;
		case 2:/*stereo*/
			gtk_combo_box_set_active(GTK_COMBO_BOX(SndNumChan),2);
	   default:
	    /*set Default to NUM_CHANNELS*/
	    	Sound_NumChan=NUM_CHANNELS;	
	}
	if (Sound_enable) gtk_widget_set_sensitive (SndNumChan, TRUE);
	else gtk_widget_set_sensitive (SndNumChan, FALSE);
	g_signal_connect (GTK_COMBO_BOX(SndNumChan), "changed",
    	G_CALLBACK (SndNumChan_changed), NULL);
	
	label_SndNumChan = gtk_label_new("Chanels:");
	gtk_misc_set_alignment (GTK_MISC (label_SndNumChan), 1, 0.5);

    gtk_table_attach (GTK_TABLE(table2), label_SndNumChan, 0, 1, 10, 11,
                    GTK_FILL, 0, 0, 0);

    gtk_widget_show (label_SndNumChan);
	printf("SampleRate:%d Channels:%d\n",Sound_SampRate,Sound_NumChan);
	
	//~ SndSampleBits= gtk_combo_box_new_text ();
	//~ gtk_combo_box_append_text(GTK_COMBO_BOX(SndSampleBits),"16");
	//~ gtk_combo_box_append_text(GTK_COMBO_BOX(SndSampleBits),"8");
	
	
	//~ gtk_table_attach(GTK_TABLE(table2), SndSampleBits, 1, 3, 9, 10,
                    //~ GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	//~ gtk_widget_show (SndSampleBits);
	
	//~ switch (Sound_SampBits) {
	   //~ case 16:/*16 bits*/
			//~ gtk_combo_box_set_active(GTK_COMBO_BOX(SndSampleRate),0);
			//~ Sound_Format=paInt16;
	    //~ break;
	   //~ case 8:/*8 bits*/	
	    	//~ gtk_combo_box_set_active(GTK_COMBO_BOX(SndSampleRate),1);
			//~ Sound_Format=paInt8;
	    //~ break;
	   //~ default:
	    //~ /*set Default to 16*/
	    	//~ Sound_SampBits=16;
			//~ Sound_Format=paInt16;
		    //~ gtk_combo_box_set_active(GTK_COMBO_BOX(SndSampleBits),0);
 	//~ }
	
	//~ gtk_widget_set_sensitive (SndSampleBits, TRUE);
	//~ g_signal_connect (GTK_COMBO_BOX(SndSampleBits), "changed",
    	//~ G_CALLBACK (SndSampleBits_changed), NULL);
	
	//~ label_SndSampRate = gtk_label_new("Sample Bits:");
	//~ gtk_misc_set_alignment (GTK_MISC (label_SndSampBits), 1, 0.5);

    //~ gtk_table_attach (GTK_TABLE(table2), label_SndSampBits, 0, 1, 9, 10,
                    //~ GTK_FILL, 0, 0, 0);

    //~ gtk_widget_show (label_SndSampBits);
    
/*------------------------------ SDL init video ---------------------*/
	pscreen =
	SDL_SetVideoMode(videoIn->width, videoIn->height, bpp,
			 SDL_VIDEO_Flags);
	overlay =
	SDL_CreateYUVOverlay(videoIn->width, videoIn->height,
			     SDL_YUY2_OVERLAY, pscreen);
	
	p = (unsigned char *) overlay->pixels[0];
    
	drect.x = 0;
	drect.y = 0;
	drect.w = pscreen->w;
	drect.h = pscreen->h;

	SDL_WM_SetCaption("GUVCVideo", NULL);
	//lasttime = SDL_GetTicks();
	
	/* main container -----------------------------------------*/
	gtk_container_add (GTK_CONTAINER (mainwin), boxh);
	gtk_widget_show (boxh);
	
	gtk_widget_show (mainwin);
	
	/*------------------- avi capture from start -----------------------------*/
	if(avifile) { 
		AviOut = AVI_open_output_file(avifile);
	   	/*4CC compression "YUY2" (YUV) or "DIB " (RGB24)  or  "MJPG"*/
		char *compression="MJPG";

		switch (AVIFormat) {
	 		case 0:
				compression="MJPG";
				break;
	 		case 1:
				compression="YUY2";
				break;
	 		case 2:
				compression="DIB ";
				break;
	 		default:
				compression="MJPG";
		}
	   AVI_set_video(AviOut, videoIn->width, videoIn->height, videoIn->fps,compression);		
	   /* audio will be set in aviClose - if enabled*/
	   videoIn->AVIFName = avifile;		
	   videoIn->capAVI = TRUE;
	   /*disabling sound and avi compression controls*/
	   gtk_widget_set_sensitive (AVIComp, FALSE);
	   gtk_widget_set_sensitive (SndEnable,FALSE); 
 	   gtk_widget_set_sensitive (SndSampleRate,FALSE);
 	   gtk_widget_set_sensitive (SndDevice,FALSE);
 	   gtk_widget_set_sensitive (SndNumChan,FALSE);
	   /* Creating the sound capture loop thread if Sound Enable*/ 
	   if(Sound_enable > 0) { 
	      int rsnd = pthread_create(&sndthread, &sndattr, sound_capture, NULL); 
          if (rsnd)
          {
             printf("ERROR; return code from snd pthread_create() is %d\n", rsnd);
          }
		}
		AVIstarttime = ms_time();
	}
	
	/* Creating the main loop (video) thread*/
	int rc = pthread_create(&mythread, &attr, main_loop, NULL); 
 	if (rc) {
         	printf("ERROR; return code from pthread_create() is %d\n", rc);
        	exit(2);
      	}
    
	/* The last thing to get called */
	gtk_main();

	//input_free_controls (s->control, s->num_controls);

	return 0;
}

/************************ clean up and shut down ***************************/
gint 
shutd (gint restart) 
{
 	int exec_status=0;
	int i;
	int tstatus;
	
    printf("Shuting Down Thread\n");
	videoIn->signalquit=0;
	printf("waiting for thread to finish\n");
	/*shuting down while in avi capture mode*/
	/*must close avi						*/
	if(videoIn->capAVI) {
		printf("stoping AVI capture\n");
	 	AVIstoptime = ms_time();
	 	//printf("AVI stop time:%d\n",AVIstoptime);	
	 	videoIn->capAVI = FALSE;
	 	aviClose();
		
		/*wait for io thread*/
		pthread_attr_destroy(&diskIOattr);
		int rio = pthread_join(diskIOthread, (void **)&tstatus);
    	printf("Completed join with thread io status= %d\n", tstatus);
	}
	
	/* Free attribute and wait for the main loop (video) thread */
    pthread_attr_destroy(&attr);
	int rc = pthread_join(mythread, (void **)&tstatus);
    if (rc)
      {
         printf("ERROR; return code from pthread_join() is %d\n", rc);
         exit(-1);
      }
    printf("Completed join with thread status= %d\n", tstatus);
	
	
	gtk_window_get_size(GTK_WINDOW(mainwin),&winwidth,&winheight);//mainwin or widget
	
	
	close_v4l2(videoIn);
	close(videoIn->fd);
	printf("closed strutures\n");
    free(videoIn);
	free(capt);
	free(sndfile);
	if (avifile) free (avifile);
	SDL_Quit();
	printf("SDL Quit\n");
	printf("cleaned allocations - 50%%\n");
	gtk_main_quit();
	
    printf("GTK quit\n");
    writeConf(confPath);
	input_free_controls (s->control, s->num_controls);
	printf("free controls - vidState\n");
	
	char locpath[30];
	char fullpath[50];
	char *pwd;
	
 	if (restart==1) {
		 printf("restarting guvcview with command: %s\n",EXEC_CALL);
		 exec_status = execlp(EXEC_CALL,EXEC_CALL,NULL);/*No parameters passed*/
 	}
	
	free(EXEC_CALL);
	printf("cleanig allocations - 100%%\n");
	return exec_status;
}

