/* gskngliconlibrary.c
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include <gdk/gdkglcontextprivate.h>
#include <gdk/gdkmemoryformatprivate.h>
#include <gdk/gdkmemorytextureprivate.h>
#include <gdk/gdkprofilerprivate.h>
#include <gdk/gdktextureprivate.h>

#include "gsknglcommandqueueprivate.h"
#include "gskngldriverprivate.h"
#include "gskngliconlibraryprivate.h"

struct _GskNglIconLibrary
{
  GskNglTextureLibrary parent_instance;
};

G_DEFINE_TYPE (GskNglIconLibrary, gsk_ngl_icon_library, GSK_TYPE_GL_TEXTURE_LIBRARY)

GskNglIconLibrary *
gsk_ngl_icon_library_new (GskNglDriver *driver)
{
  g_return_val_if_fail (GSK_IS_NGL_DRIVER (driver), NULL);

  return g_object_new (GSK_TYPE_GL_ICON_LIBRARY,
                       "driver", driver,
                       NULL);
}

static void
gsk_ngl_icon_data_free (gpointer data)
{
  GskNglIconData *icon_data = data;

  g_clear_object (&icon_data->source_texture);
  g_slice_free (GskNglIconData, icon_data);
}

static void
gsk_ngl_icon_library_class_init (GskNglIconLibraryClass *klass)
{
}

static void
gsk_ngl_icon_library_init (GskNglIconLibrary *self)
{
  GskNglTextureLibrary *tl = (GskNglTextureLibrary *)self;

  tl->max_entry_size = 128;
  gsk_ngl_texture_library_set_funcs (tl,
                                     NULL, NULL, NULL,
                                     gsk_ngl_icon_data_free);
}

static GdkMemoryTexture *
gsk_ngl_texture_prepare_upload (GdkGLContext *context,
                                GdkTexture   *texture,
                                GLenum       *gl_internalformat,
                                GLenum       *gl_format,
                                GLenum       *gl_type)
{
  GdkMemoryTexture *memtex;
  GdkMemoryFormat format;

  memtex = GDK_MEMORY_TEXTURE (gdk_texture_download_texture (texture));
  format = gdk_memory_texture_get_format (memtex);

  if (!gdk_memory_format_gl_format (format,
                                    gdk_gl_context_get_use_es (context),
                                    gl_internalformat,
                                    gl_format,
                                    gl_type))
    {
      format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
      if (!gdk_memory_format_gl_format (format,
                                        gdk_gl_context_get_use_es (context),
                                        gl_internalformat,
                                        gl_format,
                                        gl_type))
        {
          g_assert_not_reached ();
        }
    }

  return gdk_memory_texture_convert (memtex,
                                     format,
                                     gdk_color_profile_get_srgb (),
                                     NULL);
}

static void
straightTexSubImage2D (int           tex,
                       int           level,
                       int           x,
                       int           y,
                       int           width,
                       int           height,
                       GLenum        gl_format,
                       GLenum        gl_type,
                       const guchar *data,
                       gsize         stride)
{
  glTexSubImage2D (tex, level,
                   x, y,
                   width, height,
                   gl_format, gl_type,
                   data);
}

static void
strideTexSubImage2D (int           tex,
                     int           level,
                     int           x,
                     int           y,
                     int           width,
                     int           height,
                     GLenum        gl_format,
                     GLenum        gl_type,
                     const guchar *data,
                     gsize         stride)
{
  for (int i = 0; i < height; i++)
    {
      glTexSubImage2D (tex, level,
                       x, y + i,
                       width, height,
                       gl_format, gl_type,
                       data + i * stride);
    }
}

void
gsk_ngl_icon_library_add (GskNglIconLibrary     *self,
                          GdkTexture            *key,
                          const GskNglIconData **out_value)
{
  GskNglTextureLibrary *tl = (GskNglTextureLibrary *)self;
  GdkGLContext *context = gdk_gl_context_get_current ();
  G_GNUC_UNUSED gint64 start_time = GDK_PROFILER_CURRENT_TIME;
  GdkMemoryTexture *memtex;
  GskNglIconData *icon_data;
  const guchar *pixel_data;
  gsize stride, bpp;
  GdkMemoryFormat format;
  GLenum gl_internalformat;
  GLenum gl_format;
  GLenum gl_type;
  guint packed_x;
  guint packed_y;
  int width;
  int height;
  guint texture_id;
  void (* upload_func) (int, int, int, int, int, int, GLenum, GLenum, const guchar *, gsize);

  g_assert (GSK_IS_NGL_ICON_LIBRARY (self));
  g_assert (GDK_IS_TEXTURE (key));
  g_assert (out_value != NULL);

  width = key->width;
  height = key->height;

  icon_data = gsk_ngl_texture_library_pack (tl,
                                            key,
                                            sizeof (GskNglIconData),
                                            width, height, 1,
                                            &packed_x, &packed_y);
  icon_data->source_texture = g_object_ref (key);

  /* actually upload the texture */
  gdk_gl_context_push_debug_group_printf (context,
                                          "Uploading texture");
  memtex = gsk_ngl_texture_prepare_upload (context,
                                           key,
                                           &gl_internalformat,
                                           &gl_format,
                                           &gl_type);

  pixel_data = gdk_memory_texture_get_data (memtex);
  stride = gdk_memory_texture_get_stride (memtex);
  format = gdk_memory_texture_get_format (memtex);
  bpp = gdk_memory_format_bytes_per_pixel (format);

  texture_id = GSK_NGL_TEXTURE_ATLAS_ENTRY_TEXTURE (icon_data);

  glBindTexture (GL_TEXTURE_2D, texture_id);

  glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
  /* GL_UNPACK_ROW_LENGTH is available on desktop GL, OpenGL ES >= 3.0, or if
   * the GL_EXT_unpack_subimage extension for OpenGL ES 2.0 is available
   */
  if ((stride % bpp == 0) &&
      (!gdk_gl_context_get_use_es (context) ||
       gdk_gl_context_has_version (context, 3, 0) ||
       gdk_gl_context_has_unpack_subimage (context)))
    {
      upload_func = straightTexSubImage2D;
      glPixelStorei (GL_UNPACK_ROW_LENGTH, stride / bpp);
    }
  else
    {
      upload_func = strideTexSubImage2D;
    }

  upload_func (GL_TEXTURE_2D, 0,
               packed_x + 1, packed_y + 1,
               width, height,
               gl_format, gl_type,
               pixel_data,
               stride);
  /* Padding top */
  upload_func (GL_TEXTURE_2D, 0,
               packed_x + 1, packed_y,
               width, 1,
               gl_format, gl_type,
               pixel_data,
               stride);
  /* Padding left */
  upload_func (GL_TEXTURE_2D, 0,
               packed_x, packed_y + 1,
               1, height,
               gl_format, gl_type,
               pixel_data,
               stride);
  /* Padding top left */
  upload_func (GL_TEXTURE_2D, 0,
               packed_x, packed_y,
               1, 1,
               gl_format, gl_type,
               pixel_data,
               stride);

  /* Padding right */
  upload_func (GL_TEXTURE_2D, 0,
               packed_x + width + 1, packed_y + 1,
               1, height,
               gl_format, gl_type,
               pixel_data + (width - 1) * bpp,
               stride);
  /* Padding top right */
  upload_func (GL_TEXTURE_2D, 0,
               packed_x + width + 1, packed_y,
               1, 1,
               gl_format, gl_type,
               pixel_data + (width - 1) * bpp,
               stride);
  /* Padding bottom */
  upload_func (GL_TEXTURE_2D, 0,
               packed_x + 1, packed_y + 1 + height,
               width, 1,
               gl_format, gl_type,
               pixel_data + (height - 1) * stride,
               stride);
  /* Padding bottom left */
  upload_func (GL_TEXTURE_2D, 0,
               packed_x, packed_y + 1 + height,
               1, 1,
               gl_format, gl_type,
               pixel_data + (height - 1) * stride,
               stride);
  /* Padding bottom right */
  upload_func (GL_TEXTURE_2D, 0,
               packed_x + 1 + width, packed_y + 1 + height,
               1, 1,
               gl_format, gl_type,
               pixel_data + (width - 1) * bpp + (height - 1) * stride,
               stride);

  /* Reset this */
  glPixelStorei (GL_UNPACK_ALIGNMENT, 4);
  if ((stride % bpp == 0) &&
      (!gdk_gl_context_get_use_es (context) ||
      gdk_gl_context_has_version (context, 3, 0) ||
      gdk_gl_context_has_unpack_subimage (context)))
    glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);

  gdk_gl_context_pop_debug_group (gdk_gl_context_get_current ());

  *out_value = icon_data;

  g_object_unref (memtex);

  tl->driver->command_queue->n_uploads++;

  if (gdk_profiler_is_running ())
    {
      char message[64];
      g_snprintf (message, sizeof message, "Size %dx%d", width, height);
      gdk_profiler_add_mark (start_time, GDK_PROFILER_CURRENT_TIME-start_time, "Upload Icon", message);
    }
}
