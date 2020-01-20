/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2005-2007 Vincent Untz
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "wnck-icon-cache-private.h"

#include <cairo-xlib.h>
#if HAVE_CAIRO_XLIB_XRENDER
#include <cairo-xlib-xrender.h>
#endif

#include "private.h"
#include "xutils.h"

typedef enum
{
  /* These MUST be in ascending order of preference;
   * i.e. if we get _NET_WM_ICON and already have
   * WM_HINTS, we prefer _NET_WM_ICON
   */
  USING_NO_ICON,
  USING_FALLBACK_ICON,
  USING_WM_HINTS,
  USING_NET_WM_ICON
} IconOrigin;

struct _WnckIconCache
{
  IconOrigin origin;
  Pixmap prev_pixmap;
  Pixmap prev_mask;
  cairo_surface_t *icon;
  cairo_surface_t *mini_icon;
  int ideal_size;
  int ideal_mini_size;
  guint want_fallback : 1;
  /* TRUE if these props have changed */
  guint wm_hints_dirty : 1;
  guint net_wm_icon_dirty : 1;
};

static gboolean
find_best_size (gulong  *data,
                gulong   nitems,
                int      ideal_size,
                int     *width,
                int     *height,
                gulong **start)
{
  int best_w;
  int best_h;
  gulong *best_start;

  *width = 0;
  *height = 0;
  *start = NULL;

  best_w = 0;
  best_h = 0;
  best_start = NULL;

  while (nitems > 0)
    {
      int w, h;
      gboolean replace;

      replace = FALSE;

      if (nitems < 3)
        return FALSE; /* no space for w, h */

      w = data[0];
      h = data[1];

      if (nitems < ((gulong) (w * h) + 2))
        break; /* not enough data */

      if (best_start == NULL)
        {
          replace = TRUE;
        }
      else
        {
          /* work with averages */
          int best_size = (best_w + best_h) / 2;
          int this_size = (w + h) / 2;

          /* larger than desired is always better than smaller */
          if (best_size < ideal_size &&
              this_size >= ideal_size)
            replace = TRUE;
          /* if we have too small, pick anything bigger */
          else if (best_size < ideal_size &&
                   this_size > best_size)
            replace = TRUE;
          /* if we have too large, pick anything smaller
           * but still >= the ideal
           */
          else if (best_size > ideal_size &&
                   this_size >= ideal_size &&
                   this_size < best_size)
            replace = TRUE;
        }

      if (replace)
        {
          best_start = data + 2;
          best_w = w;
          best_h = h;
        }

      data += (w * h) + 2;
      nitems -= (w * h) + 2;
    }

  if (best_start)
    {
      *start = best_start;
      *width = best_w;
      *height = best_h;
      return TRUE;
    }
  else
    return FALSE;
}

static cairo_surface_t *
argbdata_to_surface (gulong *argb_data,
                     int     w,
                     int     h,
                     int     ideal_w,
                     int     ideal_h,
                     int     scaling_factor)
{
  cairo_surface_t *surface, *icon;
  cairo_t *cr;
  int y, x, stride;
  uint32_t *data;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
  cairo_surface_set_device_scale (surface, (double)scaling_factor, (double)scaling_factor);
  stride = cairo_image_surface_get_stride (surface) / sizeof (uint32_t);
  data = (uint32_t *) cairo_image_surface_get_data (surface);

  /* One could speed this up a lot. */
  for (y = 0; y < h; y++)
    {
      for (x = 0; x < w; x++)
        {
          uint32_t *p = &data[y * stride + x];
          gulong *d = &argb_data[y * w + x];
          *p = *d;
        }
    }

  cairo_surface_mark_dirty (surface);

  icon = cairo_surface_create_similar_image (surface,
                                             cairo_image_surface_get_format (surface),
                                             ideal_w, ideal_h);

  cairo_surface_set_device_scale (icon, (double)scaling_factor, (double)scaling_factor);

  cr = cairo_create (icon);
  cairo_scale (cr, ideal_w / (double)w, ideal_h / (double)h);
  cairo_set_source_surface (cr, surface, 0, 0);
  cairo_paint (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_IN);
  cairo_paint (cr);

  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  return icon;
}

