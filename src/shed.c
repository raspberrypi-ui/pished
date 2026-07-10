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

#define NPRESETS 14

#define KB_KEY      0
#define KB_ACTION   1
#define KB_NAME     2
#define KB_ARG      3
#define KB_REL      4
#define KB_LABEL    5

/*----------------------------------------------------------------------------*/
/* Global data                                                                */
/*----------------------------------------------------------------------------*/

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

const char *pres[NPRESETS * 2] = {
    N_("Volume Increase"),          "wfpanelctl volumepulse volu",
    N_("Volume Decrease"),          "wfpanelctl volumepulse vold",
    N_("Volume Mute"),              "wfpanelctl volumepulse mute",
    N_("Show Main Menu"),           "wfpanelctl smenu menu",
    N_("Show Network Menu"),        "wfpanelctl netman menu",
    N_("Show Bluetooth Menu"),      "wfpanelctl bluetooth menu",
    N_("Show Icon Launcher"),       "wfpanelctl nmenu menu",
    N_("Capture Entire Screen"),    "gui-screenshot",
    N_("Capture Part of Screen"),   "gui-screenshot -a",
    N_("Run Command"),              "gui-runcmd",
    N_("Install Screen Reader"),    "gui-pkinst orca reboot",
    N_("Show Shutdown Options"),    "pishutdown",
    N_("Lock Screen"),              "swaylock -p",
    N_("Open Terminal"),            "lxterminal"
};

/* Flag to indicate window manager in use */
static wm_type wm;

static GtkBuilder *builder;
static GtkWidget *main_dlg, *tv, *newbtn, *editbtn, *delbtn, *hlpbtn, *conf;
static GtkWidget *se, *se_ok, *se_can, *keyentry, *keylabel, *actcb, *pbox, *paramlbl, *paramentry, *paramcb, *prescb, *relchk;
static GtkListStore *bindings, *actions, *dirs_lrud, *dirs_lrudc, *dirs_bhv, *decor, *policy, *presets;
static GtkTreeModel *bind_sort, *act_sort, *pre_sort;
static GtkTreeIter miter;
static gboolean keylog = FALSE;
static gboolean pressed;
static double press_x, press_y;
static char *app_id;

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void check_directory (const char *path);
static void read_xml (const char *file);
static gboolean match_field (const xmlChar *val);
static void read_defaults (void);
static char *decamel (const char *in);
static void add_or_replace (GtkListStore *ls, const char *key, const char *act, const char *name, const char *param, gboolean rel);
static void write_xml (const char *key, const char *act, const char *name, const char *param, gboolean rel);
static void reload_bindings (void);
static void show_editor (char *key, char *act, char *name, char *param, gboolean rel);
static void init_combo (GtkComboBox *cb, const char *init);
static gboolean reset_appid (GtkWidget *, GdkEvent *, gpointer);
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
static void help_button (GtkWidget *, gpointer);
static GtkWidget *popup_menu (int x, int y);
static gboolean tv_button (GtkWidget *wid, GdkEventButton event, gpointer);
static void gesture_pressed (GtkGestureLongPress *, gdouble x, gdouble y, gpointer);
static void gesture_end (GtkGestureLongPress *, GdkEventSequence *, gpointer);
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
    xmlXPathObjectPtr xpathObj, xpathObj2, xpathObj3;
    xmlXPathContextPtr xpathCtx;
    xmlNode *node;
    xmlAttr *attr, *attr2;
    char *key, *act, *name, *param;
    gboolean rel;
    int i, j;

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
            key = NULL;
            act = NULL;
            name = NULL;
            param = NULL;
            rel = FALSE;

            node = xpathObj->nodesetval->nodeTab[i];
            for (attr = node->properties; attr; attr = attr->next)
            {
                if (!xmlStrcmp (attr->name, XC ("key")))
                    key = g_strdup ((char *) attr->children->content);
                if (!xmlStrcmp (attr->name, XC ("onRelease")) && !xmlStrcmp (attr->children->content, XC ("yes"))) rel = TRUE;
            }
            xpathObj2 = xmlXPathNodeEval (node, XC ("./o:action"), xpathCtx);
            if (!xmlXPathNodeSetIsEmpty (xpathObj2->nodesetval))
            {
                for (attr2 = xpathObj2->nodesetval->nodeTab[0]->properties; attr2; attr2 = attr2->next)
                {
                    if (!xmlStrcmp (attr2->name, XC ("name")))
                        act = g_strdup ((char *) attr2->children->content);
                    if (match_field (attr2->name))
                    {
                        name = g_strdup ((char *) attr2->name);
                        param = g_strdup ((char *) attr2->children->content);
                    }
                }

                node = xpathObj2->nodesetval->nodeTab[0];
                xpathObj3 = xmlXPathNodeEval (node, XC ("./o:*"), xpathCtx);
                if (!xmlXPathNodeSetIsEmpty (xpathObj3->nodesetval))
                {
                    for (j = 0; j < xpathObj3->nodesetval->nodeNr; j++)
                    {
                        node = xpathObj3->nodesetval->nodeTab[j];
                        if (act == NULL && !xmlStrcmp (node->name, XC ("name")))
                            act = g_strdup ((char *) xmlNodeGetContent (node));
                        if (name == NULL && match_field (node->name))
                        {
                            name = g_strdup ((char *) node->name);
                            param = g_strdup ((char *) xmlNodeGetContent (node));
                        }
                    }
                }
                xmlXPathFreeObject (xpathObj3);
            }
            xmlXPathFreeObject (xpathObj2);

            add_or_replace (bindings, key, act, name, param, rel);

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

