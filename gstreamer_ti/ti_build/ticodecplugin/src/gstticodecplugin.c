/*
 * gstticodecplugin.c
 *
 * This file defines the main entry point of the TI Codec Plugin for GStreamer.
 *
 * Original Author:
 *     Don Darling, Texas Instruments, Inc.
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/resource.h>
#include <stdlib.h>

#include <gst/gst.h>

#include <xdc/std.h>
#include <ti/sdo/ce/Engine.h>
#include <ti/sdo/ce/CERuntime.h>
#include <ti/sdo/dmai/Dmai.h>

#include "gsttiauddec1.h"
#include "gsttividdec2.h"
#include "gsttiimgenc1.h"
#include "gsttiimgdec1.h"
#include "gsttidmaivideosink.h"
#include "gsttividenc1.h"
#include "gsttiaudenc1.h"
#include "gsttividresize.h"
#include "gsttidmaiperf.h"

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
TICodecPlugin_init (GstPlugin * TICodecPlugin)
{
    Char    *env_value;

    /* Initialize the codec engine run time */
    CERuntime_init();

    /* Initialize DMAI */
    Dmai_init(); 

    env_value = getenv("GST_TI_TIVidenc1_DISABLE");

    if ((!env_value || strcmp(env_value,"1")) && !gst_element_register(
        TICodecPlugin, "TIVidenc1", GST_RANK_PRIMARY,
        GST_TYPE_TIVIDENC1))
        return FALSE;

    env_value = getenv("GST_TI_TIViddec2_DISABLE");

    if ((!env_value || strcmp(env_value,"1")) && !gst_element_register(
        TICodecPlugin, "TIViddec2", GST_RANK_PRIMARY,
        GST_TYPE_TIVIDDEC2))
        return FALSE;

    env_value = getenv("GST_TI_TIImgenc1_DISABLE");

    if ((!env_value || strcmp(env_value,"1")) && !gst_element_register(
        TICodecPlugin, "TIImgenc1", GST_RANK_PRIMARY,
        GST_TYPE_TIIMGENC1))
        return FALSE;

    env_value = getenv("GST_TI_TIImgdec1_DISABLE");

    if ((!env_value || strcmp(env_value,"1")) && !gst_element_register(
        TICodecPlugin, "TIImgdec1", GST_RANK_PRIMARY,
        GST_TYPE_TIIMGDEC1))
        return FALSE;

    env_value = getenv("GST_TI_TIAuddec1_DISABLE");

    if ((!env_value || strcmp(env_value,"1")) && !gst_element_register(
        TICodecPlugin, "TIAuddec1", GST_RANK_PRIMARY,
        GST_TYPE_TIAUDDEC1))
        return FALSE;

    env_value = getenv("GST_TI_TIAudenc1_DISABLE");

    if ((!env_value || strcmp(env_value,"1")) && !gst_element_register(
        TICodecPlugin, "TIAudenc1", GST_RANK_PRIMARY,
        GST_TYPE_TIAUDENC1))
        return FALSE;

    env_value = getenv("GST_TI_TIDmaiVideoSink_DISABLE");

    if ((!env_value || strcmp(env_value,"1")) && !gst_element_register(
        TICodecPlugin, "TIDmaiVideoSink", GST_RANK_PRIMARY,
        GST_TYPE_TIDMAIVIDEOSINK))
        return FALSE;

    env_value = getenv("GST_TI_TIVidResize_DISABLE");

    if ((!env_value || strcmp(env_value,"1")) && !gst_element_register(
        TICodecPlugin, "TIVidResize", GST_RANK_PRIMARY,
        GST_TYPE_TIVIDRESIZE))
        return FALSE;

    env_value = getenv("GST_TI_TIDmaiPerf_DISABLE");

    if ((!env_value || strcmp(env_value,"1")) && !gst_element_register(
        TICodecPlugin, "dmaiperf", GST_RANK_PRIMARY,
        GST_TYPE_DMAIPERF))
        return FALSE;

    return TRUE;
}

/* gstreamer looks for this structure to register TICodecPlugins */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "TICodecPlugin",
    "Plugin for TI xDM-Based Codecs",
    TICodecPlugin_init,
    VERSION,
    GST_LICENSE_UNKNOWN,
    "TI",
    "http://www.ti.com/"
)


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
