/* Xlib utils */
/* vim: set sw=2 et: */

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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include "xutils.h"
#include <string.h>
#include <stdio.h>
#include <cairo-xlib.h>
#if HAVE_CAIRO_XLIB_XRENDER
#include <cairo-xlib-xrender.h>
#endif
#include "private.h"

gboolean
_wnck_get_window (Screen *screen,
                  Window  xwindow,
                  Atom    atom,
                  Window *val)
{
  Display *display;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  Window *w;
  int err, result;

  display = DisplayOfScreen (screen);

  *val = 0;

  _wnck_error_trap_push (display);
  type = None;
  result = XGetWindowProperty (display,
			       xwindow,
			       atom,
			       0, G_MAXLONG,
			       False, XA_WINDOW, &type, &format, &nitems,
			       &bytes_after, (void*)&w);
  err = _wnck_error_trap_pop (display);
  if (err != Success ||
      result != Success)
    return FALSE;

  if (type != XA_WINDOW)
    {
      XFree (w);
      return FALSE;
    }

  *val = *w;

  XFree (w);

  return TRUE;
}

gboolean
_wnck_get_atom (Screen *screen,
                Window  xwindow,
                Atom    atom,
                Atom   *val)
{
  Display *display;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  Atom *a;
  int err, result;

  display = DisplayOfScreen (screen);

  *val = 0;

  _wnck_error_trap_push (display);
  type = None;
  result = XGetWindowProperty (display,
			       xwindow,
			       atom,
			       0, G_MAXLONG,
			       False, XA_ATOM, &type, &format, &nitems,
			       &bytes_after, (void*)&a);
  err = _wnck_error_trap_pop (display);
  if (err != Success ||
      result != Success)
    return FALSE;

  if (type != XA_ATOM)
    {
      XFree (a);
      return FALSE;
    }

  *val = *a;

  XFree (a);

  return TRUE;
}

void
_wnck_error_trap_push (Display *display)
{
  GdkDisplay *gdk_display;

  gdk_display = gdk_x11_lookup_xdisplay (display);
  g_assert (gdk_display != NULL);

  gdk_x11_display_error_trap_push (gdk_display);
}

int
_wnck_error_trap_pop (Display *display)
{
  GdkDisplay *gdk_display;

  gdk_display = gdk_x11_lookup_xdisplay (display);
  g_assert (gdk_display != NULL);

  gdk_display_flush (gdk_display);
  return gdk_x11_display_error_trap_pop (gdk_display);
}

cairo_surface_t *
_wnck_cairo_surface_get_from_pixmap (Screen *screen,
                                     Pixmap  xpixmap)
{
  cairo_surface_t *surface;
  Display *display;
  Window root_return;
  int x_ret, y_ret;
  unsigned int w_ret, h_ret, bw_ret, depth_ret;
  XWindowAttributes attrs;

  surface = NULL;
  display = DisplayOfScreen (screen);

  _wnck_error_trap_push (display);

  if (!XGetGeometry (display, xpixmap, &root_return,
                     &x_ret, &y_ret, &w_ret, &h_ret, &bw_ret, &depth_ret))
    goto TRAP_POP;

  if (depth_ret == 1)
    {
      surface = cairo_xlib_surface_create_for_bitmap (display,
                                                      xpixmap,
                                                      screen,
                                                      w_ret,
                                                      h_ret);
    }
  else
    {
      if (!XGetWindowAttributes (display, root_return, &attrs))
        goto TRAP_POP;

      if (depth_ret == (unsigned int) attrs.depth)
	{
	  surface = cairo_xlib_surface_create (display,
					       xpixmap,
					       attrs.visual,
					       w_ret, h_ret);
	}
      else
	{
#if HAVE_CAIRO_XLIB_XRENDER
	  int std;

	  switch (depth_ret) {
	  case 1: std = PictStandardA1; break;
	  case 4: std = PictStandardA4; break;
	  case 8: std = PictStandardA8; break;
	  case 24: std = PictStandardRGB24; break;
	  case 32: std = PictStandardARGB32; break;
	  default: goto TRAP_POP;
	  }

	  surface = cairo_xlib_surface_create_with_xrender_format (display,
								   xpixmap,
								   attrs.screen,
								   XRenderFindStandardFormat (display, std),
								   w_ret, h_ret);
#endif
	}
    }

TRAP_POP:
  _wnck_error_trap_pop (display);

  return surface;
}