static gboolean match_field (const xmlChar *val)
{
    if (!xmlStrcmp (val, XC ("command"))
        || !xmlStrcmp (val, XC ("direction"))
        || !xmlStrcmp (val, XC ("menu"))
        || !xmlStrcmp (val, XC ("decorations"))
        || !xmlStrcmp (val, XC ("region"))
        || !xmlStrcmp (val, XC ("policy")))
        return TRUE;
    else return FALSE;
}

static void read_defaults (void)
{
    for (int i = 0; key_combos[i].binding; i++)
    {
        struct key_combos *current = &key_combos[i];
        add_or_replace (bindings, current->binding, current->action, current->attributes[0].name, current->attributes[0].value, FALSE);
    }
}

static char *decamel (const char *in)
{
    char *out = NULL, *tmp;

    while (*in)
    {
        tmp = out;
        if (tmp == NULL)
            out = g_strdup_printf ("%c", *in);
        else if (*in >= 'A' && *in <= 'Z')
            out = g_strdup_printf ("%s %c", tmp, *in);
        else
            out = g_strdup_printf ("%s%c", tmp, *in);
        g_free (tmp);
        in++;
    }

    return out;
}

static void add_or_replace (GtkListStore *ls, const char *key, const char *act, const char *name, const char *param, gboolean rel)
{
    GtkTreeIter iter;
    gboolean valid;
    char *str, *lbl, *desc = NULL;

    // look for a preset which matches this action if it is an Execute, and use it as the description
    if (!g_strcmp0 (act, "Execute") && !g_strcmp0 (name, "command") && param)
    {
        gtk_tree_model_get_iter_first (GTK_TREE_MODEL (pre_sort), &iter);
        while (1)
        {
            gtk_tree_model_get (GTK_TREE_MODEL (pre_sort), &iter, 0, &lbl, 1, &str, -1);
            if (!g_strcmp0 (param, str))
            {
                desc = g_strdup (lbl);
                g_free (str);
                g_free (lbl);
                break;
            }
            g_free (str);
            g_free (lbl);
            if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (pre_sort), &iter)) break;
        }
    }

    // otherwise create a description from the camel case action name
    if (act && !desc)
    {
        str = decamel (act);
        if (param && param[0]) desc = g_strdup_printf ("%s '%s'", str, param);
        else desc = g_strdup (str);
        g_free (str);
    }

    valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (bindings), &iter);
    while (valid)
    {
        gtk_tree_model_get (GTK_TREE_MODEL (bindings), &iter, KB_KEY, &str, -1);
        if (!g_ascii_strcasecmp (str, key))
        {
            if (act)
                gtk_list_store_set (bindings, &iter, KB_KEY, key, KB_ACTION, act, KB_NAME, name,
                    KB_ARG, param, KB_REL, rel, KB_LABEL, desc, -1);
            else
                gtk_list_store_remove (bindings, &iter);
            g_free (str);
            return;
        }
        g_free (str);
        valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (bindings), &iter);
    }

    if (act)
        gtk_list_store_insert_with_values (bindings, NULL, -1, KB_KEY, key, KB_ACTION, act, KB_NAME, name,
            KB_ARG, param, KB_REL, rel, KB_LABEL, desc, -1);

    g_free (desc);
}

