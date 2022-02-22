/* Private stuff */
/* vim: set sw=2 et: */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2006-2007 Vincent Untz
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

#ifndef WNCK_PRIVATE_H
#define WNCK_PRIVATE_H

#include <libwnck/libwnck.h>
#include "pager.h"

G_BEGIN_DECLS

#define WNCK_ACTIVATE_TIMEOUT 1


/* this one is in pager.c since it needs code from there to draw the icon */
void _wnck_window_set_as_drag_icon (WnckWindow     *window,
                                    GdkDragContext *context,
                                    GtkWidget      *drag_source);

void           _make_gtk_label_bold   (GtkLabel *label);
void           _make_gtk_label_normal (GtkLabel *label);

void           _wnck_pager_activate_workspace   (WnckWorkspace *wspace,
                                                 guint32        timestamp);
int            _wnck_pager_get_n_workspaces     (WnckPager     *pager);
const char*    _wnck_pager_get_workspace_name   (WnckPager     *pager,
                                                 int            i);
WnckWorkspace* _wnck_pager_get_active_workspace (WnckPager     *pager);
WnckWorkspace* _wnck_pager_get_workspace        (WnckPager     *pager,
                                                 int            i);
void           _wnck_pager_get_workspace_rect   (WnckPager     *pager,
                                                 int            i,
                                                 GdkRectangle  *rect);

void           _wnck_selector_set_window_icon   (GtkWidget     *image,
                                                 WnckWindow    *window);

void           _wnck_ensure_fallback_style      (void);

G_END_DECLS

#endif /* WNCK_PRIVATE_H */
