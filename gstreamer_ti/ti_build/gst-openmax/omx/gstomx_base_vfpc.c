/*
 * Copyright (C) 2011-2012 Texas Instruments Inc.
 *
 * Author: Brijesh Singh <bksingh@ti.com>
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

#include "gstomx_base_vfpc.h"
#include "gstomx.h"

#include <gst/video/video.h>

#include <OMX_TI_Index.h>

#include <string.h> /* for memset */

enum
{
    ARG_0,
    ARG_CHANNEL,
};

GSTOMX_BOILERPLATE (GstOmxBaseVfpc, gst_omx_base_vfpc, GstOmxBaseFilter, GST_OMX_BASE_FILTER_TYPE);

static GstStaticPadTemplate src_template =
        GST_STATIC_PAD_TEMPLATE ("src",
                GST_PAD_SRC,
                GST_PAD_ALWAYS,
                GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV ( "{YUY2}" ))
        );

static GstFlowReturn push_buffer (GstOmxBaseFilter *self, GstBuffer *buf);

static gboolean
pad_event (GstPad *pad, GstEvent *event)
{
    GstOmxBaseVfpc *self;
    GstOmxBaseFilter *omx_base;

    self = GST_OMX_BASE_VFPC (GST_OBJECT_PARENT (pad));
    omx_base = GST_OMX_BASE_FILTER (self);

    GST_INFO_OBJECT (self, "begin: event=%s", GST_EVENT_TYPE_NAME (event));

    switch (GST_EVENT_TYPE (event))
    {
        case GST_EVENT_CROP:
        {
            gst_event_parse_crop (event, &self->top, &self->left, NULL, NULL);
            return TRUE;
        }
        default:
        {
            return parent_class->pad_event (pad, event);
        }
    }
}

static void
type_base_init (gpointer g_class)
{
    GstElementClass *element_class;
    GstOmxBaseFilterClass *bfilter_class = GST_OMX_BASE_FILTER_CLASS (g_class);

    element_class = GST_ELEMENT_CLASS (g_class);

    gst_element_class_add_pad_template (element_class,
        gst_static_pad_template_get (&src_template));

    bfilter_class->pad_event = pad_event;
}

static GstFlowReturn
push_buffer (GstOmxBaseFilter *omx_base, GstBuffer *buf)
{
    return parent_class->push_buffer (omx_base, buf);
}

static void
settings_changed_cb (GOmxCore *core)
{
    GstOmxBaseFilter *omx_base;
    GstOmxBaseVfpc *self;
    GstCaps *new_caps;

    omx_base = core->object;
    self = GST_OMX_BASE_VFPC (omx_base);

    GST_DEBUG_OBJECT (omx_base, "settings changed");

    new_caps = gst_caps_intersect (gst_pad_get_caps (omx_base->srcpad),
           gst_pad_peer_get_caps (omx_base->srcpad));

    if (!gst_caps_is_fixed (new_caps))
    {
        gst_caps_do_simplify (new_caps);
        GST_INFO_OBJECT (omx_base, "pre-fixated caps: %" GST_PTR_FORMAT, new_caps);
        gst_pad_fixate_caps (omx_base->srcpad, new_caps);
    }

    GST_INFO_OBJECT (omx_base, "caps are: %" GST_PTR_FORMAT, new_caps);
    GST_INFO_OBJECT (omx_base, "old caps are: %" GST_PTR_FORMAT, GST_PAD_CAPS (omx_base->srcpad));

    gst_pad_set_caps (omx_base->srcpad, new_caps);
}

