/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#                                                                               #
# This program is free software; you can redistribute it and/or modify          #
# it under the terms of the GNU General Public License as published by          #
# the Free Software Foundation; either version 2 of the License, or             #
# (at your option) any later version.                                           #
#                                                                               #
# This program is distributed in the hope that it will be useful,               #
# but WITHOUT ANY WARRANTY; without even the implied warranty of                #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 #
# GNU General Public License for more details.                                  #
#                                                                               #
# You should have received a copy of the GNU General Public License             #
# along with this program; if not, write to the Free Software                   #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA     #
#                                                                               #
********************************************************************************/
#include "../config.h"

#ifdef PULSEAUDIO

#include <glib.h>
#include <string.h>
#include "pulse_audio.h"
#include "audio_effects.h"
#include "ms_time.h"

#include <errno.h>

#define __AMUTEX &pdata->mutex

//#define TIME_EVENT_USEC 50000

// From pulsecore/macro.h
#define pa_memzero(x,l) (memset((x), 0, (l)))
#define pa_zero(x) (pa_memzero(&(x), sizeof(x)))

static pa_stream *recordstream = NULL; // pulse audio stream
static pa_context *pa_ctx = NULL; //pulse context

static uint32_t latency_ms = 15; // requested initial latency in milisec: 0 use max
static pa_usec_t latency = 0; //real latency in usec (for timestamping)

//static int64_t timestamp = 0;
//static int64_t totalFrames = 0;

static int sink_index = 0;
static int source_index = 0;

static void
finish(pa_context *pa_ctx, pa_mainloop *pa_ml)
{
	/* clean up and disconnect */
	pa_context_disconnect(pa_ctx);
	pa_context_unref(pa_ctx);
	pa_mainloop_free(pa_ml);
}

/* This callback gets called when our context changes state.  We really only
 * care about when it's ready or if it has failed
 */
void
pa_state_cb(pa_context *c, void *userdata)
{
	pa_context_state_t state;
	int *pa_ready = userdata;
	state = pa_context_get_state(c);
	switch  (state)
	{
		// These are just here for reference
		case PA_CONTEXT_UNCONNECTED:
		case PA_CONTEXT_CONNECTING:
		case PA_CONTEXT_AUTHORIZING:
		case PA_CONTEXT_SETTING_NAME:
		default:
			break;
		case PA_CONTEXT_FAILED:
		case PA_CONTEXT_TERMINATED:
			*pa_ready = 2;
			break;
		case PA_CONTEXT_READY:
			*pa_ready = 1;
			break;
	}
}

/*
 * pa_mainloop will call this function when it's ready to tell us about a source.
 * Since we're not threading when listing devices, there's no need for mutexes
 * on the devicelist structure
 */
void pa_sourcelist_cb(pa_context *c, const pa_source_info *l, int eol, void *userdata)
{
    struct GLOBAL *global = userdata;
    int channels = 1;

    if (eol > 0)
	{
        return;
    }

	source_index++;

	if(l->sample_spec.channels <1)
		channels = 1;
	else if (l->sample_spec.channels > 2)
		channels = 2;
	else
		channels = l->sample_spec.channels;

	if(global->debug)
	{
		g_print("=======[ Input Device #%d ]=======\n", source_index);
		g_print("Description: %s\n", l->description);
		g_print("Name: %s\n", l->name);
		g_print("Index: %d\n", l->index);
		g_print("Channels: %d (default to: %d)\n", l->sample_spec.channels, channels);
		g_print("SampleRate: %d\n", l->sample_spec.rate);
		g_print("Latency: %llu (usec)\n", (long long unsigned) l->latency);
		g_print("Card: %d\n", l->card);
		g_print("\n");
	}

	if(l->monitor_of_sink == PA_INVALID_INDEX)
	{
		global->Sound_numInputDev++;
		//allocate new Sound Device Info
		global->Sound_IndexDev = g_renew(sndDev, global->Sound_IndexDev, global->Sound_numInputDev);
		//fill structure with sound data
		global->Sound_IndexDev[global->Sound_numInputDev-1].id = l->index; /*saves dev id*/
		strncpy(global->Sound_IndexDev[global->Sound_numInputDev-1].name,  l->name, 511);
		strncpy(global->Sound_IndexDev[global->Sound_numInputDev-1].description, l->description, 255);
		global->Sound_IndexDev[global->Sound_numInputDev-1].chan = channels;
		global->Sound_IndexDev[global->Sound_numInputDev-1].samprate = l->sample_spec.rate;
	}
}

