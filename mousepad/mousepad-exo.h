/* $Id$ */
/*-
 * Copyright (c) 2004-2006 os-cillation e.K.
 * Copyright (c) 2004      Victor Porton (http://ex-code.com/~porton/)
 *
 * Written by Benedikt Meurer <benny@xfce.org>.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __MOUSEPAD_EXO_H__
#define __MOUSEPAD_EXO_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _ExoBindingBase    ExoBindingBase;
typedef struct _ExoBindingLink    ExoBindingLink;
typedef struct _ExoBinding        ExoBinding;
typedef struct _ExoMutualBinding  ExoMutualBinding;

typedef gboolean  (*ExoBindingTransform)  (const GValue *src_value,
                                           GValue       *dst_value,
                                           gpointer      user_data);

struct _ExoBindingBase
{
  GDestroyNotify  destroy;
};

struct _ExoBindingLink
{
  GObject             *dst_object;
  GParamSpec          *dst_pspec;
  gulong               dst_handler; /* only set for mutual bindings */
  gulong               handler;
  ExoBindingTransform  transform;
  gpointer             user_data;
};

struct _ExoBinding
{
  /*< private >*/
  GObject         *src_object;
  ExoBindingBase   base;
  ExoBindingLink   link;
};

struct _ExoMutualBinding
{
  /*< private >*/
  ExoBindingBase  base;
  ExoBindingLink  direct;
  ExoBindingLink  reverse;
};


ExoBinding        *exo_binding_new                      (GObject            *src_object,
                                                         const gchar        *src_property,
                                                         GObject            *dst_object,
                                                         const gchar        *dst_property);
ExoBinding        *exo_binding_new_full                 (GObject            *src_object,
                                                         const gchar        *src_property,
                                                         GObject            *dst_object,
                                                         const gchar        *dst_property,
                                                         ExoBindingTransform transform,
                                                         GDestroyNotify      destroy_notify,
                                                         gpointer            user_data);
void               exo_binding_unbind                   (ExoBinding         *binding);

ExoMutualBinding  *exo_mutual_binding_new               (GObject            *object1,
                                                         const gchar        *property1,
                                                         GObject            *object2,
                                                         const gchar        *property2);
ExoMutualBinding  *exo_mutual_binding_new_full          (GObject            *object1,
                                                         const gchar        *property1,
                                                         GObject            *object2,
                                                         const gchar        *property2,
                                                         ExoBindingTransform transform,
                                                         ExoBindingTransform reverse_transform,
                                                         GDestroyNotify      destroy_notify,
                                                         gpointer            user_data);
void               exo_mutual_binding_unbind            (ExoMutualBinding   *binding);

G_END_DECLS

#endif /* !__MOUSEPAD_EXO_H__ */