static gboolean 
setup_ports (GstOmxBaseFilter *omx_base)
{
    GOmxCore *gomx;
    OMX_ERRORTYPE err;
    OMX_PARAM_PORTDEFINITIONTYPE paramPort;
    OMX_PARAM_BUFFER_MEMORYTYPE memTypeCfg;
    OMX_PARAM_VFPC_NUMCHANNELPERHANDLE numChannels;
    OMX_CONFIG_VIDCHANNEL_RESOLUTION chResolution;
    OMX_CONFIG_ALG_ENABLE algEnable;
    GstOmxBaseVfpc *self;
    gboolean ret = FALSE;

    gomx = (GOmxCore *) omx_base->gomx;
    self = GST_OMX_BASE_VFPC (omx_base);

    GST_LOG_OBJECT (self, "begin");
    
    /* Setting Memory type at input port to Raw Memory */
    GST_LOG_OBJECT (self, "Setting input port to Raw memory");

    _G_OMX_INIT_PARAM (&memTypeCfg);
    memTypeCfg.nPortIndex = OMX_VFPC_INPUT_PORT_START_INDEX + self->channel_index;
    memTypeCfg.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;    
    err = OMX_SetParameter (gomx->omx_handle, OMX_TI_IndexParamBuffMemType, &memTypeCfg);

    if (err != OMX_ErrorNone)
        return ret;

    /* Setting Memory type at output port to Raw Memory */
    GST_LOG_OBJECT (self, "Setting output port to Raw memory");

    _G_OMX_INIT_PARAM (&memTypeCfg);
    memTypeCfg.nPortIndex = OMX_VFPC_OUTPUT_PORT_START_INDEX + self->channel_index;
    memTypeCfg.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;
    err = OMX_SetParameter (gomx->omx_handle, OMX_TI_IndexParamBuffMemType, &memTypeCfg);

    if (err != OMX_ErrorNone)
        return ret;

    /* Input port configuration. */
    GST_LOG_OBJECT (self, "Setting port definition (input)");

    G_OMX_PORT_GET_DEFINITION (omx_base->in_port, &paramPort);
    paramPort.format.video.nFrameWidth = self->in_width;
    paramPort.format.video.nFrameHeight = self->in_height;
    paramPort.format.video.nStride = self->in_stride;
    paramPort.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    paramPort.format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
    paramPort.nBufferSize =  self->in_stride * self->in_height * 1.5;
    paramPort.nBufferAlignment = 0;
    paramPort.bBuffersContiguous = 0;
    G_OMX_PORT_SET_DEFINITION (omx_base->in_port, &paramPort);
    g_omx_port_setup (omx_base->in_port, &paramPort);

    /* Output port configuration. */
    GST_LOG_OBJECT (self, "Setting port definition (output)");

    G_OMX_PORT_GET_DEFINITION (omx_base->out_port, &paramPort);
    paramPort.format.video.nFrameWidth = self->out_width;
    paramPort.format.video.nFrameHeight = self->out_height;
    paramPort.format.video.nStride = self->out_stride;
    paramPort.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    paramPort.format.video.eColorFormat = OMX_COLOR_FormatYCbYCr;
    paramPort.nBufferSize =  self->out_stride * self->out_height;
    paramPort.nBufferAlignment = 0;
    paramPort.bBuffersContiguous = 0;
    G_OMX_PORT_SET_DEFINITION (omx_base->out_port, &paramPort);
    g_omx_port_setup (omx_base->out_port, &paramPort);

    /* Set number of channles */
    GST_LOG_OBJECT (self, "Setting number of channels");

    _G_OMX_INIT_PARAM (&numChannels);
    numChannels.nNumChannelsPerHandle = 1;    
    err = OMX_SetParameter (gomx->omx_handle, 
        (OMX_INDEXTYPE) OMX_TI_IndexParamVFPCNumChPerHandle, &numChannels);

    if (err != OMX_ErrorNone)
        return ret;

    /* Set input channel resolution */
    GST_LOG_OBJECT (self, "Setting channel resolution (input)");

    _G_OMX_INIT_PARAM (&chResolution);
    chResolution.Frm0Width = self->in_width;
    chResolution.Frm0Height = self->in_height;
    chResolution.Frm0Pitch = self->in_stride;
    chResolution.Frm1Width = 0;
    chResolution.Frm1Height = 0;
    chResolution.Frm1Pitch = 0;
    chResolution.FrmStartX = self->left;
    chResolution.FrmStartY = self->top;
    chResolution.FrmCropWidth = 0;
    chResolution.FrmCropHeight = 0;
    chResolution.eDir = OMX_DirInput;
    chResolution.nChId = 0;
    err = OMX_SetConfig (gomx->omx_handle, OMX_TI_IndexConfigVidChResolution, &chResolution);

    if (err != OMX_ErrorNone)
        return ret;

    /* Set output channel resolution */
    GST_LOG_OBJECT (self, "Setting channel resolution (output)");

    _G_OMX_INIT_PARAM (&chResolution);
    chResolution.Frm0Width = self->out_width;
    chResolution.Frm0Height = self->out_height;
    chResolution.Frm0Pitch = self->out_stride;
    chResolution.Frm1Width = 0;
    chResolution.Frm1Height = 0;
    chResolution.Frm1Pitch = 0;
    chResolution.FrmStartX = 0;
    chResolution.FrmStartY = 0;
    chResolution.FrmCropWidth = 0;
    chResolution.FrmCropHeight = 0;
    chResolution.eDir = OMX_DirOutput;
    chResolution.nChId = 0;
    err = OMX_SetConfig (gomx->omx_handle, OMX_TI_IndexConfigVidChResolution, &chResolution);

    if (err != OMX_ErrorNone)
        return ret;

    _G_OMX_INIT_PARAM (&algEnable);
    algEnable.nPortIndex = 0;
    algEnable.nChId = 0;
    algEnable.bAlgBypass = OMX_FALSE;

    err = OMX_SetConfig (gomx->omx_handle, (OMX_INDEXTYPE) OMX_TI_IndexConfigAlgEnable, &algEnable);

    if (err != OMX_ErrorNone)
        return ret;

    return TRUE;
}

