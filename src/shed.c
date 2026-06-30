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
	"Execute",
	"Exit",
	"MoveToEdge",           // snapWindows
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
	"SetDecorations",       // forceSSD
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
	//"ResizeRelative",       // left right up down
	//"MoveTo",               // x y
	//"ResizeTo",             // width height
	"MoveToCursor",
	//"MoveRelative",         // x y
	//"SendToDesktop",        // to follow wrap
	//"GoToDesktop",          // to wrap
	"ToggleSnapToRegion",
	"SnapToRegion",
	"UnSnap",
	"ToggleKeybinds",
	//"FocusOutput",          // output direction (lrud) wrap
	//"MoveToOutput",         // output direction (lrud) wrap
	"FitToOutput",
	//"If",                   // !!!!!
	//"ForEach",              // !!!!!
	//"VirtualOutputAdd",     // output_name
	//"VirtualOutputRemove",  // output_remove
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
	//"WarpCursor",           // to x y
	"HideCursor",
	NULL
};

const char *lrudc[] = {
    "left",
    "right",
    "up",
    "down",
    "center"
};

const char *bhv[] = {
    "both",
    "horizontal",
    "vertical"
};

const char *fbn[] = {
    "full",
    "border",
    "none"
};

const char *accc[] = {
    "automatic",
    "cursor",
    "center",
    "cascade"
};

#define NPRESETS 14

const char *pres[NPRESETS * 2] = {
    N_("Volume increase"),          "wfpanelctl volumepulse volu",
    N_("Volume decrease"),          "wfpanelctl volumepulse vold",
    N_("Volume mute"),              "wfpanelctl volumepulse mute",
    N_("Show main menu"),           "wfpanelctl smenu menu",
    N_("Show network menu"),        "wfpanelctl netman menu",
    N_("Show Bluetooth menu"),      "wfpanelctl bluetooth menu",
    N_("Show icon launcher"),       "wfpanelctl nmenu menu",
    N_("Capture entire screen"),    "gui-screenshot",
    N_("Capture part of screen"),   "gui-screenshot -a",
    N_("Run command"),              "gui-runcmd",
    N_("Install screen reader"),    "gui-pkinst orca reboot",
    N_("Show shutdown options"),    "pishutdown",
    N_("Lock screen"),              "swaylock -p",
    N_("Open terminal"),            "lxterminal"
};

/*----------------------------------------------------------------------------*/
/* Global data                                                                */
/*----------------------------------------------------------------------------*/

/* Flag to indicate window manager in use */
static wm_type wm;

