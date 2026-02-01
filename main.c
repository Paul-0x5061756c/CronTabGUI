#include <gtk/gtk.h>
#include <glib.h>
#include <stdio.h>
#include <sys/wait.h>

typedef struct _CronLine {
  GObject parent_instance;

  char     *line;
  gboolean  editing;
} CronLine;

typedef struct _CronLineClass {
  GObjectClass parent_class;
} CronLineClass;

G_DEFINE_TYPE(CronLine, cron_line, G_TYPE_OBJECT);

static const char *WEEKDAYS[7] = {
    "Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"
};

static const char *MONTHS[12] = {
    "January","February","March","April","May","June",
    "July","August","September","October","November","December"
};

/* 
 * Converts a cron timing (first 5 fields) into human-readable text.
 */
char *cron_timing_to_text(const char *line) {
    char min[16], hour[16], dom[16], mon[16], dow[16];
    static char result[256];

    if (sscanf(line, "%15s %15s %15s %15s %15s",
               min, hour, dom, mon, dow) != 5) {
        snprintf(result, sizeof(result), "Invalid cron timing");
        return result;
    }

    char buf[128] = "";

    // minute
    if (strcmp(min, "*") == 0) {
        strcat(buf, "Every minute");
    } else {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "At minute %s", min);
        strcat(buf, tmp);
    }

    // hour
    if (strcmp(hour, "*") == 0) {
        if (strcmp(min, "*") == 0)
            strcat(buf, " of every hour");
        else
            strcat(buf, " of every hour");
    } else {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), " at %2s:%s", hour, min);
        strcpy(buf, tmp);
    }

    // day of month
    if (strcmp(dom, "*") != 0) {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "%s on day %s of the month", buf, dom);
        strcpy(buf, tmp);
    }

    // month
    if (strcmp(mon, "*") != 0) {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "%s in month %s", buf, mon);
        strcpy(buf, tmp);
    }

    // day of week
    if (strcmp(dow, "*") != 0) {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "%s on day(s) %s of the week", buf, dow);
        strcpy(buf, tmp);
    }

    snprintf(result, sizeof(result), "%s", buf);
    return result;
}


static void cron_line_finalize(GObject *object) {
  CronLine *self = (CronLine *)object;
  g_free(self->line);
  G_OBJECT_CLASS(cron_line_parent_class)->finalize(object);
}

static void cron_line_class_init(CronLineClass *klass) {
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->finalize = cron_line_finalize;
}

static void cron_line_init(CronLine *self) {
  self->editing = FALSE;
}


typedef struct {
  CronLine   *line;
  GListStore *store;
} EditContext;