static gboolean
read_rgb_icon (Screen           *screen,
               Window            xwindow,
               int               ideal_size,
               int               ideal_mini_size,
               cairo_surface_t **iconp,
               cairo_surface_t **mini_iconp,
               int               scaling_factor)
{
  Display *display;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  int result, err;
  gulong *data;
  gulong *best;
  int w, h;
  gulong *best_mini;
  int mini_w, mini_h;

  display = DisplayOfScreen (screen);

  _wnck_error_trap_push (display);
  type = None;
  data = NULL;
  result = XGetWindowProperty (display,
			       xwindow,
			       _wnck_atom_get ("_NET_WM_ICON"),
			       0, G_MAXLONG,
			       False, XA_CARDINAL, &type, &format, &nitems,
			       &bytes_after, (void*)&data);

  err = _wnck_error_trap_pop (display);

  if (err != Success ||
      result != Success)
    return FALSE;

  if (type != XA_CARDINAL)
    {
      XFree (data);
      return FALSE;
    }

  if (!find_best_size (data, nitems,
                       ideal_size,
                       &w, &h, &best))
    {
      XFree (data);
      return FALSE;
    }

  if (!find_best_size (data, nitems,
                       ideal_mini_size,
                       &mini_w, &mini_h, &best_mini))
    {
      XFree (data);
      return FALSE;
    }

  *iconp = argbdata_to_surface (best, w, h, ideal_size, ideal_size, scaling_factor);
  *mini_iconp = argbdata_to_surface (best_mini, mini_w, mini_h, ideal_mini_size, ideal_mini_size, scaling_factor);

  XFree (data);

  return TRUE;
}

static gboolean
try_pixmap_and_mask (Screen           *screen,
                     Pixmap            src_pixmap,
                     Pixmap            src_mask,
                     cairo_surface_t **iconp,
                     int               ideal_size,
                     cairo_surface_t **mini_iconp,
                     int               ideal_mini_size,
                     int               scaling_factor)
{
  cairo_surface_t *surface, *mask_surface, *image;
  GdkDisplay *gdk_display;
  int width, height;
  cairo_t *cr;

  if (src_pixmap == None)
    return FALSE;

  surface = _wnck_cairo_surface_get_from_pixmap (screen, src_pixmap, scaling_factor);

  if (surface && src_mask != None)
    mask_surface = _wnck_cairo_surface_get_from_pixmap (screen, src_mask, scaling_factor);
  else
    mask_surface = NULL;

  if (surface == NULL)
    return FALSE;

  gdk_display = gdk_x11_lookup_xdisplay (XDisplayOfScreen (screen));
  g_assert (gdk_display != NULL);

  gdk_x11_display_error_trap_push (gdk_display);

  width = cairo_xlib_surface_get_width (surface);
  height = cairo_xlib_surface_get_height (surface);

  image = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                      width, height);
  cr = cairo_create (image);

  /* Need special code for alpha-only surfaces. We only get those
   * for bitmaps. And in that case, it's a differentiation between
   * foreground (white) and background (black).
   */
  if (cairo_surface_get_content (surface) & CAIRO_CONTENT_ALPHA)
    {
      cairo_push_group (cr);

      /* black background */
      cairo_set_source_rgb (cr, 0, 0, 0);
      cairo_paint (cr);
      /* mask with white foreground */
      cairo_set_source_rgb (cr, 1, 1, 1);
      cairo_mask_surface (cr, surface, 0, 0);

      cairo_pop_group_to_source (cr);
    }
  else
    cairo_set_source_surface (cr, surface, 0, 0);

  if (mask_surface)
    {
      cairo_mask_surface (cr, mask_surface, 0, 0);
      cairo_surface_destroy (mask_surface);
    }
  else
    cairo_paint (cr);

  cairo_surface_destroy (surface);
  cairo_destroy (cr);

  if (gdk_x11_display_error_trap_pop (gdk_display) != Success)
    {
      cairo_surface_destroy (image);
      return FALSE;
    }

  if (image)
    {
      int image_w, image_h;

      image_w = cairo_image_surface_get_width (image);
      image_h = cairo_image_surface_get_height (image);

      *iconp = cairo_surface_create_similar (image,
                                             cairo_surface_get_content (image),
                                             ideal_size,
                                             ideal_size);

      cairo_surface_set_device_scale (*iconp, (double)scaling_factor, (double)scaling_factor);

      cr = cairo_create (*iconp);
      cairo_scale (cr, ideal_size / (double)image_w, ideal_size / (double)image_h);
      cairo_set_source_surface (cr, image, 0, 0);
      cairo_paint (cr);
      cairo_destroy (cr);

      *mini_iconp = cairo_surface_create_similar (image,
                                                  cairo_surface_get_content (image),
                                                  ideal_mini_size,
                                                  ideal_mini_size);

      cairo_surface_set_device_scale (*mini_iconp, (double)scaling_factor, (double)scaling_factor);

      cr = cairo_create (*mini_iconp);
      cairo_scale (cr, ideal_mini_size / (double)image_w, ideal_mini_size / (double)image_h);
      cairo_set_source_surface (cr, image, 0, 0);
      cairo_paint (cr);
      cairo_destroy (cr);

      cairo_surface_destroy (image);

      return TRUE;
    }
  else
    return FALSE;
}

