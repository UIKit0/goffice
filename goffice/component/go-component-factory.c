/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * go-component-factory.c :  
 *
 * Copyright (C) 2005 Jean Brefort (jean.brefort@normalesup.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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
#include <goffice/component/goffice-component.h>
#include <goffice/component/go-component.h>
#include <goffice/component/go-component-factory.h>
#include <goffice/app/go-plugin-service.h>
#include <goffice/app/go-plugin-service-impl.h>
#include <goffice/app/error-info.h>

#include <glib/gi18n-lib.h>
#include <gsf/gsf-impl-utils.h>

#include <string.h>

static GSList *refd_plugins = NULL;

/***************************************************************************/
/* Support component engines in plugins */

#define GO_COMPONENT_ENGINE_SERVICE_TYPE  (go_component_engine_service_get_type ())
#define GO_COMPONENT_ENGINE_SERVICE(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), GO_COMPONENT_ENGINE_SERVICE_TYPE, GOComponentEngineService))
#define IS_GO_COMPONENT_ENGINE_SERVICE(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GO_COMPONENT_ENGINE_SERVICE_TYPE))

static GType go_component_engine_service_get_type (void);

typedef PluginServiceGObjectLoader GOComponentEngineService;
typedef PluginServiceGObjectLoaderClass GOComponentEngineServiceClass;

static GHashTable *pending_engines = NULL;
static GHashTable *mime_types = NULL;
static GHashTable *suffixes = NULL;

static GSList *mime_types_names = NULL;

static char *
go_component_engine_service_get_description (GOPluginService * service)
{
	return g_strdup (_("Component Engine"));
}

static void
go_component_engine_service_class_init (PluginServiceGObjectLoaderClass *
					gobj_loader_class)
{
	GOPluginServiceClass *ps_class = GPS_CLASS (gobj_loader_class);

	ps_class->get_description =
		go_component_engine_service_get_description;

	gobj_loader_class->pending =
		pending_engines = g_hash_table_new (g_str_hash, g_str_equal);
}

GSF_CLASS (GOComponentEngineService, go_component_engine_service,
	   go_component_engine_service_class_init, NULL,
	   GO_PLUGIN_SERVICE_GOBJECT_LOADER_TYPE)
/***************************************************************************/
/* Use a plugin service to define where to find plot types */
#define GO_COMPONENT_TYPE_SERVICE_TYPE  (go_component_type_service_get_type ())
#define GO_COMPONENT_TYPE_SERVICE(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), GO_COMPONENT_TYPE_SERVICE_TYPE, GOComponentTypeService))
#define IS_GO_COMPONENT_TYPE_SERVICE(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GO_COMPONENT_TYPE_SERVICE_TYPE))
GType go_component_type_service_get_type (void);

typedef struct
{
	PluginServiceSimple base;

	GSList *mime_types;
} GOComponentTypeService;

typedef PluginServiceSimpleClass GOComponentTypeServiceClass;

static GObjectClass *component_type_parent_klass;
char const *GOPriorityName[] = {
	"display",
	"print",
	"partial",
	"full",
	"native"
};

static void
go_mime_type_free (GOMimeType * mime_type)
{
	if (mime_type->component_type_name)
		g_free (mime_type->component_type_name);
	g_free (mime_type);
}

static void
go_component_type_service_read_xml (GOPluginService * service, xmlNode * tree,
				    ErrorInfo ** ret_error)
{
	xmlNode *ptr;

	g_return_if_fail (service->id != NULL);

	if (mime_types == NULL)
		mime_types =
			g_hash_table_new_full (g_str_hash, g_str_equal,
					       g_free,
					       (GDestroyNotify)
					       go_mime_type_free);
	for (ptr = tree->xmlChildrenNode; ptr != NULL; ptr = ptr->next)
		if (0 == xmlStrcmp (ptr->name, "mime_type"))
		{
			char *name = xmlGetProp (ptr, "name");
			char *priority = xmlGetProp (ptr, "priority");
			char *support_clipboard = xmlGetProp (ptr, "clipboard");
			GOMimeType *mime_type =
				g_hash_table_lookup (mime_types, name);
			int i;

			for (i = 4; i >= 0; i--)
				if (!strcmp (priority, GOPriorityName[i]))
					break;
			g_free (priority);

/* FIXME FIXME FIXME the code should take into account that a plugin might be deactivated ! */
			if (mime_type == NULL) {
				mime_type = g_new (GOMimeType, 1);
				mime_type->priority = i;
				mime_type->component_type_name =
					g_strdup (service->id);
				mime_type->support_clipboard = (support_clipboard &&
					!strcmp (support_clipboard, "yes"))? TRUE: FALSE;
				mime_types_names =
					g_slist_append (mime_types_names,
							name);
				g_hash_table_replace (mime_types, name,
						      mime_type);
			} else if (i > mime_type->priority) {
				g_free (name);
				g_free (mime_type->component_type_name);
				mime_type->component_type_name =
					g_strdup (service->id);
				mime_type->priority = i;
			}
		}
}

static char *
go_component_type_service_get_description (GOPluginService * service)
{
	return g_strdup (_("Component Type"));
}

static void
go_component_type_service_finalize (GObject * obj)
{
	GOComponentTypeService *service = GO_COMPONENT_TYPE_SERVICE (obj);
	GSList *ptr;

	for (ptr = service->mime_types; ptr != NULL; ptr = ptr->next)
	{
		g_free (ptr->data);
	}
	g_slist_free (service->mime_types);
	service->mime_types = NULL;

	(component_type_parent_klass->finalize) (obj);
}

static void
go_component_type_service_init (GObject * obj)
{
	GOComponentTypeService *service = GO_COMPONENT_TYPE_SERVICE (obj);

	service->mime_types = NULL;
}

static void
go_component_type_service_class_init (GObjectClass * gobject_klass)
{
	GOPluginServiceClass *ps_class = GPS_CLASS (gobject_klass);

	component_type_parent_klass =
		g_type_class_peek_parent (gobject_klass);
	gobject_klass->finalize = go_component_type_service_finalize;
	ps_class->read_xml = go_component_type_service_read_xml;
	ps_class->get_description = go_component_type_service_get_description;
}

GSF_CLASS (GOComponentTypeService, go_component_type_service,
	   go_component_type_service_class_init,
	   go_component_type_service_init, GO_PLUGIN_SERVICE_SIMPLE_TYPE)
/***************************************************************************/
     void goc_plugin_services_init (void)
{
	plugin_service_define ("component_engine",
			       &go_component_engine_service_get_type);
	plugin_service_define ("component_type",
			       &go_component_type_service_get_type);
}

void
goc_plugin_services_shutdown (void)
{
	g_slist_foreach (refd_plugins, (GFunc) go_plugin_use_unref, NULL);
	g_slist_foreach (refd_plugins, (GFunc) g_object_unref, NULL);
	g_slist_free (refd_plugins);
	if (pending_engines)
		g_hash_table_destroy (pending_engines);
	if (mime_types)
		g_hash_table_destroy (mime_types);
	if (suffixes)
		g_hash_table_destroy (suffixes);
}

GSList *
go_components_get_mime_types ()
{
	return mime_types_names;
}

GOMimePriority
go_components_get_priority (char const *mime_type)
{
	GOMimeType *t = g_hash_table_lookup (mime_types, mime_type);
	return (t) ? t->priority : GO_MIME_PRIORITY_INVALID;
}

gboolean
go_components_support_clipboard (char const *mime_type)
{
	GOMimeType *t = g_hash_table_lookup (mime_types, mime_type);
	return (t) ? t->support_clipboard : FALSE;
}

void
go_components_add_mime_type (char *mime, GOMimePriority priority, char const *service_id)
{
	GOMimeType *mime_type = g_hash_table_lookup (mime_types, mime);
	if (mime_type == NULL)
	{
		mime_type = g_new (GOMimeType, 1);
		mime_type->priority = priority;
		mime_type->component_type_name = g_strdup (service_id);
		mime_types_names = g_slist_append (mime_types_names, mime);
		g_hash_table_replace (mime_types, mime, mime_type);
	}
	else if (priority > mime_type->priority)
	{
		mime_type->priority = priority;
	}
}

void
go_components_set_mime_suffix (char const *mime, char const *suffix)
{
	if (suffixes == NULL)
		suffixes =
			g_hash_table_new_full (g_str_hash, g_str_equal,
					       g_free, g_free);
	g_hash_table_insert (suffixes, g_strdup (mime), g_strdup (suffix));
}

char const *
go_components_get_mime_suffix (char const *mime)
{
	return (suffixes)? g_hash_table_lookup (suffixes, mime): NULL;
}

GOComponent *
go_component_new_by_mime_type (char const *mime_type)
{
	GType type;
	GOMimeType *mtype = g_hash_table_lookup (mime_types, mime_type);
	if (mtype == NULL)
		return NULL;
	type = g_type_from_name (mtype->component_type_name);
	if (type == 0)
	{
		ErrorInfo *err = NULL;
		GOPluginService *service =
			pending_engines
			? g_hash_table_lookup (pending_engines,
					       mtype->component_type_name) : NULL;
		GOPlugin *plugin;

		if (!service || !service->is_active)
			return NULL;

		g_return_val_if_fail (!service->is_loaded, NULL);

		plugin_service_load (service, &err);
		type = g_type_from_name (mtype->component_type_name);

		if (err != NULL)
		{
			error_info_print (err);
			error_info_free (err);
		}

		g_return_val_if_fail (type != 0, NULL);

		/*
		 * The plugin defined a gtype so it must not be unloaded.
		 */
		plugin = plugin_service_get_plugin (service);
		refd_plugins = g_slist_prepend (refd_plugins, plugin);
		g_object_ref (plugin);
		go_plugin_use_ref (plugin);
	}

	return g_object_new (type, "mime-type", mime_type, NULL);
}