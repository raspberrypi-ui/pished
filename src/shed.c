/*============================================================================
Copyright (c) 2014-2025 Raspberry Pi
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
============================================================================*/

#include <locale.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libxml/xpathInternals.h>
#include <xkbcommon/xkbcommon.h>

#include "default-bindings.h"

/*----------------------------------------------------------------------------*/
/* Typedefs and macros                                                        */
/*----------------------------------------------------------------------------*/

#ifdef PLUGIN_NAME
extern const char *dgetfixt (const char *domain, const char *msgctxid);
#undef _
#define _(a) dgettext(GETTEXT_PACKAGE,a)
#undef C_
#define C_(a,b) dgetfixt(GETTEXT_PACKAGE,a"\004"b)
#endif

#define SUDO_PREFIX "env SUDO_ASKPASS=/usr/bin/sudopwd sudo -A "

typedef enum {
    WM_OPENBOX,
    WM_WAYFIRE,
    WM_LABWC } 
wm_type;

#define XC(str) ((xmlChar *) str)

const char *action_names[] = {
	"INVALID",
	"None",
	"Close",
	"Kill",
	"Debug",
	"Execute",
	"Exit",
	"MoveToEdge",
	"ToggleSnapToEdge",
	"SnapToEdge",
	"GrowToEdge",
	"ShrinkToEdge",
	"NextWindow",
	"PreviousWindow",
	"Reconfigure",
	"ShowMenu",
	"ToggleMaximize",
	"Maximize",
	"UnMaximize",
	"ToggleFullscreen",
	"SetDecorations",
	"ToggleDecorations",
	"ToggleAlwaysOnTop",
	"ToggleAlwaysOnBottom",
	"ToggleOmnipresent",
	"Focus",
	"Unfocus",
	"Iconify",
	"Move",
	"Raise",
	"Lower",
	"Resize",
	"ResizeRelative",
	"MoveTo",
	"ResizeTo",
	"MoveToCursor",
	"MoveRelative",
	"SendToDesktop",
	"GoToDesktop",
	"ToggleSnapToRegion",
	"SnapToRegion",
	"UnSnap",
	"ToggleKeybinds",
	"FocusOutput",
	"MoveToOutput",
	"FitToOutput",
	"If",
	"ForEach",
	"VirtualOutputAdd",
	"VirtualOutputRemove",
	"AutoPlace",
	"ToggleTearing",
	"Shade",
	"Unshade",
	"ToggleShade",
	"EnableScrollWheelEmulation",
	"DisableScrollWheelEmulation",
	"ToggleScrollWheelEmulation",
	"EnableTabletMouseEmulation",
	"DisableTabletMouseEmulation",
	"ToggleTabletMouseEmulation",
	"ToggleMagnify",
	"ZoomIn",
	"ZoomOut",
	"WarpCursor",
	"HideCursor",
	NULL
};


/*----------------------------------------------------------------------------*/
/* Global data                                                                */
/*----------------------------------------------------------------------------*/

static GtkBuilder *builder;

/* Dialogs */
static GtkWidget *main_dlg;

/* Flag to indicate window manager in use */
static wm_type wm;

static GtkWidget *tv, *se, *se_ok, *se_can, *keyentry;
static GtkListStore *ls;
static GtkTreeModelSort *sorted;
static GtkTreeIter miter;
static gboolean keylog = FALSE;

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void init_config (void);

/*----------------------------------------------------------------------------*/
/* Function definitions                                                       */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* Helpers                                                                    */
/*----------------------------------------------------------------------------*/

int vsystem (const char *fmt, ...)
{
    char *cmdline;
    int res;

    va_list arg;
    va_start (arg, fmt);
    g_vasprintf (&cmdline, fmt, arg);
    va_end (arg);
    res = system (cmdline);
    g_free (cmdline);
    return res;
}

char *get_string (char *cmd)
{
    char *line = NULL, *res = NULL;
    size_t len = 0;
    FILE *fp = popen (cmd, "r");

    if (fp == NULL) return g_strdup ("");
    if (getline (&line, &len, fp) > 0)
    {
        res = line;
        while (*res++) if (g_ascii_isspace (*res)) *res = 0;
        res = g_strdup (line);
    }
    pclose (fp);
    g_free (line);
    return res ? res : g_strdup ("");
}

char *get_quoted_string (char *cmd)
{
    char *line = NULL, *res = NULL;
    size_t len = 0;
    FILE *fp = popen (cmd, "r");

    if (fp == NULL) return g_strdup ("");
    if (getline (&line, &len, fp) > 0)
    {
        res = line;
        while (*res++) if (*res == '\'') *res = 0;
        res = g_strdup (line + 1);
    }
    pclose (fp);
    g_free (line);
    return res ? res : g_strdup ("");
}

