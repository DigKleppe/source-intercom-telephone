/*
 * ringThread.c
 *
 *  Created on: May 5, 2019
 *      Author: dig
 */

#include "telefoon.h"
#include "keys.h"
#include "connections.h"

#include <gst/gst.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include "videoThread.h"



void* ringThread(void* args){
	GstElement *audiopipeline,  *audioSource, *mpegaudioparser,* mpg123audiodec, *audioconvert;
	GstElement *volume , *audiopanorama ,*audiosink ;
	GstBus *bus;
	GstMessage *msg;
	bool stop = false;
	GstStateChangeReturn ret;
	threadStatus_t  * pThreadStatus = args;
	float oldRingVolume = ringVolume;

	pThreadStatus->mustStop = false;
	pThreadStatus->isRunning = true;

	if ( pThreadStatus->cardno == -1 ) // no USB audiocard found
		return ( NULL);

	audiopipeline = gst_pipeline_new ("audiopipeline");

	audioSource = gst_element_factory_make ("filesrc", "filesrc");
	g_object_set (G_OBJECT (audioSource), "location",(gchar *) ringTone,NULL);

	mpegaudioparser = gst_element_factory_make ("mpegaudioparse", "mpegaudioparse");
	mpg123audiodec = gst_element_factory_make ("mpg123audiodec", "mpg123audiodec");

	volume = gst_element_factory_make ("volume", "volume");
	g_object_set (G_OBJECT (volume), "volume",ringVolume/10.0,NULL);

	audioconvert = gst_element_factory_make ("audioconvert", "audioconvert");

	audiopanorama = gst_element_factory_make ("audiopanorama", "audiopanorama"); // balance
	g_object_set (G_OBJECT (audiopanorama), "panorama",SPEAKERCHANNEL,NULL);

	audiosink = gst_element_factory_make ("alsasink", "alsasink");
	g_object_set (G_OBJECT (audiosink), "device","hw:1",NULL);

	if (!audiopipeline || !audioconvert || !volume || !audiopanorama || !audiosink) {
		g_printerr ("Not all audio elements could be created.\n");
		stop = true;
	}
	if (!audioSource  || !mpegaudioparser || !mpg123audiodec ){
		g_printerr ("Not all audio elements could be created.\n");
		stop = true;
	}

	gst_bin_add_many (GST_BIN (audiopipeline), audioSource,mpegaudioparser,mpg123audiodec, volume, audioconvert, audiopanorama, audiosink ,NULL);

	if (gst_element_link (audioSource, mpegaudioparser  ) != TRUE)
		stop = true;

	if (gst_element_link (mpegaudioparser, mpg123audiodec ) != TRUE)
		stop = true;

	if (gst_element_link (mpg123audiodec, volume) != TRUE)
		stop = true;

	if (gst_element_link (volume, audioconvert) != TRUE)
		stop = true;

	if (gst_element_link (audioconvert, audiopanorama) != TRUE)
		stop = true;

	if (gst_element_link (audiopanorama, audiosink) != TRUE)
		stop = true;

	ret = gst_element_set_state (audiopipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_printerr ("Unable to set the audio pipeline to the playing state.\n");
		stop = true;
	}
	if (stop) {
		gst_object_unref(audiopipeline);
		return NULL;
	}

	while( !pThreadStatus->mustStop && !stop ) {
		usleep(10 * 1000);
		msg = NULL;
			/* Wait until error or EOS */
		bus = gst_element_get_bus (audiopipeline);
			//	msg = gst_bus_timed_pop_filtered (bus, TEST_INTERVAL2  * 1000 * 1000LL, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
		msg = gst_bus_pop (bus);
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
			//	gst_element_set_state (audiopipeline, GST_STATE_PLAYING); // start again
				break;
			default:
				//	g_printerr ("Unexpected message received.\n");
				break;
			}
			gst_message_unref (msg);
		}
		if ( pThreadStatus->mustStop )
			stop = true;

		if(!(oldRingVolume == ringVolume)) { // then ringvolume changed
			gst_element_set_state (audiopipeline, GST_STATE_NULL);
			g_object_set (G_OBJECT (volume), "volume",ringVolume/10.0 ,NULL);
			ret = gst_element_set_state (audiopipeline, GST_STATE_PLAYING);
			oldRingVolume = ringVolume;
		}
	}
	gst_element_set_state (audiopipeline, GST_STATE_NULL);
	gst_object_unref(audiopipeline);
	gst_object_unref (bus);
	pThreadStatus->isRunning = false;
	pthread_exit(args);
	return ( NULL);
}



