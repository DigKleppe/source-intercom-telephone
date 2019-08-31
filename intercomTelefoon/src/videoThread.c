/*
 * video.c
 *
 *  Created on: Jan 27, 2019
 *      Author: dig
 */

#include <gst/gst.h>
#include "telefoon.h"
#include "videoThread.h"
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>


gboolean link_elements_with_filter (GstElement *element1, GstElement *element2, GstCaps *caps)
{
	gboolean link_ok;
	link_ok = gst_element_link_filtered (element1, element2, caps);
	gst_caps_unref (caps);

	if (!link_ok) {
		g_warning ("Filter failed to link element1 and element2!");
	}
	return link_ok;
}


//gst-launch-1.0 v4l2src device=/dev/video0 ! image/jpeg,framerate=30/1,width=800,height=600 ! rtpjpegpay ! udpsink port=5001 host=192.168.2.255
//gst-launch-1.0 videotestsrc pattern=zone-plate kx2=20 ky2=20 kt=1 ! video/x-raw,width=800,height=600 ! jpegenc ! rtpjpegpay ! udpsink port=5001 host=192.168.2.255


//gst-launch-1.0 -v udpsrc port=5001 ! application/x-rtp,encoding-name=JPEG,payload=26 ! rtpjpegdepay ! jpegdec ! videoconvert !  videoflip method=counterclockwise ! nxvideosink
//gst-launch-1.0 -v udpsrc port=5001 ! application/x-rtp,encoding-name=JPEG,payload=26 ! rtpjpegdepay ! jpegdec ! videoconvert !  autovideosink

void* videoThread(void* args)
{
	GstElement *videopipeline, *videoSource, *rtpjpegdepay,  *jpegdec ,*videoconvert , *videoflip,*videosink;
	GstElement *jpegenc; // used for testscreen

	GstBus *bus;
	GstMessage *msg;
	GstCaps *caps;
	GstStateChangeReturn ret;
	bool stop = false;
	threadStatus_t  * pThreadStatus = args;
	floor_t  floorID;
	floor_t * p = ( floor_t *) (pThreadStatus->info);
	floorID = * p;

	printf("videoThread started\n");

	pThreadStatus->mustStop = false;
	pThreadStatus->isRunning = true;

	videopipeline = gst_pipeline_new ("videopipeline");
	videoSource = gst_element_factory_make( "udpsrc","udpsrc");

	if ( floorID == BASE_FLOOR)  // used to receive from station 1 or 2
		g_object_set (videoSource, "port", VIDEOPORT1, NULL);
	else
		g_object_set (videoSource, "port", VIDEOPORT2, NULL);

	rtpjpegdepay = gst_element_factory_make("rtpjpegdepay","rtpjpegdepay");
	jpegdec = gst_element_factory_make("jpegdec","jpegdec");
	videoconvert = gst_element_factory_make("videoconvert","videoconvert");
	videoflip = gst_element_factory_make("videoflip","videoflip");

	g_object_set (videoflip,"method", 3,NULL);

	videosink = gst_element_factory_make ("nxvideosink", "nxvideosink");

	if (!videopipeline || !videoSource || !rtpjpegdepay || !jpegdec  || !videoconvert ||!videoflip || !videosink ) {
		g_printerr ("Not all elements could be created.\n");
		stop = true;
	}
	if (!stop) {
		/* Build the pipeline */
		gst_bin_add_many (GST_BIN (videopipeline),videoSource,rtpjpegdepay, jpegdec, videoconvert, videoflip, videosink, NULL);

		caps = gst_caps_new_simple ("application/x-rtp",
				//	"framerate",GST_TYPE_FRACTION, 30, 1,
				"encoding-name", G_TYPE_STRING, "JPEG",
				"payload", G_TYPE_INT, 26,
				NULL);
		if (link_elements_with_filter (videoSource, rtpjpegdepay, caps ) != TRUE) {
			g_printerr ("rtpjpegdepay elements could not be linked.\n");
			stop = true;
		}

		if (gst_element_link (rtpjpegdepay , jpegdec) != TRUE) {
			g_printerr ("jpegenc and videopayloader could not be linked.\n");
			stop = true;
		}

		if (gst_element_link (jpegdec , videoconvert) != TRUE) {
			g_printerr ("jpegenc and videopayloader could not be linked.\n");
			stop = true;
		}

		if (gst_element_link (videoconvert , videoflip) != TRUE) {
			g_printerr ("jpegenc and videopayloader could not be linked.\n");
			stop = true;
		}

		if (gst_element_link (videoflip , videosink) != TRUE) {
			g_printerr ("jpegenc and videopayloader could not be linked.\n");
			stop = true;
		}

	}

	if ( !stop ) {
		/* Start playing */
		ret = gst_element_set_state (videopipeline, GST_STATE_PLAYING);
		if (ret == GST_STATE_CHANGE_FAILURE) {
			g_printerr ("Unable to set the pipeline to the playing state.\n");
			//	gst_object_unref (videopipeline);
			//	return -1;
			stop = true;		}
		/* Wait until error or EOS */
	}
	while (!stop ) {
		usleep(100000);
		bus = gst_element_get_bus (videopipeline);
		//	msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
		//msg = gst_bus_timed_pop_filtered (bus, TEST_INTERVAL, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
		msg = gst_bus_pop (bus );
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
				g_print ("video End-Of-Stream reached.\n");
				stop = true;
				break;
			default:
				/* We should not reach here because we only asked for ERRORs and EOS */
				//		g_printerr ("Unexpected message received.\n");
				break;
			}
			gst_message_unref (msg);
		}
		if ( pThreadStatus->mustStop){
			stop = true;
		}
	}

	printf("videoThread stopped\n");
	/* Free resources */
	gst_object_unref (bus);
	gst_element_set_state (videopipeline, GST_STATE_NULL);
	gst_object_unref(videopipeline);
	pThreadStatus->isRunning = false;
	pthread_exit(args);
	return (void *) NULL;
}

