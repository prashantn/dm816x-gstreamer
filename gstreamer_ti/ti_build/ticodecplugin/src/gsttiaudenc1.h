/*
 * gsttiaudenc1.h
 *
 * This file declares the "TIAudenc1" element, which encodes an xDM 1.x audio
 * stream.
 *
 * Original Author:
 *     Chase Maupin, Texas Instruments, Inc.
 *
 * Based on TIAuddec1 from:
 *     Brijesh Singh, Texas Instruments, Inc.
 *
 * Copyright (C) 2008-2010 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation version 2.1 of the License.
 *
 * This program is distributed #as is# WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */

#ifndef __GST_TIAUDENC1_H__
#define __GST_TIAUDENC1_H__

#include <pthread.h>

#include <gst/gst.h>

#include <xdc/std.h>
#include <ti/sdo/ce/Engine.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/Fifo.h>
#include <ti/sdo/dmai/Rendezvous.h>
#include <ti/sdo/dmai/ce/Aenc1.h>

#include "gstticircbuffer.h"
#include "gsttidmaibuftab.h"

G_BEGIN_DECLS

/* Standard macros for maniuplating TIAudenc1 objects */
#define GST_TYPE_TIAUDENC1 \
  (gst_tiaudenc1_get_type())
#define GST_TIAUDENC1(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TIAUDENC1,GstTIAudenc1))
#define GST_TIAUDENC1_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TIAUDENC1,GstTIAudenc1Class))
#define GST_IS_TIAUDENC1(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TIAUDENC1))
#define GST_IS_TIAUDENC1_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TIAUDENC1))

typedef struct _GstTIAudenc1      GstTIAudenc1;
typedef struct _GstTIAudenc1Class GstTIAudenc1Class;

/* Defaults for audio encoder element which override the DMAI defaults
 * which are set too high for AAC-HE codec.
 */
#define TIAUDENC1_SAMPLEFREQ_DEFAULT 44100
#define TIAUDENC1_BITRATE_DEFAULT    64000

/* _GstTIAudenc1 object */
struct _GstTIAudenc1
{
  /* gStreamer infrastructure */
  GstElement     element;
  GstPad        *sinkpad;
  GstPad        *srcpad;

  /* Element properties */
  const gchar*   engineName;
  const gchar*   codecName;
  gboolean       displayBuffer;
  gboolean       genTimeStamps;

  /* Element state */
  Engine_Handle    hEngine;
  Aenc1_Handle     hAe;
  gint             channels;
  gint             bitrate;
  gint             samplefreq;
  gboolean         drainingEOS;
  pthread_mutex_t  threadStatusMutex;
  UInt32           threadStatus;

  /* Encode thread */
  pthread_t          encodeThread;
  Rendezvous_Handle  waitOnEncodeThread;
  Rendezvous_Handle  waitOnEncodeDrain;
  
  /* Buffer management */
  UInt32            numOutputBufs;
  GstTIDmaiBufTab  *hOutBufTab;
  GstTICircBuffer  *circBuf;

  /* AAC header (qtdemuxer) */
  GstBuffer       *aac_header_data;
};

/* _GstTIAudenc1Class object */
struct _GstTIAudenc1Class 
{
  GstElementClass parent_class;
};

/* External function enclarations */
GType gst_tiaudenc1_get_type(void);

G_END_DECLS

#endif /* __GST_TIAUDENC1_H__ */


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
