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
	"Execute",              // command
	"Exit",
	"MoveToEdge",           // direction (lrud) snapWindows
	"ToggleSnapToEdge",     // direction (lrud + centre)
	"SnapToEdge",           // direction (lrud + centre)
	"GrowToEdge",           // direction (lrud)
	"ShrinkToEdge",         // direction (lrud)
	"NextWindow",
	"PreviousWindow",
	"Reconfigure",
	"ShowMenu",             // menu
	"ToggleMaximize",       // direction (hv both)
	"Maximize",             // direction (hv both)
	"UnMaximize",           // direction (hv both)
	"ToggleFullscreen",
	"SetDecorations",       // decorations forceSSD
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
	"ResizeRelative",       // left right up down NOT DIRECTION - separate params
	"MoveTo",               // x y
	"ResizeTo",             // width height
	"MoveToCursor",
	"MoveRelative",         // x y
	"SendToDesktop",        // to follow wrap
	"GoToDesktop",          // to wrap
	"ToggleSnapToRegion",   // region
	"SnapToRegion",         // region
	"UnSnap",
	"ToggleKeybinds",
	"FocusOutput",          // output direction (lrud) wrap
	"MoveToOutput",         // output direction (lrud) wrap
	"FitToOutput",
	"If",                   // !!!!!
	"ForEach",              // !!!!!
	"VirtualOutputAdd",     // output_name
	"VirtualOutputRemove",  // output_remove
	"AutoPlace",            // policy
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
	"WarpCursor",           // to x y
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

static GtkWidget *tv, *se, *se_ok, *se_can, *keyentry, *keylabel, *actcb, *pbox, *paramlbl, *paramentry, *paramcb;
static GtkListStore *bindings, *actions, *dirs_lrud, *dirs_lrudc, *dirs_bhv;
static GtkTreeModel *bind_sort, *act_sort;
static GtkTreeIter miter;
static gboolean keylog = FALSE;

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void check_directory (const char *path);
static void read_xml (const char *file);
static void read_defaults (void);
static void add_or_replace (GtkListStore *ls, const char *key, const char *act, const char *name, const char *param);
static void write_xml (const char *key, const char *act, const char *name, const char *param);
static void reload_bindings (void);
static void show_editor (char *key, char *act, char *name, char *param);
static void init_combo (GtkComboBox *cb, const char *init);
static void edit_ok (GtkWidget *, gpointer);
static void edit_cancel (GtkWidget *, gpointer);
static void action_changed (GtkComboBox *cb, gpointer);
static gboolean keypress (GtkWidget *, GdkEventKey *event, gpointer);
static gboolean keyrel (GtkWidget *, GdkEventKey *event, gpointer);
static void show_keystring (guint keycode, guint mods);
static void new_button (GtkWidget *, gpointer);
static void edit_button (GtkWidget *, gpointer);
static void delete_button (GtkWidget *, gpointer);
static gboolean tv_button (GtkWidget *wid, GdkEventButton *event, gpointer);
static void edit_item (GtkWidget *, gpointer);
static void delete_item (GtkWidget *, gpointer);
static void init_config (void);

/*----------------------------------------------------------------------------*/
/* Function definitions                                                       */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* Helpers                                                                    */
/*----------------------------------------------------------------------------*/

static void check_directory (const char *path)
{
    char *dir = g_path_get_dirname (path);
    g_mkdir_with_parents (dir, S_IRUSR | S_IWUSR | S_IXUSR);
    g_free (dir);
}

/*----------------------------------------------------------------------------*/
/* Read in current bindings                                                   */
/*----------------------------------------------------------------------------*/

static void read_xml (const char *file)
{
    xmlDocPtr xDoc;
    xmlXPathObjectPtr xpathObj, xpathObj2;
    xmlXPathContextPtr xpathCtx;
    xmlNode *node;
    xmlAttr *attr, *attr2;
    char *key, *act, *name, *param;
    int i;

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
            name = NULL;
            param = NULL;

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
                        name = g_strdup ((char *) attr2->name);
                        param = g_strdup ((char *) attr2->children->content);
                    }
                }
            }
            xmlXPathFreeObject (xpathObj2);

            add_or_replace (bindings, key, act, name, param);

            g_free (key);
            g_free (act);
            g_free (name);
            g_free (param);
        }
    }
    xmlXPathFreeObject (xpathObj);

    // cleanup XML
    xmlXPathFreeContext (xpathCtx);
    xmlFreeDoc (xDoc);
    xmlCleanupParser ();
}