/*
gst-launch-1.0 videotestsrc ! textoverlay text="Room A \n Room B" valignment=top halignment=left font-desc="Sans, 72" ! video/x-raw,width=800,height=480 ! videoflip method=counterclockwise ! nxvideosink
gst-launch-1.0 videotestsrc pattern=zone-plate kx2=20 ky2=20 kt=1 ! video/x-raw,width=800,height=480 ! videoflip method=counterclockwise ! nxvideosink
*/

// puts a message on screen

void* messageThread(void* args){

	GstElement *videopipeline, *videoSource,*videoconvert, *videosink , *videoflip, *textoverlay;
	GstCaps *caps;
	GstBus *bus;
	GstMessage *msg;
	bool stop = false;
	GstStateChangeReturn ret;
	threadStatus_t  * pThreadStatus = args;
	char * pText =  (char *) pThreadStatus->info;
	char textBuffer[100];


	printf("messageThread started\n");

	if ( strlen (pText ) < sizeof (textBuffer))
		strcpy( textBuffer, pText);
	else {
		strncpy( textBuffer, pText, sizeof (textBuffer-1));
		textBuffer[sizeof (textBuffer)-1] = 0;
	}

	pThreadStatus->mustStop = false;
	pThreadStatus->isRunning = true;

	videopipeline = gst_pipeline_new ("messageVideoPipeline");

	videoSource = gst_element_factory_make ("videotestsrc", "videoSource");

	g_object_set (G_OBJECT (videoSource), "pattern",GST_VIDEO_TEST_SRC_BLUE ,NULL);

	textoverlay = gst_element_factory_make("textoverlay", "textoverlay" );
	g_object_set (G_OBJECT (textoverlay), "valignment",GST_BASE_TEXT_OVERLAY_VALIGN_TOP,NULL);
	g_object_set (G_OBJECT (textoverlay), "halignment",GST_BASE_TEXT_OVERLAY_HALIGN_LEFT,NULL);

	g_object_set (G_OBJECT (textoverlay), "font-desc","Sans, 30",NULL);
	g_object_set (G_OBJECT (textoverlay), "text",pText,NULL);

	videoflip = gst_element_factory_make("videoflip", "videoflip");
	g_object_set (G_OBJECT (videoflip), "method",GST_VIDEO_FLIP_METHOD_90L ,NULL);
	videosink = gst_element_factory_make ("nxvideosink", "nxvideosink");
	gst_bin_add_many (GST_BIN (videopipeline),videoSource,textoverlay, videoflip, videosink, NULL);
	caps = gst_caps_new_simple ("video/x-raw",
			//	"framerate",GST_TYPE_FRACTION, 30, 1,
			"width", G_TYPE_INT, 800,
			"height", G_TYPE_INT, 480,
			NULL);
	if (link_elements_with_filter (videoSource, textoverlay, caps ) != TRUE)
		stop = true;
	if (gst_element_link (textoverlay , videoflip) != TRUE)
		stop = true;
	if (gst_element_link (videoflip , videosink) != TRUE)
		stop = true;

	usleep(1000);  // delay, else segm fault?

	ret = gst_element_set_state (videopipeline, GST_STATE_PLAYING);

	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_printerr ("Unable to set the video pipeline to the playing state.\n");
		stop= true;
	}

	while (!stop ) {
		usleep(100000);
		if ( strcmp (pText, textBuffer) != 0 ){ // text changed ?
			gst_element_set_state (videopipeline, GST_STATE_NULL); // stop
			g_object_set (G_OBJECT (textoverlay), "text",pText,NULL);
			ret = gst_element_set_state (videopipeline, GST_STATE_PLAYING);
			if (ret == GST_STATE_CHANGE_FAILURE) {
				g_printerr ("Unable to set the video pipeline to the playing state.\n");
				stop = true;
			}
			if ( strlen (pText ) < sizeof (textBuffer))
				strcpy( textBuffer, pText);
			else {
				strncpy( textBuffer, pText, sizeof (textBuffer-1));
				textBuffer[sizeof (textBuffer)-1] = 0;
			}
		}

		bus = gst_element_get_bus (videopipeline);
		//	msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
		//msg = gst_bus_timed_pop_filtered (bus, TEST_INTERVAL, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
		msg = gst_bus_pop (bus );
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
				break;
			}
			gst_message_unref (msg);
		}
		if ( pThreadStatus->mustStop){
			stop = true;
		}
	}

	printf("messageThread stopped\n");
	/* Free resources */
	gst_object_unref (bus);
	gst_element_set_state (videopipeline, GST_STATE_NULL);
	gst_object_unref(videopipeline);
	pThreadStatus->isRunning = false;
	pthread_exit(args);
	return (void *) NULL;

}
