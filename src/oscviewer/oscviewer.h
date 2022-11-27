// Copyright (C) Scott Dwyer and OpenSuperClone contributors.
// You may use/distribute/modify this freely, under the terms of
// the GNU General Public License version 2 or later version.
// This software is distributed WITHOUT ANY WARRANTY.

#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <math.h>
#include <malloc.h>
#include <gtk/gtk.h>
#include <cairo.h>
#include <sys/stat.h>
#include <libintl.h>
#include <locale.h>

#define _(str) gettext(str)
#define gettext_noop(str) (str)
#define N_(str) gettext_noop(str)

char program_title[256];

#define BLACK 0x000000   // (0,0,0)
#define WHITE 0xFFFFFF   // (255,255,255)
#define RED 0xFF0000     // (255,0,0)
#define LIME 0x00FF00    // (0,255,0)
#define BLUE 0x0000FF    // (0,0,255)
#define YELLOW 0xFFFF00  // (255,255,0)
#define CYAN 0x00FFFF    // (0,255,255)
#define MAGENTA 0xFF00FF // (255,0,255)
#define SILVER 0xC0C0C0  // (192,192,192)
#define GRAY 0x808080    // (128,128,128)
#define LIGHTGRAY 0xD3D3D3
#define MAROON 0x800000   // (128,0,0)
#define OLIVE 0x808000    // (128,128,0)
#define GREEN 0x008000    // (0,128,0)
#define PURPLE 0x800080   // (128,0,128)
#define TEAL 0x008080     // (0,128,128)
#define NAVY 0x000080     // (0,0,128)
#define ORANGE 0xFFA500   // (255,165,0)
#define MYBLUE 0x0080FF   // (0,128,255)
#define MYORANGE 0xFF8000 // (255,128,0)

#define NONTRIED 0
#define NONTRIMMED 0x10
#define NONDIVIDED 0x20
#define NONSCRAPED 0x30
#define BAD 0x40
#define BAD_HEAD 0x80
#define FINISHED 0x7f
#define STATUS_MASK 0xff
#define INFO_MASK 0xffffff00
#define BAD_HEAD_MASK 0xffffffffffffff7f

#define NONTRIED_BIT 0x0001
#define NONTRIMMED_BIT 0x0002
#define NONDIVIDED_BIT 0x0004
#define NONSCRAPED_BIT 0x0008
#define BAD_BIT 0x0010
#define BAD_HEAD_BIT 0x0020
#define FINISHED_BIT 0x0040
#define UNKNOWN_BIT 0x0080

#define MAINWINDOWWIDTH 800
#define MAINWINDOWHEIGTH 600
#define LEFTSQUARESIZE 2
#define MAINSQUARESIZE 12
#define MAINGRIDSIZE 4096

#define DISPLAY_MESSAGE_SIZE 4096
#define EXIT_MESSAGE_SIZE 2048
#define TEMP_MESSAGE_SIZE 2048
#define DEBUG_MESSAGE_SIZE 4096
#define ERROR_MESSAGE_SIZE 1024
#define MAX_LINE_LENGTH 256

gint main_window_width = MAINWINDOWWIDTH;
gint main_window_height = MAINWINDOWHEIGTH;
gint main_vbox_width;
gint main_vbox_height;
gint main_hbox_width;
gint main_hbox_height;
gint left_vbox_width;
gint left_vbox_height;
gint right_vbox_width;
gint right_vbox_height;
gint top_hbox_width;
gint top_hbox_height;
gint top_info_width;
gint top_info_height;
gint top_drawing_area_width;
gint top_drawing_area_height;
gint main_drawing_area_width;
gint main_drawing_area_height;
gint left_drawing_area_width;
gint left_drawing_area_height;
gint main_scrolled_window_width;
gint main_scrolled_window_height;
gint main_drawing_vbox_width;
gint main_drawing_vbox_height;

GtkWidget *language_window;
GtkWidget *main_window;
GtkWidget *main_vbox;
GtkWidget *main_hbox;
GtkWidget *left_vbox;
GtkWidget *right_vbox;
GtkWidget *top_hbox;
GtkWidget *top_info_box;
GtkWidget *top_drawing_area;
GtkWidget *main_drawing_area;
GtkWidget *left_drawing_area;
GtkWidget *main_scrolled_window;
GtkWidget *main_drawing_vbox;
GtkWidget *frame;

GtkWidget *progress_log_label;
GtkWidget *domain_log_label;