static void read_defaults (void)
{
	for (int i = 0; key_combos[i].binding; i++)
    {
		struct key_combos *current = &key_combos[i];
        add_or_replace (bindings, current->binding, current->action, current->attributes[0].name, current->attributes[0].value);
    }
}

static void add_or_replace (GtkListStore *ls, const char *key, const char *act, const char *name, const char *param)
{
    GtkTreeIter iter;
    gboolean valid;
    char *str;

    valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (bindings), &iter);
    while (valid)
    {
        gtk_tree_model_get (GTK_TREE_MODEL (bindings), &iter, 0, &str, -1);
        if (!g_strcmp0 (str, key))
        {
            if (act) gtk_list_store_set (bindings, &iter, 0, key, 1, act, 2, name, 3, param, -1);
            else gtk_list_store_remove (bindings, &iter);
            g_free (str);
            return;
        }
        g_free (str);
        valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (bindings), &iter);
    }

    if (act) gtk_list_store_insert_with_values (bindings, NULL, -1, 0, key, 1, act, 2, name, 3, param, -1);
}

/*----------------------------------------------------------------------------*/
/* Write out bindings                                                         */
/*----------------------------------------------------------------------------*/

static void write_xml (const char *key, const char *act, const char *name, const char *param)
{
    xmlDocPtr xDoc;
    xmlXPathContextPtr xpathCtx;
    xmlXPathObjectPtr xpathObj, xpathObj2;
    xmlNodePtr root, knode, cur_node;
    char *user_file, *cptr;
    int i;

    user_file = g_build_filename (g_get_user_config_dir (), "labwc/rc.xml", NULL);
    check_directory (user_file);

    // read in data from XML file
    xmlInitParser ();
    LIBXML_TEST_VERSION
    if (g_file_test (user_file, G_FILE_TEST_IS_REGULAR))
    {
        xDoc = xmlReadFile (user_file, NULL, XML_PARSE_NOBLANKS);
        if (!xDoc) xDoc = xmlNewDoc (XC ("1.0"));
    }
    else xDoc = xmlNewDoc (XC ("1.0"));
    xpathCtx = xmlXPathNewContext (xDoc);
    xmlXPathRegisterNs (xpathCtx, XC ("o"), XC ("http://openbox.org/3.4/rc"));

    // check that the config and keyboard nodes exist in the document - create them if not
    xpathObj = xmlXPathEvalExpression (XC ("/o:openbox_config"), xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
    {
        root = xmlNewNode (NULL, XC ("openbox_config"));
        xmlNewNs (root, XC ("http://openbox.org/3.4/rc"), NULL);
        xmlDocSetRootElement (xDoc, root);
    }
    else root = xpathObj->nodesetval->nodeTab[0];
    xmlXPathFreeObject (xpathObj);

    xpathObj = xmlXPathEvalExpression (XC ("/o:openbox_config/o:keyboard"), xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
        knode = xmlNewChild (root, NULL, XC ("keyboard"), NULL);
    else
        knode = xpathObj->nodesetval->nodeTab[0];
    xmlXPathFreeObject (xpathObj);

    // find an existing node for this binding, or create one
    cptr = g_strdup_printf ("/o:openbox_config/o:keyboard/o:keybind[@key = '%s']", key);
    xpathObj = xmlXPathEvalExpression (XC (cptr), xpathCtx);
    g_free (cptr);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
    {
        cur_node = xmlNewChild (knode, NULL, XC ("keybind"), NULL);
        xmlSetProp (cur_node, XC ("key"), XC (key));
    }
    else cur_node = xpathObj->nodesetval->nodeTab[0];

    // delete any existing action nodes
    xpathObj2 = xmlXPathNodeEval (cur_node, XC ("./o:action"), xpathCtx);
    if (!xmlXPathNodeSetIsEmpty (xpathObj2->nodesetval))
    {
        for (i = 0; i < xpathObj2->nodesetval->nodeNr; i++)
        {
            xmlUnlinkNode (xpathObj2->nodesetval->nodeTab[i]);
            xmlFreeNode (xpathObj2->nodesetval->nodeTab[i]);
        }
    }
    xmlXPathFreeObject (xpathObj2);

    // create a new action node
    if (act)
    {
        cur_node = xmlNewChild (cur_node, NULL, XC ("action"), NULL);
        xmlSetProp (cur_node, XC ("name"), XC (act));
        if (name) xmlSetProp (cur_node, XC (name), XC (param));
    }

    // cleanup XML
    xmlXPathFreeContext (xpathCtx);
    xmlSaveFormatFile (user_file, xDoc, 1);
    xmlFreeDoc (xDoc);
    xmlCleanupParser ();

    g_free (user_file);
}

static void reload_bindings (void)
{
    char *user_file;

    gtk_list_store_clear (bindings);

    user_file = g_build_filename (g_get_user_config_dir (), "labwc/rc.xml", NULL);
    read_xml ("/etc/xdg/labwc/rc.xml");
    read_xml (user_file);
    g_free (user_file);

    system ("labwc --reconfigure");
}

/*----------------------------------------------------------------------------*/
/* Binding editor window                                                      */
/*----------------------------------------------------------------------------*/

static void show_editor (char *key, char *act, char *name, char *param)
{
    GtkBuilder *build;
    GtkTreeIter iter;
    char *str;

    build = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/shed.ui");
    se = (GtkWidget *) gtk_builder_get_object (build, "shedit");
    gtk_widget_show_all (se);

    se_ok = (GtkWidget *) gtk_builder_get_object (build, "btn_ok");
    se_can = (GtkWidget *) gtk_builder_get_object (build, "btn_cancel");

    keylabel = (GtkWidget *) gtk_builder_get_object (build, "lbl_key");
    keyentry = (GtkWidget *) gtk_builder_get_object (build, "keys");

    if (key)
    {
        gtk_label_set_text (GTK_LABEL (keylabel), key);
        gtk_widget_hide (keyentry);
    }
    else
    {
        g_signal_connect ((GObject *) keyentry, "key-press-event", G_CALLBACK (keypress), NULL);
        g_signal_connect ((GObject *) keyentry, "key-release-event", G_CALLBACK (keyrel), NULL);
        gtk_widget_hide (keylabel);
    }

    actcb = (GtkWidget *) gtk_builder_get_object (build, "cb_action");
    gtk_combo_box_set_model (GTK_COMBO_BOX (actcb), GTK_TREE_MODEL (act_sort));
    init_combo (GTK_COMBO_BOX (actcb), act ? act : "None");
    g_signal_connect ((GObject *) actcb, "changed", G_CALLBACK (action_changed), NULL);

    paramlbl = (GtkWidget *) gtk_builder_get_object (build, "lbl_param");
    pbox = (GtkWidget *) gtk_builder_get_object (build, "param_box");
    paramentry = (GtkWidget *) gtk_builder_get_object (build, "param");
    paramcb = (GtkWidget *) gtk_builder_get_object (build, "cb_param");

    if (param)
    {
        str = g_strdup_printf ("%s:", name);
        str[0] = g_ascii_toupper (str[0]);
        gtk_label_set_text (GTK_LABEL (paramlbl), str);
        g_free (str);

        if (!g_strcmp0 (act, "MoveToEdge") || !g_strcmp0 (act, "GrowToEdge") || !g_strcmp0 (act, "ShrinkToEdge"))
        {
            gtk_combo_box_set_model (GTK_COMBO_BOX (paramcb), GTK_TREE_MODEL (dirs_lrud));
            init_combo (GTK_COMBO_BOX (paramcb), param);
            gtk_widget_hide (paramentry);
            gtk_widget_show (paramcb);
        }
        else if (!g_strcmp0 (act, "SnapToEdge") || !g_strcmp0 (act, "ToggleSnapToEdge"))
        {
            gtk_combo_box_set_model (GTK_COMBO_BOX (paramcb), GTK_TREE_MODEL (dirs_lrudc));
            init_combo (GTK_COMBO_BOX (paramcb), param);
            gtk_widget_hide (paramentry);
            gtk_widget_show (paramcb);
        }
        else if (!g_strcmp0 (act, "ToggleMaximize") || !g_strcmp0 (act, "Maximize") || !g_strcmp0 (act, "UnMaximize"))
        {
            gtk_combo_box_set_model (GTK_COMBO_BOX (paramcb), GTK_TREE_MODEL (dirs_bhv));
            init_combo (GTK_COMBO_BOX (paramcb), param);
            gtk_widget_hide (paramentry);
            gtk_widget_show (paramcb);
        }
        else
        {
            gtk_entry_set_text (GTK_ENTRY (paramentry), param);
            gtk_widget_hide (paramcb);
            gtk_widget_show (paramentry);
        }
    }
    else gtk_widget_hide (pbox);

    g_signal_connect ((GObject *) se_ok, "clicked", G_CALLBACK (edit_ok), NULL);
    g_signal_connect ((GObject *) se_can, "clicked", G_CALLBACK (edit_cancel), NULL);

    gtk_window_present (GTK_WINDOW (se));
    g_object_unref (build);
}

static void init_combo (GtkComboBox *cb, const char *init)
{
    GtkTreeIter iter;
    GtkTreeModel *model = gtk_combo_box_get_model (cb);
    char *str;

    gtk_tree_model_get_iter_first (model, &iter);
    while (1)
    {
        gtk_tree_model_get (model, &iter, 0, &str, -1);
        if (!g_strcmp0 (init, str))
        {
            gtk_combo_box_set_active_iter (cb, &iter);
            g_free (str);
            return;
        }
        g_free (str);
        if (!gtk_tree_model_iter_next (model, &iter)) break;
    }
}

static void edit_ok (GtkWidget *, gpointer)
{
    const char *key, *param;
    char *act, *name;
    GtkTreeIter iter;

    if (gtk_widget_is_visible (keyentry)) key = gtk_entry_get_text (GTK_ENTRY (keyentry));
    else key = gtk_label_get_text (GTK_LABEL (keylabel));

    gtk_combo_box_get_active_iter (GTK_COMBO_BOX (actcb), &iter);
    gtk_tree_model_get (GTK_TREE_MODEL (act_sort), &iter, 0, &act, -1);
    if (!g_strcmp0 (act, "None"))
    {
        g_free (act);
        act = NULL;
    }

    if (gtk_widget_is_visible (pbox))
    {
        name = g_strdup (gtk_label_get_text (GTK_LABEL (paramlbl)));
        name[0] = g_ascii_tolower (name[0]);
        name[strlen (name) - 1] = 0;
        if (gtk_widget_is_visible (paramcb))
            param = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (paramcb));
        else param = gtk_entry_get_text (GTK_ENTRY (paramentry));
    }
    else
    {
        name = NULL;
        param = NULL;
    }

    write_xml (key, act, name, param);

    g_free (act);
    g_free (name);

    gtk_widget_destroy (se);

    reload_bindings ();
}

static void edit_cancel (GtkWidget *, gpointer)
{
    gtk_widget_destroy (se);
}

static void action_changed (GtkComboBox *cb, gpointer)
{
    const char *act;
    GtkTreeIter iter;

    gtk_combo_box_get_active_iter (GTK_COMBO_BOX (actcb), &iter);
    gtk_tree_model_get (GTK_TREE_MODEL (act_sort), &iter, 0, &act, -1);
    if (!g_strcmp0 (act, "Execute"))
    {
        gtk_label_set_text (GTK_LABEL (paramlbl), "Command:");
        gtk_entry_set_text (GTK_ENTRY (paramentry), "");
        gtk_widget_hide (paramcb);
        gtk_widget_show (paramentry);
        gtk_widget_show (pbox);
    }
    else if (!g_strcmp0 (act, "ShowMenu"))
    {
        gtk_label_set_text (GTK_LABEL (paramlbl), "Menu:");
        gtk_entry_set_text (GTK_ENTRY (paramentry), "");
        gtk_widget_hide (paramcb);
        gtk_widget_show (paramentry);
        gtk_widget_show (pbox);
    }
    else if (!g_strcmp0 (act, "MoveToEdge") || !g_strcmp0 (act, "GrowToEdge") || !g_strcmp0 (act, "ShrinkToEdge"))
    {
        gtk_label_set_text (GTK_LABEL (paramlbl), "Direction:");
        gtk_combo_box_set_model (GTK_COMBO_BOX (paramcb), GTK_TREE_MODEL (dirs_lrud));
        gtk_combo_box_set_active (GTK_COMBO_BOX (paramcb), 0);
        gtk_widget_hide (paramentry);
        gtk_widget_show (paramcb);
        gtk_widget_show (pbox);
    }
    else if (!g_strcmp0 (act, "SnapToEdge") || !g_strcmp0 (act, "ToggleSnapToEdge"))
    {
        gtk_label_set_text (GTK_LABEL (paramlbl), "Direction:");
        gtk_combo_box_set_model (GTK_COMBO_BOX (paramcb), GTK_TREE_MODEL (dirs_lrudc));
        gtk_combo_box_set_active (GTK_COMBO_BOX (paramcb), 0);
        gtk_widget_hide (paramentry);
        gtk_widget_show (paramcb);
        gtk_widget_show (pbox);
    }
    else if (!g_strcmp0 (act, "ToggleMaximize") || !g_strcmp0 (act, "Maximize") || !g_strcmp0 (act, "UnMaximize"))
    {
        gtk_label_set_text (GTK_LABEL (paramlbl), "Direction:");
        gtk_combo_box_set_model (GTK_COMBO_BOX (paramcb), GTK_TREE_MODEL (dirs_bhv));
        gtk_combo_box_set_active (GTK_COMBO_BOX (paramcb), 0);
        gtk_widget_hide (paramentry);
        gtk_widget_show (paramcb);
        gtk_widget_show (pbox);
    }
    else gtk_widget_hide (pbox);
}

static gboolean keypress (GtkWidget *, GdkEventKey *event, gpointer)
{
    keylog = TRUE;
    return TRUE;
}

static gboolean keyrel (GtkWidget *, GdkEventKey *event, gpointer)
{
    if (keylog)
    {
        show_keystring (event->keyval, event->state); 
        keylog = FALSE;
    }
    return TRUE;
}

static void show_keystring (guint keycode, guint mods)
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

/*----------------------------------------------------------------------------*/
/* Table and button handlers                                                  */
/*----------------------------------------------------------------------------*/

static void new_button (GtkWidget *, gpointer)
{
    show_editor (NULL, NULL, NULL, NULL);
}

static void edit_button (GtkWidget *, gpointer)
{
    char *key, *act, *name, *param;
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tv));
    if (selection && gtk_tree_selection_get_selected (selection, &model, &iter))
    {
        gtk_tree_model_get (model, &iter, 0, &key, 1, &act, 2, &name, 3, &param, -1);
        show_editor (key, act, name, param);

        g_free (key);
        g_free (act);
        g_free (name);
        g_free (param);
    }
}

