/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __MOUSEPAD_PRIVATE_H__
#define __MOUSEPAD_PRIVATE_H__

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gtksourceview/gtksourceview.h>
#include <gtksourceview/gtksourcestylescheme.h>
#include <gtksourceview/gtksourcestyleschememanager.h>

G_BEGIN_DECLS

/* errors inside mousepad */
enum
{
  ERROR_READING_FAILED     = -1,
  ERROR_CONVERTING_FAILED  = -2,
  ERROR_NOT_UTF8_VALID     = -3,
  ERROR_FILE_STATUS_FAILED = -4
};

/* config file locations */
#define MOUSEPAD_RC_RELPATH     ("Mousepad" G_DIR_SEPARATOR_S "mousepadrc")
#define MOUSEPAD_ACCELS_RELPATH ("Mousepad" G_DIR_SEPARATOR_S "accels.scm")

/* handling flags */
#define MOUSEPAD_SET_FLAG(flags,flag)   G_STMT_START{ ((flags) |= (flag)); }G_STMT_END
#define MOUSEPAD_UNSET_FLAG(flags,flag) G_STMT_START{ ((flags) &= ~(flag)); }G_STMT_END
#define MOUSEPAD_HAS_FLAG(flags,flag)   (((flags) & (flag)) != 0)

/* for personal testing */
#define TIMER_START    GTimer *__FUNCTION__timer = g_timer_new();
#define TIMER_SPLIT    g_print ("%s: %.2f ms\n", G_STRLOC, g_timer_elapsed (__FUNCTION__timer, NULL) * 1000);
#define TIMER_STOP     TIMER_SPLIT g_timer_destroy (__FUNCTION__timer);
#define PRINT_LOCATION g_print ("%s\n", G_STRLOC);

/* optimize the properties */
#define MOUSEPAD_PARAM_READWRITE (G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB)

/* support for canonical strings and quarks */
#define I_(string)  (g_intern_static_string (string))

/* convienient function for setting object data */
#define mousepad_object_set_data(object,key,data)              (g_object_set_qdata ((object), \
                                                                g_quark_from_static_string (key), (data)))
#define mousepad_object_set_data_full(object,key,data,destroy) (g_object_set_qdata_full ((object), \
                                                                g_quark_from_static_string (key), (data), (GDestroyNotify) (destroy)))
#define mousepad_object_get_data(object,key)                   (g_object_get_qdata ((object), g_quark_try_string (key)))

/* support macros for debugging */
#ifndef NDEBUG
#define mousepad_assert(expr)                  g_assert (expr)
#define mousepad_assert_not_reached()          g_assert_not_reached ()
#define mousepad_return_if_fail(expr)          g_return_if_fail (expr)
#define mousepad_return_val_if_fail(expr, val) g_return_val_if_fail (expr, (val))
#else
#define mousepad_assert(expr)                  G_STMT_START{ (void)0; }G_STMT_END
#define mousepad_assert_not_reached()          G_STMT_START{ (void)0; }G_STMT_END
#define mousepad_return_if_fail(expr)          G_STMT_START{ (void)0; }G_STMT_END
#define mousepad_return_val_if_fail(expr, val) G_STMT_START{ (void)0; }G_STMT_END
#endif

/* avoid trivial g_value_get_*() function calls */
#ifdef NDEBUG
#define g_value_get_boolean(v)  (((const GValue *) (v))->data[0].v_int)
#define g_value_get_char(v)     (((const GValue *) (v))->data[0].v_int)
#define g_value_get_uchar(v)    (((const GValue *) (v))->data[0].v_uint)
#define g_value_get_int(v)      (((const GValue *) (v))->data[0].v_int)
#define g_value_get_uint(v)     (((const GValue *) (v))->data[0].v_uint)
#define g_value_get_long(v)     (((const GValue *) (v))->data[0].v_long)
#define g_value_get_ulong(v)    (((const GValue *) (v))->data[0].v_ulong)
#define g_value_get_int64(v)    (((const GValue *) (v))->data[0].v_int64)
#define g_value_get_uint64(v)   (((const GValue *) (v))->data[0].v_uint64)
#define g_value_get_enum(v)     (((const GValue *) (v))->data[0].v_long)
#define g_value_get_flags(v)    (((const GValue *) (v))->data[0].v_ulong)
#define g_value_get_float(v)    (((const GValue *) (v))->data[0].v_float)
#define g_value_get_double(v)   (((const GValue *) (v))->data[0].v_double)
#define g_value_get_string(v)   (((const GValue *) (v))->data[0].v_pointer)
#define g_value_get_param(v)    (((const GValue *) (v))->data[0].v_pointer)
#define g_value_get_boxed(v)    (((const GValue *) (v))->data[0].v_pointer)
#define g_value_get_pointer(v)  (((const GValue *) (v))->data[0].v_pointer)
#define g_value_get_object(v)   (((const GValue *) (v))->data[0].v_pointer)
#endif

/* properly set guess branch probability for pure booleans */
#undef G_LIKELY
#undef G_UNLIKELY

#if defined(NDEBUG) && defined(__GNUC__) && (__GNUC__ > 2)
#define G_LIKELY(expr) (__builtin_expect (!!(expr), TRUE))
#define G_UNLIKELY(expr) (__builtin_expect (!!(expr), FALSE))
#else
#define G_LIKELY(expr) (expr)
#define G_UNLIKELY(expr) (expr)
#endif

/* tooltip api */
#if GTK_CHECK_VERSION (2,12,0)
#define mousepad_widget_set_tooltip_text(widget,text) (gtk_widget_set_tooltip_text (widget, text))
#else
#define mousepad_widget_set_tooltip_text(widget,text) (mousepad_util_set_tooltip (widget, text))
#endif

G_END_DECLS

#endif /* !__MOUSEPAD_PRIVATE_H__ */
