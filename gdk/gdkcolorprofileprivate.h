#ifndef __GDK_COLOR_PROFILE_PRIVATE_H__
#define __GDK_COLOR_PROFILE_PRIVATE_H__

#include "gdkcolorprofile.h"

#include <lcms2.h>

G_BEGIN_DECLS


cmsHPROFILE *                gdk_color_profile_get_lcms_profile           (GdkColorProfile      *self);

G_END_DECLS

#endif /* __GDK_COLOR_PROFILE_PRIVATE_H__ */
