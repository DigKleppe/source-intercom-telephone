/*
 * audioReceiveThread.c
 *
 *  Created on: Apr 28, 2019
 *      Author: dig
 */

// balance:
//gst-launch-1.0 audiotestsrc wave=saw ! audioconvert ! audiopanorama panorama=-1.00 ! audioconvert ! alsasink

#include "telefoon.h"
#include "videoThread.h"

#include <stdbool.h>
#include <gst/gst.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#define HANDSETVOLUME 1.0
/*
logitec: naam van arecord -L
gst-launch-1.0 -v alsasrc device=hw:3 ! audioconvert ! "audio/x-raw,rate=24000" ! audioresample ! alsasink
gst-launch-1.0 -v alsasrc device=hw:3 ! audioconvert ! "audio/x-raw,rate=24000" ! audioresample ! alsasink device=hw:2
gst-launch-1.0 -v alsasrc device=hw:3 ! audioconvert ! "audio/x-raw,rate=24000" ! audioresample ! vorbisenc ! rtpvorbispay config-interval=1 ! rtpstreampay ! udpsink port=5001 host=192.168.2.255

transmitter side station1:
gst-launch-1.0 -v alsasrc device=hw:1 ! audioconvert ! "audio/x-raw,rate=24000" ! audioresample ! vorbisenc ! rtpvorbispay config-interval=1 ! rtpstreampay ! udpsink port=5002 host=192.168.2.255

receiver side:
gst-launch-1.0 udpsrc port=5002 do-timestamp=true ! "application/x-rtp-stream,media=audio,clock-rate=24000,encoding-name=VORBIS" ! rtpstreamdepay ! rtpvorbisdepay ! decodebin ! audioconvert ! audioresample ! alsasink device=hw:1

gst-launch-1.0 udpsrc port=5002 do-timestamp=true ! "application/x-rtp-stream,media=audio,clock-rate=24000,encoding-name=VORBIS,configuration=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" ! rtpstreamdepay ! rtpvorbisdepay ! decodebin ! audioconvert ! audioresample ! alsasink device=hw:1
gst-launch-1.0 udpsrc port=5002 ! "application/x-rtp-stream,media=audio,clock-rate=24000,encoding-name=VORBIS,configuration=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" ! rtpstreamdepay ! rtpvorbisdepay ! decodebin ! audioconvert ! audioresample ! alsasink device=hw:1
arecord --list-devices
arecord -L
alsamixer -c 1
sudo alsactl store
aplay -l

gst-launch-1.0 -v audiotestsrc ! audioconvert ! "audio/x-raw,rate=44100" ! rtpL16pay ! udpsink host=192.168.2.255 port=5002
gst-launch-1.0 -v udpsrc port=5002 ! "application/x-rtp,media=(string)audio, clock-rate=(int)444100, width=16, height=16, encoding-name=(string)L16, encoding-params=(string)1, channels=(int)1, channel-positions=(int)1, payload=(int)96" ! rtpL16depay ! audioconvert ! alsasink device=hw:1,0 sync=false &

gst-launch-1.0 -v audiotestsrc ! audioconvert ! "audio/x-raw,rate=44100" ! rtpL16pay ! udpsink host=192.168.2.255 port=5002

gst-launch-1.0 -v audiotestsrc ! audioconvert ! "audio/x-raw,rate=44100,encoding-name=S16BE" ! rtpL16pay ! udpsink host=192.168.2.255 port=5002
Setting pipeline to PAUSED ...
Pipeline is PREROLLING ...
/GstPipeline:pipeline0/GstAudioTestSrc:audiotestsrc0.GstPad:src: caps = "audio/x-raw\,\ layout\=\(string\)interleaved\,\ rate\=\(int\)44100\,\ format\=\(string\)S16BE\,\ channels\=\(int\)1"
/GstPipeline:pipeline0/GstAudioConvert:audioconvert0.GstPad:src: caps = "audio/x-raw\,\ layout\=\(string\)interleaved\,\ rate\=\(int\)44100\,\ format\=\(string\)S16BE\,\ channels\=\(int\)1"
/GstPipeline:pipeline0/GstCapsFilter:capsfilter0.GstPad:src: caps = "audio/x-raw\,\ layout\=\(string\)interleaved\,\ rate\=\(int\)44100\,\ format\=\(string\)S16BE\,\ channels\=\(int\)1"
/GstPipeline:pipeline0/GstRtpL16Pay:rtpl16pay0.GstPad:src: caps = "application/x-rtp\,\ media\=\(string\)audio\,\ clock-rate\=\(int\)44100\,\ encoding-name\=\(string\)L16\,\ encoding-params\=\(string\)1\,\ channels\=\(int\)1\,\ payload\=\(int\)96\,\ ssrc\=\(uint\)1900555378\,\ timestamp-offset\=\(uint\)3541881202\,\ seqnum-offset\=\(uint\)26844"
/GstPipeline:pipeline0/GstUDPSink:udpsink0.GstPad:sink: caps = "application/x-rtp\,\ media\=\(string\)audio\,\ clock-rate\=\(int\)44100\,\ encoding-name\=\(string\)L16\,\ encoding-params\=\(string\)1\,\ channels\=\(int\)1\,\ payload\=\(int\)96\,\ ssrc\=\(uint\)1900555378\,\ timestamp-offset\=\(uint\)3541881202\,\ seqnum-offset\=\(uint\)26844"
/GstPipeline:pipeline0/GstRtpL16Pay:rtpl16pay0.GstPad:sink: caps = "audio/x-raw\,\ layout\=\(string\)interleaved\,\ rate\=\(int\)44100\,\ format\=\(string\)S16BE\,\ channels\=\(int\)1"
/GstPipeline:pipeline0/GstCapsFilter:capsfilter0.GstPad:sink: caps = "audio/x-raw\,\ layout\=\(string\)interleaved\,\ rate\=\(int\)44100\,\ format\=\(string\)S16BE\,\ channels\=\(int\)1"
/GstPipeline:pipeline0/GstAudioConvert:audioconvert0.GstPad:sink: caps = "audio/x-raw\,\ layout\=\(string\)interleaved\,\ rate\=\(int\)44100\,\ format\=\(string\)S16BE\,\ channels\=\(int\)1"
/GstPipeline:pipeline0/GstRtpL16Pay:rtpl16pay0: timestamp = 3541881202
/GstPipeline:pipeline0/GstRtpL16Pay:rtpl16pay0: seqnum = 26844


port 5002 van camera

gst-launch-1.0 -v audiotestsrc freq=1000 ! audioconvert ! "audio/x-raw,rate=24000" ! rtpL16pay ! udpsink host=192.168.2.255 port=5002
gst-launch-1.0 -v alsasrc device=hw:1 ! audioconvert ! "audio/x-raw,rate=24000" ! rtpL16pay ! udpsink host=192.168.2.255 port=5002


gst-launch-1.0 -v audiotestsrc ! audioconvert ! "audio/x-raw,rate=441000" ! rtpL16pay ! udpsink host=192.168.2.255 port=5004
gst-launch-1.0 udpsrc port=5002 caps='application/x-rtp, media=(string)audio, clock-rate=(int)24000, encoding-name=(string)L16, encoding-params=(string)1, channels=(int)1, payload=(int)96' ! rtpL16depay ! audioconvert ! audioresample ! audiopanorama panorama=-1 ! alsasink device=hw:1
gst-launch-1.0 udpsrc port=5002 caps='application/x-rtp, media=(string)audio, clock-rate=(int)44100, encoding-name=(string)L16, encoding-params=(string)1, channels=(int)2, payload=(int)96' ! rtpL16depay ! audioconvert ! audioresample ! audiopanorama panorama=-1 !  alsasink device=hw:1

gst-launch-1.0 udpsrc port=5002 ! "application/x-rtp, media=(string)audio, clock-rate=(int)24000, encoding-name=(string)L16, encoding-params=(string)1, channels=(int)2, payload=(int)96" ! rtpL16depay ! audioconvert ! audioresample ! audiopanorama panorama=-1 ! autoaudiosink
 */