static gboolean save_crontab(GListStore *store) {
  FILE *crontab = popen("/usr/bin/crontab -", "w");
  if (!crontab) {
    g_warning("Failed to run crontab -");
    return FALSE;
  }

  guint n = g_list_model_get_n_items(G_LIST_MODEL(store));
  for (guint i = 0; i < n; i++) {
    CronLine *line =
      g_list_model_get_item(G_LIST_MODEL(store), i);
    fprintf(crontab, "%s\n", line->line ? line->line : "");
    g_object_unref(line);
  }

  int status = pclose(crontab);
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static void load_crontab(GListStore *store) {
  char buf[1024];
  char comment_buf[1024];
  FILE *crontab = popen("/usr/bin/crontab -l 2>/dev/null", "r");
  if (!crontab) {
    g_warning("Failed to run crontab -l");
    return;
  }

  while (fgets(buf, sizeof(buf), crontab)) {
    buf[strcspn(buf, "\n")] = 0;

    if(buf[0] == '\0' || buf[0] == '#'){
      continue;
    }

    CronLine *line =
      g_object_new(cron_line_get_type(), NULL);

    line->line = g_strdup(buf);

    g_list_store_append(store, line);
    g_object_unref(line);
  }

  pclose(crontab);
}


static void on_edit_clicked(GtkButton *button, gpointer user_data) {
  EditContext *ctx = user_data;
  CronLine *line = ctx->line;
  GListStore *store = ctx->store;

  GtkWidget *box = gtk_widget_get_parent(GTK_WIDGET(button));
  GtkWidget *entry = gtk_widget_get_first_child(box);

  if (line->editing) {
    g_free(line->line);
    line->line = g_strdup(
      gtk_editable_get_text(GTK_EDITABLE(entry)));

    char *readable = cron_timing_to_text(line->line);
    gtk_widget_set_tooltip_text(GTK_WIDGET(gtk_widget_get_parent(GTK_WIDGET(button))), readable);

    if (!save_crontab(store)) {
      g_warning("Failed to save crontab");
      return;
    }
  }

  line->editing = !line->editing;

  gtk_editable_set_editable(GTK_EDITABLE(entry), line->editing);

  gtk_button_set_icon_name(
    button,
    line->editing
      ? "document-save-symbolic"
      : "document-edit-symbolic");
}

static void on_setup(GtkListItemFactory *factory,
                     GtkListItem *item,
                     gpointer user_data) {
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

  GtkWidget *entry = gtk_entry_new();
  gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
  gtk_widget_set_hexpand(entry, TRUE);

  GtkWidget *button =
    gtk_button_new_from_icon_name("document-edit-symbolic");

  GtkWidget *delete_button =
    gtk_button_new_from_icon_name("user-trash-symbolic");

  gtk_box_append(GTK_BOX(box), entry);
  gtk_box_append(GTK_BOX(box), button);
  gtk_box_append(GTK_BOX(box), delete_button);

  gtk_list_item_set_child(item, box);

  g_object_set_data(G_OBJECT(item), "entry", entry);
  g_object_set_data(G_OBJECT(item), "button", button);
  g_object_set_data(G_OBJECT(item), "delete_button", delete_button);

  gtk_widget_set_margin_top(entry, 4);
  gtk_widget_set_margin_bottom(entry, 4);
}

static void on_bind(GtkListItemFactory *factory,
                    GtkListItem *item,
                    gpointer user_data) {
  CronLine *line = gtk_list_item_get_item(item);
  GListStore *store = user_data;

  GtkWidget *entry =
    g_object_get_data(G_OBJECT(item), "entry");
  GtkWidget *button =
    g_object_get_data(G_OBJECT(item), "button");

  gtk_editable_set_text(GTK_EDITABLE(entry), line->line ? line->line : "");
  gtk_editable_set_editable(GTK_EDITABLE(entry), line->editing);

  gtk_button_set_icon_name(
    GTK_BUTTON(button),
    line->editing
      ? "document-save-symbolic"
      : "document-edit-symbolic");

  g_signal_handlers_disconnect_by_data(button, line);

  EditContext *ctx = g_new0(EditContext, 1);
  ctx->line = line;
  ctx->store = store;

  g_signal_connect_data(
    button, "clicked",
    G_CALLBACK(on_edit_clicked),
    ctx,
    (GClosureNotify)g_free,
    0);

  char *readable = cron_timing_to_text(line->line);
  gtk_widget_set_tooltip_text(GTK_WIDGET(gtk_list_item_get_child(item)), readable);

  g_object_unref(line);
}


static void activate(GtkApplication *app, gpointer user_data) {
  GtkWidget *window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "CronTabGUI");
  gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

  GListStore *store = g_list_store_new(cron_line_get_type());
  load_crontab(store);

  GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
  g_signal_connect(factory, "setup", G_CALLBACK(on_setup), NULL);
  g_signal_connect(factory, "bind", G_CALLBACK(on_bind), store);

  GtkSelectionModel *selection =
    GTK_SELECTION_MODEL(
      gtk_no_selection_new(G_LIST_MODEL(store)));

  GtkWidget *list = gtk_list_view_new(selection, factory);

  GtkWidget *scroller = gtk_scrolled_window_new();
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), list);
  gtk_widget_set_vexpand(scroller, TRUE);
  gtk_widget_set_hexpand(scroller, TRUE);

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_box_append(GTK_BOX(box), scroller);

  gtk_window_set_child(GTK_WINDOW(window), box);
  gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
  GtkApplication *app =
    gtk_application_new(
      "dev.pauldenboer.crontabgui",
      G_APPLICATION_DEFAULT_FLAGS);

  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

  int status = g_application_run(G_APPLICATION(app), argc, argv);

  g_object_unref(app);
  return status;
}
