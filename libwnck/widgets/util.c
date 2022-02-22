/* util header */
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

#include <config.h>

#include <glib/gi18n-lib.h>
#include "xutils.h"
#include "private.h"
#include <gdk/gdkx.h>
#include <string.h>

#ifdef HAVE_STARTUP_NOTIFICATION
#include <libsn/sn.h>
#endif

/**
 * SECTION:misc
 * @short_description: other additional features.
 * @stability: Unstable
 *
 * These functions are utility functions providing some additional features to
 * libwnck users.
 */

/**
 * SECTION:icons
 * @short_description: icons related functions.
 * @stability: Unstable
 *
 * These functions are utility functions to manage icons for #WnckWindow and
 * #WnckApplication.
 */

/**
 * _make_gtk_label_bold:
 * @label: The label.
 *
 * Switches the font of label to a bold equivalent.
 **/
void
_make_gtk_label_bold (GtkLabel *label)
{
  GtkStyleContext *context;

  _wnck_ensure_fallback_style ();

  context = gtk_widget_get_style_context (GTK_WIDGET (label));
  gtk_style_context_add_class (context, "wnck-needs-attention");
}

void
_make_gtk_label_normal (GtkLabel *label)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (label));
  gtk_style_context_remove_class (context, "wnck-needs-attention");
}

#ifdef HAVE_STARTUP_NOTIFICATION
static gboolean
_wnck_util_sn_utf8_validator (const char *str,
                              int         max_len)
{
  return g_utf8_validate (str, max_len, NULL);
}
#endif /* HAVE_STARTUP_NOTIFICATION */

void
_wnck_init (void)
{
  static gboolean done = FALSE;

  if (!done)
    {
      bindtextdomain (GETTEXT_PACKAGE, WNCK_LOCALEDIR);
      bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

#ifdef HAVE_STARTUP_NOTIFICATION
      sn_set_utf8_validator (_wnck_util_sn_utf8_validator);
#endif /* HAVE_STARTUP_NOTIFICATION */

      done = TRUE;
    }
}

Display *
_wnck_get_default_display (void)
{
  GdkDisplay *display = gdk_display_get_default ();
  /* FIXME: when we fix libwnck to not use the GDK default display, we will
   * need to fix wnckprop accordingly. */
  if (!GDK_IS_X11_DISPLAY (display))
    {
      g_warning ("libwnck is designed to work in X11 only, no valid display found");
      return NULL;
    }

  return GDK_DISPLAY_XDISPLAY (display);
}

void
_wnck_ensure_fallback_style (void)
{
  static gboolean css_loaded = FALSE;
  GtkCssProvider *provider;
  guint priority;

  if (css_loaded)
    return;

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/gnome/libwnck/wnck.css");

  priority = GTK_STYLE_PROVIDER_PRIORITY_FALLBACK;
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (provider),
                                             priority);

  g_object_unref (provider);

  css_loaded = TRUE;
}