void* audioReceiveThread(void* args) {

//	GstElement *audiopipeline,  *audioSource, *rtpstreamdepay, *rtpvorbisdepay;
//	GstElement * decodebin , *audioconvert, * audioresample ,*audiosink;

	GstElement *audiopipeline,  *audioSource, *rtpL16depay, *audioconvert, * audioresample ,* audiopanorama, *volume , *audiosink;

	GstBus *bus = NULL;
	GstMessage *msg;
	GstCaps *caps = NULL;
	GstStateChangeReturn ret;
	threadStatus_t  * pThreadStatus = args;
	pThreadStatus->mustStop = false;
	pThreadStatus->isRunning = true;
	char devicename[20];
	bool stop = false;
	floor_t  floorID= 1;

	floor_t * p = ( floor_t *) (pThreadStatus->info);
	floorID = * p;

	printf("audioRxthread started\n");

	audioSource = gst_element_factory_make ("udpsrc", "udpsrc");


	if (floorID == BASE_FLOOR)
		g_object_set (audioSource, "port", AUDIO_RX_PORT1, NULL);
	else
		g_object_set (audioSource, "port", AUDIO_RX_PORT2, NULL);

	//g_object_set (audioSource, "do-timestamp", TRUE, NULL);

	rtpL16depay = gst_element_factory_make ("rtpL16depay", "rtpL16depay");

	audioconvert = gst_element_factory_make ("audioconvert", "audioconvert");
	audioresample = gst_element_factory_make ("audioresample", "audioresample");
	audiopanorama = gst_element_factory_make ("audiopanorama", "audiopanorama");
	g_object_set (audiopanorama,"panorama",HANDSETCHANNEL,NULL);  // sound to handset

	volume = gst_element_factory_make ("volume", "volume");
	g_object_set (volume,"volume",HANDSETVOLUME,NULL);

	audiosink = gst_element_factory_make ("alsasink", "alsasink");
	sprintf(devicename, "hw:%d",pThreadStatus->cardno);
	g_object_set (audiosink,"device",devicename,NULL);

	if (!audioSource || !rtpL16depay ||!audioconvert || !audioresample || !audiopanorama || !audiosink || !volume) {
		g_printerr ("Not all audio elements could be created.\n");
		stop = true;
	}

	audiopipeline = gst_pipeline_new ("receive audiopipeline");
	gst_bin_add_many (GST_BIN (audiopipeline), audioSource, rtpL16depay,
			audioconvert, audioresample, volume, audiopanorama, audiosink ,NULL);

//application/x-rtp, media=(string)audio, clock-rate=(int)24000, encoding-name=(string)L16, encoding-params=(string)1, channels=(int)1, payload=(int)96

	caps = gst_caps_new_simple ("application/x-rtp",
			"media", G_TYPE_STRING,"audio",
		//	"clock-rate", G_TYPE_INT,24000, // logitec
			"clock-rate", G_TYPE_INT,44100, // USB sound
			"encoding-name",G_TYPE_STRING,"L16",
			"encoding-params",(G_TYPE_STRING),"1",
			"channels", G_TYPE_INT, 1,
			"payload", G_TYPE_INT,96,
			NULL);

	if (link_elements_with_filter (audioSource, rtpL16depay, caps) != TRUE)
		stop = true;

	if (gst_element_link (rtpL16depay, audioconvert  ) != TRUE)
		stop = true;

	if (gst_element_link (audioconvert, audioresample  ) != TRUE)
		stop = true;

	if (gst_element_link (audioresample, volume  ) != TRUE)
			stop = true;

	if (gst_element_link (volume, audiopanorama  ) != TRUE)
		stop = true;

	if (gst_element_link (audiopanorama, audiosink  ) != TRUE)
		stop = true;


	if (stop)
		g_printerr ("Elements could not be linked.\n");
	else {
		ret = gst_element_set_state (audiopipeline, GST_STATE_PLAYING);
		if (ret == GST_STATE_CHANGE_FAILURE) {
			g_printerr (" audio Receive Unable to set the audio pipeline to the playing state.\n");
			stop = true;
		}
	}

	usleep(TEST_INTERVAL * 1000);

	while(!stop ) {
		usleep(TEST_INTERVAL * 1000);
		if ( pThreadStatus->mustStop)
			stop = true;
		/* Wait until error or EOS */
		bus = gst_element_get_bus (audiopipeline);
		msg = gst_bus_pop(bus);
		/* Parse message */
		if (msg != NULL) {
			GError *err;
			gchar *debug_info;
			switch (GST_MESSAGE_TYPE (msg)) {
			case GST_MESSAGE_ERROR:
				gst_message_parse_error (msg, &err, &debug_info);
				g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
				g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
				g_clear_error (&err);
				g_free (debug_info);
				stop = true;
				break;
			case GST_MESSAGE_EOS:
				g_print ("End-Of-Stream reached.\n");
				stop = true;
				break;
			default:
				/* We should not reach here because we only asked for ERRORs and EOS */
			//	g_printerr ("Unexpected message received.\n");
				break;
			}
			gst_message_unref (msg);

		}
	}
	printf("audioRxthread stopped\n");
	gst_element_set_state (audiopipeline, GST_STATE_NULL);
	if ( bus)
		gst_object_unref (bus);
//	if ( caps)
//		gst_object_unref (caps);
	if (audiopipeline)
		gst_object_unref(audiopipeline);

	pThreadStatus->isRunning = false;
	pthread_exit(args);
	return ( NULL);
}


