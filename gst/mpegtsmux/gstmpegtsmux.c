/*
 * Copyright 2006, 2007, 2008, 2009, 2010 Fluendo S.A.
 *  Authors: Jan Schmidt <jan@fluendo.com>
 *           Kapil Agrawal <kapil@fluendo.com>
 *           Julien Moutte <julien@fluendo.com>
 *
 * Copyright (C) 2011 Jan Schmidt <thaytan@noraisin.net>
 *
 * This library is licensed under 3 different licenses and you
 * can choose to use it under the terms of any one of them. The
 * three licenses are the MPL 1.1, the LGPL and the MIT license.
 *
 * MPL:
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/.
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * LGPL:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * MIT:
 *
 * Unless otherwise indicated, Source Code is licensed under MIT license.
 * See further explanation attached in License Statement (distributed in the file
 * LICENSE).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MPL-1.1 OR MIT OR LGPL-2.0-or-later
 */

/**
 * SECTION: element-mpegtsmux
 * @title: MPEG Transport Stream muxer
 *
 * mpegtsmux muxes different streams into an MPEG Transport stream
 *
 * SI sections can be specified through a custom event:
 *
 * {{ tests/examples/mpegts/ts-section-writer.c }}
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstmpegtsmux.h"
#include "gstbasetsmux.h"
#include "tsmux/tsmux.h"
#include <string.h>

#define MPEGTSMUX_DEFAULT_M2TS         FALSE

/* Default PID values */
#define MPEGTSMUX_DEFAULT_AUDIO_PID    0x0100
#define MPEGTSMUX_DEFAULT_VIDEO_PID    0x0101
#define MPEGTSMUX_DEFAULT_PMT_PID      0x1000
#define MPEGTSMUX_DEFAULT_SERVICE_ID   0x0001

#define M2TS_PACKET_LENGTH      192

enum
{
  PROP_0,
  PROP_M2TS_MODE,
  PROP_AUDIO_PID,
  PROP_VIDEO_PID,
  PROP_PMT_PID,
  PROP_SERVICE_ID,
};

static GstStaticPadTemplate gst_mpeg_ts_mux_sink_factory =
    GST_STATIC_PAD_TEMPLATE ("sink_%d",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("video/mpeg, "
        "parsed = (boolean) TRUE, "
        "mpegversion = (int) { 1, 2, 4 }, "
        "systemstream = (boolean) false; "
        "video/x-dirac;"
        "image/x-jpc, alignment = (string) frame;"
        "video/x-h264,stream-format=(string)byte-stream,"
        "alignment=(string){au, nal}; "
        "video/x-h265,stream-format=(string)byte-stream,"
        "alignment=(string){au, nal}; "
        "audio/mpeg, "
        "parsed = (boolean) TRUE, "
        "mpegversion = (int) 1;"
        "audio/mpeg, "
        "framed = (boolean) TRUE, "
        "mpegversion = (int) {2, 4}, stream-format = (string) { adts, raw };"
        "audio/x-lpcm, "
        "width = (int) { 16, 20, 24 }, "
        "rate = (int) { 48000, 96000 }, "
        "channels = (int) [ 1, 8 ], "
        "dynamic_range = (int) [ 0, 255 ], "
        "emphasis = (boolean) { FALSE, TRUE }, "
        "mute = (boolean) { FALSE, TRUE }; "
        "audio/x-ac3, framed = (boolean) TRUE;"
        "audio/x-dts, framed = (boolean) TRUE;"
        "audio/x-opus, "
        "channels = (int) [1, 8], "
        "channel-mapping-family = (int) {0, 1};"
        "subpicture/x-dvb; application/x-teletext; meta/x-klv, parsed=true;"
        "image/x-jpc, alignment = (string) frame, profile = (int)[0, 49151];"));

static GstStaticPadTemplate gst_mpeg_ts_mux_src_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/mpegts, "
        "systemstream = (boolean) true, " "packetsize = (int) { 188, 192} ")
    );

