/* Copyright (C) 2016 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <math.h>
#include <librsvg/rsvg.h>
#include <gdk/gdk.h>
#include "gdkpixbufutilsprivate.h"
#include "gtkscalerprivate.h"

#include "gdk/gdktextureprivate.h"

static GdkPixbuf *
load_from_stream (GdkPixbufLoader  *loader,
                  GInputStream     *stream,
                  GCancellable     *cancellable,
                  GError          **error)
{
  GdkPixbuf *pixbuf;
  gssize n_read;
  guchar buffer[65536];
  gboolean res;

  res = TRUE;
  while (1)
    {
      n_read = g_input_stream_read (stream, buffer, sizeof (buffer), cancellable, error);
      if (n_read < 0)
        {
          res = FALSE;
          error = NULL; /* Ignore further errors */
          break;
        }

      if (n_read == 0)
        break;

      if (!gdk_pixbuf_loader_write (loader, buffer, n_read, error))
        {
          res = FALSE;
          error = NULL;
          break;
        }
    }

  if (!gdk_pixbuf_loader_close (loader, error))
    {
      res = FALSE;
      error = NULL;
    }

  pixbuf = NULL;

  if (res)
    {
      pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
      if (pixbuf)
        g_object_ref (pixbuf);
    }

  return pixbuf;
}

static void
size_prepared_cb (GdkPixbufLoader *loader,
                  int              width,
                  int              height,
                  gpointer         data)
{
  double *scale = data;

  width = MAX (*scale * width, 1);
  height = MAX (*scale * height, 1);

  gdk_pixbuf_loader_set_size (loader, width, height);
}

/* Like gdk_pixbuf_new_from_stream_at_scale, but
 * load the image at its original size times the
 * given scale.
 */
GdkPixbuf *
_gdk_pixbuf_new_from_stream_scaled (GInputStream  *stream,
                                    double         scale,
                                    GCancellable  *cancellable,
                                    GError       **error)
{
  GdkPixbufLoader *loader;
  GdkPixbuf *pixbuf;

  loader = gdk_pixbuf_loader_new ();

  if (scale != 0)
    g_signal_connect (loader, "size-prepared",
                      G_CALLBACK (size_prepared_cb), &scale);

  pixbuf = load_from_stream (loader, stream, cancellable, error);

  g_object_unref (loader);

  return pixbuf;
}

static void
size_prepared_cb2 (GdkPixbufLoader *loader,
                   int              width,
                   int              height,
                   gpointer         data)
{
  int *scales = data;

  if (scales[2]) /* keep same aspect ratio as original, while fitting in given size */
    {
      double aspect = (double) height / width;

      /* First use given width and calculate size */
      width = scales[0];
      height = scales[0] * aspect;

      /* Check if it fits given height, otherwise scale down */
      if (height > scales[1])
        {
          width *= (double) scales[1] / height;
          height = scales[1];
        }
    }
  else
    {
      width = scales[0];
      height = scales[1];
    }

  gdk_pixbuf_loader_set_size (loader, width, height);
}

GdkPixbuf *
_gdk_pixbuf_new_from_stream_at_scale (GInputStream  *stream,
                                      int            width,
                                      int            height,
                                      gboolean       aspect,
                                      GCancellable  *cancellable,
                                      GError       **error)
{
  GdkPixbufLoader *loader;
  GdkPixbuf *pixbuf;
  int scales[3];

  loader = gdk_pixbuf_loader_new ();

  scales[0] = width;
  scales[1] = height;
  scales[2] = aspect;
  g_signal_connect (loader, "size-prepared",
                    G_CALLBACK (size_prepared_cb2), scales);

  pixbuf = load_from_stream (loader, stream, cancellable, error);

  g_object_unref (loader);

  return pixbuf;
}

GdkPixbuf *
_gdk_pixbuf_new_from_stream (GInputStream  *stream,
                             GCancellable  *cancellable,
                             GError       **error)
{
  return _gdk_pixbuf_new_from_stream_scaled (stream, 0, cancellable, error);
}

/* Like gdk_pixbuf_new_from_resource_at_scale, but
 * load the image at its original size times the
 * given scale.
 */
