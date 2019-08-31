/*
 * audioThread.c
 *
 *  Created on: Jan 27, 2019
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
/*
logitec: naam van arecord -L
gst-launch-1.0 -v alsasrc device=hw:3 ! audioconvert ! "audio/x-raw,rate=24000" ! audioresample ! alsasink
gst-launch-1.0 -v alsasrc device=hw:3 ! audioconvert ! "audio/x-raw,rate=24000" ! audioresample ! alsasink device=hw:2
gst-launch-1.0 -v alsasrc device=hw:1 ! audioconvert ! "audio/x-raw,rate=44100" ! audioresample ! vorbisenc ! rtpvorbispay config-interval=1 ! rtpstreampay ! udpsink port=5004 host=192.168.2.255

gst-launch-1.0 -v alsasrc device=hw:1 ! audioconvert ! "audio/x-raw,rate=44100,format=(string)S16BE" ! rtpL16pay  ! udpsink host=192.168.2.255 port=5004
gst-launch-1.0 -v alsasrc device=hw:1 ! audioconvert ! "audio/x-raw,rate=44100,format=(string)S16BE" ! volume volume=2 ! audioconvert ! rtpL16pay  ! udpsink host=192.168.2.255 port=5004
gst-launch-1.0 -v alsasrc device=hw:2 ! volume volume=2 ! audioconvert ! "audio/x-raw,rate=44100" ! rtpL16pay  ! udpsink host=192.168.2.255 port=5004

gst-launch-1.0 -v alsasrc device=hw:2 ! volume volume=1 ! audioconvert ! "audio/x-raw,rate=44100" ! audioresample ! alsasink device=hw:2

gst-launch-1.0 -v alsasrc device=hw:1 ! audioconvert ! "audio/x-raw,rate=44100" ! alsasink device=hw:1

gst-launch-1.0 -v audiotestsrc freq=1000 ! audioconvert ! "audio/x-raw,rate=44100,format=(string)S16BE" ! rtpL16pay  ! udpsink host=192.168.2.255 port=5004
arecord --list-devices
arecord -L
alsamixer -c 1


gst-inspect-1.0 alsasrc
 */

