/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2021 Red Hat, Inc.
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

#include "gdktiffprivate.h"

#include "gdkcolorprofileprivate.h"
#include "gdkintl.h"
#include "gdkmemoryformatprivate.h"
#include "gdkmemorytextureprivate.h"
#include "gdkprofilerprivate.h"
#include "gdktexture.h"
#include "gdktextureprivate.h"

#include <tiffio.h>

/* Our main interest in tiff as an image format is that it is
 * flexible enough to save all our texture formats without
 * lossy conversions.
 *
 * The loader isn't meant to be a very versatile. It just aims
 * to load the subset that we're saving ourselves. For anything
 * else, we fall back to TIFFRGBAImage, which is the same api
 * that gdk-pixbuf uses.
 */

/* {{{ IO handling */

typedef struct
{
  GBytes **out_bytes;
  gchar *data;
  gsize size;
  gsize position;
} TiffIO;

static void
tiff_io_warning (const char *module,
                 const char *fmt,
                 va_list     ap) G_GNUC_PRINTF(2, 0);
static void
tiff_io_warning (const char *module,
                 const char *fmt,
                 va_list     ap)
{
  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, fmt, ap);
}

static void
tiff_io_error (const char *module,
               const char *fmt,
               va_list     ap) G_GNUC_PRINTF(2, 0);
static void
tiff_io_error (const char *module,
               const char *fmt,
               va_list     ap)
{
  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, fmt, ap);
}

static tsize_t
tiff_io_no_read (thandle_t handle,
                 tdata_t   buffer,
                 tsize_t   size)
{
  return (tsize_t) -1;
}

static tsize_t
tiff_io_read (thandle_t handle,
              tdata_t   buffer,
              tsize_t   size)
{
  TiffIO *io = (TiffIO *) handle;
  gsize read;

  if (io->position >= io->size)
    return 0;

  read = MIN (size, io->size - io->position);

  memcpy (buffer, io->data + io->position, read);
  io->position += read;

  return (tsize_t) read;
}

static tsize_t
tiff_io_no_write (thandle_t handle,
                  tdata_t   buffer,
                  tsize_t   size)
{
  return (tsize_t) -1;
}

static tsize_t
tiff_io_write (thandle_t handle,
               tdata_t   buffer,
               tsize_t   size)
{
  TiffIO *io = (TiffIO *) handle;

  if (io->position > io->size ||
      io->size - io->position < size)
    {
      io->size = io->position + size;
      io->data = g_realloc (io->data, io->size);
    }

  memcpy (io->data + io->position, buffer, size);
  io->position += size;

  return (tsize_t) size;
}

static toff_t
tiff_io_seek (thandle_t handle,
              toff_t    offset,
              int       whence)
{
  TiffIO *io = (TiffIO *) handle;

  switch (whence)
    {
    default:
      return -1;
    case SEEK_SET:
      break;
    case SEEK_CUR:
      offset += io->position;
      break;
    case SEEK_END:
      offset += io->size;
      break;
    }
  if (offset < 0)
    return -1;

  io->position = offset;

  return offset;
}

static int
tiff_io_close (thandle_t handle)
{
  TiffIO *io = (TiffIO *) handle;

  if (io->out_bytes)
    *io->out_bytes = g_bytes_new_take (io->data, io->size);

  g_free (io);

  return 0;
}

static toff_t
tiff_io_get_file_size (thandle_t handle)
{
  TiffIO *io = (TiffIO *) handle;

  return io->size;
}

static TIFF *
tiff_open_read (GBytes *bytes)
{
  TiffIO *io;

  TIFFSetWarningHandler ((TIFFErrorHandler) tiff_io_warning);
  TIFFSetErrorHandler ((TIFFErrorHandler) tiff_io_error);

  io = g_new0 (TiffIO, 1);

  io->data = (char *) g_bytes_get_data (bytes, &io->size);

  return TIFFClientOpen ("GTK-read", "r",
                         (thandle_t) io,
                         tiff_io_read,
                         tiff_io_no_write,
                         tiff_io_seek,
                         tiff_io_close,
                         tiff_io_get_file_size,
                         NULL, NULL);
}

