#include "glib.h"
#include <gtk/gtk.h>
#include <stdio.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800
#define CRONTAB_LINE_BUFFER_SIZE 1024

typedef struct {
  GtkTextView *view;
  GtkButton   *button;
  gboolean     editable;
} AppState;

static gboolean save_crontab(GtkTextView *view) {
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(view);
  GtkTextIter start, end;

  gtk_text_buffer_get_bounds(buffer, &start, &end);
  char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

  FILE *crontab = popen("crontab -", "w");
  if (!crontab) {
    g_warning("Failed to run crontab -");
    g_free(text);
    return FALSE;
  }

  fputs(text, crontab);
  int status = pclose(crontab);

  g_free(text);
  return status == 0;
}
static void toggle_editable(GtkButton *button, gpointer user_data) {
  AppState *state = user_data;

  if (state->editable) {
    if (!save_crontab(state->view)) {
      g_warning("Failed to save crontab");
      return;
    }
  }

  state->editable = !state->editable;

  gtk_text_view_set_editable(state->view, state->editable);
  gtk_button_set_label(
      button,
      state->editable ? "Stop editing" : "Start editing"
  );
}

static void load_crontab(GtkTextView *view) {
  char line[CRONTAB_LINE_BUFFER_SIZE];
  FILE *crontab = popen("crontab -l 2>/dev/null", "r");
  if (!crontab) {
    g_warning("Failed to run crontab -l");
    return;
  }

  GString *text = g_string_new(NULL);

  while (fgets(line, sizeof(line), crontab)) {
    if (line[0] != '#')
      g_string_append(text, line);
  }

  pclose(crontab);

  GtkTextBuffer *buffer = gtk_text_view_get_buffer(view);
  gtk_text_buffer_set_text(buffer, text->str, -1);
  g_string_free(text, TRUE);
}

static void activate(GtkApplication *app, gpointer user_data) {
  GtkWidget *window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "CronTabGUI");
  gtk_window_set_default_size(GTK_WINDOW(window), 800, 800);

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_window_set_child(GTK_WINDOW(window), box);

  GtkWidget *button = gtk_button_new_with_label("Start editing");
  gtk_box_append(GTK_BOX(box), button);

  GtkWidget *view = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
  gtk_widget_set_vexpand(view, TRUE);
  gtk_box_append(GTK_BOX(box), view);

  load_crontab(GTK_TEXT_VIEW(view));

  AppState *state = g_new0(AppState, 1);
  state->view = GTK_TEXT_VIEW(view);
  state->button = GTK_BUTTON(button);
  state->editable = FALSE;

  g_signal_connect(button, "clicked",
                   G_CALLBACK(toggle_editable), state);

  gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
  GtkApplication *app;
  int status;

  app = gtk_application_new("dev.pauldenboer.crontabgui",
                            G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);

  return status;
}
