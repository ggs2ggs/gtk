#pragma once

#include "gskgpushaderopprivate.h"

#include "gskrendernode.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_linear_gradient_op                      (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         gboolean                        repeating,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *start,
                                                                         const graphene_point_t         *end,
                                                                         const graphene_point_t         *offset,
                                                                         GdkColorState                 *in,
                                                                         GdkColorState                 *target,
                                                                         GskHueInterpolation            hue_interp,
                                                                         const GskColorStop2           *stops,
                                                                         gsize                          n_stops);


int  gsk_get_hue_coord (GdkColorState       *in);
void gsk_adjust_hue    (GskHueInterpolation  interp,
                        float               *h1,
                        float               *h2);

G_END_DECLS

