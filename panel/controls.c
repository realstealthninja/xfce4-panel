/*  controls.c
 *  
 *  Copyright (C) 2002 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

/*  Plugins
 *  -------
 *  Plugins are accessed through the GModule interface. They must provide these 
 *  two symbols:
 *  - is_xfce_panel_control : only checked for existence
 *  - module_init : this function takes a PanelControl structure as argument
 *    and must fill it with the appropriate values and pointers.
*/

/*  Panel Control Configuration Dialog
 *  ----------------------------------
 *  All panel controls may provide an add_options() functions to allow
 *  options to be changed. The arguments to this function are a PanelControl,
 *  a GtkContainer to pack options into, and two GtkButton's. 
 *
 *  The first button is used to revert changes. The button is initially 
 *  insensitive. The panel control should make it sensitive when an option
 *  is changed. The panel control should also connect a callback that 
 *  reverts the options to the initial state. This implies the initial state
 *  should be saved when creating the options.
 *
 *  The second button is used to close the dialog. Any changes that weren't 
 *  applied immediately should be applied now.
 *  
 *  You can connect to the destroy signal on the option widgets (or the 
 *  continainer) to know when to clean up the backup data. 
*/

#include "xfce.h"
#include "item.h"
#include "callbacks.h"
#include "groups.h"
#include "plugins.h"

/*  Modules
 *  -------
*/
/* new module interface 
 * - panel controls are created using 'modules'.
 * - modules can be plugins or builtin (launchers)
 * - open plugins only once
 * - keep list of modules 
 * - create new controls using these modules 
*/

#define SOEXT 		("." G_MODULE_SUFFIX)
#define SOEXT_LEN 	(strlen (SOEXT))

#define API_VERSION 2

gchar *xfce_plugin_check_version(gint version)
{
    if (version != API_VERSION)
	return "Incompatible plugin version";
    else
	return NULL;
}

static void free_module(PanelModule *module)
{
    if (module->id == PLUGIN)
	g_module_close(module->gmodule);

    g_free(module);
}

static GSList *module_list = NULL;

const GSList *get_module_list(void)
{
    return module_list;
}

static gint compare_modules (gconstpointer module_a, gconstpointer module_b)
{
    g_assert (module_a != NULL);
    g_assert (module_b != NULL);
    
    return (g_ascii_strcasecmp(PANEL_MODULE(module_a)->name, 
			       PANEL_MODULE(module_b)->name));
}

#if 0
static gint lookup_modules (gconstpointer module, gconstpointer name)
{
    g_assert (module != NULL);
    g_assert (name != NULL);
    
    return (g_ascii_strcasecmp(PANEL_MODULE(module)->name, name));
}
#endif
static gint lookup_modules_by_filename (gconstpointer module, 
					gconstpointer filename)
{
    char *fn;
    int result;
    
    g_assert (module != NULL);
    g_assert (filename != NULL);
    
    if (PANEL_MODULE(module)->gmodule)
	fn = g_path_get_basename(g_module_name(PANEL_MODULE(module)->gmodule));
    else
	return -1;
    
    result = g_ascii_strcasecmp(fn, filename);
    g_free(fn);

    return result;
}

static gint lookup_modules_by_id (gconstpointer module, 
				  gconstpointer id)
{
    int real_id;
    
    g_assert (module != NULL);
    g_assert (id != NULL);
    
    real_id = GPOINTER_TO_INT(id);
    
    return (PANEL_MODULE(module)->id == real_id);
}

static void load_plugin(gchar *path)
{
    gpointer tmp;
    PanelModule *module;
    GModule *gm;
    
    void (*init)(PanelModule *mp);
        
    module = g_new0(PanelModule, 1);
    
    gm = g_module_open(path,0);
    module->gmodule = gm;

    if (gm && g_module_symbol(gm, "xfce_plugin_init", &tmp))
    {
        init = tmp;
        init(module);
	
	if (g_slist_find_custom(module_list, module, compare_modules))
	{
            g_message ("xfce4: module %s has already been loaded", 
		       module->name);
	    free_module(module);
	}
	else
	{
            g_message ("xfce4: module %s successfully loaded", module->name);
	    module_list = g_slist_append(module_list, module);
	}
    }
    else if (gm)
    {
        g_warning ("xfce4: incompatible module %s",  path);
        g_module_close (gm);
	g_free(module);
    }
    else
    {
        g_warning ("xfce4: module %s cannot be opened (%s)",  
		   path, g_module_error());
	g_free(module);
    }
}

