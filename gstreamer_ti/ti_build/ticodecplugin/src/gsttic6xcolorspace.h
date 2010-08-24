/*
 * gsttic6xcolorspace.h
 *
 * This file enclares the "TIC6xColorspace" element, which does the 
 * image color conversion using TI C6Accel library.
 *
 * Original Author:
 *     Brijesh Singh, Texas Instruments, Inc.
 *
 * Copyright (C) 2010 Texas Instruments Incorporated - http://www.ti.com/
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

#ifndef __GST_TIC6XCOLORSPACE_H__
#define __GST_TIC6XCOLORSPACE_H__

#include <pthread.h>

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

#include <xdc/std.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/ColorSpace.h>
#include <ti/sdo/dmai/Rendezvous.h>

#include <c6accelw.h>

#include "gsttidmaibuftab.h"

G_BEGIN_DECLS

/* Standard macros for maniuplating TIC6xColorspace objects */
#define GST_TYPE_TIC6XCOLORSPACE \
  (gst_tic6xcolorspace_get_type())
#define GST_TIC6XCOLORSPACE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TIC6XCOLORSPACE,GstTIC6xColorspace))
#define GST_TIC6XCOLORSPACE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TIC6XCOLORSPACE,GstTIC6xColorspaceClass))
#define GST_IS_TIC6XCOLORSPACE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TIC6XCOLORSPACE))
#define GST_IS_TIC6XCOLORSPACE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TIC6XCOLORSPACE))

typedef struct _GstTIC6xColorspace      GstTIC6xColorspace;
typedef struct _GstTIC6xColorspaceClass GstTIC6xColorspaceClass;

/* _GstTIC6xColorspace object */
struct _GstTIC6xColorspace
{
  /* gStreamer infrastructure */
  GstBaseTransform  element;
  GstPad            *sinkpad;
  GstPad            *srcpad;

  /* Element property */
  gboolean          contiguousInputFrame;
  gint              numOutputBufs;
  const gchar*      engineName;

  /* Element state */
  gint              width;
  gint              height;
  ColorSpace_Type   srcColorSpace;
  ColorSpace_Type   dstColorSpace;
  GstTIDmaiBufTab  *hOutBufTab;
  Rendezvous_Handle waitOnBufTab;
  C6accel_Handle    hC6;
  Engine_Handle     hEngine;
  Buffer_Handle     hCoeff;
};

/* _GstTIC6xColorspaceClass object */
struct _GstTIC6xColorspaceClass 
{
  GstBaseTransformClass parent_class;
};

/* External function enclarations */
GType gst_tic6xcolorspace_get_type(void);

G_END_DECLS

#endif /* __GST_TIC6XCOLORSPACE_H__ */


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
