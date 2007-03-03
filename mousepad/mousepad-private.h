/* $Id$ */
/*
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2007 Nick Schermer <nick@xfce.org>
 *
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

#include <exo/exo.h>

G_BEGIN_DECLS

#define DEBUG_LINE g_print ("%d\n", __LINE__);


/* support macros for debugging */
#ifndef NDEBUG
#define _mousepad_assert(expr)                  g_assert (expr)
#define _mousepad_assert_not_reached()          g_assert_not_reached ()
#define _mousepad_return_if_fail(expr)          g_return_if_fail (expr)
#define _mousepad_return_val_if_fail(expr, val) g_return_val_if_fail (expr, (val))
#else
#define _mousepad_assert(expr)                  G_STMT_START{ (void)0; }G_STMT_END
#define _mousepad_assert_not_reached()          G_STMT_START{ (void)0; }G_STMT_END
#define _mousepad_return_if_fail(expr)          G_STMT_START{ (void)0; }G_STMT_END
#define _mousepad_return_val_if_fail(expr, val) G_STMT_START{ (void)0; }G_STMT_END
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



/* don't use the invalid property warning since testing in debug builds is enough */
#ifndef G_ENABLE_DEBUG
#undef G_OBJECT_WARN_INVALID_PROPERTY_ID
#define G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec) G_STMT_START{ (void)0; }G_STMT_END
#endif

G_END_DECLS

#endif /* !__MOUSEPAD_PRIVATE_H__ */