/*----------------------------------------------------------------------------*/
/* Write out bindings                                                         */
/*----------------------------------------------------------------------------*/

static void write_xml (const char *key, const char *act, const char *name, const char *param, gboolean rel)
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

    if (rel) xmlSetProp (cur_node, XC ("onRelease"), XC ("yes"));

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

static void show_editor (char *key, char *act, char *name, char *param, gboolean rel)
{
    GtkBuilder *build;
    GtkTreeIter iter;
    char *str;

    textdomain (GETTEXT_PACKAGE);
    build = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/shed.ui");
    se = (GtkWidget *) gtk_builder_get_object (build, "shedit");
    gtk_window_set_transient_for (GTK_WINDOW (se), GTK_WINDOW (main_dlg));

    app_id = g_strdup (g_get_prgname ());
    if (!key) g_set_prgname ("pished_edit_shortcut");

    gtk_widget_show_all (se);

    se_ok = (GtkWidget *) gtk_builder_get_object (build, "btn_ok");
    se_can = (GtkWidget *) gtk_builder_get_object (build, "btn_cancel");

    keylabel = (GtkWidget *) gtk_builder_get_object (build, "lbl_key");
    keyentry = (GtkWidget *) gtk_builder_get_object (build, "keys");
    relchk = (GtkWidget *) gtk_builder_get_object (build, "check_rel");

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

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (relchk), rel);

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

    g_signal_connect (se, "destroy", G_CALLBACK (reset_appid), NULL);

    gtk_window_present (GTK_WINDOW (se));
    g_object_unref (build);
}

static gboolean reset_appid (GtkWidget *, GdkEvent *, gpointer)
{
    g_set_prgname (app_id);
    g_free (app_id);
    return FALSE;
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
    gboolean rel;
    GtkTreeIter iter;

    if (gtk_widget_is_visible (keyentry)) key = gtk_entry_get_text (GTK_ENTRY (keyentry));
    else key = gtk_label_get_text (GTK_LABEL (keylabel));

    rel = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (relchk));

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

    write_xml (key, act, name, param, rel);

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
    if (*(ptr + 1) == 0) *ptr = g_ascii_tolower (*ptr);  // labwc wants lower-case single letters

    gtk_entry_set_text (GTK_ENTRY (keyentry), buf);
}

/*----------------------------------------------------------------------------*/
/* Table and button handlers                                                  */
/*----------------------------------------------------------------------------*/

static void new_button (GtkWidget *, gpointer)
{
    show_editor (NULL, NULL, NULL, NULL, FALSE);
}

static void edit_button (GtkWidget *, gpointer)
{
    char *key, *act, *name, *param;
    gboolean rel;
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tv));
    if (selection && gtk_tree_selection_get_selected (selection, &model, &iter))
    {
        gtk_tree_model_get (model, &iter, 0, &key, 1, &act, 2, &name, 3, &param, 4, &rel, -1);
        show_editor (key, act, name, param, rel);

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

static void help_button (GtkWidget *, gpointer)
{
    system ("xdg-open https://labwc.github.io/labwc-actions.5.html &");
}

static GtkWidget *popup_menu (int x, int y)
{
    GtkWidget *menu, *item;
    GtkTreeIter iter;
    GtkTreePath *path;

    if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (tv), x, y, &path, NULL, NULL, NULL))
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
    }
    else menu = NULL;

    return menu;
}

static gboolean tv_button (GtkWidget *wid, GdkEventButton event, gpointer)
{
    GtkWidget *menu;

    if (event.type == GDK_BUTTON_PRESS && event.button == 3)
    {
        menu = popup_menu (event.x, event.y);
        gtk_menu_popup_at_pointer (GTK_MENU (menu), gtk_get_current_event ());

        return FALSE;
    }
    return FALSE;
}

