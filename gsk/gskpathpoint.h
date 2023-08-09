/*
 * Copyright © 2023 Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#pragma once

#if !defined (__GSK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gsk/gsk.h> can be included directly."
#endif


#include <gsk/gsktypes.h>

G_BEGIN_DECLS

#define GSK_TYPE_PATH_POINT (gsk_path_point_get_type ())

typedef struct _GskPathPoint GskPathPoint;
struct _GskPathPoint {
  /*< private >*/
  union {
    float f[8];
    gpointer p[8];
  } data;
};

GDK_AVAILABLE_IN_4_14
GType                   gsk_path_point_get_type        (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_4_14
GskPathPoint *          gsk_path_point_copy            (GskPathPoint       *point);

GDK_AVAILABLE_IN_4_14
void                    gsk_path_point_free            (GskPathPoint       *point);

GDK_AVAILABLE_IN_4_14
void                    gsk_path_point_get_position    (GskPath            *path,
                                                        const GskPathPoint *point,
                                                        graphene_point_t   *position);

GDK_AVAILABLE_IN_4_14
void                    gsk_path_point_get_tangent     (GskPath            *path,
                                                        const GskPathPoint *point,
                                                        GskPathDirection    direction,
                                                        graphene_vec2_t    *tangent);

GDK_AVAILABLE_IN_4_14
float                   gsk_path_point_get_curvature   (GskPath            *path,
                                                        const GskPathPoint *point,
                                                        graphene_point_t   *center);

GDK_AVAILABLE_IN_4_14
float                   gsk_path_point_get_distance    (GskPathMeasure     *measure,
                                                        const GskPathPoint *point);

G_END_DECLS