static TIFF *
tiff_open_write (GBytes **result)
{
  TiffIO *io;

  TIFFSetWarningHandler ((TIFFErrorHandler) tiff_io_warning);
  TIFFSetErrorHandler ((TIFFErrorHandler) tiff_io_error);

  io = g_new0 (TiffIO, 1);

  io->out_bytes = result;

  return TIFFClientOpen ("GTK-write", "w",
                         (thandle_t) io,
                         tiff_io_no_read,
                         tiff_io_write,
                         tiff_io_seek,
                         tiff_io_close,
                         tiff_io_get_file_size,
                         NULL, NULL);
}

/* }}} */
/* {{{ Format conversion */

static void
flip_02 (guchar *data,
         int     width,
         int     height,
         int     stride)
{ 
  gsize x, y;
  
  for (y = 0; y < height; y++)
    {
      for (x = 0; x < width; x++)
        {
          guchar tmp;
          tmp = data[x * 4];
          data[x * 4] = data[x * 4 + 2];
          data[x * 4 + 2] = tmp;
        }
      data += stride;
    }
}

/* }}} */
/* {{{ Color profile handling */

static GdkColorProfile *
gdk_tiff_get_color_profile (TIFF *tiff)
{
  const char *icc_data;
  guint icc_len;

  if (TIFFGetField (tiff, TIFFTAG_ICCPROFILE, &icc_len, &icc_data))
    {
      GBytes *icc_bytes;
      GdkColorProfile *profile;

      icc_bytes = g_bytes_new (icc_data, icc_len);
      profile = gdk_color_profile_new_from_icc_bytes (icc_bytes, NULL);
      g_bytes_unref (icc_bytes);

      if (profile)
        return profile;
    }

  return g_object_ref (gdk_color_profile_get_srgb ());
}

static void
gdk_tiff_set_color_profile (TIFF            *tiff,
                            GdkColorProfile *profile)
{
  GBytes *bytes = gdk_color_profile_get_icc_profile (profile);

  TIFFSetField (tiff, TIFFTAG_ICCPROFILE,
                g_bytes_get_size (bytes),
                g_bytes_get_data (bytes, NULL));

  g_bytes_unref (bytes);
}

/* }}} */
/* {{{ Public API */

static struct {
  GdkMemoryFormat format;
  guint16 bits_per_sample;
  guint16 samples_per_pixel;
  guint16 sample_format;
} format_data[] = {
  { GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,            8, 4, SAMPLEFORMAT_UINT   },
  { GDK_MEMORY_R8G8B8,                            8, 3, SAMPLEFORMAT_UINT   },
  { GDK_MEMORY_R16G16B16,                        16, 3, SAMPLEFORMAT_UINT   },
  { GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,       16, 4, SAMPLEFORMAT_UINT   },
  { GDK_MEMORY_R16G16B16_FLOAT,                  16, 3, SAMPLEFORMAT_IEEEFP },
  { GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED, 16, 4, SAMPLEFORMAT_IEEEFP },
  { GDK_MEMORY_R32G32B32_FLOAT,                  32, 3, SAMPLEFORMAT_IEEEFP },
  { GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED, 32, 4, SAMPLEFORMAT_IEEEFP },
};