GdkPixbuf *
_gdk_pixbuf_new_from_resource_scaled (const char  *resource_path,
                                      double       scale,
                                      GError     **error)
{
  GInputStream *stream;
  GdkPixbuf *pixbuf;

  stream = g_resources_open_stream (resource_path, 0, error);
  if (stream == NULL)
    return NULL;

  pixbuf = _gdk_pixbuf_new_from_stream_scaled (stream, scale, NULL, error);
  g_object_unref (stream);

  return pixbuf;
}

GdkPixbuf *
_gdk_pixbuf_new_from_resource (const char   *resource_path,
                               GError      **error)
{
  return _gdk_pixbuf_new_from_resource_scaled (resource_path, 0, error);
}

GdkPixbuf *
_gdk_pixbuf_new_from_resource_at_scale (const char   *resource_path,
                                        int           width,
                                        int           height,
                                        gboolean      preserve_aspect,
                                        GError      **error)
{
  GInputStream *stream;
  GdkPixbuf *pixbuf;

  stream = g_resources_open_stream (resource_path, 0, error);
  if (stream == NULL)
    return NULL;

  pixbuf = _gdk_pixbuf_new_from_stream_at_scale (stream, width, height, preserve_aspect, NULL, error);
  g_object_unref (stream);

  return pixbuf;

}

static GdkPixbuf *
pixbuf_from_rsvg_handle (RsvgHandle *handle, int width, int height, const char *path, GError **error)
{
  cairo_surface_t *surface;
  GdkPixbuf *pixbuf = NULL;
  RsvgRectangle viewport;
  cairo_t *cr = NULL;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  if (cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS)
    {
      g_set_error (error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                   "Not enough memory to load %s",
                   path);
      goto out;
    }

  cr = cairo_create (surface);
  if (cairo_status (cr) != CAIRO_STATUS_SUCCESS)
    {
      goto out;
    }

  viewport.x = 0.0;
  viewport.y = 0.0;
  viewport.width = width;
  viewport.height = height;

  if (rsvg_handle_render_document (handle, cr, &viewport, error))
    {
      pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, width, height);
      if (!pixbuf)
        {
          g_set_error (error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                       "Not enough memory to convert SVG from %s to pixbuf",
                       path);
        }
    }
  else
    {
      g_prefix_error (error, "Could not render symbolic icon from %s: ", path);
    }

 out:

  if (cr)
    {
      cairo_destroy (cr);
    }

  if (surface)
    {
      cairo_surface_destroy (surface);
    }

  return pixbuf;
}

static char *
make_stylesheet (const char *fg_string,
                 const char *success_color_string,
                 const char *warning_color_string,
                 const char *error_color_string)
{
  return g_strconcat ("    rect,circle,path {\n"
                      "      fill: ", fg_string," !important;\n"
                      "    }\n"
                      "    .warning {\n"
                      "      fill: ", warning_color_string, " !important;\n"
                      "    }\n"
                      "    .error {\n"
                      "      fill: ", error_color_string ," !important;\n"
                      "    }\n"
                      "    .success {\n"
                      "      fill: ", success_color_string, " !important;\n"
                      "    }\n",
                      NULL);
}


static GdkPixbuf *
load_symbolic_svg (const char     *file_data,
                   gsize           file_data_len,
                   int             width,
                   int             height,
                   const char     *icon_width_str,
                   const char     *icon_height_str,
                   const char     *fg_string,
                   const char     *success_color_string,
                   const char     *warning_color_string,
                   const char     *error_color_string,
                   const char     *path,
                   GError        **error)
{
  GInputStream *stream;
  GdkPixbuf *pixbuf = NULL;
  char *stylesheet = make_stylesheet (fg_string, success_color_string, warning_color_string, error_color_string);
  char *data;
  RsvgHandle *handle;
  char *escaped_file_data = g_base64_encode ((guchar *) file_data, file_data_len);

  data = g_strconcat ("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
                      "<svg version=\"1.1\"\n"
                      "     xmlns=\"http://www.w3.org/2000/svg\"\n"
                      "     xmlns:xi=\"http://www.w3.org/2001/XInclude\"\n"
                      "     width=\"", icon_width_str, "\"\n"
                      "     height=\"", icon_height_str, "\">\n"
                      "  <style type=\"text/css\">\n",
                      stylesheet,
                      "  </style>\n"
                      "  <xi:include href=\"data:text/xml;base64,", escaped_file_data, "\"/>\n"
                      "</svg>",
                      NULL);

  stream = g_memory_input_stream_new_from_data (data, -1, g_free);

  handle = rsvg_handle_new_from_stream_sync (stream, NULL, RSVG_HANDLE_FLAGS_NONE, NULL, error);
  if (handle)
    {
      pixbuf = pixbuf_from_rsvg_handle (stream, width, height, path, error);
      g_object_unref (handle);
    }
  else
    {
      g_prefix_error (error, "Could not load symbolic icon from %s: ", path);
    }

  if (!rsvg_handle_set_stylesheet (handle, (const guint8 *) stylesheet, strlen (stylesheet), error))
    {
      g_prefix_error (error, "Could not set stylesheet for %s:", path);
      goto out;
    }

  pixbuf = pixbuf_from_rsvg_handle (handle, width, height, path, error);

 out:

  if (handle)
    g_object_unref (handle);

  g_object_unref (stream);
  g_free (stylesheet);
  g_free (escaped_file_data);

  return pixbuf;
}