GdkPixbuf*
_wnck_gdk_pixbuf_get_from_pixmap (Screen *screen,
                                  Pixmap  xpixmap)
{
  cairo_surface_t *surface;
  GdkPixbuf *retval;

  surface = _wnck_cairo_surface_get_from_pixmap (screen, xpixmap);

  if (surface == NULL)
    return NULL;

  retval = gdk_pixbuf_get_from_surface (surface,
                                        0,
                                        0,
                                        cairo_xlib_surface_get_width (surface),
                                        cairo_xlib_surface_get_height (surface));
  cairo_surface_destroy (surface);

  return retval;
}

static GdkPixbuf*
default_icon_at_size (int size)
{
  GdkPixbuf *base;

  base = gdk_pixbuf_new_from_resource ("/org/gnome/libwnck/default_icon.png", NULL);

  g_assert (base);

  if (gdk_pixbuf_get_width (base) == size &&
      gdk_pixbuf_get_height (base) == size)
    {
      return base;
    }
  else
    {
      GdkPixbuf *scaled;

      scaled = gdk_pixbuf_scale_simple (base, size, size, GDK_INTERP_BILINEAR);
      g_object_unref (G_OBJECT (base));

      return scaled;
    }
}

void
_wnck_get_fallback_icons (GdkPixbuf **iconp,
                          int         ideal_size,
                          GdkPixbuf **mini_iconp,
                          int         ideal_mini_size)
{
  if (iconp)
    *iconp = default_icon_at_size (ideal_size);

  if (mini_iconp)
    *mini_iconp = default_icon_at_size (ideal_mini_size);
}

void
_wnck_get_window_geometry (Screen *screen,
			   Window  xwindow,
                           int    *xp,
                           int    *yp,
                           int    *widthp,
                           int    *heightp)
{
  Display *display;
  int x, y;
  unsigned int width, height, bw, depth;
  Window root_window;

  width = 1;
  height = 1;

  display = DisplayOfScreen (screen);

  _wnck_error_trap_push (display);

  XGetGeometry (display,
                xwindow,
                &root_window,
                &x, &y, &width, &height, &bw, &depth);

  _wnck_error_trap_pop (display);

  _wnck_get_window_position (screen, xwindow, xp, yp);

  if (widthp)
    *widthp = width;
  if (heightp)
    *heightp = height;
}

void
_wnck_get_window_position (Screen *screen,
			   Window  xwindow,
                           int    *xp,
                           int    *yp)
{
  Display *display;
  Window   root;
  int      x, y;
  Window   child;

  x = 0;
  y = 0;

  display = DisplayOfScreen (screen);
  root = RootWindowOfScreen (screen);

  _wnck_error_trap_push (display);
  XTranslateCoordinates (display,
                         xwindow,
			 root,
                         0, 0,
                         &x, &y, &child);
  _wnck_error_trap_pop (display);

  if (xp)
    *xp = x;
  if (yp)
    *yp = y;
}

void
_wnck_set_icon_geometry  (Screen *screen,
                          Window  xwindow,
			  int     x,
			  int     y,
			  int     width,
			  int     height)
{
  Display *display;
  gulong data[4];

  display = DisplayOfScreen (screen);

  data[0] = x;
  data[1] = y;
  data[2] = width;
  data[3] = height;

  _wnck_error_trap_push (display);

  XChangeProperty (display,
		   xwindow,
		   _wnck_atom_get ("_NET_WM_ICON_GEOMETRY"),
		   XA_CARDINAL, 32, PropModeReplace,
		   (guchar *)&data, 4);

  _wnck_error_trap_pop (display);
}

GdkDisplay*
_wnck_gdk_display_lookup_from_display (Display *display)
{
  GdkDisplay *gdkdisplay = NULL;

  gdkdisplay = gdk_x11_lookup_xdisplay (display);

  if (!gdkdisplay)
    g_warning ("No GdkDisplay matching Display \"%s\" was found.\n",
               DisplayString (display));

  return gdkdisplay;
}

GdkWindow*
_wnck_gdk_window_lookup_from_window (Screen *screen,
                                     Window  xwindow)
{
  Display    *display;
  GdkDisplay *gdkdisplay;

  display = DisplayOfScreen (screen);
  gdkdisplay = _wnck_gdk_display_lookup_from_display (display);
  if (!gdkdisplay)
    return NULL;

  return gdk_x11_window_lookup_for_display (gdkdisplay, xwindow);
}