static void
clear_icon_cache (WnckIconCache *icon_cache,
                  gboolean       dirty_all)
{
  g_clear_pointer (&icon_cache->icon, cairo_surface_destroy);
  g_clear_pointer (&icon_cache->mini_icon, cairo_surface_destroy);

  icon_cache->origin = USING_NO_ICON;

  if (dirty_all)
    {
      icon_cache->wm_hints_dirty = TRUE;
      icon_cache->net_wm_icon_dirty = TRUE;
    }
}

static void
replace_cache (WnckIconCache   *icon_cache,
               IconOrigin       origin,
               cairo_surface_t *new_icon,
               cairo_surface_t *new_mini_icon)
{
  clear_icon_cache (icon_cache, FALSE);

  icon_cache->origin = origin;

  if (new_icon)
    cairo_surface_reference (new_icon);

  icon_cache->icon = new_icon;

  if (new_mini_icon)
    cairo_surface_reference (new_mini_icon);

  icon_cache->mini_icon = new_mini_icon;
}

WnckIconCache*
_wnck_icon_cache_new (void)
{
  WnckIconCache *icon_cache;

  icon_cache = g_slice_new0 (WnckIconCache);

  icon_cache->origin = USING_NO_ICON;
  icon_cache->prev_pixmap = None;
  icon_cache->icon = NULL;
  icon_cache->mini_icon = NULL;
  icon_cache->ideal_size = -1; /* won't be a legit size */
  icon_cache->ideal_mini_size = -1;
  icon_cache->want_fallback = TRUE;
  icon_cache->wm_hints_dirty = TRUE;
  icon_cache->net_wm_icon_dirty = TRUE;

  return icon_cache;
}

void
_wnck_icon_cache_free (WnckIconCache *icon_cache)
{
  clear_icon_cache (icon_cache, FALSE);

  g_slice_free (WnckIconCache, icon_cache);
}

void
_wnck_icon_cache_property_changed (WnckIconCache *icon_cache,
                                   Atom           atom)
{
  if (atom == _wnck_atom_get ("_NET_WM_ICON"))
    icon_cache->net_wm_icon_dirty = TRUE;
  else if (atom == _wnck_atom_get ("WM_HINTS"))
    icon_cache->wm_hints_dirty = TRUE;
}

gboolean
_wnck_icon_cache_get_icon_invalidated (WnckIconCache *icon_cache)
{
  if (icon_cache->origin <= USING_WM_HINTS &&
      icon_cache->wm_hints_dirty)
    return TRUE;
  else if (icon_cache->origin <= USING_NET_WM_ICON &&
           icon_cache->net_wm_icon_dirty)
    return TRUE;
  else if (icon_cache->origin < USING_FALLBACK_ICON &&
           icon_cache->want_fallback)
    return TRUE;
  else if (icon_cache->origin == USING_NO_ICON)
    return TRUE;
  else if (icon_cache->origin == USING_FALLBACK_ICON &&
           !icon_cache->want_fallback)
    return TRUE;
  else
    return FALSE;
}

void
_wnck_icon_cache_set_want_fallback (WnckIconCache *icon_cache,
                                    gboolean       setting)
{
  icon_cache->want_fallback = setting;
}

