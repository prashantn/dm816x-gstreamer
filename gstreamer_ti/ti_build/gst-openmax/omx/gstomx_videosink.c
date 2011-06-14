/*
 * Copyright (C) 2007-2009 Nokia Corporation.
 *
 * Author: Felipe Contreras <felipe.contreras@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "gstomx_videosink.h"
#include "gstomx_base_sink.h"
#include "gstomx.h"

#include <string.h> /* for memset, strcmp */

#include <xdc/std.h>
#include <OMX_TI_Index.h>
#include <OMX_TI_Common.h>
#include <omx_vfdc.h>

GSTOMX_BOILERPLATE (GstOmxVideoSink, gst_omx_videosink, GstOmxBaseSink, GST_OMX_BASE_SINK_TYPE);

enum
{
    ARG_0,
    ARG_X_SCALE,
    ARG_Y_SCALE,
    ARG_ROTATION,
    ARG_TOP,
    ARG_LEFT,
};

static GstCaps *
generate_sink_template (void)
{
    GstCaps *caps;
    GstStructure *struc;

    caps = gst_caps_new_empty ();

    struc = gst_structure_new ("video/x-raw-yuv",
                               "width", GST_TYPE_INT_RANGE, 16, 4096,
                               "height", GST_TYPE_INT_RANGE, 16, 4096,
                               "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1,
                               NULL);

    {
        GValue list;
        GValue val;

        list.g_type = val.g_type = 0;

        g_value_init (&list, GST_TYPE_LIST);
        g_value_init (&val, GST_TYPE_FOURCC);
#if 0
        gst_value_set_fourcc (&val, GST_MAKE_FOURCC ('I', '4', '2', '0'));
        gst_value_list_append_value (&list, &val);
#endif
        gst_value_set_fourcc (&val, GST_MAKE_FOURCC ('Y', 'U', 'Y', '2'));
        gst_value_list_append_value (&list, &val);
#if 0
        gst_value_set_fourcc (&val, GST_MAKE_FOURCC ('U', 'Y', 'V', 'Y'));
        gst_value_list_append_value (&list, &val);
#endif
        gst_structure_set_value (struc, "format", &list);

        g_value_unset (&val);
        g_value_unset (&list);
    }

    gst_caps_append_structure (caps, struc);

    return caps;
}

static void
type_base_init (gpointer g_class)
{
    GstElementClass *element_class;

    element_class = GST_ELEMENT_CLASS (g_class);

    {
        GstElementDetails details;

        details.longname = "OpenMAX IL videosink element";
        details.klass = "Video/Sink";
        details.description = "Renders video";
        details.author = "Felipe Contreras";

        gst_element_class_set_details (element_class, &details);
    }

    {
        GstPadTemplate *template;

        template = gst_pad_template_new ("sink", GST_PAD_SINK,
                                         GST_PAD_ALWAYS,
                                         generate_sink_template ());

        gst_element_class_add_pad_template (element_class, template);
    }
}

static void
omx_setup (GstBaseSink *gst_sink, GstCaps *caps)
{
    GstOmxBaseSink *omx_base, *self;
    GOmxCore *gomx;
    OMX_PARAM_PORTDEFINITIONTYPE param;
    OMX_PARAM_VFDC_DRIVERINSTID driverId;
    OMX_PARAM_VFDC_CREATEMOSAICLAYOUT mosaicLayout;
    OMX_CONFIG_VFDC_MOSAICLAYOUT_PORT2WINMAP port2Winmap;
    OMX_PARAM_BUFFER_MEMORYTYPE memTypeCfg;
    GstStructure *structure;
    gint width;
    gint height;

    self = omx_base = GST_OMX_BASE_SINK (gst_sink);
    gomx = (GOmxCore *) omx_base->gomx;

    structure = gst_caps_get_structure (caps, 0);

    gst_structure_get_int (structure, "width", &width);
    gst_structure_get_int (structure, "height", &height);

    /* set input port definition */
    G_OMX_PORT_GET_DEFINITION (omx_base->in_port, &param);

    param.nBufferSize = (width * height * 2);
    param.format.video.nFrameWidth = width;
    param.format.video.nFrameHeight = height;
    param.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    param.format.video.eColorFormat = OMX_COLOR_FormatYCbYCr;

    G_OMX_PORT_SET_DEFINITION (omx_base->in_port, &param);
    g_omx_port_setup (omx_base->in_port, &param);
 
    /* set display driver mode */
    _G_OMX_INIT_PARAM (&driverId);
    driverId.nDrvInstID = 0; /* on chip HDMI */
    driverId.eDispVencMode = OMX_DC_MODE_1080P_60;

    OMX_SetParameter (gomx->omx_handle, (OMX_INDEXTYPE) OMX_TI_IndexParamVFDCDriverInstId, &driverId);

    /* set mosiac window information */
    _G_OMX_INIT_PARAM (&mosaicLayout);
    mosaicLayout.nPortIndex = 0;
    mosaicLayout.sMosaicWinFmt[0].winStartX = 0;
    mosaicLayout.sMosaicWinFmt[0].winStartY = 0;
    mosaicLayout.sMosaicWinFmt[0].winWidth = width;
    mosaicLayout.sMosaicWinFmt[0].winHeight = height;
    mosaicLayout.sMosaicWinFmt[0].pitch[VFDC_YUV_INT_ADDR_IDX] = width * 2;
    mosaicLayout.sMosaicWinFmt[0].dataFormat =  VFDC_DF_YUV422I_YVYU;
    mosaicLayout.sMosaicWinFmt[0].bpp = VFDC_BPP_BITS16;
    mosaicLayout.sMosaicWinFmt[0].priority = 0;
    mosaicLayout.nDisChannelNum = 0;
    mosaicLayout.nNumWindows = 1;

    OMX_SetParameter (gomx->omx_handle, (OMX_INDEXTYPE) OMX_TI_IndexParamVFDCCreateMosaicLayout, 
        &mosaicLayout);

    /* set port to window mapping */
    _G_OMX_INIT_PARAM (&port2Winmap);
    port2Winmap.nLayoutId = 0; 
    port2Winmap.numWindows = 1; 
    port2Winmap.omxPortList[0] = OMX_VFDC_INPUT_PORT_START_INDEX + 0;

    OMX_SetConfig (gomx->omx_handle, (OMX_INDEXTYPE) OMX_TI_IndexConfigVFDCMosaicPort2WinMap, &port2Winmap);

    /* set default input memory to Raw */
    _G_OMX_INIT_PARAM (&memTypeCfg);
    memTypeCfg.nPortIndex = 0;
    memTypeCfg.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;
    
    OMX_SetParameter (gomx->omx_handle, OMX_TI_IndexParamBuffMemType, &memTypeCfg);
 
    /* enable the input port */
    OMX_SendCommand (gomx->omx_handle, OMX_CommandPortEnable, omx_base->in_port->port_index, NULL);
    g_sem_down (omx_base->in_port->core->port_sem);

    return;
}


