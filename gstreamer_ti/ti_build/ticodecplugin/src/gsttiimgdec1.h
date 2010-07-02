/*
 * gsttiimgdec1.h
 *
 * This file declares the "TIImgdec1" element, which decodes an image in
 * jpeg format
 *
 * Original Author:
 *     Chase Maupin, Texas Instruments, Inc.
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
 *
 */

#ifndef __GST_TIIMGDEC1_H__
#define __GST_TIIMGDEC1_H__

#include <pthread.h>

#include <gst/gst.h>
#include "gstticircbuffer.h"
#include "gsttidmaibuftab.h"

#include <xdc/std.h>
#include <ti/sdo/ce/Engine.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/Fifo.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/Rendezvous.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/ce/Idec1.h>

G_BEGIN_DECLS

/* Standard macros for maniuplating TIImgdec1 objects */
#define GST_TYPE_TIIMGDEC1 \
  (gst_tiimgdec1_get_type())
#define GST_TIIMGDEC1(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TIIMGDEC1,GstTIImgdec1))
#define GST_TIIMGDEC1_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TIIMGDEC1,GstTIImgdec1Class))
#define GST_IS_TIIMGDEC1(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TIIMGDEC1))
#define GST_IS_TIIMGDEC1_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TIIMGDEC1))

typedef struct _GstTIImgdec1      GstTIImgdec1;
typedef struct _GstTIImgdec1Class GstTIImgdec1Class;

/* _GstTIImgdec1 object */
struct _GstTIImgdec1
{
  /* gStreamer infrastructure */
  GstElement                element;
  GstPad                    *sinkpad;
  GstPad                    *srcpad;

  /* Element properties */
  const gchar*              engineName;
  const gchar*              codecName;
  gboolean                  displayBuffer;
  gboolean                  genTimeStamps;
  /* Resolution input */
  gint                      width;
  gint                      height;


  /* Element state */
  Engine_Handle             hEngine;
  Idec1_Handle              hIe;
  gboolean                  drainingEOS;
  pthread_mutex_t           threadStatusMutex;
  UInt32                    threadStatus;
  gboolean                  capsSet;

  /* Codec Parameters */
  IMGDEC1_Params            params;
  IMGDEC1_DynamicParams     dynParams;

  /* Decode thread */
  pthread_t                 decodeThread;
  Rendezvous_Handle         waitOnDecodeThread;
  Rendezvous_Handle         waitOnDecodeDrain;
  
  /* Framerate (Num/Den) */
  gint                      framerateNum;
  gint                      framerateDen;

  /* Buffer management */
  UInt32                    numOutputBufs;
  GstTIDmaiBufTab          *hOutBufTab;
  GstTICircBuffer          *circBuf;
  Buffer_Handle             hInBuf;
};

/* _GstTIImgdec1Class object */
struct _GstTIImgdec1Class 
{
  GstElementClass parent_class;
};

/* External function declarations */
GType gst_tiimgdec1_get_type(void);

G_END_DECLS

#endif /* __GST_TIIMGDEC1_H__ */


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
