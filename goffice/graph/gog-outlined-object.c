/*
 * gog-outlined-object.c : some utility classes for objects with outlines.
 *
 * Copyright (C) 2003-2004 Jody Goldberg (jody@gnome.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#include <goffice/goffice-config.h>
#include <goffice/goffice.h>

#include <glib/gi18n-lib.h>
#include <gsf/gsf-impl-utils.h>

enum {
	OUTLINED_OBJECT_PROP_0,
	OUTLINED_OBJECT_PROP_PADDING_PTS
};

static void
gog_outlined_object_set_property (GObject *obj, guint param_id,
				GValue const *value, GParamSpec *pspec)
{
	GogOutlinedObject *goo = GOG_OUTLINED_OBJECT (obj);

	switch (param_id) {
	case OUTLINED_OBJECT_PROP_PADDING_PTS :
		goo->padding_pts = g_value_get_double (value);
		gog_object_emit_changed (GOG_OBJECT (obj), TRUE);
		break;

	default: G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, param_id, pspec);
		 return; /* NOTE : RETURN */
	}
}

static void
gog_outlined_object_get_property (GObject *obj, guint param_id,
			     GValue *value, GParamSpec *pspec)
{
	GogOutlinedObject *goo = GOG_OUTLINED_OBJECT (obj);

	switch (param_id) {
	case OUTLINED_OBJECT_PROP_PADDING_PTS:
		g_value_set_double (value, goo->padding_pts);
		break;

	default: G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, param_id, pspec);
		 break;
	}
}

static void
gog_outlined_object_class_init (GObjectClass *gobject_klass)
{
	gobject_klass->set_property = gog_outlined_object_set_property;
	gobject_klass->get_property = gog_outlined_object_get_property;

	g_object_class_install_property (gobject_klass, OUTLINED_OBJECT_PROP_PADDING_PTS,
		g_param_spec_double ("padding-pts",
			_("Padding Pts"),
			_("Number of pts separating charts in the grid"),
			0, G_MAXDOUBLE, 0,
			GSF_PARAM_STATIC | G_PARAM_READWRITE | GO_PARAM_PERSISTENT));
}

static void
gog_outlined_object_init (GogOutlinedObject *goo)
{
	goo->padding_pts = GO_CM_TO_PT ((double).25);
}

GSF_CLASS (GogOutlinedObject, gog_outlined_object,
	   gog_outlined_object_class_init, gog_outlined_object_init,
	   GOG_TYPE_STYLED_OBJECT)

double
gog_outlined_object_get_pad (GogOutlinedObject const *goo)
{
	g_return_val_if_fail (GOG_IS_OUTLINED_OBJECT (goo), 0.);
	return goo->padding_pts;
}

/*****************************************************************************/

/**
 * GogOutlinedViewClass:
 * @call_parent_render: whether the parent class render() method must be
 * called when done.
 **/

static GogViewClass *oview_parent_klass;

static void
gog_outlined_view_size_request (GogView *v,
				GogViewRequisition const *available,
				GogViewRequisition *req)
{
	GogOutlinedObject *goo = GOG_OUTLINED_OBJECT (v->model);
	double outline = gog_renderer_line_size (v->renderer,
						 goo->base.style->line.width);
	double is_outline_visible = go_style_is_outline_visible (goo->base.style);

	if (goo->base.style->fill.type != GO_STYLE_FILL_NONE || is_outline_visible) {
		req->w += outline * 2 +
			gog_renderer_pt2r_y (v->renderer, goo->padding_pts);
		req->h += outline * 2 +
			gog_renderer_pt2r_y (v->renderer, goo->padding_pts);
	}
}

static void
gog_outlined_view_size_allocate (GogView *v, GogViewAllocation const *a)
{
	GogOutlinedObject *goo = GOG_OUTLINED_OBJECT (v->model);
	GogViewAllocation res = *a;
	double outline = gog_renderer_line_size (v->renderer,
						 goo->base.style->line.width);
	double is_outline_visible = go_style_is_outline_visible (goo->base.style);

	/* We only need internal padding if there is an outline or a pattern */
	if (goo->base.style->fill.type != GO_STYLE_FILL_NONE || is_outline_visible) {
		double pad_x = gog_renderer_pt2r_x (v->renderer, goo->padding_pts);
		double pad_y = gog_renderer_pt2r_y (v->renderer, goo->padding_pts);
		res.x += outline + pad_x/2;
		res.y += outline + pad_y/2;
		res.w -= outline * 2. + pad_x;
		res.h -= outline * 2. + pad_y;
	}
	(oview_parent_klass->size_allocate) (v, &res);
}

static void
gog_outlined_view_render (GogView *view, GogViewAllocation const *bbox)
{
	GogOutlinedViewClass *klass = GOG_OUTLINED_VIEW_GET_CLASS (view);

	GogStyledObject *sobj = GOG_STYLED_OBJECT (view->model);
	gog_renderer_push_style (view->renderer, sobj->style);
	gog_renderer_draw_rectangle (view->renderer, &view->allocation);
	gog_renderer_pop_style (view->renderer);

	if (klass->call_parent_render)
		(oview_parent_klass->render) (view, bbox);
}

static void
gog_outlined_view_class_init (GogOutlinedViewClass *oview_klass)
{
	GogViewClass *view_klass = (GogViewClass *) oview_klass;

	oview_parent_klass = g_type_class_peek_parent (view_klass);
	view_klass->size_request  = gog_outlined_view_size_request;
	view_klass->size_allocate = gog_outlined_view_size_allocate;
	view_klass->render	  = gog_outlined_view_render;

	oview_klass->call_parent_render = TRUE;
}

GSF_CLASS_ABSTRACT (GogOutlinedView, gog_outlined_view,
	   gog_outlined_view_class_init, NULL,
	   GOG_TYPE_VIEW)