static gboolean
setcaps (GstBaseSink *gst_sink,
         GstCaps *caps)
{
    GstOmxBaseSink *omx_base;
    GstOmxVideoSink *self;
    GOmxCore *gomx;

    omx_base = GST_OMX_BASE_SINK (gst_sink);
    self = GST_OMX_VIDEOSINK (gst_sink);
    gomx = (GOmxCore *) omx_base->gomx;

    GST_INFO_OBJECT (omx_base, "setcaps (sink): %" GST_PTR_FORMAT, caps);

    g_return_val_if_fail (gst_caps_get_size (caps) == 1, FALSE);

    omx_setup (gst_sink, caps);

    return TRUE;
}

static void
set_property (GObject *object,
              guint prop_id,
              const GValue *value,
              GParamSpec *pspec)
{
    GstOmxVideoSink *self;

    self = GST_OMX_VIDEOSINK (object);

    switch (prop_id)
    {
        case ARG_X_SCALE:
            self->x_scale = g_value_get_uint (value);
            break;
        case ARG_Y_SCALE:
            self->y_scale = g_value_get_uint (value);
            break;
        case ARG_ROTATION:
            self->rotation = g_value_get_uint (value);
            break;
        case ARG_TOP:
            self->top = g_value_get_uint (value);
            break;
        case ARG_LEFT:
            self->left = g_value_get_uint (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
get_property (GObject *object,
              guint prop_id,
              GValue *value,
              GParamSpec *pspec)
{
    GstOmxVideoSink *self;

    self = GST_OMX_VIDEOSINK (object);

    switch (prop_id)
    {
        case ARG_X_SCALE:
            g_value_set_uint (value, self->x_scale);
            break;
        case ARG_Y_SCALE:
            g_value_set_uint (value, self->y_scale);
            break;
        case ARG_ROTATION:
            g_value_set_uint (value, self->rotation);
            break;
        case ARG_LEFT:
            g_value_set_uint (value, self->left);
            break;
        case ARG_TOP:
            g_value_set_uint (value, self->top);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
type_class_init (gpointer g_class,
                 gpointer class_data)
{
    GObjectClass *gobject_class;
    GstBaseSinkClass *gst_base_sink_class;

    gobject_class = (GObjectClass *) g_class;
    gst_base_sink_class = GST_BASE_SINK_CLASS (g_class);

    gst_base_sink_class->set_caps = setcaps;

    gobject_class->set_property = set_property;
    gobject_class->get_property = get_property;

    #if 0
    g_object_class_install_property (gobject_class, ARG_X_SCALE,
                                     g_param_spec_uint ("x-scale", "X Scale",
                                                        "How much to scale the image in the X axis (100 means nothing)",
                                                        0, G_MAXUINT, 100, G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, ARG_Y_SCALE,
                                     g_param_spec_uint ("y-scale", "Y Scale",
                                                        "How much to scale the image in the Y axis (100 means nothing)",
                                                        0, G_MAXUINT, 100, G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, ARG_ROTATION,
                                     g_param_spec_uint ("rotation", "Rotation",
                                                        "Rotation angle",
                                                        0, G_MAXUINT, 360, G_PARAM_READWRITE));
    #endif

    g_object_class_install_property (gobject_class, ARG_TOP,
                                     g_param_spec_uint ("top", "Top",
                                                        "The top most co-ordinate on video display",
                                                        0, G_MAXUINT, 100, G_PARAM_READWRITE));
    g_object_class_install_property (gobject_class, ARG_LEFT,
                                     g_param_spec_uint ("left", "left",
                                                        "The left most co-ordinate on video display",
                                                        0, G_MAXUINT, 100, G_PARAM_READWRITE));
}

static void
type_instance_init (GTypeInstance *instance,
                    gpointer g_class)
{
    GstOmxBaseSink *omx_base;

    omx_base = GST_OMX_BASE_SINK (instance);
    omx_base->omx_setup = omx_setup;

    GST_DEBUG_OBJECT (omx_base, "start");
}