static void
load_plugin_dir (const char *dir)
{
    GDir *gdir = g_dir_open(dir, 0, NULL);
    const char *file;

    if(!gdir)
	return;

    while((file = g_dir_read_name(gdir)))
    {
	const char *s = file;

	s += strlen(file) - SOEXT_LEN;
	
	if(strequal(s, SOEXT))
	{
	    char *path = g_build_filename(dir, file, NULL);
	    
	    load_plugin (path);
	    g_free (path);
	}
    }
    
    g_dir_close (gdir);
}

static void add_builtin_modules(void)
{
    /* there are currently no builtin modules */
}

static void add_launcher_module(void)
{
    PanelModule *module;

    module = g_new0(PanelModule, 1);

    module->name = "icon";
    module->caption = _("Launcher");
    module->create_control = (CreateControlFunc)create_panel_item;

    module_list = g_slist_append(module_list, module);
}

void modules_init(void)
{
    char **dirs, **d;

    /* launcher module */
    add_launcher_module();

    /* builtin modules */
    add_builtin_modules();

    /* plugins */    
    dirs = get_plugin_dirs();

    for(d = dirs; *d; d++)
	load_plugin_dir(*d);

    g_strfreev(dirs);
}

void modules_cleanup(void)
{
    GSList *li;

    for (li = module_list; li; li = li->next)
    {
	PanelModule *module = li->data;

	free_module(module);
    }

    g_slist_free(module_list);
    module_list = NULL;
}

gboolean create_builtin(PanelControl *pc)
{
    GSList *li = NULL;
    PanelModule *module;

    li = g_slist_find_custom(module_list, GINT_TO_POINTER(pc->id), 
	    		     lookup_modules_by_id);

    if (!li)
	return FALSE;

    module = li->data;
    
    return module->create_control(pc);
}

gboolean create_plugin(PanelControl *pc)
{
    GSList *li = NULL;
    PanelModule *module;

    li = g_slist_find_custom(module_list, pc->filename, 
	    		     lookup_modules_by_filename);

    if (!li)
	return FALSE;

    module = li->data;
    
    return module->create_control(pc);
}

void create_launcher(PanelControl *pc)
{
    GSList *li = NULL;
    PanelModule *module;

    li = g_slist_find_custom(module_list, GINT_TO_POINTER(ICON), 
			     lookup_modules_by_id);
    module = li->data;
    
    module->create_control(pc);
}

#if 0
/*  Plugins
 *  -------
*/
static char *find_plugin(const char *name)
{
    char **dirs, **d;
    char *path;

    dirs = get_plugin_dirs();

    for(d = dirs; *d; d++)
    {
        path = g_build_filename(*d, name, NULL);

        if(g_file_test(path, G_FILE_TEST_EXISTS))
        {
            g_strfreev(dirs);
            return path;
        }
        else
            g_free(path);
    }

    return NULL;
}