static GtkBuilder *builder;
static GtkWidget *main_dlg, *tv, *newbtn, *editbtn, *delbtn, *conf;
static GtkWidget *se, *se_ok, *se_can, *keyentry, *keylabel, *actcb, *pbox, *paramlbl, *paramentry, *paramcb, *prescb;
static GtkListStore *bindings, *actions, *dirs_lrud, *dirs_lrudc, *dirs_bhv, *decor, *policy, *presets;
static GtkTreeModel *bind_sort, *act_sort, *pre_sort;
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
static void preset_changed (GtkComboBox *cb, gpointer);
static void param_changed (GtkEditable *, gpointer);
static gboolean keypress (GtkWidget *, GdkEventKey *event, gpointer);
static gboolean keyrel (GtkWidget *, GdkEventKey *event, gpointer);
static void show_keystring (guint keycode, guint mods);
static void new_button (GtkWidget *, gpointer);
static void edit_button (GtkWidget *, gpointer);
static void delete_button (GtkWidget *, gpointer);
static gboolean tv_button (GtkWidget *wid, GdkEventButton *event, gpointer);
static void tv_cursor (GtkTreeView *tv, gpointer);
static void edit_item (GtkWidget *, gpointer);
static void delete_item (GtkWidget *, gpointer);
static void show_confirm_dialog (void);
static void conf_ok (GtkButton *, gpointer);
static void conf_cancel (GtkButton *, gpointer);
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
                        || !g_strcmp0 ((char *) attr2->name, "menu") || !g_strcmp0 ((char *) attr2->name, "decorations")
                        || !g_strcmp0 ((char *) attr2->name, "region")|| !g_strcmp0 ((char *) attr2->name, "policy"))
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

    textdomain (GETTEXT_PACKAGE);
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
        g_signal_connect (keyentry, "key-press-event", G_CALLBACK (keypress), NULL);
        g_signal_connect (keyentry, "key-release-event", G_CALLBACK (keyrel), NULL);
        gtk_widget_hide (keylabel);
        gtk_widget_set_sensitive (se_ok, FALSE);
    }

    actcb = (GtkWidget *) gtk_builder_get_object (build, "cb_action");
    gtk_combo_box_set_model (GTK_COMBO_BOX (actcb), GTK_TREE_MODEL (act_sort));
    init_combo (GTK_COMBO_BOX (actcb), act ? act : "None");
    g_signal_connect (actcb, "changed", G_CALLBACK (action_changed), NULL);

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

        if (strstr (act, "ToEdge") || strstr (act, "Maximize") || strstr (act, "SetDecorations") || strstr (act, "AutoPlace"))
        {
            if (!g_strcmp0 (act, "MoveToEdge") || !g_strcmp0 (act, "GrowToEdge") || !g_strcmp0 (act, "ShrinkToEdge"))
                gtk_combo_box_set_model (GTK_COMBO_BOX (paramcb), GTK_TREE_MODEL (dirs_lrud));
            else if (!g_strcmp0 (act, "SnapToEdge") || !g_strcmp0 (act, "ToggleSnapToEdge"))
                gtk_combo_box_set_model (GTK_COMBO_BOX (paramcb), GTK_TREE_MODEL (dirs_lrudc));
            else if (!g_strcmp0 (act, "ToggleMaximize") || !g_strcmp0 (act, "Maximize") || !g_strcmp0 (act, "UnMaximize"))
                gtk_combo_box_set_model (GTK_COMBO_BOX (paramcb), GTK_TREE_MODEL (dirs_bhv));
            else if (!g_strcmp0 (act, "SetDecorations"))
                gtk_combo_box_set_model (GTK_COMBO_BOX (paramcb), GTK_TREE_MODEL (decor));
            else if (!g_strcmp0 (act, "AutoPlace"))
                gtk_combo_box_set_model (GTK_COMBO_BOX (paramcb), GTK_TREE_MODEL (policy));

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
    g_signal_connect (paramentry, "changed", G_CALLBACK (param_changed), NULL);

    prescb = (GtkWidget *) gtk_builder_get_object (build, "cb_preset");
    gtk_combo_box_set_model (GTK_COMBO_BOX (prescb), GTK_TREE_MODEL (pre_sort));
    gtk_combo_box_set_active (GTK_COMBO_BOX (prescb), -1);
    if (!g_strcmp0 (act, "Execute") && !g_strcmp0 (name, "command") && param)
    {
        gtk_tree_model_get_iter_first (GTK_TREE_MODEL (pre_sort), &iter);
        while (1)
        {
            gtk_tree_model_get (GTK_TREE_MODEL (pre_sort), &iter, 1, &str, -1);
            if (!g_strcmp0 (param, str))
            {
                gtk_combo_box_set_active_iter (GTK_COMBO_BOX (prescb), &iter);
                g_free (str);
                break;
            }
            g_free (str);
            if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (pre_sort), &iter)) break;
        }
    }
    g_signal_connect (prescb, "changed", G_CALLBACK (preset_changed), NULL);

    g_signal_connect (se_ok, "clicked", G_CALLBACK (edit_ok), NULL);
    g_signal_connect (se_can, "clicked", G_CALLBACK (edit_cancel), NULL);

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

    gtk_combo_box_get_active_iter (cb, &iter);
    gtk_tree_model_get (GTK_TREE_MODEL (act_sort), &iter, 0, &act, -1);
    if (g_strcmp0 (act, "Execute")) gtk_combo_box_set_active (GTK_COMBO_BOX (prescb), -1);

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
    else if (strstr (act, "SnapToRegion"))
    {
        gtk_label_set_text (GTK_LABEL (paramlbl), "Region:");
        gtk_entry_set_text (GTK_ENTRY (paramentry), "");
        gtk_widget_hide (paramcb);
        gtk_widget_show (paramentry);
        gtk_widget_show (pbox);
    }
    else if (strstr (act, "ToEdge") || strstr (act, "Maximize") || strstr (act, "SetDecorations") || strstr (act, "AutoPlace"))
    {
        if (!g_strcmp0 (act, "MoveToEdge") || !g_strcmp0 (act, "GrowToEdge") || !g_strcmp0 (act, "ShrinkToEdge"))
        {
            gtk_combo_box_set_model (GTK_COMBO_BOX (paramcb), GTK_TREE_MODEL (dirs_lrud));
            gtk_label_set_text (GTK_LABEL (paramlbl), "Direction:");
        }
        else if (!g_strcmp0 (act, "SnapToEdge") || !g_strcmp0 (act, "ToggleSnapToEdge"))
        {
            gtk_combo_box_set_model (GTK_COMBO_BOX (paramcb), GTK_TREE_MODEL (dirs_lrudc));
            gtk_label_set_text (GTK_LABEL (paramlbl), "Direction:");
        }
        else if (!g_strcmp0 (act, "ToggleMaximize") || !g_strcmp0 (act, "Maximize") || !g_strcmp0 (act, "UnMaximize"))
        {
            gtk_combo_box_set_model (GTK_COMBO_BOX (paramcb), GTK_TREE_MODEL (dirs_bhv));
            gtk_label_set_text (GTK_LABEL (paramlbl), "Direction:");
        }
        else if (!g_strcmp0 (act, "SetDecorations"))
        {
            gtk_combo_box_set_model (GTK_COMBO_BOX (paramcb), GTK_TREE_MODEL (decor));
            gtk_label_set_text (GTK_LABEL (paramlbl), "Decorations:");
        }
        else if (!g_strcmp0 (act, "AutoPlace"))
        {
            gtk_combo_box_set_model (GTK_COMBO_BOX (paramcb), GTK_TREE_MODEL (policy));
            gtk_label_set_text (GTK_LABEL (paramlbl), "Policy:");
        }

        gtk_combo_box_set_active (GTK_COMBO_BOX (paramcb), 0);
        gtk_widget_hide (paramentry);
        gtk_widget_show (paramcb);
        gtk_widget_show (pbox);
    }
    else gtk_widget_hide (pbox);
}