/*
 * See above.  This callback is pretty much identical to the previous
 * but it will only print the output devices
 */
void pa_sinklist_cb(pa_context *c, const pa_sink_info *l, int eol, void *userdata)
{
	struct GLOBAL *global = userdata;

    /* If eol is set to a positive number, you're at the end of the list */
    if (eol > 0)
	{
        return;
    }

	sink_index++;

	if(global->debug)
	{
		g_print("=======[ Output Device #%d ]=======\n", sink_index);
		g_print("Description: %s\n", l->description);
		g_print("Name: %s\n", l->name);
		g_print("Index: %d\n", l->index);
		g_print("Channels: %d\n", l->channel_map.channels);
		g_print("SampleRate: %d\n", l->sample_spec.rate);
		g_print("Latency: %llu (usec)\n", (long long unsigned) l->latency);
		g_print("Card: %d\n", l->card);
		g_print("\n");
	}
}

/*
 * iterate the main loop until all devices are listed
 */
int pa_get_devicelist(struct GLOBAL *global)
{
	/* Define our pulse audio loop and connection variables */
	pa_mainloop *pa_ml;
	pa_mainloop_api *pa_mlapi;
	pa_operation *pa_op = NULL;
	pa_context *pa_ctx;

	/* We'll need these state variables to keep track of our requests */
    int state = 0;
    int pa_ready = 0;

    /* Create a mainloop API and connection to the default server */
    pa_ml = pa_mainloop_new();
    pa_mlapi = pa_mainloop_get_api(pa_ml);
    pa_ctx = pa_context_new(pa_mlapi, "getDevices");

    /* This function connects to the pulse server */
    pa_context_connect(pa_ctx, NULL, 0, NULL);

    /*
	 * This function defines a callback so the server will tell us it's state.
     * Our callback will wait for the state to be ready.  The callback will
     * modify the variable to 1 so we know when we have a connection and it's
     * ready.
     * If there's an error, the callback will set pa_ready to 2
	 */
    pa_context_set_state_callback(pa_ctx, pa_state_cb, &pa_ready);

    /*
	 * Now we'll enter into an infinite loop until we get the data we receive
     * or if there's an error
	 */
    for (;;)
	{
        /*
		 * We can't do anything until PA is ready, so just iterate the mainloop
         * and continue
		 */
        if (pa_ready == 0)
		{
            pa_mainloop_iterate(pa_ml, 1, NULL);
            continue;
        }
        /* We couldn't get a connection to the server, so exit out */
        if (pa_ready == 2)
		{
            finish(pa_ctx, pa_ml);
            return -1;
        }
        /*
		 * At this point, we're connected to the server and ready to make
		 * requests
		 */
        switch (state)
		{
            /* State 0: we haven't done anything yet */
            case 0:
                /*
				 * This sends an operation to the server.  pa_sinklist_cb is
                 * our callback function and a pointer to our devicelist will
                 * be passed to the callback (global) The operation ID is stored in the
                 * pa_op variable
				 */
                pa_op = pa_context_get_sink_info_list(pa_ctx,
                        pa_sinklist_cb,
                        global //userdata
                        );

                /* Update state for next iteration through the loop */
                state++;
                break;
            case 1:
                /*
				 * Now we wait for our operation to complete.  When it's
                 * complete our pa_output_devicelist is filled out, and we move
                 * along to the next state
				 */
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE)
				{
                    pa_operation_unref(pa_op);

                    /*
					 * Now we perform another operation to get the source
                     * (input device) list just like before.  This time we pass
                     * a pointer to our input structure
					 */
                    pa_op = pa_context_get_source_info_list(pa_ctx,
                            pa_sourcelist_cb,
                            global //userdata
                            );
                    /* Update the state so we know what to do next */
                    state++;
                }
                break;
            case 2:
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE)
				{
                    /* Now we're done, clean up and disconnect and return */
                    pa_operation_unref(pa_op);
                    finish(pa_ctx, pa_ml);
                    return 0;
                }
                break;
            default:
                /* We should never see this state */
                g_print("AUDIO: Pulseaudio in state %d\n", state);
                return -1;
        }
        /*
		 * Iterate the main loop and go again.  The second argument is whether
         * or not the iteration should block until something is ready to be
         * done.  Set it to zero for non-blocking.
		 */
        pa_mainloop_iterate(pa_ml, 1, NULL);
    }
}