static void gesture_pressed (GtkGestureLongPress *, gdouble x, gdouble y, gpointer)
{
    pressed = TRUE;
    press_x = x;
    press_y = y;
}

static void gesture_end (GtkGestureLongPress *, GdkEventSequence *, gpointer)
{
    GtkWidget *menu;
    int x, y;

    if (pressed)
    {
        gtk_tree_view_convert_widget_to_bin_window_coords (GTK_TREE_VIEW (tv), press_x, press_y, &x, &y);
        menu = popup_menu (x, y);
        GdkRectangle rect = {press_x, press_y, 0, 0};
        gtk_menu_popup_at_rect (GTK_MENU (menu), gtk_widget_get_window (tv), &rect, GDK_GRAVITY_CENTER, GDK_GRAVITY_NORTH_WEST, NULL);
    }
    pressed = FALSE;
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
    gboolean rel;

    gtk_tree_model_get (GTK_TREE_MODEL (bindings), &miter, KB_KEY, &key, KB_ACTION, &act,
        KB_NAME, &name, KB_ARG, &param, KB_REL, &rel, -1);
    show_editor (key, act, name, param, rel);

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

    gtk_tree_model_get (GTK_TREE_MODEL (bindings), &miter, KB_KEY, &key, -1);
    write_xml (key, NULL, NULL, NULL, FALSE);
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
    GtkGesture *gesture;
    char *user_file;
    int i;

    bindings = gtk_list_store_new (6, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_STRING);
    bind_sort = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (bindings));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (bind_sort), 0, GTK_SORT_ASCENDING);

    tv = (GtkWidget *) gtk_builder_get_object (builder, "shortcuts_tv");
    newbtn = (GtkWidget *) gtk_builder_get_object (builder, "new_btn");
    editbtn = (GtkWidget *) gtk_builder_get_object (builder, "edit_btn");
    delbtn = (GtkWidget *) gtk_builder_get_object (builder, "del_btn");
    hlpbtn = (GtkWidget *) gtk_builder_get_object (builder, "help_btn");
    gtk_widget_set_sensitive (editbtn, FALSE);
    gtk_widget_set_sensitive (delbtn, FALSE);

    gtk_tree_view_set_model (GTK_TREE_VIEW (tv), bind_sort);

    trend = gtk_cell_renderer_text_new ();

    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tv), -1, _("Key"), trend, "text", KB_KEY, NULL);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tv), -1, _("Function"), trend, "text", KB_LABEL, NULL);

    for (i = 0; i < 2; i++)
    {
        gtk_tree_view_column_set_resizable (gtk_tree_view_get_column (GTK_TREE_VIEW (tv), i), TRUE);
        gtk_tree_view_column_set_sizing (gtk_tree_view_get_column (GTK_TREE_VIEW (tv), i), GTK_TREE_VIEW_COLUMN_GROW_ONLY);
        gtk_tree_view_column_set_sort_column_id (gtk_tree_view_get_column (GTK_TREE_VIEW (tv), i), i == 0 ? KB_KEY : KB_LABEL);
    }

    g_signal_connect (tv, "button-press-event", G_CALLBACK (tv_button), NULL);
    g_signal_connect (tv, "cursor-changed", G_CALLBACK (tv_cursor), NULL);
    g_signal_connect (newbtn, "clicked", G_CALLBACK (new_button), NULL);
    g_signal_connect (editbtn, "clicked", G_CALLBACK (edit_button), NULL);
    g_signal_connect (delbtn, "clicked", G_CALLBACK (delete_button), NULL);
    g_signal_connect (hlpbtn, "clicked", G_CALLBACK (help_button), NULL);

    gesture = gtk_gesture_long_press_new (tv);
    gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), FALSE);
    g_signal_connect (gesture, "pressed", G_CALLBACK (gesture_pressed), NULL);
    g_signal_connect (gesture, "end", G_CALLBACK (gesture_end), NULL);
    gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture), GTK_PHASE_TARGET);
    pressed = FALSE;

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