static void preset_changed (GtkComboBox *cb, gpointer)
{
    const char *str;
    GtkTreeIter iter;

    if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (cb), &iter)) return;
    gtk_tree_model_get (GTK_TREE_MODEL (pre_sort), &iter, 1, &str, -1);

    init_combo (GTK_COMBO_BOX (actcb), "Execute");
    gtk_label_set_text (GTK_LABEL (paramlbl), "Command:");
    gtk_entry_set_text (GTK_ENTRY (paramentry), str);
    gtk_widget_hide (paramcb);
    gtk_widget_show (paramentry);
    gtk_widget_show (pbox);
}

static void param_changed (GtkEditable *, gpointer)
{
    GtkTreeIter iter;
    char *str;

    gtk_combo_box_get_active_iter (GTK_COMBO_BOX (actcb), &iter);
    gtk_tree_model_get (GTK_TREE_MODEL (act_sort), &iter, 0, &str, -1);
    if (!g_strcmp0 (str, "Execute"))
    {
        g_free (str);
        gtk_combo_box_set_active (GTK_COMBO_BOX (prescb), -1);
        gtk_tree_model_get_iter_first (GTK_TREE_MODEL (pre_sort), &iter);
        while (1)
        {
            gtk_tree_model_get (GTK_TREE_MODEL (pre_sort), &iter, 1, &str, -1);
            if (!g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (paramentry)), str))
            {
                gtk_combo_box_set_active_iter (GTK_COMBO_BOX (prescb), &iter);
                g_free (str);
                break;
            }
            g_free (str);
            if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (pre_sort), &iter)) break;
        }
    }
    else g_free (str);
}

static gboolean keypress (GtkWidget *, GdkEventKey *event, gpointer)
{
    keylog = TRUE;
    return TRUE;
}

static gboolean keyrel (GtkWidget *, GdkEventKey *event, gpointer)
{
    gtk_widget_set_sensitive (se_ok, TRUE);
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
    GtkTreeIter iter;
    GtkTreeSelection *selection;

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tv));
    if (selection && gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
        gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (bind_sort), &miter, &iter);
        show_confirm_dialog ();
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

static void tv_cursor (GtkTreeView *tv, gpointer)
{
    GtkTreePath *path;
    gboolean sens = FALSE;
    gtk_tree_view_get_cursor (tv, &path, NULL);
    if (path)
    {
        sens = TRUE;
        gtk_tree_path_free (path);
    }
    gtk_widget_set_sensitive (editbtn, sens);
    gtk_widget_set_sensitive (delbtn, sens);
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
    show_confirm_dialog ();
}

/*----------------------------------------------------------------------------*/
/* Confirmation dialog                                                        */
/*----------------------------------------------------------------------------*/