gboolean create_plugin(PanelControl * pc)
{
    gpointer tmp;
    char *path;
    void (*init) (PanelControl * pc);

    /* find the module somewhere */
    g_return_val_if_fail(pc->filename, FALSE);
    g_return_val_if_fail(g_module_supported(), FALSE);

    if(pc->dirname)
        path = g_build_filename(pc->dirname, pc->filename, NULL);
    else
        path = find_plugin(pc->filename);

    g_return_val_if_fail(path, FALSE);

    /* try to load it */
    pc->gmodule = g_module_open(path, 0);

    if(!pc->gmodule)
    {
        const char *err = g_module_error();

        g_printerr("gmodule could not be opened: %s\n", path);
        g_printerr("	%s\n", err);
        g_free(path);
        return FALSE;
    }
    /* TODO: perhaps we should do versioning here ? */
    else if(!g_module_symbol(pc->gmodule, "is_xfce_panel_control", &tmp))
    {
        g_printerr("Not a panel module: %s\n", path);
        g_free(path);
        return FALSE;
    }

    g_free(path);

    if(g_module_symbol(pc->gmodule, "module_init", &tmp))
    {
        init = tmp;
        init(pc);
    }
    else
    {
        g_module_close(pc->gmodule);
        pc->gmodule = NULL;

        return FALSE;
    }

    /* TODO: should we do more testing to verify the module ? */
    return TRUE;
}
#endif

/*  The PanelControl interface
 *
*/

/* static functions */
static void panel_control_read_config(PanelControl * pc, xmlNodePtr node)
{
    if(pc->read_config)
        pc->read_config(pc, node);
}

/*  creation
*/
PanelControl *panel_control_new(int index)
{
    PanelControl *pc = g_new(PanelControl, 1);

    pc->index = index;
    pc->id = ICON;
    pc->filename = NULL;
    pc->dirname = NULL;
    pc->gmodule = NULL;

    /* this is the only widget created here
     * We use the alignment as the simplest container 
     * (a frame adds a 2 pixel border)
     */
    pc->base = gtk_event_box_new();/*gtk_alignment_new(0,0,1,1);*/
    gtk_widget_show(pc->base);

    /* protect against destruction when unpacking */
    g_object_ref(pc->base);
    gtk_object_sink(GTK_OBJECT(pc->base));

    pc->caption = NULL;
    pc->data = NULL;
    pc->with_popup = TRUE;

    pc->free = NULL;
    pc->read_config = NULL;
    pc->write_config = NULL;
    pc->attach_callback = NULL;

    pc->add_options = NULL;

    pc->set_orientation = NULL;
    pc->set_size = NULL;
    pc->set_style = NULL;
    pc->set_theme = NULL;

    return pc;
}

#if 0
void create_panel_control(PanelControl * pc)
{
    switch (pc->id)
    {
        case PLUGIN:
            create_plugin(pc);
            break;
        default:
            create_panel_item(pc);
            break;
    }

    /* these are required for proper operation */
    if(!pc->caption || !pc->attach_callback || 
	    !gtk_bin_get_child(GTK_BIN(pc->base)))
    {
        if(pc->free)
            pc->free(pc);

        create_panel_item(pc);
    }

    pc->attach_callback(pc, "button-press-event", 
	    		G_CALLBACK(panel_control_press_cb), pc);

    g_signal_connect(pc->base, "button-press-event", 
	    	     G_CALLBACK(panel_control_press_cb), pc);

    panel_control_set_settings(pc);
}
#endif

void create_panel_control(PanelControl *pc)
{
    switch (pc->id)
    {
	case ICON:
	    create_launcher(pc);
	    break;
	case PLUGIN:
	    if (!create_plugin(pc))
	    {
		g_warning("xfce4: failed to load module %s\n", pc->filename);
		create_launcher(pc);
	    }
	    break;
	default:
	    if (!create_builtin(pc))
		create_launcher(pc);
    }

    /* these are required for proper operation */
    if(!pc->caption || !pc->attach_callback || 
	    !gtk_bin_get_child(GTK_BIN(pc->base)))
    {
        if(pc->free)
            pc->free(pc);

        create_launcher(pc);
    }

    pc->attach_callback(pc, "button-press-event", 
	    		G_CALLBACK(panel_control_press_cb), pc);

    g_signal_connect(pc->base, "button-press-event", 
	    	     G_CALLBACK(panel_control_press_cb), pc);

    panel_control_set_settings(pc);
}