GST_DEBUG_CATEGORY (gst_mpeg_ts_mux_debug);
#define GST_CAT_DEFAULT gst_mpeg_ts_mux_debug

#define parent_class gst_mpeg_ts_mux_parent_class
G_DEFINE_TYPE (GstMpegTsMux, gst_mpeg_ts_mux, GST_TYPE_BASE_TS_MUX);
GST_ELEMENT_REGISTER_DEFINE (mpegtsmux, "mpegtsmux", GST_RANK_PRIMARY,
    gst_mpeg_ts_mux_get_type ());

/* Internals */

static gboolean
new_packet_m2ts (GstMpegTsMux * mux, GstBuffer * buf, gint64 new_pcr)
{
  GstBuffer *out_buf;
  int chunk_bytes;
  GstMapInfo map;

  GST_LOG_OBJECT (mux, "Have buffer %p with new_pcr=%" G_GINT64_FORMAT,
      buf, new_pcr);

  chunk_bytes = gst_adapter_available (mux->adapter);

  if (G_LIKELY (buf)) {
    if (new_pcr < 0) {
      /* If there is no pcr in current ts packet then just add the packet
         to the adapter for later output when we see a PCR */
      GST_LOG_OBJECT (mux, "Accumulating non-PCR packet");
      gst_adapter_push (mux->adapter, buf);
      goto exit;
    }

    /* no first interpolation point yet, then this is the one,
     * otherwise it is the second interpolation point */
    if (mux->previous_pcr < 0 && chunk_bytes) {
      mux->previous_pcr = new_pcr;
      mux->previous_offset = chunk_bytes;
      GST_LOG_OBJECT (mux, "Accumulating non-PCR packet");
      gst_adapter_push (mux->adapter, buf);
      goto exit;
    }
  } else {
    g_assert (new_pcr == -1);
  }

  /* interpolate if needed, and 2 points available */
  if (chunk_bytes && (new_pcr != mux->previous_pcr)) {
    gint64 offset = 0;

    GST_LOG_OBJECT (mux, "Processing pending packets; "
        "previous pcr %" G_GINT64_FORMAT ", previous offset %d, "
        "current pcr %" G_GINT64_FORMAT ", current offset %d",
        mux->previous_pcr, (gint) mux->previous_offset,
        new_pcr, (gint) chunk_bytes);

    g_assert (chunk_bytes > mux->previous_offset);
    /* if draining, use previous rate */
    if (G_LIKELY (new_pcr > 0)) {
      mux->pcr_rate_num = new_pcr - mux->previous_pcr;
      mux->pcr_rate_den = chunk_bytes - mux->previous_offset;
    }

    while (offset < chunk_bytes) {
      guint64 cur_pcr, ts;

      /* Loop, pulling packets of the adapter, updating their 4 byte
       * timestamp header and pushing */

      /* interpolate PCR */
      if (G_LIKELY (offset >= mux->previous_offset))
        cur_pcr = mux->previous_pcr +
            gst_util_uint64_scale (offset - mux->previous_offset,
            mux->pcr_rate_num, mux->pcr_rate_den);
      else
        cur_pcr = mux->previous_pcr -
            gst_util_uint64_scale (mux->previous_offset - offset,
            mux->pcr_rate_num, mux->pcr_rate_den);

      /* FIXME: what about DTS here? */
      ts = gst_adapter_prev_pts (mux->adapter, NULL);
      out_buf = gst_adapter_take_buffer (mux->adapter, M2TS_PACKET_LENGTH);
      g_assert (out_buf);
      offset += M2TS_PACKET_LENGTH;

      GST_BUFFER_PTS (out_buf) = ts;

      gst_buffer_map (out_buf, &map, GST_MAP_WRITE);

      /* The header is the bottom 30 bits of the PCR, apparently not
       * encoded into base + ext as in the packets themselves */
      GST_WRITE_UINT32_BE (map.data, cur_pcr & 0x3FFFFFFF);
      gst_buffer_unmap (out_buf, &map);

      GST_LOG_OBJECT (mux, "Outputting a packet of length %d PCR %"
          G_GUINT64_FORMAT, M2TS_PACKET_LENGTH, cur_pcr);
      ((GstBaseTsMuxClass *)
          parent_class)->output_packet (GST_BASE_TS_MUX (mux), out_buf, -1);
    }
  }

  if (G_UNLIKELY (!buf))
    goto exit;

  gst_buffer_map (buf, &map, GST_MAP_WRITE);

  /* Finally, output the passed in packet */
  /* Only write the bottom 30 bits of the PCR */
  GST_WRITE_UINT32_BE (map.data, new_pcr & 0x3FFFFFFF);

  gst_buffer_unmap (buf, &map);

  GST_LOG_OBJECT (mux, "Outputting a packet of length %d PCR %"
      G_GUINT64_FORMAT, M2TS_PACKET_LENGTH, new_pcr);

  ((GstBaseTsMuxClass *) parent_class)->output_packet (GST_BASE_TS_MUX (mux),
      buf, -1);

  if (new_pcr != mux->previous_pcr) {
    mux->previous_pcr = new_pcr;
    mux->previous_offset = -M2TS_PACKET_LENGTH;
  }

exit:
  return TRUE;
}