static void show_confirm_dialog (void)
{
    GtkBuilder *build;

    textdomain (GETTEXT_PACKAGE);
    build = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/shed.ui");

    conf = (GtkWidget *) gtk_builder_get_object (build, "modal");
    gtk_window_set_transient_for (GTK_WINDOW (conf), GTK_WINDOW (main_dlg));
    g_signal_connect (gtk_builder_get_object (build, "modal_ok"), "clicked", G_CALLBACK (conf_ok), NULL);
    g_signal_connect (gtk_builder_get_object (build, "modal_cancel"), "clicked", G_CALLBACK (conf_cancel), NULL);
    gtk_widget_show (conf);
    g_object_unref (build);
}

static void conf_ok (GtkButton *, gpointer)
{
    char *key;

    gtk_widget_destroy (conf);

    gtk_tree_model_get (GTK_TREE_MODEL (bindings), &miter, 0, &key, -1);
    write_xml (key, NULL, NULL, NULL);
    g_free (key);

    reload_bindings ();
}

static void conf_cancel (GtkButton *, gpointer)
{
    gtk_widget_destroy (conf);
}

/*----------------------------------------------------------------------------*/
/* Page initialisation                                                        */
/*----------------------------------------------------------------------------*/

static void init_config (void)
{
    GtkCellRenderer *trend;
    char *user_file;
    int i;

    bindings = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    bind_sort = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (bindings));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (bind_sort), 0, GTK_SORT_ASCENDING);

    tv = (GtkWidget *) gtk_builder_get_object (builder, "shortcuts_tv");
    newbtn = (GtkWidget *) gtk_builder_get_object (builder, "new_btn");
    editbtn = (GtkWidget *) gtk_builder_get_object (builder, "edit_btn");
    delbtn = (GtkWidget *) gtk_builder_get_object (builder, "del_btn");
    gtk_widget_set_sensitive (editbtn, FALSE);
    gtk_widget_set_sensitive (delbtn, FALSE);

    gtk_tree_view_set_model (GTK_TREE_VIEW (tv), bind_sort);

    trend = gtk_cell_renderer_text_new ();

    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tv), -1, _("Key"), trend, "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tv), -1, _("Action"), trend, "text", 1, NULL);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tv), -1, _("Parameter"), trend, "text", 3, NULL);

    for (i = 0; i < 3; i++)
    {
        gtk_tree_view_column_set_resizable (gtk_tree_view_get_column (GTK_TREE_VIEW (tv), i), TRUE);
        gtk_tree_view_column_set_sizing (gtk_tree_view_get_column (GTK_TREE_VIEW (tv), i), GTK_TREE_VIEW_COLUMN_GROW_ONLY);
    }

    g_signal_connect (tv, "button-release-event", G_CALLBACK (tv_button), NULL);
    g_signal_connect (tv, "cursor-changed", G_CALLBACK (tv_cursor), NULL);
    g_signal_connect (newbtn, "clicked", G_CALLBACK (new_button), NULL);
    g_signal_connect (editbtn, "clicked", G_CALLBACK (edit_button), NULL);
    g_signal_connect (delbtn, "clicked", G_CALLBACK (delete_button), NULL);

    actions = gtk_list_store_new (1, G_TYPE_STRING);
    for (i = 1; action_names[i]; i++) gtk_list_store_insert_with_values (actions, NULL, -1, 0, action_names[i], -1);
    act_sort = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (actions));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (act_sort), 0, GTK_SORT_ASCENDING);

    dirs_lrud = gtk_list_store_new (1, G_TYPE_STRING);
    for (i = 0; i < 4; i++) gtk_list_store_insert_with_values (dirs_lrud, NULL, -1, 0, lrudc[i], -1);

    dirs_lrudc = gtk_list_store_new (1, G_TYPE_STRING);
    for (i = 0; i < 5; i++) gtk_list_store_insert_with_values (dirs_lrudc, NULL, -1, 0, lrudc[i], -1);

    dirs_bhv = gtk_list_store_new (1, G_TYPE_STRING);
    for (i = 0; i < 3; i++) gtk_list_store_insert_with_values (dirs_bhv, NULL, -1, 0, bhv[i], -1);

    decor = gtk_list_store_new (1, G_TYPE_STRING);
    for (i = 0; i < 3; i++) gtk_list_store_insert_with_values (decor, NULL, -1, 0, fbn[i], -1);

    policy = gtk_list_store_new (1, G_TYPE_STRING);
    for (i = 0; i < 4; i++) gtk_list_store_insert_with_values (policy, NULL, -1, 0, accc[i], -1);

    presets = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
    for (i = 0; i < NPRESETS; i++) gtk_list_store_insert_with_values (presets, NULL, -1, 0, _(pres[i * 2]), 1, pres[i * 2 + 1], -1);
    pre_sort = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (presets));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (pre_sort), 0, GTK_SORT_ASCENDING);

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