static gboolean
sink_setcaps (GstPad *pad,
              GstCaps *caps)
{
    GstStructure *structure;
    GstOmxBaseVfpc *self;
    GstOmxBaseFilter *omx_base;
    GOmxCore *gomx;

    self = GST_OMX_BASE_VFPC (GST_PAD_PARENT (pad));
    omx_base = GST_OMX_BASE_FILTER (self);

    gomx = (GOmxCore *) omx_base->gomx;

    GST_INFO_OBJECT (self, "setcaps (sink): %" GST_PTR_FORMAT, caps);

    g_return_val_if_fail (caps, FALSE);
    g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

    structure = gst_caps_get_structure (caps, 0);

    g_return_val_if_fail (structure, FALSE);

    if (!(gst_structure_get_int (structure, "width", &self->in_width) &&
            gst_structure_get_int (structure, "height", &self->in_height)))
    {
        GST_WARNING_OBJECT (self, "width and/or height not set in caps: %dx%d",
                self->in_width, self->in_height);
        return FALSE;
    }

    if (!(gst_structure_get_int (structure, "rowstride", &self->in_stride)))
    {
        GST_WARNING_OBJECT (self, "does not have rowstride, defaulting to %d",
                self->in_stride);
    }

    if (!self->in_stride)
        self->in_stride = self->in_width;

    /* we assume no scaling unless src caps requests */
    self->out_width = self->in_width;
    self->out_height = self->in_height;
    self->out_stride = self->out_width * 2;
    
    if (self->sink_setcaps)
        self->sink_setcaps (pad, caps);

    return gst_pad_set_caps (pad, caps);
}

static GstCaps *
src_getcaps (GstPad *pad)
{
    GstCaps *caps;
    GstOmxBaseVfpc *self   = GST_OMX_BASE_VFPC (GST_PAD_PARENT (pad));
    GstOmxBaseFilter *omx_base = GST_OMX_BASE_FILTER (self);

    if (omx_base->gomx->omx_state > OMX_StateLoaded)
    {
        /* currently, we cannot change caps once out of loaded..  later this
         * could possibly be supported by enabling/disabling the port..
         */
        GST_DEBUG_OBJECT (self, "cannot getcaps in %d state", omx_base->gomx->omx_state);
        return GST_PAD_CAPS (pad);
    }

    if (self->port_configured)
    {
        /* if we already have src-caps, we want to take the already configured
         * width/height/etc.  But we can still support any option of rowstride,
         * so we still don't want to return fixed caps
         */
        OMX_PARAM_PORTDEFINITIONTYPE param;
        GstStructure *struc;

        G_OMX_PORT_GET_DEFINITION (omx_base->out_port, &param);

        caps = gst_caps_new_empty ();

        struc = gst_structure_new (("video/x-raw-yuv"),
            "width",  G_TYPE_INT, param.format.video.nFrameWidth,
            "height", G_TYPE_INT, param.format.video.nFrameHeight,
            "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('Y', 'U', 'Y', '2'),
            NULL);

        if (self->framerate_denom)
        {
            gst_structure_set (struc,
                "framerate", GST_TYPE_FRACTION, self->framerate_num, self->framerate_denom,
                NULL);
        }

        gst_caps_append_structure (caps, struc);
    }
    else
    {
        /* we don't have valid width/height/etc yet, so just use the template.. */
        caps = gst_static_pad_template_get_caps (&src_template);
        GST_DEBUG_OBJECT (self, "caps=%"GST_PTR_FORMAT, caps);
    }

    GST_DEBUG_OBJECT (self, "caps=%"GST_PTR_FORMAT, caps);

    return caps;
}

static gboolean
src_setcaps (GstPad *pad, GstCaps *caps)
{
    GstOmxBaseVfpc *self;
    GstOmxBaseFilter *omx_base;
    GstVideoFormat format;

    self = GST_OMX_BASE_VFPC (GST_PAD_PARENT (pad));
    omx_base = GST_OMX_BASE_FILTER (self);

    GST_INFO_OBJECT (omx_base, "setcaps (src): %" GST_PTR_FORMAT, caps);
    g_return_val_if_fail (caps, FALSE);
    g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

    if (!gst_video_format_parse_caps(caps,
            &format, &self->out_width, &self->out_height))
    {
        GST_DEBUG_OBJECT (self, "failed to get source resolution" );
        self->out_width = self->in_width;
        self->out_height = self->in_height;
    }

    self->out_stride = self->out_width;

    /* save the src caps later needed by omx transport buffer */
    if (omx_base->out_port->caps)
        gst_caps_unref (omx_base->out_port->caps);
    omx_base->out_port->caps = gst_caps_copy (caps);

    return TRUE;
}