gboolean
_wnck_icon_cache_get_is_fallback (WnckIconCache *icon_cache)
{
  return icon_cache->origin == USING_FALLBACK_ICON;
}

gboolean
_wnck_read_icons (WnckScreen       *screen,
                  Window            xwindow,
                  WnckIconCache    *icon_cache,
                  cairo_surface_t **iconp,
                  int               ideal_size,
                  cairo_surface_t **mini_iconp,
                  int               ideal_mini_size,
                  int               scaling_factor)
{
  Screen *xscreen;
  Display *display;
  XWMHints *hints;

  /* Return value is whether the icon changed */

  g_return_val_if_fail (icon_cache != NULL, FALSE);

  xscreen = _wnck_screen_get_xscreen (screen);
  display = DisplayOfScreen (xscreen);

  *iconp = NULL;
  *mini_iconp = NULL;

  ideal_size *= scaling_factor;
  ideal_mini_size *= scaling_factor;

  if (ideal_size != icon_cache->ideal_size ||
      ideal_mini_size != icon_cache->ideal_mini_size)
    clear_icon_cache (icon_cache, TRUE);

  icon_cache->ideal_size = ideal_size;
  icon_cache->ideal_mini_size = ideal_mini_size;

  if (!_wnck_icon_cache_get_icon_invalidated (icon_cache))
    return FALSE; /* we have no new info to use */

  /* Our algorithm here assumes that we can't have for example origin
   * < USING_NET_WM_ICON and icon_cache->net_wm_icon_dirty == FALSE
   * unless we have tried to read NET_WM_ICON.
   *
   * Put another way, if an icon origin is not dirty, then we have
   * tried to read it at the current size. If it is dirty, then
   * we haven't done that since the last change.
   */

  if (icon_cache->origin <= USING_NET_WM_ICON &&
      icon_cache->net_wm_icon_dirty)
    {
      icon_cache->net_wm_icon_dirty = FALSE;

      if (read_rgb_icon (xscreen, xwindow,
                         ideal_size,
                         ideal_mini_size,
                         iconp, mini_iconp,
                         scaling_factor))
        {
          replace_cache (icon_cache, USING_NET_WM_ICON,
                         *iconp, *mini_iconp);

          return TRUE;
        }
    }

  if (icon_cache->origin <= USING_WM_HINTS &&
      icon_cache->wm_hints_dirty)
    {
      Pixmap pixmap;
      Pixmap mask;

      icon_cache->wm_hints_dirty = FALSE;

      _wnck_error_trap_push (display);
      hints = XGetWMHints (display, xwindow);
      _wnck_error_trap_pop (display);
      pixmap = None;
      mask = None;
      if (hints)
        {
          if (hints->flags & IconPixmapHint)
            pixmap = hints->icon_pixmap;
          if (hints->flags & IconMaskHint)
            mask = hints->icon_mask;

          XFree (hints);
          hints = NULL;
        }

      /* We won't update if pixmap is unchanged;
       * avoids a get_from_drawable() on every geometry
       * hints change
       */
      if ((pixmap != icon_cache->prev_pixmap ||
           mask != icon_cache->prev_mask) &&
          pixmap != None)
        {
          if (try_pixmap_and_mask (xscreen, pixmap, mask,
                                   iconp, ideal_size,
                                   mini_iconp, ideal_mini_size,
                                   scaling_factor))
            {
              icon_cache->prev_pixmap = pixmap;
              icon_cache->prev_mask = mask;

              replace_cache (icon_cache, USING_WM_HINTS,
                             *iconp, *mini_iconp);

              return TRUE;
            }
        }
    }

  if (icon_cache->want_fallback &&
      icon_cache->origin < USING_FALLBACK_ICON)
    {
      _wnck_get_fallback_icons (iconp,
                                ideal_size,
                                mini_iconp,
                                ideal_mini_size);

      replace_cache (icon_cache, USING_FALLBACK_ICON,
                     *iconp, *mini_iconp);

      return TRUE;
    }

  if (!icon_cache->want_fallback &&
      icon_cache->origin == USING_FALLBACK_ICON)
    {
      /* Get rid of current icon */
      clear_icon_cache (icon_cache, FALSE);

      return TRUE;
    }

  /* found nothing new */
  return FALSE;
}