static void delete_button (GtkWidget *, gpointer)
{
    char *key;
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tv));
    if (selection && gtk_tree_selection_get_selected (selection, &model, &iter))
    {
        gtk_tree_model_get (model, &iter, 0, &key, -1);
        write_xml (key, NULL, NULL, NULL);
        g_free (key);

        reload_bindings ();
    }
}

static gboolean tv_button (GtkWidget *wid, GdkEventButton *event, gpointer)
{
    GtkWidget *menu, *item;
    GtkTreeIter iter;
    GtkTreePath *path;

    if (event->button == 3)
    {
        if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (tv), event->x, event->y, &path, NULL, NULL, NULL))
        {
            gtk_tree_model_get_iter (bind_sort, &iter, path);
            gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (bind_sort), &miter, &iter);
            gtk_tree_path_free (path);

            menu = gtk_menu_new ();

            item = gtk_menu_item_new_with_label (_("New..."));
            g_signal_connect (item, "activate", G_CALLBACK (new_button), NULL);
            gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

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

static void edit_item (GtkWidget *, gpointer)
{
    char *key, *act, *name, *param;

    gtk_tree_model_get (GTK_TREE_MODEL (bindings), &miter, 0, &key, 1, &act, 2, &name, 3, &param, -1);
    show_editor (key, act, name, param);

    g_free (key);
    g_free (act);
    g_free (name);
    g_free (param);
}

