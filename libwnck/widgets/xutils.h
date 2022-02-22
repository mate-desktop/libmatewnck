/* Xlib utilities */
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

#ifndef WNCK_XUTILS_H
#define WNCK_XUTILS_H

#include <glib.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <libwnck/libwnck.h>

G_BEGIN_DECLS

#define WNCK_APP_WINDOW_EVENT_MASK (PropertyChangeMask | StructureNotifyMask)

gboolean _wnck_get_window        (Screen *screen,
                                  Window  xwindow,
                                  Atom    atom,
                                  Window *val);
gboolean _wnck_get_atom          (Screen *screen,
                                  Window  xwindow,
                                  Atom    atom,
                                  Atom   *val);
void _wnck_error_trap_push (Display *display);
int  _wnck_error_trap_pop  (Display *display);

#define _wnck_atom_get(atom_name) gdk_x11_get_xatom_by_name (atom_name)
#define _wnck_atom_name(atom)     gdk_x11_get_xatom_name (atom)

void _wnck_get_fallback_icons (GdkPixbuf **iconp,
                               int         ideal_size,
                               GdkPixbuf **mini_iconp,
                               int         ideal_mini_size);

void _wnck_get_window_geometry (Screen *screen,
				Window  xwindow,
                                int    *xp,
                                int    *yp,
                                int    *widthp,
                                int    *heightp);

void _wnck_get_window_position (Screen *screen,
				Window  xwindow,
                                int    *xp,
                                int    *yp);

void _wnck_set_icon_geometry  (Screen *screen,
                               Window  xwindow,
			       int     x,
			       int     y,
			       int     width,
			       int     height);

cairo_surface_t *_wnck_cairo_surface_get_from_pixmap (Screen *screen,
                                                      Pixmap  xpixmap);

GdkPixbuf* _wnck_gdk_pixbuf_get_from_pixmap (Screen *screen,
                                             Pixmap  xpixmap);

GdkDisplay* _wnck_gdk_display_lookup_from_display (Display *display);

GdkWindow* _wnck_gdk_window_lookup_from_window (Screen *screen,
                                                Window  xwindow);

G_END_DECLS

#endif /* WNCK_XUTILS_H */