char *rgba_to_gdk_color_string (GdkRGBA *col)
{
    int r, g, b;
    r = col->red * 255;
    g = col->green * 255;
    b = col->blue * 255;
    return g_strdup_printf ("#%02X%02X%02X", r, g, b);
}

void check_directory (const char *path)
{
    char *dir = g_path_get_dirname (path);
    g_mkdir_with_parents (dir, S_IRUSR | S_IWUSR | S_IXUSR);
    g_free (dir);
}

/*----------------------------------------------------------------------------*/
/* Initial configuration                                                      */
/*----------------------------------------------------------------------------*/

void add_or_replace (GtkListStore *ls, const char *key, const char *act, const char *nam, const char *val)
{
    GtkTreeIter iter;
    gboolean valid;
    char *str;

    valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (ls), &iter);
    while (valid)
    {
        gtk_tree_model_get (GTK_TREE_MODEL (ls), &iter, 0, &str, -1);
        if (!g_strcmp0 (str, key))
        {
            if (act) gtk_list_store_set (ls, &iter, 0, key, 1, act, 2, nam, 3, val, -1);
            else gtk_list_store_remove (ls, &iter);
            g_free (str);
            return;
        }
        g_free (str);
        valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (ls), &iter);
    }

    if (act) gtk_list_store_insert_with_values (ls, NULL, -1, 0, key, 1, act, 2, nam, 3, val, -1);
}

void read_defaults (void)
{
	for (int i = 0; key_combos[i].binding; i++)
    {
		struct key_combos *current = &key_combos[i];
        add_or_replace (ls, current->binding, current->action, current->attributes[0].name, current->attributes[0].value);
    }
}