GtkWidget *auto_update_label;

GtkWidget *block_information_label;

// menu items
GtkWidget *menubar;
GtkWidget *menuvbox;
GtkWidget *menuseparater;
// help menu
GtkWidget *helpmi;
GtkWidget *aboutmi;
// file menu
GtkWidget *filemenu;
GtkWidget *filemi;
GtkWidget *quitmi;
GtkWidget *openmi;
GtkWidget *domainmi;
GtkWidget *dmdedomainmi;
GtkWidget *filew;
GtkWidget *domainw;
// options menu
GtkWidget *optionsmenu;
GtkWidget *optionsmi;
GtkWidget *leftresmenu;
GtkWidget *leftresolutionmi;
GtkWidget *leftresbutton1;
GtkWidget *leftresbutton2;
GtkWidget *leftresbutton3;
GtkWidget *leftresbutton4;
GtkWidget *mainresmenu;
GtkWidget *mainresolutionmi;
GtkWidget *mainresbutton4;
GtkWidget *mainresbutton6;
GtkWidget *mainresbutton8;
GtkWidget *mainresbutton10;
GtkWidget *mainresbutton12;
GtkWidget *mainresbutton14;
GtkWidget *mainresbutton16;
GtkWidget *maingridsizemi;
GtkWidget *mainsizemenu;
GtkWidget *mainsizebutton4k;
GtkWidget *mainsizebutton8k;
GtkWidget *mainsizebutton16k;
GtkWidget *mainsizebutton32k;
GtkWidget *mainsizebutton64k;
GtkWidget *mainsizebutton128k;
GtkWidget *mainsizebutton256k;
GtkWidget *mainsizebutton512k;
GtkWidget *mainsizebutton1m;
GtkWidget *mainsizebutton2m;
GtkWidget *mainsizebutton4m;
GtkWidget *mainsizebutton8m;
GtkWidget *mainsizebutton16m;
GtkWidget *autoupdatemi;
GtkWidget *autoupdatemenu;
GtkWidget *autoupdatebuttonoff;
GtkWidget *autoupdatebutton5s;
GtkWidget *autoupdatebutton10s;
GtkWidget *autoupdatebutton30s;
GtkWidget *autoupdatebutton1m;
GtkWidget *autoupdatebutton2m;
GtkWidget *autoupdatebutton5m;
GtkWidget *showbadcheck;
GtkWidget *showdomaincheck;
GtkWidget *optionsw;
GSList *leftresgroup = NULL;
GSList *mainresgroup = NULL;
GSList *mainsizegroup = NULL;
GSList *autoupgroup = NULL;
GtkWidget *showgoodcheck;
GtkWidget *showtimemi;
GtkWidget *showtimingbuttonoff;
GtkWidget *showtimingbutton1;
GtkWidget *showtimingbutton2;
GtkWidget *showtimingbutton3;
GtkWidget *showtimingbutton4;
GtkWidget *showtimingbutton5;
GtkWidget *showtimingbutton7;
GtkWidget *showtimingbutton10;
GtkWidget *showtimingbutton15;
GtkWidget *showtimingbutton20;
GtkWidget *showtimingbutton25;
GtkWidget *showtimingbutton30;
GtkWidget *showtimingbutton40;
GtkWidget *showtimingbutton50;
GtkWidget *showtimingbutton60;