/*
 * get the device list from pulse server
 */
int
pulse_list_snd_devices(struct GLOBAL *global)
{
    global->Sound_numInputDev = 0; //reset input device count
    global->Sound_DefDev = 0;

    if (pa_get_devicelist(global) < 0)
    {
        g_print("AUDIO: Pulseaudio failed to get audio device list from PULSE server\n");
        return 1;
    }

    return 0;
}

void get_latency(pa_stream *s)
{
	pa_usec_t l;
	int negative;

	pa_stream_get_timing_info(s);

	if (pa_stream_get_latency(s, &l, &negative) != 0)
	{
		g_printerr("AUDIO: Pulseaudio pa_stream_get_latency() failed\n");
		return;
	}

	//latency = l * (negative?-1:1);
	latency = l; /*can only be negative in monitoring streams*/

	//g_print("%0.0f usec    \r", (float)latency);
}

/*
 * audio record callback
 */
static void
stream_request_cb(pa_stream *s, size_t length, void *userdata)
{

    //struct paRecordData *pdata = (struct paRecordData*) userdata;

    //__LOCK_MUTEX( __AMUTEX );
    //    int channels = pdata->channels;
    //    int samprate = pdata->samprate;
    //__UNLOCK_MUTEX( __AMUTEX );

	int64_t timestamp = 0;

	while (pa_stream_readable_size(s) > 0)
	{
		const void *inputBuffer;
		size_t length;

		/*read from stream*/
		if (pa_stream_peek(s, &inputBuffer, &length) < 0)
		{
			g_print( "AUDIO: pa_stream_peek() failed\n");
			return;
		}

		get_latency(s);

		timestamp = ns_time_monotonic() - (latency * 1000);

		if(length == 0)
		{
			g_print( "AUDIO: empty buffer!\n");
			return; /*buffer is empty*/
		}

		/*timestamp*/
		int numSamples= length / sizeof(SAMPLE);

		if(inputBuffer == NULL) /*it's a hole*/
		{
			record_silence (numSamples, userdata);
		}
		else
			record_sound ( inputBuffer, numSamples, timestamp, userdata );

		pa_stream_drop(s); /*clean the samples*/
	}

}

/*
 * Iterate the main loop while recording is on.
 * This function runs under it's own thread called by pulse_init_audio
 */