void read_xml (const char *file)
{
    xmlDocPtr xDoc;
    xmlXPathObjectPtr xpathObj, xpathObj2;
    xmlXPathContextPtr xpathCtx;
    xmlNode *node;
    xmlAttr *attr, *attr2;
    int i;
    char *key, *act, *cmd, *arg;

    // read in data from XML file
    xmlInitParser ();
    LIBXML_TEST_VERSION
    xDoc = xmlReadFile (file, NULL, XML_PARSE_NOBLANKS);
    if (xDoc == NULL)
    {
        xmlCleanupParser ();
        return;
    }

    xpathCtx = xmlXPathNewContext (xDoc);
    xmlXPathRegisterNs (xpathCtx, XC ("o"), XC ("http://openbox.org/3.4/rc"));

    xpathObj = xmlXPathEvalExpression (XC ("/o:openbox_config/o:keyboard/o:default"), xpathCtx);
    if (!xmlXPathNodeSetIsEmpty (xpathObj->nodesetval)) read_defaults ();
    xmlXPathFreeObject (xpathObj);

    xpathObj = xmlXPathEvalExpression (XC ("/o:openbox_config/o:keyboard/o:keybind"), xpathCtx);
    if (!xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
    {
        for (i = 0; i < xpathObj->nodesetval->nodeNr; i++)
        {
            act = NULL;
            cmd = NULL;
            arg = NULL;

            node = xpathObj->nodesetval->nodeTab[i];
            for (attr = node->properties; attr; attr = attr->next)
            {
                if (!g_strcmp0 ((char *) attr->name, "key"))
                    key = g_strdup ((char *) attr->children->content);
            }
            xpathObj2 = xmlXPathNodeEval (node, XC ("./o:action"), xpathCtx);
            if (!xmlXPathNodeSetIsEmpty (xpathObj2->nodesetval))
            {
                for (attr2 = xpathObj2->nodesetval->nodeTab[0]->properties; attr2; attr2 = attr2->next)
                {
                    if (!g_strcmp0 ((char *) attr2->name, "name"))
                        act = g_strdup ((char *) attr2->children->content);
                    if (!g_strcmp0 ((char *) attr2->name, "command") || !g_strcmp0 ((char *) attr2->name, "direction")
                        || !g_strcmp0 ((char *) attr2->name, "menu"))
                    {
                        cmd = g_strdup ((char *) attr2->name);
                        arg = g_strdup ((char *) attr2->children->content);
                    }
                }
            }
            xmlXPathFreeObject (xpathObj2);

            add_or_replace (ls, key, act, cmd, arg);

            g_free (key);
            g_free (act);
            g_free (cmd);
            g_free (arg);
        }
    }
    xmlXPathFreeObject (xpathObj);

    // cleanup XML
    xmlXPathFreeContext (xpathCtx);
    xmlFreeDoc (xDoc);
    xmlCleanupParser ();
}

static void edit_ok (GtkWidget *, gpointer)
{
    gtk_widget_destroy (se);
}

static void edit_cancel (GtkWidget *, gpointer)
{
    gtk_widget_destroy (se);
}

static void show_keys_rel (guint keycode, guint mods)
{
    char buf[64], *ptr = buf;

    if (mods & GDK_SHIFT_MASK && keycode != XKB_KEY_Shift_L && keycode != XKB_KEY_Shift_R)
    {
        sprintf (ptr, "S-");
        ptr += 2;
    }
    if (mods & GDK_CONTROL_MASK && keycode != XKB_KEY_Control_L && keycode != XKB_KEY_Control_R)
    {
        sprintf (ptr, "C-");
        ptr += 2;
    }
    if (mods & GDK_MOD1_MASK && keycode != XKB_KEY_Alt_L && keycode != XKB_KEY_Alt_R)
    {
        sprintf (ptr, "A-");
        ptr += 2;
    }
    if (mods & GDK_MOD3_MASK && keycode != XKB_KEY_Hyper_L && keycode != XKB_KEY_Hyper_R)
    {
        sprintf (ptr, "H-");
        ptr += 2;
    }
    if (mods & GDK_MOD4_MASK && keycode != XKB_KEY_Super_L && keycode != XKB_KEY_Super_R)
    {
        sprintf (ptr, "W-");
        ptr += 2;
    }
    if (mods & GDK_MOD5_MASK && keycode != XKB_KEY_Meta_L && keycode != XKB_KEY_Meta_R)
    {
        sprintf (ptr, "M-");
        ptr += 2;
    }

    xkb_keysym_get_name (keycode, ptr, sizeof (buf) - (ptr - buf));

    gtk_entry_set_text (GTK_ENTRY (keyentry), buf);
}

static gboolean keypress (GtkWidget *, GdkEventKey *event, gpointer user_data)
{
    //printf ("key press %d %x %d %d\n", event->keyval, event->state, event->hardware_keycode, event->is_modifier);
    keylog = TRUE;
    return TRUE;
}

static gboolean keyrel (GtkWidget *, GdkEventKey *event, gpointer user_data)
{
    if (keylog)
    {
        //printf ("key release %d %x %d %d\n", event->keyval, event->state, event->hardware_keycode, event->is_modifier);
        show_keys_rel (event->keyval, event->state); 
        keylog = FALSE;
    }
    return TRUE;
}

static void show_editor (char *key, char *act, char *name, char *param)
{
    char *str;
    GtkBuilder *build;
    GtkWidget *wid;
    int i;

    build = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/shed.ui");
    se = (GtkWidget *) gtk_builder_get_object (build, "shedit");
    gtk_widget_show_all (se);

    se_ok = (GtkWidget *) gtk_builder_get_object (build, "btn_ok");
    se_can = (GtkWidget *) gtk_builder_get_object (build, "btn_cancel");

    if (key)
    {
        wid = (GtkWidget *) gtk_builder_get_object (build, "lbl_key");
        gtk_label_set_text (GTK_LABEL (wid), key);

        wid = (GtkWidget *) gtk_builder_get_object (build, "keys");
        gtk_widget_hide (wid);
    }
    else
    {
        wid = (GtkWidget *) gtk_builder_get_object (build, "lbl_key");
        gtk_widget_hide (wid);
    }

    wid = (GtkWidget *) gtk_builder_get_object (build, "cb_action");
    for (i = 1; action_names[i]; i++)
    {
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (wid), action_names[i]);
        if (!g_strcmp0 (act, action_names[i])) gtk_combo_box_set_active (GTK_COMBO_BOX (wid), i - 1);
    }
    if (!act) gtk_combo_box_set_active (GTK_COMBO_BOX (wid), 0);

    keyentry = (GtkWidget *) gtk_builder_get_object (build, "keys");
    g_signal_connect ((GObject *) keyentry, "key-press-event", G_CALLBACK (keypress), NULL);
    g_signal_connect ((GObject *) keyentry, "key-release-event", G_CALLBACK (keyrel), NULL);

    if (param)
    {
        wid = (GtkWidget *) gtk_builder_get_object (build, "lbl_param");
        str = g_strdup_printf ("%s:", name);
        str[0] = g_ascii_toupper (str[0]);
        gtk_label_set_text (GTK_LABEL (wid), str);
        g_free (str);

        wid = (GtkWidget *) gtk_builder_get_object (build, "param");
        gtk_entry_set_text (GTK_ENTRY (wid), param);
    }
    else
    {
        wid = (GtkWidget *) gtk_builder_get_object (build, "param_box");
        gtk_widget_hide (wid);
    }

    g_signal_connect ((GObject *) se_ok, "clicked", G_CALLBACK (edit_ok), NULL);
    g_signal_connect ((GObject *) se_can, "clicked", G_CALLBACK (edit_cancel), NULL);

    gtk_window_present (GTK_WINDOW (se));
    g_object_unref (build);
}