static void
set_property (GObject *obj,
              guint prop_id,
              const GValue *value,
              GParamSpec *pspec)
{
    GstOmxBaseVfpc *self;

    self = GST_OMX_BASE_VFPC (obj);

    switch (prop_id)
    {
        case ARG_CHANNEL:
            self->channel_index = g_value_get_uint (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
            break;
    }
}

static void
get_property (GObject *obj,
              guint prop_id,
              GValue *value,
              GParamSpec *pspec)
{
    GstOmxBaseVfpc *self;

    self = GST_OMX_BASE_VFPC (obj);

    switch (prop_id)
    {
        case ARG_CHANNEL:
            g_value_set_uint (value, self->channel_index);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
            break;
    }
}

static void
omx_setup (GstOmxBaseFilter *omx_base)
{
    GstOmxBaseVfpc *self;
    GOmxCore *gomx;
    GOmxPort *port;

    self = GST_OMX_BASE_VFPC (omx_base);
    gomx = (GOmxCore *) omx_base->gomx;

    GST_INFO_OBJECT (omx_base, "begin");

    /* configure input and output port information */
    setup_ports (omx_base);

    /* enable input port */
    port = omx_base->in_port;
    OMX_SendCommand (g_omx_core_get_handle (port->core),
            OMX_CommandPortEnable, port->port_index, NULL);
    g_sem_down (port->core->port_sem);

    /* enable output port */
    port = omx_base->out_port;
    OMX_SendCommand (g_omx_core_get_handle (port->core),
            OMX_CommandPortEnable, port->port_index, NULL);
    g_sem_down (port->core->port_sem);

    /* indicate the port is now configured */
    self->port_configured = TRUE;

    GST_INFO_OBJECT (omx_base, "end");
}

static void
type_class_init (gpointer g_class,
                 gpointer class_data)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS (g_class);
    GST_OMX_BASE_FILTER_CLASS (g_class)->push_buffer = push_buffer;

    /* Properties stuff */
    {
        gobject_class->set_property = set_property;
        gobject_class->get_property = get_property;

        g_object_class_install_property (gobject_class, ARG_CHANNEL,
                                         g_param_spec_uint ("use-channel", "channel",
                                                            "which channel to use",
                                                            0, 8, 0, G_PARAM_READWRITE));
    }
}

static void
type_instance_init (GTypeInstance *instance,
                    gpointer g_class)
{
    GstOmxBaseFilter *omx_base;

    omx_base = GST_OMX_BASE_FILTER (instance);

    omx_base->omx_setup = omx_setup;

    omx_base->gomx->settings_changed_cb = settings_changed_cb;

    /* GOmx */
    g_omx_core_free (omx_base->gomx);
    g_omx_port_free (omx_base->in_port);
    g_omx_port_free (omx_base->out_port);

    omx_base->gomx = g_omx_core_new (omx_base, g_class);
    omx_base->in_port = g_omx_core_get_port (omx_base->gomx, "in", OMX_VFPC_INPUT_PORT_START_INDEX);
    omx_base->out_port = g_omx_core_get_port (omx_base->gomx, "out", OMX_VFPC_OUTPUT_PORT_START_INDEX);

    omx_base->in_port->omx_allocate = TRUE;
    omx_base->in_port->share_buffer = FALSE;
    omx_base->in_port->always_copy  = FALSE;

    omx_base->out_port->omx_allocate = TRUE;
    omx_base->out_port->share_buffer = FALSE;
    omx_base->out_port->always_copy = FALSE;

    omx_base->in_port->port_index = OMX_VFPC_INPUT_PORT_START_INDEX ;
    omx_base->out_port->port_index = OMX_VFPC_OUTPUT_PORT_START_INDEX;

    gst_pad_set_setcaps_function (omx_base->sinkpad,
            GST_DEBUG_FUNCPTR (sink_setcaps));
    gst_pad_set_getcaps_function (omx_base->srcpad,
            GST_DEBUG_FUNCPTR (src_getcaps));
    gst_pad_set_setcaps_function (omx_base->srcpad,
            GST_DEBUG_FUNCPTR (src_setcaps));
}