static void delete_item (GtkWidget *, gpointer)
{
    char *key;
    gtk_tree_model_get (GTK_TREE_MODEL (bindings), &miter, 0, &key, -1);

    write_xml (key, NULL, NULL, NULL);
    g_free (key);

    reload_bindings ();
}

/*----------------------------------------------------------------------------*/
/* Page initialisation                                                        */
/*----------------------------------------------------------------------------*/

static void init_config (void)
{
    GtkTreeIter iter;
    GtkCellRenderer *trend;
    char *user_file;
    int i;

    bindings = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    bind_sort = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (bindings));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (bind_sort), 0, GTK_SORT_ASCENDING);

    tv = (GtkWidget *) gtk_builder_get_object (builder, "shortcuts_tv");
    gtk_tree_view_set_model (GTK_TREE_VIEW (tv), bind_sort);

    trend = gtk_cell_renderer_text_new ();

    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tv), -1, _("Key"), trend, "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tv), -1, _("Action"), trend, "text", 1, NULL);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tv), -1, _("Parameter"), trend, "text", 3, NULL);

    for (i = 0; i < 3; i++)
        gtk_tree_view_column_set_resizable (gtk_tree_view_get_column (GTK_TREE_VIEW (tv), i), TRUE);

    g_signal_connect (tv, "button-release-event", G_CALLBACK (tv_button), NULL);
    g_signal_connect ((GtkWidget *) gtk_builder_get_object (builder, "new_btn"), "clicked", G_CALLBACK (new_button), NULL);
    g_signal_connect ((GtkWidget *) gtk_builder_get_object (builder, "edit_btn"), "clicked", G_CALLBACK (edit_button), NULL);
    g_signal_connect ((GtkWidget *) gtk_builder_get_object (builder, "del_btn"), "clicked", G_CALLBACK (delete_button), NULL);

    actions = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
    for (i = 1; action_names[i]; i++)
    {
        gtk_list_store_append (actions, &iter);
        gtk_list_store_set (actions, &iter, 0, action_names[i], -1);
    }
    act_sort = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (actions));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (act_sort), 0, GTK_SORT_ASCENDING);

    dirs_lrud = gtk_list_store_new (1, G_TYPE_STRING);
    gtk_list_store_append (dirs_lrud, &iter);
    gtk_list_store_set (dirs_lrud, &iter, 0, "left", -1);
    gtk_list_store_append (dirs_lrud, &iter);
    gtk_list_store_set (dirs_lrud, &iter, 0, "right", -1);
    gtk_list_store_append (dirs_lrud, &iter);
    gtk_list_store_set (dirs_lrud, &iter, 0, "up", -1);
    gtk_list_store_append (dirs_lrud, &iter);
    gtk_list_store_set (dirs_lrud, &iter, 0, "down", -1);

    dirs_lrudc = gtk_list_store_new (1, G_TYPE_STRING);
    gtk_list_store_append (dirs_lrudc, &iter);
    gtk_list_store_set (dirs_lrudc, &iter, 0, "left", -1);
    gtk_list_store_append (dirs_lrudc, &iter);
    gtk_list_store_set (dirs_lrudc, &iter, 0, "right", -1);
    gtk_list_store_append (dirs_lrudc, &iter);
    gtk_list_store_set (dirs_lrudc, &iter, 0, "up", -1);
    gtk_list_store_append (dirs_lrudc, &iter);
    gtk_list_store_set (dirs_lrudc, &iter, 0, "down", -1);
    gtk_list_store_append (dirs_lrudc, &iter);
    gtk_list_store_set (dirs_lrudc, &iter, 0, "center", -1);

    dirs_bhv = gtk_list_store_new (1, G_TYPE_STRING);
    gtk_list_store_append (dirs_bhv, &iter);
    gtk_list_store_set (dirs_bhv, &iter, 0, "both", -1);
    gtk_list_store_append (dirs_bhv, &iter);
    gtk_list_store_set (dirs_bhv, &iter, 0, "horizontal", -1);
    gtk_list_store_append (dirs_bhv, &iter);
    gtk_list_store_set (dirs_bhv, &iter, 0, "vertical", -1);

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
    if (wm == WM_LABWC) return 1;
    else return 0;
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