static void
extract_plane (GdkPixbuf *src,
               GdkPixbuf *dst,
               int        from_plane,
               int        to_plane)
{
  guchar *src_data, *dst_data;
  int width, height, src_stride, dst_stride;
  guchar *src_row, *dst_row;
  int x, y;

  width = gdk_pixbuf_get_width (src);
  height = gdk_pixbuf_get_height (src);

  g_assert (width <= gdk_pixbuf_get_width (dst));
  g_assert (height <= gdk_pixbuf_get_height (dst));

  src_stride = gdk_pixbuf_get_rowstride (src);
  src_data = gdk_pixbuf_get_pixels (src);

  dst_data = gdk_pixbuf_get_pixels (dst);
  dst_stride = gdk_pixbuf_get_rowstride (dst);

  for (y = 0; y < height; y++)
    {
      src_row = src_data + src_stride * y;
      dst_row = dst_data + dst_stride * y;
      for (x = 0; x < width; x++)
        {
          dst_row[to_plane] = src_row[from_plane];
          src_row += 4;
          dst_row += 4;
        }
    }
}

/* @path is just used for error messages */
GdkPixbuf *
gtk_make_symbolic_pixbuf_from_data (const char  *file_data,
                                    gsize        file_len,
                                    int          width,
                                    int          height,
                                    double       scale,
                                    const char  *path,
                                    const char  *debug_output_basename,
                                    GError     **error)

{
  const char *r_string = "rgb(255,0,0)";
  const char *g_string = "rgb(0,255,0)";
  char *icon_width_str;
  char *icon_height_str;
  GdkPixbuf *loaded;
  GdkPixbuf *pixbuf = NULL;
  int plane;
  int icon_width, icon_height;

  /* Fetch size from the original icon */
  {
    GInputStream *stream = g_memory_input_stream_new_from_data (file_data, file_len, NULL);
    RsvgHandle *handle = rsvg_handle_new_from_stream_sync (stream, NULL, RSVG_HANDLE_FLAGS_NONE, NULL, error);
    gdouble svg_width, svg_height;
    gboolean has_size;

    g_object_unref (stream);

    if (!handle)
      {
        g_prefix_error (error, "Could not load symbolic icon from %s: ", path);
        return NULL;
      }

    has_size = rsvg_handle_get_intrinsic_size_in_pixels (handle, &svg_width, &svg_height);
    g_object_unref (handle);

    if (!has_size)
      {
        /* FIXME: get a better error domain/code */
        g_set_error (error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                     "Symbolic icon %s has no intrinsic size; please set one in its SVG",
                     path);
        return NULL;
      }

    icon_width = ceil (svg_width);
    icon_height = ceil (svg_height);
  }

  icon_width_str = g_strdup_printf ("%d", icon_width);
  icon_height_str = g_strdup_printf ("%d", icon_height);

  if (width == 0)
    width = icon_width * scale;
  if (height == 0)
    height = icon_height * scale;

  for (plane = 0; plane < 3; plane++)
    {
      /* Here we render the svg with all colors solid, this should
       * always make the alpha channel the same and it should match
       * the final alpha channel for all possible renderings. We
       * Just use it as-is for final alpha.
       *
       * For the 3 non-fg colors, we render once each with that
       * color as red, and every other color as green. The resulting
       * red will describe the amount of that color is in the
       * opaque part of the color. We store these as the rgb
       * channels, with the color of the fg being implicitly
       * the "rest", as all color fractions should add up to 1.
       */
      loaded = load_symbolic_svg (file_data, file_len,
                                  width, height,
                                  icon_width_str,
                                  icon_height_str,
                                  g_string,
                                  plane == 0 ? r_string : g_string,
                                  plane == 1 ? r_string : g_string,
                                  plane == 2 ? r_string : g_string,
                                  path,
                                  error);
      if (loaded == NULL)
        goto out;

      if (debug_output_basename)
        {
          char *filename;

          filename = g_strdup_printf ("%s.debug%d.png", debug_output_basename, plane);
          g_print ("Writing %s\n", filename);
          gdk_pixbuf_save (loaded, filename, "png", NULL, NULL);
          g_free (filename);
        }

      if (pixbuf == NULL)
        {
          pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8,
                                   gdk_pixbuf_get_width (loaded),
                                   gdk_pixbuf_get_height (loaded));
          gdk_pixbuf_fill (pixbuf, 0);
        }

      if (plane == 0)
        extract_plane (loaded, pixbuf, 3, 3);

      extract_plane (loaded, pixbuf, 0, plane);

      g_object_unref (loaded);
    }