void* audioTransmitThread(void* args) {

	GstElement *audiopipeline,  *audioSource, *audioconvert,* rtpL16pay, *audiosink;
	GstBus *bus;
	GstCaps *caps = NULL;
	GstMessage *msg;
	GstStateChangeReturn ret;
	threadStatus_t  * pThreadStatus = args;
	pThreadStatus->mustStop = false;
	pThreadStatus->isRunning = true;
	char devicename[20];
	bool stop = false;

	floor_t * p = ( floor_t *) (pThreadStatus->info);
	floor_t floorID = * p;

	printf("audioTxthread started\n");

//	audioSource = gst_element_factory_make ("audiotestsrc", "audioSource");

	audioSource = gst_element_factory_make ("alsasrc", "audioSource");
	sprintf(devicename, "hw:%d",pThreadStatus->cardno);
	g_object_set (audioSource,"device",devicename,NULL);

	audioconvert = gst_element_factory_make ("audioconvert", "audioconvert");
	rtpL16pay = gst_element_factory_make ("rtpL16pay", "rtpL16pay");
	audiosink = gst_element_factory_make ("udpsink", "udpsink");

	if (floorID == BASE_FLOOR )  // transmit to base of firstfloor camera
		g_object_set (audiosink, "port", AUDIO_TX_PORT1, NULL);
	else
		g_object_set (audiosink, "port", AUDIO_TX_PORT2, NULL);

	g_object_set (audiosink, "host", HOST, NULL );  // broadcast

	if (!audioSource || !audioconvert || !rtpL16pay|| !audiosink ) {
		g_printerr ("Not all audio elements could be created.\n");
		stop = true;
	}

	audiopipeline = gst_pipeline_new ("transmit audiopipeline");
	gst_bin_add_many (GST_BIN (audiopipeline), audioSource, audioconvert, rtpL16pay, audiosink ,NULL);

	if (gst_element_link (audioSource, audioconvert  ) != TRUE) {
		g_printerr ("Elements could not be linked.\n");
		gst_object_unref (audiopipeline);
		stop = true;
	}
	caps = gst_caps_new_simple ("audio/x-raw",
		//	"rate", G_TYPE_INT,24000,  //
			"rate", G_TYPE_INT,44100,
//			"format", G_TYPE_STRING,"L16",
//			"endianness", G_TYPE_INT, 1234,
//			"signed", G_TYPE_BOOLEAN, TRUE,
//			"width", G_TYPE_INT, 16,
//			"depth", G_TYPE_INT, 16,
//			"channels", G_TYPE_INT, 1,
//			"rate", G_TYPE_INT, 44100,
			NULL);

	if (link_elements_with_filter (audioconvert, rtpL16pay,caps) != TRUE) {
		g_printerr ("Filter element could not be linked.\n");
		gst_object_unref (audiopipeline);
		stop = true;
	}

	if (gst_element_link (rtpL16pay, audiosink  ) != TRUE) {
		g_printerr ("Elements could not be linked.\n");
		gst_object_unref (audiopipeline);
		stop = true;
	}

	ret = gst_element_set_state (audiopipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_printerr ("AudioTransmit: Unable to set the audio pipeline to the playing state.\n");
		gst_object_unref (audiopipeline);
		stop = true;
	}

	while(!stop ) {
		usleep(TEST_INTERVAL * 1000);
		if ( pThreadStatus->mustStop)
			stop = true;

		/* Wait until error or EOS */
		bus = gst_element_get_bus (audiopipeline);
		msg = gst_bus_pop(bus); // , GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
		/* Parse message */
		if (msg != NULL) {
			GError *err;
			gchar *debug_info;
			switch (GST_MESSAGE_TYPE (msg)) {
			case GST_MESSAGE_ERROR:
				gst_message_parse_error (msg, &err, &debug_info);
				g_printerr ("AudioTransmit: Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
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
				//g_printerr ("Unexpected message received.\n");
				break;
			}
			gst_message_unref (msg);
		}
	}

	printf("audioTxthread stopped\n");
	gst_element_set_state (audiopipeline, GST_STATE_NULL);

	gst_object_unref (bus);
	gst_object_unref(audiopipeline);
	pThreadStatus->isRunning = false;
	pthread_exit(args);
	return ( NULL);
}
//
//
//
//
//
//void* audioTransmitThread(void* args) {
//
//	GstElement *audiopipeline= NULL,  *audioSource, *audioconvert,* audioresample;
//	GstElement * audioencoder,*audiorptpayloder, *audiostreampayloader,*audiosink;
//	GstBus *bus= NULL;
//	GstCaps *caps = NULL;
//	GstMessage *msg;
//	GstStateChangeReturn ret;
//	threadStatus_t  * pThreadStatus = args;
//	pThreadStatus->mustStop = false;
//	pThreadStatus->isRunning = true;
//	char devicename[20];
//	bool error = false;
//	if ( pThreadStatus->cardno == -1)
//		audioSource = gst_element_factory_make ("audiotestsrc", "audioSource");
//	else {
//		audioSource = gst_element_factory_make ("alsasrc", "audioSource");
//		sprintf(devicename, "hw:%d",pThreadStatus->cardno);
//		g_object_set (audioSource,"device",devicename,NULL);
//	}
//
//	audioconvert = gst_element_factory_make ("audioconvert", "audioconvert");
//	audioresample = gst_element_factory_make ("audioresample", "audioresample");
//	audioencoder = gst_element_factory_make ("vorbisenc", "audioencoder");
//	audiorptpayloder = gst_element_factory_make ("rtpvorbispay", "audiorptpayloder");
//	audiostreampayloader = gst_element_factory_make ("rtpstreampay", "audiostreampayloader");
//	audiosink = gst_element_factory_make ("udpsink", "udpsink");
//
//	g_object_set (audiorptpayloder, "config-interval", 10, NULL);
//	g_object_set (audiosink, "host", HOST, NULL );
//
//
//	if (* (int * ) pThreadStatus->info == 1 )
//		g_object_set (audiosink, "port", AUDIO_TX_PORT1, NULL);
//	else
//		g_object_set (audiosink, "port", AUDIO_TX_PORT2, NULL);
//
//	if (!audioSource || !audioconvert || !audioresample ||  !audioencoder || !audiorptpayloder
//			|| !audiostreampayloader  || !audiosink ) {
//		g_printerr ("Not all audio elements could be created.\n");
//		error = true;
//	}
//
//	audiopipeline = gst_pipeline_new ("transmit audiopipeline");
//	gst_bin_add_many (GST_BIN (audiopipeline), audioSource, audioconvert, audioresample, audioencoder,
//			audiorptpayloder, audiostreampayloader, audiosink ,NULL);
//
//	if (gst_element_link (audioSource, audioconvert  ) != TRUE) {
//		error = true;
//	}
//	caps = gst_caps_new_simple ("audio/x-raw",
//			"rate", G_TYPE_INT,24000,
//			NULL);
//
//	if (link_elements_with_filter (audioconvert, audioresample, caps) != TRUE)
//		error = true;
//
//	if (gst_element_link (audioresample, audioencoder  ) != TRUE)
//		error = true;
//
//	if (gst_element_link (audioencoder, audiorptpayloder  ) != TRUE)
//		error = true;
//
//	if (gst_element_link (audiorptpayloder, audiostreampayloader  ) != TRUE)
//		error = true;
//
//	if (gst_element_link (audiostreampayloader, audiosink  ) != TRUE)
//		error = true;
//
//	if ( error)
//		g_printerr ("Elements could not be linked.\n");
//	else {
//		ret = gst_element_set_state (audiopipeline, GST_STATE_PLAYING);
//		if (ret == GST_STATE_CHANGE_FAILURE) {
//			g_printerr ("Unable to set the audio pipeline to the playing state.\n");
//			error = true;
//		}
//	}
//
//	while(1 && !error ) {
//		usleep(TEST_INTERVAL * 1000);
//		/* Wait until error or EOS */
//		bus = gst_element_get_bus (audiopipeline);
////		msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
//		msg = gst_bus_pop(bus);
//		/* Parse message */
//		if (msg != NULL) {
//			GError *err;
//			gchar *debug_info;
//			switch (GST_MESSAGE_TYPE (msg)) {
//			case GST_MESSAGE_ERROR:
//				gst_message_parse_error (msg, &err, &debug_info);
//				g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
//				g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
//				g_clear_error (&err);
//				g_free (debug_info);
//				error = true;
//				break;
//			case GST_MESSAGE_EOS:
//				g_print ("End-Of-Stream reached.\n");
//				error = true;
//				break;
//			default:
//				/* We should not reach here because we only asked for ERRORs and EOS */
//			//	g_printerr ("Unexpected message received.\n");
//				break;
//			}
//			gst_message_unref (msg);
//
//		}
//	}
//	printf("audioTxthread stopped");
//	gst_element_set_state (audiopipeline, GST_STATE_NULL);
//	if ( bus)
//		gst_object_unref (bus);
//	if( caps)
//		gst_object_unref (caps);
//	if ( audiopipeline)
//		gst_object_unref(audiopipeline);
//	pThreadStatus->isRunning = false;
//	pthread_exit(args);
//	return ( NULL);
//}
//
//