static int
pulse_read_audio(void *userdata)
{

    struct paRecordData *pdata = (struct paRecordData*) userdata;

    g_print("Pulse audio Thread started\n");
    pa_mainloop *pa_ml;
    pa_mainloop_api *pa_mlapi;
    pa_buffer_attr bufattr;
    pa_sample_spec ss;
    pa_stream_flags_t flags = 0;
    int r;
    int pa_ready = 0;

    /* Create a mainloop API and connection to the default server */
    pa_ml = pa_mainloop_new();
    pa_mlapi = pa_mainloop_get_api(pa_ml);
    pa_ctx = pa_context_new(pa_mlapi, "guvcview Pulse API");
    pa_context_connect(pa_ctx, NULL, 0, NULL);

    /*
	 * This function defines a callback so the server will tell us it's state.
     * Our callback will wait for the state to be ready.  The callback will
     * modify the variable to 1 so we know when we have a connection and it's
     * ready.
     * If there's an error, the callback will set pa_ready to 2
	 */
    pa_context_set_state_callback(pa_ctx, pa_state_cb, &pa_ready);

    /*
     * This function defines a time event callback (called every TIME_EVENT_USEC)
     */
    //pa_context_rttime_new(pa_ctx, pa_rtclock_now() + TIME_EVENT_USEC, time_event_callback, NULL);

    /*
	 * We can't do anything until PA is ready, so just iterate the mainloop
     * and continue
	 */
    while (pa_ready == 0)
    {
        pa_mainloop_iterate(pa_ml, 1, NULL);
    }
    if (pa_ready == 2)
    {
        finish(pa_ctx, pa_ml);
        return -1;
    }

	/* set the sample spec (frame rate, channels and format) */
    ss.rate = pdata->samprate;
    ss.channels = pdata->channels;
    ss.format = PULSE_SAMPLE_TYPE;

    recordstream = pa_stream_new(pa_ctx, "Record", &ss, NULL);
    if (!recordstream)
        g_print("AUDIO: pa_stream_new failed\n");

    /* define the callbacks */
    pa_stream_set_read_callback(recordstream, stream_request_cb, (void *) pdata);

	// Set properties of the record buffer
    pa_zero(bufattr);
    /* optimal value for all is (uint32_t)-1   ~= 2 sec */
    bufattr.maxlength = (uint32_t) -1;
    bufattr.prebuf = (uint32_t) -1;
    bufattr.minreq = (uint32_t) -1;

    if (latency_ms > 0)
    {
      bufattr.fragsize = bufattr.tlength = pa_usec_to_bytes(latency_ms * PA_USEC_PER_MSEC, &ss);
      flags |= PA_STREAM_ADJUST_LATENCY;
    }
    else
      bufattr.fragsize = bufattr.tlength = (uint32_t) -1;

	flags |= PA_STREAM_INTERPOLATE_TIMING;
    flags |= PA_STREAM_AUTO_TIMING_UPDATE;

    char * dev = pdata->device_name;
    g_print("AUDIO: Pulseaudio connecting to device %s\n\t (channels %d rate %d)\n", dev, ss.channels, ss.rate);
    r = pa_stream_connect_record(recordstream, dev, &bufattr, flags);
    if (r < 0)
    {
        g_print("AUDIO: skip latency adjustment\n");
        /* Old pulse audio servers don't like the ADJUST_LATENCY flag,
		 * so retry without that
		 */
        r = pa_stream_connect_record(recordstream, dev, &bufattr,
                                     PA_STREAM_INTERPOLATE_TIMING|
                                     PA_STREAM_AUTO_TIMING_UPDATE);
    }
    if (r < 0)
    {
        g_print("AUDIO: pa_stream_connect_record failed\n");
        finish(pa_ctx, pa_ml);
        return -1;
    }

    get_latency(recordstream);

    pdata->streaming=TRUE;

    /*
     * Iterate the main loop while streaming.  The second argument is whether
     * or not the iteration should block until something is ready to be
     * done.  Set it to zero for non-blocking.
     */
    while (pdata->capVid)
    {
        pa_mainloop_iterate(pa_ml, 1, NULL);
    }

    pa_stream_disconnect (recordstream);
    pa_stream_unref (recordstream);
    finish(pa_ctx, pa_ml);
    return 0;
}

/*
 * Launch the main loop iteration thread
 */
int
pulse_init_audio(struct paRecordData* pdata)
{
    /* start audio capture thread */
    if(__THREAD_CREATE(&pdata->pulse_read_th, (GThreadFunc) pulse_read_audio, pdata))
    {
        g_printerr("Pulse thread creation failed\n");
		pdata->streaming=FALSE;

        return (-1);
    }

    return 0;
}

/*
 * join the main loop iteration thread
 */
int
pulse_close_audio(struct paRecordData* pdata)
{
	__THREAD_JOIN( pdata->pulse_read_th );
    g_print("AUDIO: pulse read thread joined\n");
    return 0;
}

#endif
