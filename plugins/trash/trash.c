/*  trash.c
 *
 *  Copyright (C) 2002 Jasper Huijsmans(huysmans@users.sourceforge.net)
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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <libxfcegui4/xfce_iconbutton.h>

#include "xfce.h"
#include "plugins.h"

/* panel control configuration
   Global widget used in all the module configuration
   to revert the settings
*/
GtkWidget *revert_button;

/* this is checked when the control is loaded */
int is_xfce_panel_control = 1;


enum
{
    TRASH_EMPTY_ICON,
    TRASH_FULL_ICON,
    MODULE_ICONS
};

static char *trash_icon_names[] = {
    "trash_empty",
    "trash_full"
};

/*  Trash module
 *  ------------
*/
typedef struct
{
    int timeout_id;

    char *dirname;
    char *command;
    gboolean in_terminal;

    gboolean empty;

    GdkPixbuf *empty_pb;
    GdkPixbuf *full_pb;

    GtkWidget *button;
}
t_trash;

static GdkPixbuf *get_trash_pixbuf(int id)
{
    GdkPixbuf *pb;
    
    pb = get_themed_pixbuf(trash_icon_names[id]);

    if(!pb)
        pb = get_pixbuf_by_id(UNKNOWN_ICON);

    return pb;
}

static void trash_run(t_trash * trash)
{
    exec_cmd(trash->command, trash->in_terminal);
}

void trash_dropped(GtkWidget * widget, GList * drop_data, gpointer data)
{
    t_trash *trash = (t_trash *) data;
    char *cmd;
    GList *li;

    for(li = drop_data; li && li->data; li = li->next)
    {
        cmd = g_strconcat(trash->command, " ", (char *)li->data, NULL);

        exec_cmd_silent(cmd, FALSE);

        g_free(cmd);
    }
}

static t_trash *trash_new(void)
{
    t_trash *trash = g_new(t_trash, 1);
    const char *home = g_getenv("HOME");
    GtkWidget *b;

    trash->timeout_id = 0;

    trash->dirname = g_strconcat(home, "/.xfce/trash", NULL);

    trash->empty = TRUE;

    trash->command = g_strdup("xftrash");
    trash->in_terminal = FALSE;

    trash->empty_pb = get_trash_pixbuf(TRASH_EMPTY_ICON);
    trash->full_pb = get_trash_pixbuf(TRASH_FULL_ICON);

    trash->button = xfce_iconbutton_new_from_pixbuf(trash->empty_pb);
    gtk_widget_show(trash->button);
    gtk_button_set_relief(GTK_BUTTON(trash->button), GTK_RELIEF_NONE);

    b = trash->button;

    add_tooltip(b, _("Trashcan: 0 files"));

    /* signals */
    dnd_set_drag_dest(b);
    dnd_set_callback(b, DROP_CALLBACK(trash_dropped), trash);

    g_signal_connect_swapped(b, "clicked", G_CALLBACK(trash_run), trash);

    return trash;
}

static gboolean check_trash(t_trash *trash)
{
    GDir *dir;
    const char *file;
    char text[MAXSTRLEN];
    gboolean changed = FALSE;

    if(!trash->dirname)
        return TRUE;

    dir = g_dir_open(trash->dirname, 0, NULL);

    if(dir)
        file = g_dir_read_name(dir);

    if(!dir || !file)
    {
        if(!trash->empty)
        {
            trash->empty = TRUE;
            changed = TRUE;
            xfce_iconbutton_set_pixbuf(XFCE_ICONBUTTON(trash->button), trash->empty_pb);
            add_tooltip(trash->button, _("Trashcan: 0 files"));
        }
    }
    else
    {
        struct stat s;
        int number = 0;
        int size = 0;
        char *cwd = g_get_current_dir();

        chdir(trash->dirname);

        if(trash->empty)
        {
            trash->empty = FALSE;
            changed = TRUE;
            xfce_iconbutton_set_pixbuf(XFCE_ICONBUTTON(trash->button), trash->full_pb);
        }

        while(file)
        {
            number++;

            stat(file, &s);
            size += s.st_size;

            file = g_dir_read_name(dir);
        }

        chdir(cwd);
        g_free(cwd);

        if(size < 1024)
            sprintf(text, _("Trashcan: %d files / %d B"), number, size);
        else if(size < 1024 * 1024)
            sprintf(text, _("Trashcan: %d files / %d KB"), number, size / 1024);
        else
            sprintf(text, _("Trashcan: %d files / %d MB"), number,
                    size / (1024 * 1024));

        add_tooltip(trash->button, text);
    }

    if(dir)
        g_dir_close(dir);

    return TRUE;
}

static void run_trash(PanelControl *pc)
{
    t_trash *trash = pc->data;

    if (trash->timeout_id > 0)
    {
	g_source_remove(trash->timeout_id);
	trash->timeout_id = 0;
    }

    /* 2 seconds */
    trash->timeout_id = g_timeout_add(2000, (GSourceFunc)check_trash, trash);
}

static void trash_free(PanelControl * pc)
{
    t_trash *trash = (t_trash *) pc->data;
    pc->data = NULL;

    if (trash->timeout_id > 0)
	g_source_remove(trash->timeout_id);

    g_free(trash->dirname);

    g_object_unref(trash->empty_pb);
    g_object_unref(trash->full_pb);

    g_free(trash);
}

static void trash_attach_callback(PanelControl *pc, const char *signal,
				  GCallback callback, gpointer data)
{
    t_trash *trash = (t_trash *) pc->data;

    g_signal_connect(trash->button, signal, callback, data);
}

static void trash_set_theme(PanelControl * pc, const char *theme)
{
    t_trash *trash = (t_trash *) pc->data;

    g_object_unref(trash->empty_pb);
    g_object_unref(trash->full_pb);

    trash->empty_pb = get_trash_pixbuf(TRASH_EMPTY_ICON);
    trash->full_pb = get_trash_pixbuf(TRASH_FULL_ICON);

    if(trash->empty)
        xfce_iconbutton_set_pixbuf(XFCE_ICONBUTTON(trash->button), trash->empty_pb);
    else
        xfce_iconbutton_set_pixbuf(XFCE_ICONBUTTON(trash->button), trash->full_pb);

    panel_control_set_size(pc, settings.size);
}

/*  create trash panel control
*/
#define CAPTION N_("Trash can")

gboolean create_trash_control(PanelControl * pc)
{
    t_trash *trash = trash_new();
    GtkWidget *b = trash->button;

    gtk_container_add(GTK_CONTAINER(pc->base), b);

    pc->caption = g_strdup(_(CAPTION));
    pc->data = trash;

    pc->free = trash_free;
    pc->attach_callback = trash_attach_callback;

    pc->set_theme = trash_set_theme;

    run_trash(pc);

    return TRUE;
}

G_MODULE_EXPORT void xfce_plugin_init(PanelModule *module)
{
    module->name = "trash";
    module->caption = _(CAPTION);
    module->create_control = (CreateControlFunc) create_trash_control;
}


XFCE_PLUGIN_CHECK_INIT

