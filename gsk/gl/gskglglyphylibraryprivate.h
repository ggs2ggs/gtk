/* gskglglyphylibraryprivate.h
 *
 * Copyright 2020-2022 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __GSK_GL_GLYPHY_LIBRARY_PRIVATE_H__
#define __GSK_GL_GLYPHY_LIBRARY_PRIVATE_H__

#include <glyphy.h>
#include <pango/pango.h>

#include "gskgltexturelibraryprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_GL_GLYPHY_LIBRARY (gsk_gl_glyphy_library_get_type())

typedef guint FontKey;

extern GQuark quark_glyphy_font_key;

static inline FontKey
gsk_gl_glyphy_library_get_font_key (PangoFont *font)
{
  FontKey key;

  key = (FontKey) GPOINTER_TO_UINT (g_object_get_qdata ((GObject *)font, quark_glyphy_font_key));
  if (key == 0)
    {
      PangoFontDescription *desc = pango_font_describe (font);
      pango_font_description_set_size (desc, 10 * PANGO_SCALE);
      key = (FontKey) pango_font_description_hash (desc);
      pango_font_description_free (desc);
      g_object_set_qdata ((GObject *)font, quark_glyphy_font_key, GUINT_TO_POINTER (key));
    }

  return key;
}

static inline float
gsk_gl_glyphy_library_get_font_scale (PangoFont *font)
{
  hb_font_t *hbfont;
  int x_scale, y_scale;

  hbfont = pango_font_get_hb_font (font);
  hb_font_get_scale (hbfont, &x_scale, &y_scale);

  return MAX (x_scale, y_scale) / 1000.0;
}


typedef struct _GskGLGlyphyKey
{
  FontKey font;
  PangoGlyph glyph;
} GskGLGlyphyKey;

typedef struct _GskGLGlyphyValue
{
  GskGLTextureAtlasEntry entry;
  struct {
    float min_x;
    float min_y;
    float max_x;
    float max_y;
  } extents;
  guint nominal_w;
  guint nominal_h;
  guint atlas_x;
  guint atlas_y;
} GskGLGlyphyValue;

G_DECLARE_FINAL_TYPE (GskGLGlyphyLibrary, gsk_gl_glyphy_library, GSK, GL_GLYPHY_LIBRARY, GskGLTextureLibrary)

struct _GskGLGlyphyLibrary
{
  GskGLTextureLibrary parent_instance;
  glyphy_arc_accumulator_t *acc;
  GArray *acc_endpoints;
  guint item_w;
  guint item_h_q;
  struct {
    GskGLGlyphyKey key;
    const GskGLGlyphyValue *value;
  } front[256];
};

GskGLGlyphyLibrary *gsk_gl_glyphy_library_new (GskGLDriver             *driver);
gboolean            gsk_gl_glyphy_library_add (GskGLGlyphyLibrary      *self,
                                               GskGLGlyphyKey          *key,
                                               PangoFont               *font,
                                               const GskGLGlyphyValue **out_value);

static inline guint
gsk_gl_glyphy_library_lookup_or_add (GskGLGlyphyLibrary      *self,
                                     const GskGLGlyphyKey    *key,
                                     PangoFont               *font,
                                     const GskGLGlyphyValue **out_value)
{
  GskGLTextureAtlasEntry *entry;
  guint front_index = key->glyph & 0xFF;

  if (memcmp (key, &self->front[front_index], sizeof *key) == 0)
    {
      *out_value = self->front[front_index].value;
    }
  else if (gsk_gl_texture_library_lookup ((GskGLTextureLibrary *)self, key, &entry))
    {
      *out_value = (GskGLGlyphyValue *)entry;
      self->front[front_index].key = *key;
      self->front[front_index].value = *out_value;
    }
  else
    {
      GskGLGlyphyKey *k = g_slice_copy (sizeof *key, key);
      gsk_gl_glyphy_library_add (self, k, font, out_value);
      self->front[front_index].key = *key;
      self->front[front_index].value = *out_value;
    }

  return GSK_GL_TEXTURE_ATLAS_ENTRY_TEXTURE (*out_value);
}

G_END_DECLS

#endif /* __GSK_GL_GLYPHY_LIBRARY_PRIVATE_H__ */