GBytes *
gdk_save_tiff (GdkTexture *texture)
{
  TIFF *tif;
  int width, height, stride;
  guint16 bits_per_sample = 0;
  guint16 samples_per_pixel = 0;
  guint16 sample_format = 0;
  const guchar *line;
  const guchar *data;
  guchar *new_data = NULL;
  GBytes *result = NULL;
  GdkTexture *memory_texture;
  GdkMemoryFormat format;
  GdkColorProfile *color_profile;

  tif = tiff_open_write (&result);

  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);
  color_profile = gdk_texture_get_color_profile (texture);

  memory_texture = gdk_texture_download_texture (texture);
  format = gdk_memory_texture_get_format (GDK_MEMORY_TEXTURE (memory_texture));

  for (int i = 0; i < G_N_ELEMENTS (format_data); i++)
    {
      if (format == format_data[i].format)
        {
          data = gdk_memory_texture_get_data (GDK_MEMORY_TEXTURE (memory_texture));
          stride = gdk_memory_texture_get_stride (GDK_MEMORY_TEXTURE (memory_texture));
          bits_per_sample = format_data[i].bits_per_sample;
          samples_per_pixel = format_data[i].samples_per_pixel;
          sample_format = format_data[i].sample_format;
          break;
        }
    }

  if (bits_per_sample == 0)
    {
      /* An 8-bit format we don't have in the table, handle
       * it by converting to R8G8B8A8_PREMULTIPLIED
       */
      stride = width * 4;
      new_data = g_malloc (stride * height);
      gdk_texture_download (memory_texture, new_data, stride);
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
      flip_02 (new_data, width, height, stride);
#endif
      data = new_data;
      bits_per_sample = 8;
      samples_per_pixel = 4;
      sample_format = SAMPLEFORMAT_UINT;
    }

  TIFFSetField (tif, TIFFTAG_SOFTWARE, "GTK");
  TIFFSetField (tif, TIFFTAG_IMAGEWIDTH, width);
  TIFFSetField (tif, TIFFTAG_IMAGELENGTH, height);
  TIFFSetField (tif, TIFFTAG_BITSPERSAMPLE, bits_per_sample);
  TIFFSetField (tif, TIFFTAG_SAMPLESPERPIXEL, samples_per_pixel);
  TIFFSetField (tif, TIFFTAG_SAMPLEFORMAT, sample_format);
  TIFFSetField (tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
  TIFFSetField (tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);

  gdk_tiff_set_color_profile (tif, color_profile);

  if (samples_per_pixel > 3)
    {
      guint16 extra_samples[] = { EXTRASAMPLE_ASSOCALPHA };
      TIFFSetField (tif, TIFFTAG_EXTRASAMPLES, 1, extra_samples);
    }

  TIFFSetField (tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
  TIFFSetField (tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

  line = (const guchar *)data;
  for (int y = 0; y < height; y++)
    {
      if (TIFFWriteScanline (tif, (void *)line, y, 0) == -1)
        {
          TIFFClose (tif);
          g_free (new_data);
          g_object_unref (memory_texture);
          return NULL;
        }

      line += stride;
    }

  TIFFFlushData (tif);
  TIFFClose (tif);

  g_assert (result);

  g_free (new_data);
  g_object_unref (memory_texture);

  return result;
}

static GdkTexture *
load_fallback (TIFF             *tif,
               GError          **error)
{
  int width, height;
  guchar *data;
  GBytes *bytes;
  GdkColorProfile *profile;
  GdkTexture *texture;

  TIFFGetField (tif, TIFFTAG_IMAGEWIDTH, &width);
  TIFFGetField (tif, TIFFTAG_IMAGELENGTH, &height);

  data = g_malloc (width * height * 4);

  if (!TIFFReadRGBAImageOriented (tif, width, height, (guint32 *)data, ORIENTATION_TOPLEFT, 1))
    {
      g_set_error_literal (error,
                           GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_CORRUPT_IMAGE,
                           _("Failed to load RGB data from TIFF file"));
      g_free (data);
      return NULL;
    }

  profile = gdk_tiff_get_color_profile (tif);

  bytes = g_bytes_new_take (data, width * height * 4);

  texture = gdk_memory_texture_new_with_color_profile (width, height,
                                                       GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
                                                       profile,
                                                       bytes,
                                                       width * 4);

  g_bytes_unref (bytes);
  g_object_unref (profile);

  return texture;
}

GdkTexture *
gdk_load_tiff (GBytes  *input_bytes,
               GError **error)
{
  TIFF *tif;
  guint16 samples_per_pixel;
  guint16 bits_per_sample;
  guint16 photometric;
  guint16 planarconfig;
  guint16 sample_format;
  guint16 orientation;
  guint32 width, height;
  GdkMemoryFormat format;
  guchar *data, *line;
  gsize stride;
  int bpp;
  GBytes *bytes;
  GdkColorProfile *profile;
  GdkTexture *texture;
  G_GNUC_UNUSED gint64 before = GDK_PROFILER_CURRENT_TIME;

  tif = tiff_open_read (input_bytes);

  TIFFSetDirectory (tif, 0);

  TIFFGetFieldDefaulted (tif, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
  TIFFGetFieldDefaulted (tif, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);
  TIFFGetFieldDefaulted (tif, TIFFTAG_SAMPLEFORMAT, &sample_format);
  TIFFGetFieldDefaulted (tif, TIFFTAG_PHOTOMETRIC, &photometric);
  TIFFGetFieldDefaulted (tif, TIFFTAG_PLANARCONFIG, &planarconfig);
  TIFFGetFieldDefaulted (tif, TIFFTAG_ORIENTATION, &orientation);
  TIFFGetFieldDefaulted (tif, TIFFTAG_IMAGEWIDTH, &width);
  TIFFGetFieldDefaulted (tif, TIFFTAG_IMAGELENGTH, &height);

  if (samples_per_pixel == 4)
    {
      guint16 extra;
      guint16 *extra_types;

      if (!TIFFGetField (tif, TIFFTAG_EXTRASAMPLES, &extra, &extra_types))
        extra = 0;

      if (extra == 0 || extra_types[0] != EXTRASAMPLE_ASSOCALPHA)
        {
          texture = load_fallback (tif, error);
          TIFFClose (tif);
          return texture;
        }
    }

  format = 0;

  for (int i = 0; i < G_N_ELEMENTS (format_data); i++)
    {
      if (format_data[i].sample_format == sample_format &&
          format_data[i].bits_per_sample == bits_per_sample &&
          format_data[i].samples_per_pixel == samples_per_pixel)
        {
          format = format_data[i].format;
          break;
        }
    }

  if (format == 0 ||
      photometric != PHOTOMETRIC_RGB ||
      planarconfig != PLANARCONFIG_CONTIG ||
      TIFFIsTiled (tif) ||
      orientation != ORIENTATION_TOPLEFT)
    {
      texture = load_fallback (tif, error);
      TIFFClose (tif);
      return texture;
    }

  stride = width * gdk_memory_format_bytes_per_pixel (format);

  g_assert (TIFFScanlineSize (tif) == stride);

  data = g_try_malloc_n (height, stride);
  if (!data)
    {
      g_set_error (error,
                   GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_TOO_LARGE,
                   _("Not enough memory for image size %ux%u"), width, height);
      TIFFClose (tif);
      return NULL;
    }

  line = data;
  for (int y = 0; y < height; y++)
    {
      if (TIFFReadScanline (tif, line, y, 0) == -1)
        {
          g_set_error (error,
                       GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_CORRUPT_IMAGE,
                       _("Reading data failed at row %d"), y);
          TIFFClose (tif);
          g_free (data);
          return NULL;
        }

      line += stride;
    }

  profile = gdk_tiff_get_color_profile (tif);

  bpp = gdk_memory_format_bytes_per_pixel (format);
  bytes = g_bytes_new_take (data, width * height * bpp);

  texture = gdk_memory_texture_new_with_color_profile (width, height,
                                                       format, profile,
                                                       bytes, width * bpp);
  g_bytes_unref (bytes);

  g_object_unref (profile);

  TIFFClose (tif);

  if (GDK_PROFILER_IS_RUNNING)
    {
      gint64 end = GDK_PROFILER_CURRENT_TIME;
      if (end - before > 500000)
        gdk_profiler_add_mark (before, end - before, "tiff load", NULL);
    }

  return texture;
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