static void edit_item (GtkWidget *, gpointer user_data)
{
    char *key, *act, *name, *param;

    gtk_tree_model_get (GTK_TREE_MODEL (ls), &miter, 0, &key, 1, &act, 2, &name, 3, &param, -1);
    show_editor (key, act, name, param);
    g_free (key);
    g_free (act);
    g_free (name);
    g_free (param);
}

static void delete_item (GtkWidget *, gpointer user_data)
{
    char *str;
    gtk_tree_model_get (GTK_TREE_MODEL (ls), &miter, 0, &str, -1);
    printf ("%s\n", str);
}

static gboolean tv_button (GtkWidget *wid, GdkEventButton *event, gpointer userdata)
{
    GtkWidget *menu, *item;
    GtkTreeIter iter;
    GtkTreePath *path;

    if (event->button == 3)
    {
        if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (tv), event->x, event->y, &path, NULL, NULL, NULL))
        {
            gtk_tree_model_get_iter (GTK_TREE_MODEL (sorted), &iter, path);
            gtk_tree_model_sort_convert_iter_to_child_iter (sorted, &miter, &iter);
            gtk_tree_path_free (path);

            menu = gtk_menu_new ();

            item = gtk_menu_item_new_with_label (_("Edit..."));
            g_signal_connect (item, "activate", G_CALLBACK (edit_item), NULL);
            gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

            item = gtk_menu_item_new_with_label (_("Delete"));
            g_signal_connect (item, "activate", G_CALLBACK (delete_item), NULL);
            gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

            gtk_widget_show_all (menu);
            gtk_menu_popup_at_pointer (GTK_MENU (menu), gtk_get_current_event ());

            return TRUE;
        }
    }
    return FALSE;
}

static void new_button (GtkWidget *, gpointer)
{
    show_editor (NULL, NULL, NULL, NULL);
}

static void init_config (void)
{
    char *user_file;
    int i;

    ls = (GtkListStore *) gtk_builder_get_object (builder, "ls_test");
    tv = (GtkWidget *) gtk_builder_get_object (builder, "shortcuts_tv");
    sorted = (GtkTreeModelSort *) gtk_builder_get_object (builder, "sorted");
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sorted), 0, GTK_SORT_ASCENDING);

    GtkCellRenderer *trend = gtk_cell_renderer_text_new ();

    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tv), -1, _("Key"), trend, "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tv), -1, _("Action"), trend, "text", 1, NULL);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tv), -1, _("Parameter"), trend, "text", 3, NULL);

    for (i = 0; i < 4; i++)
        gtk_tree_view_column_set_resizable (gtk_tree_view_get_column (GTK_TREE_VIEW (tv), i), TRUE);

    g_signal_connect (tv, "button-release-event", G_CALLBACK (tv_button), NULL);
    g_signal_connect ((GtkWidget *) gtk_builder_get_object (builder, "new_btn"), "clicked", G_CALLBACK (new_button), NULL);

    user_file = g_build_filename (g_get_user_config_dir (), "labwc/rc.xml", NULL);
    read_xml ("/etc/xdg/labwc/rc.xml");
    read_xml (user_file);
    g_free (user_file);
}

/*----------------------------------------------------------------------------*/
/* Plugin interface                                                           */
/*----------------------------------------------------------------------------*/

void init_plugin (GtkWidget *parent)
{
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    if (getenv ("WAYLAND_DISPLAY"))
    {
        if (getenv ("WAYFIRE_CONFIG_FILE")) wm = WM_WAYFIRE;
        else wm = WM_LABWC;
    }
    else wm = WM_OPENBOX;

    main_dlg = parent;
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/shed.ui");

    init_config ();
}

int plugin_tabs (void)
{
    return 1;
}

const char *tab_name (int tab)
{
    switch (tab)
    {
        case 0 : return C_("tab", "Shortcuts");
        default : return _("No such tab");
    }
}

const char *icon_name (int tab)
{
    switch (tab)
    {
        case 0 : return "input-keyboard";
        default : return NULL;
    }
}

const char *tab_id (int tab)
{
    switch (tab)
    {
        default : return NULL;
    }
}

GtkWidget *get_tab (int tab)
{
    GtkWidget *window, *plugin;

    window = (GtkWidget *) gtk_builder_get_object (builder, "main_window");
    switch (tab)
    {
        case 0 :
            plugin = (GtkWidget *) gtk_builder_get_object (builder, "vbox");
            break;
        default :
            plugin = NULL;
    }

    gtk_container_remove (GTK_CONTAINER (window), plugin);

    return plugin;
}

gboolean reboot_needed (void)
{
    return FALSE;
}

void free_plugin (void)
{
    g_object_unref (builder);
}

/* End of file */
/*----------------------------------------------------------------------------*/