int debug = 0;
int translate = 0;
char exit_message[EXIT_MESSAGE_SIZE];
char display_message[DISPLAY_MESSAGE_SIZE];
char tempmessage[TEMP_MESSAGE_SIZE];
char debug_message[DEBUG_MESSAGE_SIZE];
char error_message[ERROR_MESSAGE_SIZE];
char log_file[1024];
char domain_file[1024];
int log_rows = 1000;
int domain_rows = 1000;
int total_lines = 0;
int domain_lines = 0;
int regular_domain = 0;
long long *lposition;
long long *lsize;
long long *lstatus;
long long *temp_lposition;
long long *temp_lsize;
long long *temp_lstatus;
long long *dposition;
long long *dsize;
long long *dstatus;
long long *temp_dposition;
long long *temp_dsize;
long long *temp_dstatus;
long long current_position = 0;
long long current_status = 0;
long long total_size = 0;
int sector_size = 512;
long long temp_nontried = 0;
long long temp_nontrimmed = 0;
long long temp_nondivided = 0;
long long temp_nonscraped = 0;
long long temp_bad = 0;
long long temp_finished = 0;
long long temp_hightime = 0;
int ddrescue_log = 0;
char ddrstatus = ' ';
double rcolor = 0;
double gcolor = 0;
double bcolor = 0;
int left_square_size = LEFTSQUARESIZE;
int main_square_size = MAINSQUARESIZE;
int square_adjust = 0;
int nontried_color = LIGHTGRAY;
int nontrimmed_color = YELLOW;
int nondivided_color = NAVY;
int nonscraped_color = BLUE;
int bad_color = RED;
int good_color = LIME;
int unknown_color = BLACK;
int current_color_outer = ORANGE;
int current_color_inner = ORANGE;
int bad_head_color = PURPLE;
int selected_color = WHITE;
int time_color = MAGENTA;
int domain_color = MYBLUE;
int main_grid_size = MAINGRIDSIZE;
gint timeout_tag = 0;
int autotimer_on = 0;
int show_bad_head = 0;
int show_good_data = 0;
int show_timing = 0;
int show_domain = 0;
int mouse_x = 0;
int mouse_y = 0;
int mouse_x_old = 0;
int mouse_y_old = 0;

void about(void);

void help(void);

void version(void);

static gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, GdkWindowEdge edge);

static gboolean main_drawing_expose_event(GtkWidget *self, cairo_t *cr, gpointer user_data);

static gboolean left_vbox_expose_event(GtkWidget *self, cairo_t *cr, gpointer user_data);

static gboolean top_drawing_expose_event(GtkWidget *self, cairo_t *cr, gpointer user_data);

void getsize_main_window(GtkWidget *widget, GtkAllocation *allocation, void *data);

void getsize_main_vbox(GtkWidget *widget, GtkAllocation *allocation, void *data);

void getsize_main_hbox(GtkWidget *widget, GtkAllocation *allocation, void *data);

void getsize_left_vbox(GtkWidget *widget, GtkAllocation *allocation, void *data);

void getsize_right_vbox(GtkWidget *widget, GtkAllocation *allocation, void *data);

void getsize_top_hbox(GtkWidget *widget, GtkAllocation *allocation, void *data);

void getsize_top_info_box(GtkWidget *widget, GtkAllocation *allocation, void *data);

void getsize_top_drawing_area(GtkWidget *widget, GtkAllocation *allocation, void *data);

void getsize_main_drawing_area(GtkWidget *widget, GtkAllocation *allocation, void *data);

void getsize_left_drawing_area(GtkWidget *widget, GtkAllocation *allocation, void *data);

void getsize_main_drawing_vbox(GtkWidget *widget, GtkAllocation *allocation, void *data);

void getsize_main_scrolled_window(GtkWidget *widget, GtkAllocation *allocation, void *data);

void change_main_resolution(GtkWidget *w, gpointer data);

void change_main_grid_size(GtkWidget *w, gpointer data);

void change_left_resolution(GtkWidget *w, gpointer data);

int initialize_memory(void);

int increase_log_memory(int new_lines);

int increase_domain_memory(int new_lines);

int message_exit(char *message);

int message_display(char *message);

int message_now(char *message);

int message_debug(char *message, int level);

int message_error(char *message);

int clear_error_message(void);

int read_log_file(char *log_file);

int read_domain_file(char *domain_file);

int read_domain_dmde_file(char *dmde_file);

int add_to_domain(long long position, long long size);

int insert_domain_line(int line, long long position, long long size, long long status);

int find_domain_block(long long position);

int delete_domain_line(int line);

gint reload_file(void);

void set_autoupdate_timer(GtkWidget *w, gpointer data);

void toggle_showbad(GtkWidget *w, gpointer data);

void toggle_showgood(GtkWidget *w, gpointer data);

void set_show_timing(GtkWidget *w, gpointer data);

void toggle_showdomain(GtkWidget *w, gpointer data);

int check_log(void);

void select_file(void);

void select_domain(void);

void select_dmde_domain(void);

void get_rgb_color(int color);

int get_color_from_status(int status_bits);

int get_block_status(long long position, long long size);

int get_block_timing(long long position, long long size);

int get_block_information(long long position, long long size);

int process_domain(long long position, int size, int status, int status_mask);

int find_block(long long position);

int process_status(int line);

long long process_information(long long position, long long size, int line);

int print_gui_error_message(char *message, char *title, int type);

struct MemoryStruct
{
    char *memory;
    size_t size;
};
