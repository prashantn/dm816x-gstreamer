/*
 * Copyright (C) 2010-2011 Texas Instruments Incorporated - http://www.ti.com/
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


#ifndef __GST_TIDISPLAYSINK2_H__
#define __GST_TIDISPLAYSINK2_H__

#include <gst/gst.h>
#include <ti/sdo/dmai/Display.h>
#include <ti/sdo/dmai/Cpu.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/Framecopy.h>

#include "gsttidmaibuftab.h"
#include "gsttidmaibuffertransport.h"

G_BEGIN_DECLS

#define GST_TYPE_TIDISPLAYSINK2 \
  (gst_tidisplaysink2_get_type())
#define GST_TIDISPLAYSINK2(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TIDISPLAYSINK2,GstTIDisplaySink2))
#define GST_TIDISPLAYSINK2_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TIDISPLAYSINK2,GstTIDisplaySink2Class))
#define GST_IS_TIDISPLAYSINK2(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TIDISPLAYSINK2))
#define GST_IS_TIDISPLAYSINK2_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TIDISPLAYSINK2))

struct gst_tidisplay_sink {
    GstBaseSink parent;

    GstTIDmaiBufTab	*hBufTab;
    Display_Handle hDisplay;
    Display_Attrs   dAttrs;
    Framecopy_Handle hFc;
    gboolean  overlay_set;

    gchar *device, *display_output, *video_std;
    gint  rotate, numbufs;
    gint  overlay_top, overlay_left;
    guint  overlay_height, overlay_width;
    gboolean  mmap_buffer, dma_copy;    
    guint64 framecounts;
    gint fd;
};

struct gst_tidisplay_sink_class {
    GstBaseSinkClass parent_class;
};

typedef struct gst_tidisplay_sink GstTIDisplaySink2;
typedef struct gst_tidisplay_sink_class GstTIDisplaySink2Class;

GType gst_tidisplaysink2_get_type(void);

G_END_DECLS

#endif /* __GST_TIDISPLAYSINK2_H__ */

/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
