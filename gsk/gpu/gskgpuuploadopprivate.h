#pragma once

#include "gskgpuopprivate.h"

#include "gsktypes.h"
#include "gsk/gskscaleprivate.h"

G_BEGIN_DECLS

typedef void            (* GskGpuCairoFunc)                             (gpointer                        user_data,
                                                                         cairo_t                        *cr);

GskGpuImage *           gsk_gpu_upload_texture_op_try                   (GskGpuFrame                    *frame,
                                                                         gboolean                        with_mipmap,
                                                                         GdkTexture                     *texture);

GskGpuImage *           gsk_gpu_upload_cairo_op                         (GskGpuFrame                    *frame,
                                                                         const GskScale                 *scale,
                                                                         const graphene_rect_t          *viewport,
                                                                         GskGpuCairoFunc                 func,
                                                                         gpointer                        user_data,
                                                                         GDestroyNotify                  user_destroy);

void                    gsk_gpu_upload_glyph_op                         (GskGpuFrame                    *frame,
                                                                         GskGpuImage                    *image,
                                                                         PangoFont                      *font,
                                                                         PangoGlyph                      glyph,
                                                                         const cairo_rectangle_int_t    *area,
                                                                         float                           scale,
                                                                         const graphene_point_t         *origin);

G_END_DECLS