out:
  g_free (icon_width_str);
  g_free (icon_height_str);

  return pixbuf;
}

GdkPixbuf *
gtk_make_symbolic_pixbuf_from_resource (const char  *path,
                                        int          width,
                                        int          height,
                                        double       scale,
                                        GError     **error)
{
  GBytes *bytes;
  const char *data;
  gsize size;
  GdkPixbuf *pixbuf;

  bytes = g_resources_lookup_data (path, G_RESOURCE_LOOKUP_FLAGS_NONE, error);
  if (bytes == NULL)
    return NULL;

  data = g_bytes_get_data (bytes, &size);

  pixbuf = gtk_make_symbolic_pixbuf_from_data (data, size, width, height, scale, path, NULL, error);

  g_bytes_unref (bytes);

  return pixbuf;
}

GdkPixbuf *
gtk_make_symbolic_pixbuf_from_path (const char  *path,
                                    int          width,
                                    int          height,
                                    double       scale,
                                    GError     **error)
{
  char *data;
  gsize size;
  GdkPixbuf *pixbuf;

  if (!g_file_get_contents (path, &data, &size, error))
    return NULL;

  pixbuf = gtk_make_symbolic_pixbuf_from_data (data, size, width, height, scale, path, NULL, error);

  g_free (data);

  return pixbuf;
}

GdkPixbuf *
gtk_make_symbolic_pixbuf_from_file (GFile       *file,
                                    int          width,
                                    int          height,
                                    double       scale,
                                    GError     **error)
{
  char *data;
  gsize size;
  GdkPixbuf *pixbuf;
  char *uri;

  if (!g_file_load_contents (file, NULL, &data, &size, NULL, error))
    return NULL;

  uri = g_file_get_uri (file);
  pixbuf = gtk_make_symbolic_pixbuf_from_data (data, size, width, height, scale, uri, NULL, error);
  g_free (uri);

  g_free (data);

  return pixbuf;
}

GdkTexture *
gtk_load_symbolic_texture_from_resource (const char *path)
{
  return gdk_texture_new_from_resource (path);
}

GdkTexture *
gtk_make_symbolic_texture_from_resource (const char  *path,
                                         int          width,
                                         int          height,
                                         double       scale,
                                         GError     **error)
{
  GdkPixbuf *pixbuf;
  GdkTexture *texture = NULL;

  pixbuf = gtk_make_symbolic_pixbuf_from_resource (path, width, height, scale, error);
  if (pixbuf)
    {
      texture = gdk_texture_new_for_pixbuf (pixbuf);
      g_object_unref (pixbuf);
    }

  return texture;
}

