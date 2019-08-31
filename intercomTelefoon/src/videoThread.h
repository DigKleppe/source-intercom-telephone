/*
 * videoThread.h
 *
 *  Created on: Jan 27, 2019
 *      Author: dig
 */

#ifndef VIDEOTHREAD_H_
#define VIDEOTHREAD_H_

#include <gst/gst.h>

void* videoThread(void* args);
void* messageThread(void* args);

gboolean link_elements_with_filter (GstElement *element1, GstElement *element2, GstCaps *caps);


typedef enum {
  GST_VIDEO_FLIP_METHOD_IDENTITY,
  GST_VIDEO_FLIP_METHOD_90R,
  GST_VIDEO_FLIP_METHOD_180,
  GST_VIDEO_FLIP_METHOD_90L,
  GST_VIDEO_FLIP_METHOD_HORIZ,
  GST_VIDEO_FLIP_METHOD_VERT,
  GST_VIDEO_FLIP_METHOD_TRANS,
  GST_VIDEO_FLIP_METHOD_OTHER
} GstVideoFlipMethod;

typedef enum {
    GST_BASE_TEXT_OVERLAY_VALIGN_BASELINE,
    GST_BASE_TEXT_OVERLAY_VALIGN_BOTTOM,
    GST_BASE_TEXT_OVERLAY_VALIGN_TOP,
    GST_BASE_TEXT_OVERLAY_VALIGN_POS,
    GST_BASE_TEXT_OVERLAY_VALIGN_CENTER,
    GST_BASE_TEXT_OVERLAY_VALIGN_ABSOLUTE
} GstBaseTextOverlayVAlign;

typedef enum {
    GST_BASE_TEXT_OVERLAY_HALIGN_LEFT,
    GST_BASE_TEXT_OVERLAY_HALIGN_CENTER,
    GST_BASE_TEXT_OVERLAY_HALIGN_RIGHT,
    GST_BASE_TEXT_OVERLAY_HALIGN_UNUSED,
    GST_BASE_TEXT_OVERLAY_HALIGN_POS,
    GST_BASE_TEXT_OVERLAY_HALIGN_ABSOLUTE
} GstBaseTextOverlayHAlign;


typedef enum {
  GST_VIDEO_TEST_SRC_SMPTE,
  GST_VIDEO_TEST_SRC_SNOW,
  GST_VIDEO_TEST_SRC_BLACK,
  GST_VIDEO_TEST_SRC_WHITE,
  GST_VIDEO_TEST_SRC_RED,
  GST_VIDEO_TEST_SRC_GREEN,
  GST_VIDEO_TEST_SRC_BLUE,
  GST_VIDEO_TEST_SRC_CHECKERS1,
  GST_VIDEO_TEST_SRC_CHECKERS2,
  GST_VIDEO_TEST_SRC_CHECKERS4,
  GST_VIDEO_TEST_SRC_CHECKERS8,
  GST_VIDEO_TEST_SRC_CIRCULAR,
  GST_VIDEO_TEST_SRC_BLINK,
  GST_VIDEO_TEST_SRC_SMPTE75,
  GST_VIDEO_TEST_SRC_ZONE_PLATE,
  GST_VIDEO_TEST_SRC_GAMUT,
  GST_VIDEO_TEST_SRC_CHROMA_ZONE_PLATE,
  GST_VIDEO_TEST_SRC_SOLID,
  GST_VIDEO_TEST_SRC_BALL,
  GST_VIDEO_TEST_SRC_SMPTE100,
  GST_VIDEO_TEST_SRC_BAR
} GstVideoTestSrcPattern;

#endif /* VIDEOTHREAD_H_ */
