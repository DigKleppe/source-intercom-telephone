/*
 * textThread.c
 *
 *  Created on: Feb 17, 2019
 *      Author: dig
 */


/*
gst-launch-1.0 -v videotestsrc ! textoverlay text="Room A \n Room B" valignment=top halignment=left font-desc="Sans, 72" ! autovideosink


gst-launch-1.0 textoverlay name=overlay ! videoconvert ! videoscale ! autovideosink. videotestsrc  !  videoconvert ! overlay.video_sink ! udpsrc port=5050 ! subparse ! overlay.text_sink

gst-launch-1.0 -v filesrc location=subtitles.srt ! subparse ! txt.   videotestsrc ! timeoverlay ! textoverlay name=txt shaded-background=yes ! autovideosink

gst-launch-1.0 -v udpsrc port=5050 ! text/x-raw,format=utf8 ! txt.  videotestsrc ! textoverlay name=txt shaded-background=yes ! autovideosink

gst-launch-1.0 -v filesrc location=subtitles.srt ! text/x-raw,format=utf8 ! txt.  videotestsrc ! textoverlay name=txt shaded-background=yes ! autovideosink

gst-launch-1.0 -v videotestsrc ! textoverlay name=txt font-desc="Sans, 72" ! autovideosink. udpsrc port=5050 ! text/x-raw ! txt


gst-launch-1.0 -v filesrc location=subtitles.srt  ! text/x-raw,format=utf8 ! udpsink port=5050

*/