/* GstBaseTsMux implementation */

static void
gst_mpeg_ts_mux_allocate_packet (GstBaseTsMux * mux, GstBuffer ** buffer)
{
  ((GstBaseTsMuxClass *) parent_class)->allocate_packet (mux, buffer);

  gst_buffer_set_size (*buffer, GST_BASE_TS_MUX_NORMAL_PACKET_LENGTH);
}

static gboolean
gst_mpeg_ts_mux_output_packet (GstBaseTsMux * base_tsmux, GstBuffer * buffer,
    gint64 new_pcr)
{
  GstMpegTsMux *mux = GST_MPEG_TS_MUX (base_tsmux);
  GstMapInfo map;

  if (!mux->m2ts_mode)
    return ((GstBaseTsMuxClass *) parent_class)->output_packet (base_tsmux,
        buffer, new_pcr);

  gst_buffer_set_size (buffer, M2TS_PACKET_LENGTH);

  gst_buffer_map (buffer, &map, GST_MAP_READWRITE);

  /* there should be a better way to do this */
  memmove (map.data + 4, map.data, map.size - 4);

  gst_buffer_unmap (buffer, &map);

  return new_packet_m2ts (mux, buffer, new_pcr);
}

static void
gst_mpeg_ts_mux_reset (GstBaseTsMux * base_tsmux)
{
  GstMpegTsMux *mux = GST_MPEG_TS_MUX (base_tsmux);

  if (mux->adapter)
    gst_adapter_clear (mux->adapter);

  mux->previous_pcr = -1;
  mux->previous_offset = 0;
  mux->pcr_rate_num = mux->pcr_rate_den = 1;
}

static gboolean
gst_mpeg_ts_mux_drain (GstBaseTsMux * mux)
{
  return new_packet_m2ts (GST_MPEG_TS_MUX (mux), NULL, -1);
}


static void
gst_mpeg_ts_mux_configure_tsmux (GstMpegTsMux * mux)
{
  GstBaseTsMux *base_mux = GST_BASE_TS_MUX (mux);
  
  if (base_mux->tsmux) {
    /* Set the transport stream ID (service ID) */
    base_mux->tsmux->transport_id = mux->service_id;
    
    /* Set PMT PID for the default program if specified */
    if (mux->pmt_pid > 0) {
      GList *cur;
      for (cur = base_mux->tsmux->programs; cur; cur = cur->next) {
        TsMuxProgram *program = (TsMuxProgram *) cur->data;
        if (program->pgm_number == 1) { /* Default program */
          tsmux_program_set_pmt_pid (program, mux->pmt_pid);
          break;
        }
      }
    }
  }
}

/* GObject implementation */