GdkTexture *
gtk_load_symbolic_texture_from_file (GFile *file)
{
  GdkPixbuf *pixbuf;
  GdkTexture *texture;
  GInputStream *stream;

  stream = G_INPUT_STREAM (g_file_read (file, NULL, NULL));
  if (stream == NULL)
    return NULL;

  pixbuf = _gdk_pixbuf_new_from_stream (stream, NULL, NULL);
  g_object_unref (stream);
  if (pixbuf == NULL)
    return NULL;

  texture = gdk_texture_new_for_pixbuf (pixbuf);
  g_object_unref (pixbuf);

  return texture;
}

GdkTexture *
gtk_make_symbolic_texture_from_file (GFile       *file,
                                     int          width,
                                     int          height,
                                     double       scale,
                                     GError     **error)
{
  GdkPixbuf *pixbuf;
  GdkTexture *texture;

  pixbuf = gtk_make_symbolic_pixbuf_from_file (file, width, height, scale, error);
  texture = gdk_texture_new_for_pixbuf (pixbuf);
  g_object_unref (pixbuf);

  return texture;
}

typedef struct {
  int scale_factor;
} LoaderData;

static void
on_loader_size_prepared (GdkPixbufLoader *loader,
                         int              width,
                         int              height,
                         gpointer         user_data)
{
  LoaderData *loader_data = user_data;
  GdkPixbufFormat *format;

  /* Let the regular icon helper code path handle non-scalable images */
  format = gdk_pixbuf_loader_get_format (loader);
  if (!gdk_pixbuf_format_is_scalable (format))
    {
      loader_data->scale_factor = 1;
      return;
    }

  gdk_pixbuf_loader_set_size (loader,
                              width * loader_data->scale_factor,
                              height * loader_data->scale_factor);
}

GdkPaintable *
gdk_paintable_new_from_bytes_scaled (GBytes *bytes,
                                     int     scale_factor)
{
  LoaderData loader_data;
  GdkTexture *texture;
  GdkPaintable *paintable;

  loader_data.scale_factor = scale_factor;

  if (gdk_texture_can_load (bytes))
    {
      /* We know these formats can't be scaled */
      texture = gdk_texture_new_from_bytes (bytes, NULL);
      if (texture == NULL)
        return NULL;
    }
  else
    {
      GdkPixbufLoader *loader;
      gboolean success;

      loader = gdk_pixbuf_loader_new ();
      g_signal_connect (loader, "size-prepared",
                        G_CALLBACK (on_loader_size_prepared), &loader_data);

      success = gdk_pixbuf_loader_write_bytes (loader, bytes, NULL);
      /* close even when writing failed */
      success &= gdk_pixbuf_loader_close (loader, NULL);

      if (!success)
        return NULL;

      texture = gdk_texture_new_for_pixbuf (gdk_pixbuf_loader_get_pixbuf (loader));
      g_object_unref (loader);
    }

  if (loader_data.scale_factor != 1)
    paintable = gtk_scaler_new (GDK_PAINTABLE (texture), loader_data.scale_factor);
  else
    paintable = g_object_ref ((GdkPaintable *)texture);

  g_object_unref (texture);

  return paintable;
}

GdkPaintable *
gdk_paintable_new_from_path_scaled (const char *path,
                                    int         scale_factor)
{
  char *contents;
  gsize length;
  GBytes *bytes;
  GdkPaintable *paintable;

  if (!g_file_get_contents (path, &contents, &length, NULL))
    return NULL;

  bytes = g_bytes_new_take (contents, length);

  paintable = gdk_paintable_new_from_bytes_scaled (bytes, scale_factor);

  g_bytes_unref (bytes);

  return paintable;
}

GdkPaintable *
gdk_paintable_new_from_resource_scaled (const char *path,
                                        int         scale_factor)
{
  GBytes *bytes;
  GdkPaintable *paintable;

  bytes = g_resources_lookup_data (path, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  if (!bytes)
    return NULL;

  paintable = gdk_paintable_new_from_bytes_scaled (bytes, scale_factor);
  g_bytes_unref (bytes);

  return paintable;
}

GdkPaintable *
gdk_paintable_new_from_file_scaled (GFile *file,
                                    int    scale_factor)
{
  GBytes *bytes;
  GdkPaintable *paintable;

  bytes = g_file_load_bytes (file, NULL, NULL, NULL);
  if (!bytes)
    return NULL;

  paintable = gdk_paintable_new_from_bytes_scaled (bytes, scale_factor);

  g_bytes_unref (bytes);

  return paintable;
}