/* here the actual panel control is created */
void panel_control_set_from_xml(PanelControl * pc, xmlNodePtr node)
{
    xmlChar *value;

    if(!node)
    {
        create_panel_control(pc);

        return;
    }

    /* get id and filename */
    value = xmlGetProp(node, (const xmlChar *)"id");

    if(value)
    {
        pc->id = atoi(value);
        g_free(value);
    }

    if(pc->id == PLUGIN)
    {
        value = xmlGetProp(node, (const xmlChar *)"filename");

        if(value)
            pc->filename = (char *)value;
    }

    /* create a default control */
    create_panel_control(pc);

    /* allow the control to read its configuration */
    panel_control_read_config(pc, node);

    /* also check if added to the panel */
    if (!pc->with_popup && pc->base->parent) 
	groups_show_popup(pc->index, FALSE);
}

void panel_control_free(PanelControl * pc)
{
    g_free(pc->filename);
    g_free(pc->dirname);

    g_free(pc->caption);

    if(pc->free)
        pc->free(pc);

    gtk_widget_destroy(pc->base);
    g_object_unref(pc->base);

    g_free(pc);
}

void panel_control_write_xml(PanelControl * pc, xmlNodePtr parent)
{
    xmlNodePtr node;
    char value[4];

    node = xmlNewTextChild(parent, NULL, "Control", NULL);

    snprintf(value, 3, "%d", pc->id);
    xmlSetProp(node, "id", value);

    if(pc->filename)
        xmlSetProp(node, "filename", pc->filename);

    if(pc->write_config)
        pc->write_config(pc, node);
}

/*  packing and unpacking
*/
void panel_control_pack(PanelControl * pc, GtkBox * box)
{
    gtk_box_pack_start(box, pc->base, TRUE, TRUE, 0);

    panel_control_set_settings(pc);
}

void panel_control_unpack(PanelControl * pc)
{
    if(pc->base->parent && GTK_IS_WIDGET(pc->base->parent))
    {
        gtk_container_remove(GTK_CONTAINER(pc->base->parent), pc->base);
    }
}

/*  global settings
 *  ---------------
 *  These are mostly wrappers around the functions provided by a 
 *  panel control
*/
void panel_control_set_settings(PanelControl *pc)
{
    panel_control_set_orientation(pc, settings.size);
    panel_control_set_size(pc, settings.size);
    panel_control_set_style(pc, settings.style);

    if (settings.theme)
	panel_control_set_theme(pc, settings.theme);
}

void panel_control_set_orientation(PanelControl *pc, int orientation)
{
    if (pc->set_orientation)
	pc->set_orientation(pc, orientation);
}

void panel_control_set_size(PanelControl * pc, int size)
{
    int s = icon_size[size] + border_width;

    if(pc->set_size)
        pc->set_size(pc, size);
    else
	gtk_widget_set_size_request(pc->base, s, s);
}

void panel_control_set_style(PanelControl * pc, int style)
{
    if(pc->set_style)
        pc->set_style(pc, style);
}

void panel_control_set_theme(PanelControl * pc, const char *theme)
{
    if(pc->set_theme)
        pc->set_theme(pc, theme);
}

/*  Change panel controls
*/
void panel_control_add_options(PanelControl * pc, GtkContainer * container,
                               GtkWidget * revert, GtkWidget * done)
{
    if(pc->add_options)
        pc->add_options(pc, container, revert, done);
    else
    {
        GtkWidget *hbox, *image, *label;

        hbox = gtk_hbox_new(FALSE, 4);
        gtk_widget_show(hbox);
        gtk_container_set_border_width(GTK_CONTAINER(hbox), 10);

        image =
            gtk_image_new_from_stock(GTK_STOCK_DIALOG_INFO,
                                     GTK_ICON_SIZE_LARGE_TOOLBAR);
        gtk_widget_show(image);
        gtk_box_pack_start(GTK_BOX(hbox), image, TRUE, FALSE, 0);

        label = gtk_label_new(_("This module has no configuration options"));
        gtk_widget_show(label);
        gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, FALSE, 0);

        gtk_container_add(container, hbox);
    }
}