static void
gst_mpeg_ts_mux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMpegTsMux *mux = GST_MPEG_TS_MUX (object);

  switch (prop_id) {
    case PROP_M2TS_MODE:
      /* set in case if the output stream need to be of 192 bytes */
      mux->m2ts_mode = g_value_get_boolean (value);
      gst_base_ts_mux_set_packet_size (GST_BASE_TS_MUX (mux),
          mux->m2ts_mode ? M2TS_PACKET_LENGTH :
          GST_BASE_TS_MUX_NORMAL_PACKET_LENGTH);
      gst_base_ts_mux_set_automatic_alignment (GST_BASE_TS_MUX (mux),
          mux->m2ts_mode ? 32 : 0);
      break;
    case PROP_AUDIO_PID:
      {
        guint pid = g_value_get_uint (value);
        if (pid == 0 || (pid >= 0x0100 && pid <= 0x1FFE)) {
          mux->audio_pid = pid;
        } else {
          GST_WARNING_OBJECT (mux, "Invalid audio PID %u, must be 0 or 0x0100-0x1FFE", pid);
        }
      }
      /* Note: This only affects new streams, existing streams keep their PIDs */
      break;
    case PROP_VIDEO_PID:
      {
        guint pid = g_value_get_uint (value);
        if (pid == 0 || (pid >= 0x0100 && pid <= 0x1FFE)) {
          mux->video_pid = pid;
        } else {
          GST_WARNING_OBJECT (mux, "Invalid video PID %u, must be 0 or 0x0100-0x1FFE", pid);
        }
      }
      /* Note: This only affects new streams, existing streams keep their PIDs */
      break;
    case PROP_PMT_PID:
      {
        guint pid = g_value_get_uint (value);
        if (pid == 0 || (pid >= 0x0020 && pid <= 0x1FFE)) {
          mux->pmt_pid = pid;
          /* Reconfigure the tsmux if it already exists */
          gst_mpeg_ts_mux_configure_tsmux (mux);
        } else {
          GST_WARNING_OBJECT (mux, "Invalid PMT PID %u, must be 0 or 0x0020-0x1FFE", pid);
        }
      }
      break;
    case PROP_SERVICE_ID:
      {
        guint service_id = g_value_get_uint (value);
        if (service_id >= 0x0001 && service_id <= 0xFFFF) {
          mux->service_id = service_id;
          /* Reconfigure the tsmux if it already exists */
          gst_mpeg_ts_mux_configure_tsmux (mux);
        } else {
          GST_WARNING_OBJECT (mux, "Invalid service ID %u, must be 0x0001-0xFFFF", service_id);
        }
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mpeg_ts_mux_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstMpegTsMux *mux = GST_MPEG_TS_MUX (object);

  switch (prop_id) {
    case PROP_M2TS_MODE:
      g_value_set_boolean (value, mux->m2ts_mode);
      break;
    case PROP_AUDIO_PID:
      g_value_set_uint (value, mux->audio_pid);
      break;
    case PROP_VIDEO_PID:
      g_value_set_uint (value, mux->video_pid);
      break;
    case PROP_PMT_PID:
      g_value_set_uint (value, mux->pmt_pid);
      break;
    case PROP_SERVICE_ID:
      g_value_set_uint (value, mux->service_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static void
gst_base_ts_mux_pad_reset (GstBaseTsMuxPad * pad)
{
  pad->dts = GST_CLOCK_STIME_NONE;
  pad->prog_id = -1;

  if (pad->free_func)
    pad->free_func (pad->prepare_data);
  pad->prepare_data = NULL;
  pad->prepare_func = NULL;
  pad->free_func = NULL;

  if (pad->codec_data)
    gst_buffer_replace (&pad->codec_data, NULL);

  /* reference owned elsewhere */
  pad->stream = NULL;
  pad->prog = NULL;

  if (pad->language) {
    g_free (pad->language);
    pad->language = NULL;
  }
}

/* GstElement implementation */
static gboolean
gst_base_ts_mux_has_pad_with_pid (GstBaseTsMux * mux, guint16 pid)
{
  GList *l;
  gboolean res = FALSE;

  GST_OBJECT_LOCK (mux);

  for (l = GST_ELEMENT_CAST (mux)->sinkpads; l; l = l->next) {
    GstBaseTsMuxPad *tpad = GST_BASE_TS_MUX_PAD (l->data);

    if (tpad->pid == pid) {
      res = TRUE;
      break;
    }
  }

  GST_OBJECT_UNLOCK (mux);
  return res;
}

static GstPad *
gst_mpeg_ts_mux_request_new_pad (GstElement * element, GstPadTemplate * templ,
    const gchar * name, const GstCaps * caps)
{
  GstMpegTsMux *mux = GST_MPEG_TS_MUX (element);
  GstPad *pad = NULL;
  gint pid = -1;
  gchar *free_name = NULL;
  gboolean is_audio = FALSE;
  gboolean is_video = FALSE;

  /* Determine stream type from caps if available */
  if (caps) {
    GstStructure *s = gst_caps_get_structure (caps, 0);
    const gchar *media_type = gst_structure_get_string (s, NULL);
    
    if (media_type) {
      if (g_str_has_prefix (media_type, "audio/")) {
        is_audio = TRUE;
      } else if (g_str_has_prefix (media_type, "video/")) {
        is_video = TRUE;
      }
    }
  }

  /* Assign PID based on stream type and user preferences */
  if (name != NULL && sscanf (name, "sink_%d", &pid) == 1) {
    /* User specified PID, validate it */
    if (pid < TSMUX_START_ES_PID) {
      GST_ELEMENT_ERROR (element, STREAM, MUX,
          ("Invalid Elementary stream PID (0x%02u < 0x40)", pid), (NULL));
      return NULL;
    }
  } else {
    /* Auto-assign PID based on stream type */
    if (is_audio && mux->audio_pid > 0) {
      pid = mux->audio_pid;
    } else if (is_video && mux->video_pid > 0) {
      pid = mux->video_pid;
    } else {
      /* Use default auto-assignment from base class */
      GstBaseTsMux *base_mux = GST_BASE_TS_MUX (mux);
      do {
        pid = tsmux_get_new_pid (base_mux->tsmux);
      } while (gst_base_ts_mux_has_pad_with_pid (base_mux, pid));
    }
    
    /* Name the pad correctly after the selected pid */
    name = free_name = g_strdup_printf ("sink_%d", pid);
  }

  /* Check if PID is already in use */
  if (tsmux_find_stream (GST_BASE_TS_MUX (mux)->tsmux, pid)) {
    g_free (free_name);
    GST_ELEMENT_ERROR (element, STREAM, MUX, ("Duplicate PID requested"),
        (NULL));
    return NULL;
  }

  /* Create the pad using the base class */
  pad = (GstPad *)
      GST_ELEMENT_CLASS (gst_mpeg_ts_mux_parent_class)->request_new_pad (element,
      templ, name, caps);

  if (pad) {
    gst_base_ts_mux_pad_reset (GST_BASE_TS_MUX_PAD (pad));
    GST_BASE_TS_MUX_PAD (pad)->pid = pid;
  }

  g_free (free_name);
  return pad;
}



static void
gst_mpeg_ts_mux_dispose (GObject * object)
{
  GstMpegTsMux *mux = GST_MPEG_TS_MUX (object);

  if (mux->adapter) {
    g_object_unref (mux->adapter);
    mux->adapter = NULL;
  }

  GST_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}


static TsMux *
gst_mpeg_ts_mux_create_ts_mux (GstBaseTsMux * base_mux)
{
  GstMpegTsMux *mux = GST_MPEG_TS_MUX (base_mux);
  TsMux *tsmux = tsmux_new ();
  
  /* Set the transport stream ID (service ID) */
  tsmux->transport_id = mux->service_id;
  
  /* Set PMT PID for the default program if specified */
  if (mux->pmt_pid > 0) {
    /* Create a default program and set its PMT PID */
    TsMuxProgram *program = tsmux_program_new (tsmux, 1);
    if (program) {
      tsmux_program_set_pmt_pid (program, mux->pmt_pid);
      /* Add the program to the muxer */
      g_hash_table_insert (base_mux->programs, GINT_TO_POINTER (1), program);
    }
  }
  
  return tsmux;
}

static void
gst_mpeg_ts_mux_class_init (GstMpegTsMuxClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTsMuxClass *base_tsmux_class = GST_BASE_TS_MUX_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_mpeg_ts_mux_debug, "mpegtsmux", 0,
      "MPEG Transport Stream muxer");

  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_mpeg_ts_mux_set_property);
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_mpeg_ts_mux_get_property);
  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_mpeg_ts_mux_dispose);

  base_tsmux_class->allocate_packet =
      GST_DEBUG_FUNCPTR (gst_mpeg_ts_mux_allocate_packet);
  base_tsmux_class->output_packet =
      GST_DEBUG_FUNCPTR (gst_mpeg_ts_mux_output_packet);
  base_tsmux_class->reset = GST_DEBUG_FUNCPTR (gst_mpeg_ts_mux_reset);
  base_tsmux_class->drain = GST_DEBUG_FUNCPTR (gst_mpeg_ts_mux_drain);

  gst_element_class_set_static_metadata (gstelement_class,
      "MPEG Transport Stream Muxer", "Codec/Muxer",
      "Multiplexes media streams into an MPEG Transport Stream",
      "Fluendo <contact@fluendo.com>");

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &gst_mpeg_ts_mux_sink_factory, GST_TYPE_BASE_TS_MUX_PAD);

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &gst_mpeg_ts_mux_src_factory, GST_TYPE_AGGREGATOR_PAD);

  /* Override the request_new_pad function to handle custom PID assignment */
  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_mpeg_ts_mux_request_new_pad);

  /* Override the create_ts_mux function to configure custom PIDs and service ID */
  base_tsmux_class->create_ts_mux =
      GST_DEBUG_FUNCPTR (gst_mpeg_ts_mux_create_ts_mux);

  g_object_class_install_property (gobject_class, PROP_M2TS_MODE,
      g_param_spec_boolean ("m2ts-mode", "M2TS(192 bytes) Mode",
          "Set to TRUE to output Blu-Ray disc format with 192 byte packets. "
          "FALSE for standard TS format with 188 byte packets.",
          MPEGTSMUX_DEFAULT_M2TS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_AUDIO_PID,
      g_param_spec_uint ("audio-pid", "Audio PID",
          "PID to use for audio streams (0x0100-0x1FFE, 0 for auto)",
          0, 0x1FFE, MPEGTSMUX_DEFAULT_AUDIO_PID,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_VIDEO_PID,
      g_param_spec_uint ("video-pid", "Video PID",
          "PID to use for video streams (0x0100-0x1FFE, 0 for auto)",
          0, 0x1FFE, MPEGTSMUX_DEFAULT_VIDEO_PID,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PMT_PID,
      g_param_spec_uint ("pmt-pid", "PMT PID",
          "PID to use for PMT (0x0020-0x1FFE, 0 for auto)",
          0, 0x1FFE, MPEGTSMUX_DEFAULT_PMT_PID,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SERVICE_ID,
      g_param_spec_uint ("service-id", "Service ID",
          "Service ID (Transport Stream ID) for the PAT (0x0001-0xFFFF)",
          0x0001, 0xFFFF, MPEGTSMUX_DEFAULT_SERVICE_ID,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}



static void
gst_mpeg_ts_mux_init (GstMpegTsMux * mux)
{
  mux->m2ts_mode = MPEGTSMUX_DEFAULT_M2TS;
  mux->audio_pid = MPEGTSMUX_DEFAULT_AUDIO_PID;
  mux->video_pid = MPEGTSMUX_DEFAULT_VIDEO_PID;
  mux->pmt_pid = MPEGTSMUX_DEFAULT_PMT_PID;
  mux->service_id = MPEGTSMUX_DEFAULT_SERVICE_ID;
  mux->adapter = gst_adapter_new ();
}
