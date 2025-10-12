// Copyright (C) Scott Dwyer and OpenSuperClone contributors.
// You may use/distribute/modify this freely, under the terms of
// the GNU General Public License version 2 or later version.
// This software is distributed WITHOUT ANY WARRANTY.

#include "config.h"
#include "oscviewer.h"
#include "oscviewer_glade.h"

int main(int argc, char **argv)
{
  bindtextdomain("oscviewer", OSC_LANG_PATH);
  textdomain("oscviewer");

  snprintf(window_title, sizeof(window_title), "%s %s", title, OSC_VERSION);

  // begin processing command line arguments
  int command_line_argument;
  int arguments_required;          // required number of non-option arguments
  bool command_line_error = false; // initialize error to false
  bool log_file_specified = false; // flag to check if log file is specified
  bool domain_file_specified = false; // flag to check if domain file is specified

  while (1)
  {
    static struct option long_options[] =
        {
            {"debug", no_argument, 0, 'd'},
            {"help", no_argument, 0, 'h'},
            {"version", no_argument, 0, 'v'},
            {"logfile", required_argument, 0, 'l'},
            {"domainfile", required_argument, 0, 'f'},
            {0, 0, 0, 0}};
    // getopt_long stores the option index here.
    int option_index = 0;

    command_line_argument = getopt_long(argc, argv, "dhvt:l:f:", long_options, &option_index);

    // Detect the end of the options.
    if (command_line_argument == -1)
      break;

    switch (command_line_argument)
    {
    case 'd':
      debug++;
      break;

    case 'h':
      help();
      exit(0);

    case 'v':
      version();
      exit(0);

    case 'l':
      snprintf(log_file, sizeof(log_file), "%s", optarg);
      log_file_specified = true;
      break;

    case 'f':
      snprintf(domain_file, sizeof(domain_file), "%s", optarg);
      domain_file_specified = true;
      break;

    case '?':
      // getopt_long already printed an error message.
      command_line_error = true;
      break;

    default:
      fprintf(stdout, "Unknown error processing command line options\n");
      command_line_error = true;
    }
  }

  arguments_required = 0;
  if ((argc - optind) != arguments_required)
  {
    command_line_error = true;
    if ((argc - optind) < arguments_required)
    {
      fprintf(stdout, "Error: too few arguments\n");
    }
    if ((argc - optind) > arguments_required)
    {
      fprintf(stdout, "Error: too many arguments\n");
    }
  }

  // exit on command line error
  if (command_line_error)
  {
    fprintf(stdout, "%s: Command line error.\n", title);
    fprintf(stdout, "Try '%s --help' for more information.\n", title);
    exit(1);
  }

  // end of command line processing

  // read the configuration file
  read_config_file();

  // initialize the memory for the progress log file
  if (initialize_memory())
  {
    exit(1);
  }

  gtk_init(&argc, &argv);

  GtkBuilder *builder = gtk_builder_new();
  GError *err = NULL; // It is mandatory to initialize to NULL
  if (0 == gtk_builder_add_from_string(builder, (const gchar *)oscviewer_glade, oscviewer_glade_len, &err))
  {
    fprintf(stderr, "Error adding build from file. Error: %s\n", err->message);
  }

  main_window = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));
  if (NULL == main_window)
  {
    fprintf(stderr, "Object error: main_window \n");
  }
  gtk_builder_connect_signals(builder, NULL);
  gtk_window_set_title(GTK_WINDOW(main_window), window_title);
  gtk_window_set_default_size(GTK_WINDOW(main_window), MAINWINDOWWIDTH, MAINWINDOWHEIGTH);
  gtk_window_set_position(GTK_WINDOW(main_window), GTK_WIN_POS_CENTER);

  settings_window = GTK_WIDGET(gtk_builder_get_object(builder, "settings_window"));
  gtk_window_set_keep_above(GTK_WINDOW(settings_window), TRUE);
  gtk_window_set_title(GTK_WINDOW(settings_window), _("Settings"));
  g_signal_connect(settings_window, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);

  // set it to exit if closed
  g_signal_connect(G_OBJECT(main_window), "destroy", G_CALLBACK(gtk_main_quit), NULL);

  // start of menu

  // file menu
  filemi = GTK_WIDGET(gtk_builder_get_object(builder, "filemi"));
  quitmi = GTK_WIDGET(gtk_builder_get_object(builder, "quitmi"));
  openmi = GTK_WIDGET(gtk_builder_get_object(builder, "openmi"));
  domainmi = GTK_WIDGET(gtk_builder_get_object(builder, "domainmi"));
  dmdedomainmi = GTK_WIDGET(gtk_builder_get_object(builder, "dmdedomainmi"));
  settingsmi = GTK_WIDGET(gtk_builder_get_object(builder, "settingsmi"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(filemi), _("File"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(quitmi), _("Quit"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(openmi), _("Open"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(domainmi), _("Load Domain"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(dmdedomainmi), _("Load DMDE Bytes File"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(settingsmi), _("Settings"));

  progress_log_label = GTK_WIDGET(gtk_builder_get_object(builder, "progress_log_label"));
  domain_log_label = GTK_WIDGET(gtk_builder_get_object(builder, "domain_log_label"));
  snprintf(tempmessage, TEMP_MESSAGE_SIZE, "%s:", _("Log"));
  gtk_label_set_text(GTK_LABEL(progress_log_label), tempmessage);
  snprintf(tempmessage, TEMP_MESSAGE_SIZE, "%s:", _("Domain"));
  gtk_label_set_text(GTK_LABEL(domain_log_label), tempmessage);

  auto_update_label = GTK_WIDGET(gtk_builder_get_object(builder, "auto_update_label"));
  snprintf(tempmessage, TEMP_MESSAGE_SIZE, "%s: %s", _("Auto-Update"), _("Off"));
  gtk_label_set_text(GTK_LABEL(auto_update_label), tempmessage);

  block_information_label = GTK_WIDGET(gtk_builder_get_object(builder, "block_information_label"));

  // options menu
  optionsmi = GTK_WIDGET(gtk_builder_get_object(builder, "optionsmi"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(optionsmi), _("View"));

  // left res menu
  leftresolutionmi = GTK_WIDGET(gtk_builder_get_object(builder, "leftresolutionmi"));
  leftresbutton1 = GTK_WIDGET(gtk_builder_get_object(builder, "leftresbutton1"));
  leftresbutton2 = GTK_WIDGET(gtk_builder_get_object(builder, "leftresbutton2"));
  leftresbutton3 = GTK_WIDGET(gtk_builder_get_object(builder, "leftresbutton3"));
  leftresbutton4 = GTK_WIDGET(gtk_builder_get_object(builder, "leftresbutton4"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(leftresolutionmi), _("Left Panel Resolution"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(leftresbutton1), _("1 pt"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(leftresbutton2), _("2 pt"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(leftresbutton3), _("3 pt"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(leftresbutton4), _("4 pt"));

  if (left_square_size == 1)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(leftresbutton1), TRUE);
  }
  else if (left_square_size == 2)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(leftresbutton2), TRUE);
  }
  else if (left_square_size == 3)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(leftresbutton3), TRUE);
  }
  else if (left_square_size == 4)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(leftresbutton4), TRUE);
  }

  // main res menu
  mainresolutionmi = GTK_WIDGET(gtk_builder_get_object(builder, "mainresolutionmi"));
  mainresbutton4 = GTK_WIDGET(gtk_builder_get_object(builder, "mainresbutton4"));
  mainresbutton6 = GTK_WIDGET(gtk_builder_get_object(builder, "mainresbutton6"));
  mainresbutton8 = GTK_WIDGET(gtk_builder_get_object(builder, "mainresbutton8"));
  mainresbutton10 = GTK_WIDGET(gtk_builder_get_object(builder, "mainresbutton10"));
  mainresbutton12 = GTK_WIDGET(gtk_builder_get_object(builder, "mainresbutton12"));
  mainresbutton14 = GTK_WIDGET(gtk_builder_get_object(builder, "mainresbutton14"));
  mainresbutton16 = GTK_WIDGET(gtk_builder_get_object(builder, "mainresbutton16"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(mainresolutionmi), _("Main Panel Resolution"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(mainresbutton4), _("4 pt"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(mainresbutton6), _("6 pt"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(mainresbutton8), _("8 pt"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(mainresbutton10), _("10 pt"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(mainresbutton12), _("12 pt"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(mainresbutton14), _("14 pt"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(mainresbutton16), _("16 pt"));

  if (main_square_size == 4)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainresbutton4), TRUE);
  }
  else if (main_square_size == 6)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainresbutton6), TRUE);
  }
  else if (main_square_size == 8)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainresbutton8), TRUE);
  }
  else if (main_square_size == 10)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainresbutton10), TRUE);
  }
  else if (main_square_size == 12)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainresbutton12), TRUE);
  }
  else if (main_square_size == 14)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainresbutton14), TRUE);
  }
  else if (main_square_size == 16)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainresbutton16), TRUE);
  }

  // main size menu
  maingridsizemi = GTK_WIDGET(gtk_builder_get_object(builder, "maingridsizemi"));
  mainsizebutton4k = GTK_WIDGET(gtk_builder_get_object(builder, "mainsizebutton4k"));
  mainsizebutton8k = GTK_WIDGET(gtk_builder_get_object(builder, "mainsizebutton8k"));
  mainsizebutton16k = GTK_WIDGET(gtk_builder_get_object(builder, "mainsizebutton16k"));
  mainsizebutton32k = GTK_WIDGET(gtk_builder_get_object(builder, "mainsizebutton32k"));
  mainsizebutton64k = GTK_WIDGET(gtk_builder_get_object(builder, "mainsizebutton64k"));
  mainsizebutton128k = GTK_WIDGET(gtk_builder_get_object(builder, "mainsizebutton128k"));
  mainsizebutton256k = GTK_WIDGET(gtk_builder_get_object(builder, "mainsizebutton256k"));
  mainsizebutton512k = GTK_WIDGET(gtk_builder_get_object(builder, "mainsizebutton512k"));
  mainsizebutton1m = GTK_WIDGET(gtk_builder_get_object(builder, "mainsizebutton1m"));
  mainsizebutton2m = GTK_WIDGET(gtk_builder_get_object(builder, "mainsizebutton2m"));
  mainsizebutton4m = GTK_WIDGET(gtk_builder_get_object(builder, "mainsizebutton4m"));
  mainsizebutton8m = GTK_WIDGET(gtk_builder_get_object(builder, "mainsizebutton8m"));
  mainsizebutton16m = GTK_WIDGET(gtk_builder_get_object(builder, "mainsizebutton16m"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(maingridsizemi), _("Main Grid Size"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(mainsizebutton4k), _("4K"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(mainsizebutton8k), _("8K"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(mainsizebutton16k), _("16K"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(mainsizebutton32k), _("32K"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(mainsizebutton64k), _("64K"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(mainsizebutton128k), _("128K"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(mainsizebutton256k), _("256K"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(mainsizebutton512k), _("512K"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(mainsizebutton1m), _("1M"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(mainsizebutton2m), _("2M"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(mainsizebutton4m), _("4M"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(mainsizebutton8m), _("8M"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(mainsizebutton16m), _("16M"));

  if (main_grid_size == 4096)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainsizebutton4k), TRUE);
  }
  else if (main_grid_size == 8192)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainsizebutton8k), TRUE);
  }
  else if (main_grid_size == 16384)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainsizebutton16k), TRUE);
  }
  else if (main_grid_size == 32768)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainsizebutton32k), TRUE);
  }
  else if (main_grid_size == 65536)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainsizebutton64k), TRUE);
  }
  else if (main_grid_size == 131072)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainsizebutton128k), TRUE);
  }
  else if (main_grid_size == 262144)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainsizebutton256k), TRUE);
  }
  else if (main_grid_size == 524288)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainsizebutton512k), TRUE);
  }
  else if (main_grid_size == 1048576)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainsizebutton1m), TRUE);
  }
  else if (main_grid_size == 2097152)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainsizebutton2m), TRUE);
  }
  else if (main_grid_size == 4194304)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainsizebutton4m), TRUE);
  }
  else if (main_grid_size == 8388608)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainsizebutton8m), TRUE);
  }
  else if (main_grid_size == 16777216)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainsizebutton16m), TRUE);
  }

  // auto update menu
  autoupdatemi = GTK_WIDGET(gtk_builder_get_object(builder, "autoupdatemi"));
  autoupdatebuttonoff = GTK_WIDGET(gtk_builder_get_object(builder, "autoupdatebuttonoff"));
  autoupdatebutton5s = GTK_WIDGET(gtk_builder_get_object(builder, "autoupdatebutton5s"));
  autoupdatebutton10s = GTK_WIDGET(gtk_builder_get_object(builder, "autoupdatebutton10s"));
  autoupdatebutton30s = GTK_WIDGET(gtk_builder_get_object(builder, "autoupdatebutton30s"));
  autoupdatebutton1m = GTK_WIDGET(gtk_builder_get_object(builder, "autoupdatebutton1m"));
  autoupdatebutton2m = GTK_WIDGET(gtk_builder_get_object(builder, "autoupdatebutton2m"));
  autoupdatebutton5m = GTK_WIDGET(gtk_builder_get_object(builder, "autoupdatebutton5m"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(autoupdatemi), _("Auto-Update"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(autoupdatebuttonoff), _("Off"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(autoupdatebutton5s), _("5 seconds"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(autoupdatebutton10s), _("10 seconds"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(autoupdatebutton30s), _("30 seconds"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(autoupdatebutton1m), _("1 minute"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(autoupdatebutton2m), _("2 minutes"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(autoupdatebutton5m), _("5 minutes"));

  // show good data item
  showgoodcheck = GTK_WIDGET(gtk_builder_get_object(builder, "showgoodcheck"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(showgoodcheck), _("Highlight Good Data"));

  if (show_good_data)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(showgoodcheck), TRUE);
  }

  // show bad head item
  showbadcheck = GTK_WIDGET(gtk_builder_get_object(builder, "showbadcheck"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(showbadcheck), _("Show Bad Head"));

  if (show_bad_head)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(showbadcheck), TRUE);
  }

  showtimemi = GTK_WIDGET(gtk_builder_get_object(builder, "showtimemi"));
  showtimingbuttonoff = GTK_WIDGET(gtk_builder_get_object(builder, "showtimingbuttonoff"));
  showtimingbutton1 = GTK_WIDGET(gtk_builder_get_object(builder, "showtimingbutton1"));
  showtimingbutton2 = GTK_WIDGET(gtk_builder_get_object(builder, "showtimingbutton2"));
  showtimingbutton3 = GTK_WIDGET(gtk_builder_get_object(builder, "showtimingbutton3"));
  showtimingbutton4 = GTK_WIDGET(gtk_builder_get_object(builder, "showtimingbutton4"));
  showtimingbutton5 = GTK_WIDGET(gtk_builder_get_object(builder, "showtimingbutton5"));
  showtimingbutton7 = GTK_WIDGET(gtk_builder_get_object(builder, "showtimingbutton7"));
  showtimingbutton10 = GTK_WIDGET(gtk_builder_get_object(builder, "showtimingbutton10"));
  showtimingbutton15 = GTK_WIDGET(gtk_builder_get_object(builder, "showtimingbutton15"));
  showtimingbutton20 = GTK_WIDGET(gtk_builder_get_object(builder, "showtimingbutton20"));
  showtimingbutton25 = GTK_WIDGET(gtk_builder_get_object(builder, "showtimingbutton25"));
  showtimingbutton30 = GTK_WIDGET(gtk_builder_get_object(builder, "showtimingbutton30"));
  showtimingbutton40 = GTK_WIDGET(gtk_builder_get_object(builder, "showtimingbutton40"));
  showtimingbutton50 = GTK_WIDGET(gtk_builder_get_object(builder, "showtimingbutton50"));
  showtimingbutton60 = GTK_WIDGET(gtk_builder_get_object(builder, "showtimingbutton60"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(showtimemi), _("Show High-Time"));

  showdomaincheck = GTK_WIDGET(gtk_builder_get_object(builder, "showdomaincheck"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(showdomaincheck), _("Show Domain"));

  if (show_domain)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(showdomaincheck), TRUE);
  }

  // help menu
  helpmi = GTK_WIDGET(gtk_builder_get_object(builder, "helpmi"));
  aboutmi = GTK_WIDGET(gtk_builder_get_object(builder, "aboutmi"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(helpmi), _("Help"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(aboutmi), _("About"));

  // settings window
  nontried_color_button = GTK_WIDGET(gtk_builder_get_object(builder, "nontried_color_button"));
  nontrimmed_color_button = GTK_WIDGET(gtk_builder_get_object(builder, "nontrimmed_color_button"));
  nondivided_color_button = GTK_WIDGET(gtk_builder_get_object(builder, "nondivided_color_button"));
  nonscraped_color_button = GTK_WIDGET(gtk_builder_get_object(builder, "nonscraped_color_button"));
  bad_color_button = GTK_WIDGET(gtk_builder_get_object(builder, "bad_color_button"));
  good_color_button = GTK_WIDGET(gtk_builder_get_object(builder, "good_color_button"));
  current_color_button = GTK_WIDGET(gtk_builder_get_object(builder, "current_color_button"));
  bad_head_color_button = GTK_WIDGET(gtk_builder_get_object(builder, "bad_head_color_button"));
  selected_color_button = GTK_WIDGET(gtk_builder_get_object(builder, "selected_color_button"));
  time_color_button = GTK_WIDGET(gtk_builder_get_object(builder, "time_color_button"));
  domain_color_button = GTK_WIDGET(gtk_builder_get_object(builder, "domain_color_button"));

  nontried_color_label = GTK_WIDGET(gtk_builder_get_object(builder, "nontried_color_label"));
  nontrimmed_color_label = GTK_WIDGET(gtk_builder_get_object(builder, "nontrimmed_color_label"));
  nondivided_color_label = GTK_WIDGET(gtk_builder_get_object(builder, "nondivided_color_label"));
  nonscraped_color_label = GTK_WIDGET(gtk_builder_get_object(builder, "nonscraped_color_label"));
  bad_color_label = GTK_WIDGET(gtk_builder_get_object(builder, "bad_color_label"));
  good_color_label = GTK_WIDGET(gtk_builder_get_object(builder, "good_color_label"));
  current_color_label = GTK_WIDGET(gtk_builder_get_object(builder, "current_color_label"));
  bad_head_color_label = GTK_WIDGET(gtk_builder_get_object(builder, "bad_head_color_label"));
  selected_color_label = GTK_WIDGET(gtk_builder_get_object(builder, "selected_color_label"));
  time_color_label = GTK_WIDGET(gtk_builder_get_object(builder, "time_color_label"));
  domain_color_label = GTK_WIDGET(gtk_builder_get_object(builder, "domain_color_label"));

  block_color_label = GTK_WIDGET(gtk_builder_get_object(builder, "block_color_label"));
  marker_color_label = GTK_WIDGET(gtk_builder_get_object(builder, "marker_color_label"));

  g_signal_connect(good_color_button, "color-set", G_CALLBACK(set_color), GINT_TO_POINTER(1));
  g_signal_connect(bad_color_button, "color-set", G_CALLBACK(set_color), GINT_TO_POINTER(2));
  g_signal_connect(nontried_color_button, "color-set", G_CALLBACK(set_color), GINT_TO_POINTER(3));
  g_signal_connect(nontrimmed_color_button, "color-set", G_CALLBACK(set_color), GINT_TO_POINTER(4));
  g_signal_connect(nondivided_color_button, "color-set", G_CALLBACK(set_color), GINT_TO_POINTER(5));
  g_signal_connect(nonscraped_color_button, "color-set", G_CALLBACK(set_color), GINT_TO_POINTER(6));
  g_signal_connect(domain_color_button, "color-set", G_CALLBACK(set_color), GINT_TO_POINTER(7));
  g_signal_connect(time_color_button, "color-set", G_CALLBACK(set_color), GINT_TO_POINTER(8));
  g_signal_connect(bad_head_color_button, "color-set", G_CALLBACK(set_color), GINT_TO_POINTER(9));
  g_signal_connect(current_color_button, "color-set", G_CALLBACK(set_color), GINT_TO_POINTER(10));
  g_signal_connect(selected_color_button, "color-set", G_CALLBACK(set_color), GINT_TO_POINTER(11));

  gtk_label_set_text(GTK_LABEL(nontried_color_label), _("Non-Tried"));
  gtk_label_set_text(GTK_LABEL(nontrimmed_color_label), _("Non-Trimmed"));
  gtk_label_set_text(GTK_LABEL(nondivided_color_label), _("Non-Divided"));
  gtk_label_set_text(GTK_LABEL(nonscraped_color_label), _("Non-Scraped"));
  gtk_label_set_text(GTK_LABEL(bad_color_label), _("Bad"));
  gtk_label_set_text(GTK_LABEL(good_color_label), _("Finished"));
  gtk_label_set_text(GTK_LABEL(current_color_label), _("Current"));
  gtk_label_set_text(GTK_LABEL(bad_head_color_label), _("Bad Head"));
  gtk_label_set_text(GTK_LABEL(selected_color_label), _("Selected"));
  gtk_label_set_text(GTK_LABEL(time_color_label), _("Timing"));
  gtk_label_set_text(GTK_LABEL(domain_color_label), _("Domain"));

  gtk_label_set_text(GTK_LABEL(block_color_label), _("Block Colors"));
  gtk_label_set_text(GTK_LABEL(marker_color_label), _("Marker Colors"));

  // add keyboard shortcuts
  GtkAccelGroup *accel_group = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(main_window), accel_group);
  gtk_widget_add_accelerator(quitmi, "activate", accel_group, GDK_KEY_q, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
  gtk_widget_add_accelerator(openmi, "activate", accel_group, GDK_KEY_o, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
  gtk_widget_add_accelerator(domainmi, "activate", accel_group, GDK_KEY_d, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
  gtk_widget_add_accelerator(dmdedomainmi, "activate", accel_group, GDK_KEY_m, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
  gtk_widget_add_accelerator(showgoodcheck, "activate", accel_group, GDK_KEY_g, GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
  gtk_widget_add_accelerator(showbadcheck, "activate", accel_group, GDK_KEY_b, GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
  gtk_widget_add_accelerator(showdomaincheck, "activate", accel_group, GDK_KEY_d, GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);

  // set it to exit if the quit item is selected
  g_signal_connect(G_OBJECT(quitmi), "activate", G_CALLBACK(gtk_main_quit), NULL);

  // set it to open a file if the open item is selected
  g_signal_connect(G_OBJECT(openmi), "activate", G_CALLBACK(select_file), NULL);

  // set it to open a file if the domain item is selected
  g_signal_connect(G_OBJECT(domainmi), "activate", G_CALLBACK(select_domain), NULL);

  // set it to open a file if the dmde domain item is selected
  g_signal_connect(G_OBJECT(dmdedomainmi), "activate", G_CALLBACK(select_dmde_domain), NULL);

  // set it to open the settings window if the settings item is selected
  g_signal_connect(G_OBJECT(settingsmi), "activate", G_CALLBACK(show_settings_window), NULL);

  g_signal_connect(G_OBJECT(leftresbutton1), "activate", G_CALLBACK(change_left_resolution), GINT_TO_POINTER(1));
  g_signal_connect(G_OBJECT(leftresbutton2), "activate", G_CALLBACK(change_left_resolution), GINT_TO_POINTER(2));
  g_signal_connect(G_OBJECT(leftresbutton3), "activate", G_CALLBACK(change_left_resolution), GINT_TO_POINTER(3));
  g_signal_connect(G_OBJECT(leftresbutton4), "activate", G_CALLBACK(change_left_resolution), GINT_TO_POINTER(4));

  g_signal_connect(G_OBJECT(mainresbutton4), "activate", G_CALLBACK(change_main_resolution), GINT_TO_POINTER(4));
  g_signal_connect(G_OBJECT(mainresbutton6), "activate", G_CALLBACK(change_main_resolution), GINT_TO_POINTER(6));
  g_signal_connect(G_OBJECT(mainresbutton8), "activate", G_CALLBACK(change_main_resolution), GINT_TO_POINTER(8));
  g_signal_connect(G_OBJECT(mainresbutton10), "activate", G_CALLBACK(change_main_resolution), GINT_TO_POINTER(10));
  g_signal_connect(G_OBJECT(mainresbutton12), "activate", G_CALLBACK(change_main_resolution), GINT_TO_POINTER(12));
  g_signal_connect(G_OBJECT(mainresbutton14), "activate", G_CALLBACK(change_main_resolution), GINT_TO_POINTER(14));
  g_signal_connect(G_OBJECT(mainresbutton16), "activate", G_CALLBACK(change_main_resolution), GINT_TO_POINTER(16));

  g_signal_connect(G_OBJECT(mainsizebutton4k), "activate", G_CALLBACK(change_main_grid_size), GINT_TO_POINTER(4096));
  g_signal_connect(G_OBJECT(mainsizebutton8k), "activate", G_CALLBACK(change_main_grid_size), GINT_TO_POINTER(8192));
  g_signal_connect(G_OBJECT(mainsizebutton16k), "activate", G_CALLBACK(change_main_grid_size), GINT_TO_POINTER(16384));
  g_signal_connect(G_OBJECT(mainsizebutton32k), "activate", G_CALLBACK(change_main_grid_size), GINT_TO_POINTER(32768));
  g_signal_connect(G_OBJECT(mainsizebutton64k), "activate", G_CALLBACK(change_main_grid_size), GINT_TO_POINTER(65536));
  g_signal_connect(G_OBJECT(mainsizebutton128k), "activate", G_CALLBACK(change_main_grid_size), GINT_TO_POINTER(131072));
  g_signal_connect(G_OBJECT(mainsizebutton256k), "activate", G_CALLBACK(change_main_grid_size), GINT_TO_POINTER(262144));
  g_signal_connect(G_OBJECT(mainsizebutton512k), "activate", G_CALLBACK(change_main_grid_size), GINT_TO_POINTER(524288));
  g_signal_connect(G_OBJECT(mainsizebutton1m), "activate", G_CALLBACK(change_main_grid_size), GINT_TO_POINTER(1048576));
  g_signal_connect(G_OBJECT(mainsizebutton2m), "activate", G_CALLBACK(change_main_grid_size), GINT_TO_POINTER(2097152));
  g_signal_connect(G_OBJECT(mainsizebutton4m), "activate", G_CALLBACK(change_main_grid_size), GINT_TO_POINTER(4194304));
  g_signal_connect(G_OBJECT(mainsizebutton8m), "activate", G_CALLBACK(change_main_grid_size), GINT_TO_POINTER(8388608));
  g_signal_connect(G_OBJECT(mainsizebutton16m), "activate", G_CALLBACK(change_main_grid_size), GINT_TO_POINTER(16777216));

  g_signal_connect(G_OBJECT(autoupdatebuttonoff), "activate", G_CALLBACK(set_autoupdate_timer), GINT_TO_POINTER(0));
  g_signal_connect(G_OBJECT(autoupdatebutton5s), "activate", G_CALLBACK(set_autoupdate_timer), GINT_TO_POINTER(5000));
  g_signal_connect(G_OBJECT(autoupdatebutton10s), "activate", G_CALLBACK(set_autoupdate_timer), GINT_TO_POINTER(10000));
  g_signal_connect(G_OBJECT(autoupdatebutton30s), "activate", G_CALLBACK(set_autoupdate_timer), GINT_TO_POINTER(30000));
  g_signal_connect(G_OBJECT(autoupdatebutton1m), "activate", G_CALLBACK(set_autoupdate_timer), GINT_TO_POINTER(60000));
  g_signal_connect(G_OBJECT(autoupdatebutton2m), "activate", G_CALLBACK(set_autoupdate_timer), GINT_TO_POINTER(120000));
  g_signal_connect(G_OBJECT(autoupdatebutton5m), "activate", G_CALLBACK(set_autoupdate_timer), GINT_TO_POINTER(300000));

  g_signal_connect(G_OBJECT(showgoodcheck), "activate", G_CALLBACK(toggle_showgood), NULL);

  g_signal_connect(G_OBJECT(showbadcheck), "activate", G_CALLBACK(toggle_showbad), NULL);

  g_signal_connect(G_OBJECT(showdomaincheck), "activate", G_CALLBACK(toggle_showdomain), NULL);

  g_signal_connect(G_OBJECT(showtimingbuttonoff), "activate", G_CALLBACK(set_show_timing), GINT_TO_POINTER(0));
  g_signal_connect(G_OBJECT(showtimingbutton1), "activate", G_CALLBACK(set_show_timing), GINT_TO_POINTER(1));
  g_signal_connect(G_OBJECT(showtimingbutton2), "activate", G_CALLBACK(set_show_timing), GINT_TO_POINTER(2));
  g_signal_connect(G_OBJECT(showtimingbutton3), "activate", G_CALLBACK(set_show_timing), GINT_TO_POINTER(3));
  g_signal_connect(G_OBJECT(showtimingbutton4), "activate", G_CALLBACK(set_show_timing), GINT_TO_POINTER(4));
  g_signal_connect(G_OBJECT(showtimingbutton5), "activate", G_CALLBACK(set_show_timing), GINT_TO_POINTER(5));
  g_signal_connect(G_OBJECT(showtimingbutton7), "activate", G_CALLBACK(set_show_timing), GINT_TO_POINTER(7));
  g_signal_connect(G_OBJECT(showtimingbutton10), "activate", G_CALLBACK(set_show_timing), GINT_TO_POINTER(10));
  g_signal_connect(G_OBJECT(showtimingbutton15), "activate", G_CALLBACK(set_show_timing), GINT_TO_POINTER(15));
  g_signal_connect(G_OBJECT(showtimingbutton20), "activate", G_CALLBACK(set_show_timing), GINT_TO_POINTER(20));
  g_signal_connect(G_OBJECT(showtimingbutton25), "activate", G_CALLBACK(set_show_timing), GINT_TO_POINTER(25));
  g_signal_connect(G_OBJECT(showtimingbutton30), "activate", G_CALLBACK(set_show_timing), GINT_TO_POINTER(30));
  g_signal_connect(G_OBJECT(showtimingbutton40), "activate", G_CALLBACK(set_show_timing), GINT_TO_POINTER(40));
  g_signal_connect(G_OBJECT(showtimingbutton50), "activate", G_CALLBACK(set_show_timing), GINT_TO_POINTER(50));
  g_signal_connect(G_OBJECT(showtimingbutton60), "activate", G_CALLBACK(set_show_timing), GINT_TO_POINTER(60));

  g_signal_connect(G_OBJECT(aboutmi), "activate", G_CALLBACK(about), NULL);

  // left drawing area
  left_drawing_area = GTK_WIDGET(gtk_builder_get_object(builder, "left_drawing_area"));
  g_signal_connect(left_drawing_area, "size-allocate", G_CALLBACK(getsize_left_drawing_area), NULL);
  g_signal_connect(left_drawing_area, "draw", G_CALLBACK(left_drawing_expose_event), NULL);

  // top drawing area
  top_drawing_area = GTK_WIDGET(gtk_builder_get_object(builder, "top_drawing_area"));
  g_signal_connect(top_drawing_area, "size-allocate", G_CALLBACK(getsize_top_drawing_area), NULL);
  g_signal_connect(top_drawing_area, "draw", G_CALLBACK(top_drawing_expose_event), NULL);
  gtk_widget_set_double_buffered(top_drawing_area, FALSE);

  // main drawing vbox
  main_drawing_vbox = GTK_WIDGET(gtk_builder_get_object(builder, "main_drawing_vbox"));
  g_signal_connect(main_drawing_vbox, "size-allocate", G_CALLBACK(getsize_main_drawing_vbox), NULL);

  // main drawing scrolled window
  main_scrolled_window = GTK_WIDGET(gtk_builder_get_object(builder, "main_scrolled_window"));
  g_signal_connect(main_scrolled_window, "size-allocate", G_CALLBACK(getsize_main_scrolled_window), NULL);

  // main drawing area
  main_drawing_area = GTK_WIDGET(gtk_builder_get_object(builder, "main_drawing_area"));
  g_signal_connect(main_drawing_area, "size-allocate", G_CALLBACK(getsize_main_drawing_area), NULL);
  g_signal_connect(main_drawing_area, "draw", G_CALLBACK(main_drawing_expose_event), NULL);
  gtk_widget_add_events(main_drawing_area, GDK_BUTTON_PRESS_MASK);
  g_signal_connect(G_OBJECT(main_drawing_area), "button-press-event", G_CALLBACK(on_button_press), NULL);
  gtk_widget_set_double_buffered(main_drawing_area, FALSE);

  // if log file is specified, read it
  if (log_file_specified)
  {
    int ret = read_log_file(log_file);
    if (ret != 0)
    {
      snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing log file\n");
      message_now(tempmessage);
    }
    else
    {
      ret = check_log();
      if (ret != 0)
      {
        snprintf(tempmessage, TEMP_MESSAGE_SIZE, "there were errors found in the log file\n");
        message_now(tempmessage);
      }
      else
      {
        char *filename = strrchr(log_file, '/') + 1;
        snprintf(tempmessage, TEMP_MESSAGE_SIZE, "%s [%s]", window_title, filename);
        gtk_window_set_title(GTK_WINDOW(main_window), tempmessage);

        snprintf(tempmessage, TEMP_MESSAGE_SIZE, "%s: %s", _("File"), log_file);
        gtk_label_set_text(GTK_LABEL(progress_log_label), tempmessage);
      }
    }
  }

  // if domain file is specified, read it
  if (domain_file_specified)
  {
    int ret = read_domain_file(domain_file);
    if (ret != 0)
    {
      snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing domain file\n");
      message_now(tempmessage);
    }
    else
    {
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(showdomaincheck), TRUE);
      snprintf(tempmessage, TEMP_MESSAGE_SIZE, "%s: %s", _("Domain"), domain_file);
      gtk_label_set_text(GTK_LABEL(domain_log_label), tempmessage);
    }
  }

  g_object_unref(builder);

  gtk_widget_show_all(main_window);

  gtk_main();

  return 0;
}

static gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, GdkWindowEdge edge)
{
  if (event->type == GDK_BUTTON_PRESS)
  {
    if (event->button == 1)
    {
      mouse_x = event->x;
      mouse_y = event->y;
      snprintf(tempmessage, TEMP_MESSAGE_SIZE, "x=%d y=%d\n", mouse_x, mouse_y);
      message_debug(tempmessage, 0);
      gtk_widget_queue_draw(main_window);
    }
  }
  return TRUE;
}

static gboolean top_drawing_expose_event(GtkWidget *self, cairo_t *cr, gpointer user_data)
{
  double x, y, w, l, r, g, b;

  get_rgb_color(WHITE);
  r = rcolor;
  g = gcolor;
  b = bcolor;
  x = 0;
  y = 0;
  w = top_drawing_area_width;
  l = top_drawing_area_height;
  cairo_set_source_rgb(cr, r, g, b);
  cairo_rectangle(cr, x, y, w, l);
  cairo_fill(cr);

  cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, 10);

  get_rgb_color(nontrimmed_color);
  r = rcolor;
  g = gcolor;
  b = bcolor;
  x = 5;
  y = 5;
  w = 10;
  l = 10;
  cairo_set_source_rgb(cr, r, g, b);
  cairo_rectangle(cr, x, y, w, l);
  cairo_fill(cr);
  cairo_set_source_rgb(cr, 0, 0, 0);
  cairo_move_to(cr, x + 15, y + 9);
  cairo_show_text(cr, _("Non-Trimmed"));

  get_rgb_color(nondivided_color);
  r = rcolor;
  g = gcolor;
  b = bcolor;
  x = 5;
  y = 20;
  w = 10;
  l = 10;
  cairo_set_source_rgb(cr, r, g, b);
  cairo_rectangle(cr, x, y, w, l);
  cairo_fill(cr);
  cairo_set_source_rgb(cr, 0, 0, 0);
  cairo_move_to(cr, x + 15, y + 9);
  cairo_show_text(cr, _("Non-Divided"));

  get_rgb_color(nonscraped_color);
  r = rcolor;
  g = gcolor;
  b = bcolor;
  x = 5;
  y = 35;
  w = 10;
  l = 10;
  cairo_set_source_rgb(cr, r, g, b);
  cairo_rectangle(cr, x, y, w, l);
  cairo_fill(cr);
  cairo_set_source_rgb(cr, 0, 0, 0);
  cairo_move_to(cr, x + 15, y + 9);
  cairo_show_text(cr, _("Non-Scraped"));

  get_rgb_color(bad_color);
  r = rcolor;
  g = gcolor;
  b = bcolor;
  x = 5;
  y = 50;
  w = 10;
  l = 10;
  cairo_set_source_rgb(cr, r, g, b);
  cairo_rectangle(cr, x, y, w, l);
  cairo_fill(cr);
  cairo_set_source_rgb(cr, 0, 0, 0);
  cairo_move_to(cr, x + 15, y + 9);
  cairo_show_text(cr, _("Bad"));

  get_rgb_color(nontried_color);
  r = rcolor;
  g = gcolor;
  b = bcolor;
  x = 5;
  y = 65;
  w = 10;
  l = 10;
  cairo_set_source_rgb(cr, r, g, b);
  cairo_rectangle(cr, x, y, w, l);
  cairo_fill(cr);
  cairo_set_source_rgb(cr, 0, 0, 0);
  cairo_move_to(cr, x + 15, y + 9);
  cairo_show_text(cr, _("Non-Tried"));

  get_rgb_color(good_color);
  r = rcolor;
  g = gcolor;
  b = bcolor;
  x = 5;
  y = 80;
  w = 10;
  l = 10;
  cairo_set_source_rgb(cr, r, g, b);
  cairo_rectangle(cr, x, y, w, l);
  cairo_fill(cr);
  cairo_set_source_rgb(cr, 0, 0, 0);
  cairo_move_to(cr, x + 15, y + 9);
  cairo_show_text(cr, _("Finished"));

  get_rgb_color(current_color);
  r = rcolor;
  g = gcolor;
  b = bcolor;
  x = 200;
  y = 5;
  w = 10;
  l = 10;
  cairo_set_source_rgb(cr, r, g, b);
  cairo_rectangle(cr, x, y, w, l);
  cairo_stroke(cr);
  cairo_set_source_rgb(cr, 0, 0, 0);
  cairo_move_to(cr, x + 15, y + 9);
  cairo_show_text(cr, _("Current"));

  if (show_good_data)
  {
    get_rgb_color(good_color);
    r = rcolor;
    g = gcolor;
    b = bcolor;
    x = 200;
    y = 20;
    w = 10;
    l = 10;
    cairo_set_source_rgb(cr, r, g, b);
    cairo_rectangle(cr, x, y, w, l);
    cairo_stroke(cr);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_move_to(cr, x + 15, y + 9);
    cairo_show_text(cr, _("Good Data"));
  }

  if (show_bad_head)
  {
    get_rgb_color(bad_head_color);
    r = rcolor;
    g = gcolor;
    b = bcolor;
    x = 200;
    y = 35;
    w = 10;
    l = 10;
    cairo_set_source_rgb(cr, r, g, b);
    cairo_rectangle(cr, x, y, w, l);
    cairo_stroke(cr);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_move_to(cr, x + 15, y + 9);
    cairo_show_text(cr, _("Bad Head"));
  }

  if (show_timing)
  {
    get_rgb_color(time_color);
    r = rcolor;
    g = gcolor;
    b = bcolor;
    x = 200;
    y = 50;
    w = 10;
    l = 10;
    cairo_set_source_rgb(cr, r, g, b);
    cairo_rectangle(cr, x, y, w, l);
    cairo_stroke(cr);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_move_to(cr, x + 15, y + 9);
    cairo_show_text(cr, _("Timing"));
  }

  if (show_domain)
  {
    get_rgb_color(domain_color);
    r = rcolor;
    g = gcolor;
    b = bcolor;
    x = 200;
    y = 65;
    w = 10;
    l = 10;
    cairo_set_source_rgb(cr, r, g, b);
    cairo_rectangle(cr, x, y, w, l);
    cairo_stroke(cr);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_move_to(cr, x + 15, y + 9);
    cairo_show_text(cr, _("Domain"));
  }

  get_rgb_color(selected_color);
  r = rcolor;
  g = gcolor;
  b = bcolor;
  x = 200;
  y = 80;
  w = 10;
  l = 10;
  cairo_set_source_rgb(cr, r, g, b);
  cairo_rectangle(cr, x, y, w, l);
  cairo_stroke(cr);
  cairo_set_source_rgb(cr, 0, 0, 0);
  cairo_move_to(cr, x + 15, y + 9);
  cairo_show_text(cr, _("Selected"));

  return 0;
}

static gboolean main_drawing_expose_event(GtkWidget *self, cairo_t *cr, gpointer user_data)
{
  double total_squares = main_grid_size;
  double pixels = total_squares * main_square_size * main_square_size;

  double x, y, w, l, r, g, b;
  int max_width = main_scrolled_window_width - 25;
  int adjusted_height = (pixels / max_width) + 1;
  gtk_widget_set_size_request(GTK_WIDGET(main_drawing_area), max_width, adjusted_height + 1);

  // printf("redrawing main width = %d, height = %d\n", main_drawing_area_width, main_drawing_area_height);

  get_rgb_color(WHITE);
  r = rcolor;
  g = gcolor;
  b = bcolor;
  x = 0;
  y = 0;
  w = main_drawing_area_width;
  l = main_drawing_area_height;
  cairo_set_source_rgb(cr, r, g, b);
  cairo_rectangle(cr, x, y, w, l);
  cairo_fill(cr);

  if (total_size > 0)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "redrawing main width = %d, height = %d\n", main_drawing_area_width, main_drawing_area_height);
    message_debug(tempmessage, 0);
    gdouble scroll_position = gtk_adjustment_get_value(GTK_ADJUSTMENT(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(main_scrolled_window))));
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "scroll = %f\n", scroll_position);
    message_debug(tempmessage, 0);
    int scroll_row_start = scroll_position / main_square_size;
    int scroll_row_end = scroll_row_start + (main_drawing_vbox_height / main_square_size);
    int columns = main_drawing_area_width / main_square_size;
    int rows = main_drawing_area_height / main_square_size;
    int squares = columns * rows;
    long long blocks_per_square = total_size / (squares - 1);
    square_adjust = main_square_size / 32;
    int adjustment = 1;
    while (total_size > squares * blocks_per_square)
    {
      adjustment++;
      blocks_per_square = total_size / (squares - adjustment);
    }
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "total_size=%lld, squares*blocks_per_square=%lld\n", total_size, squares * blocks_per_square);
    message_debug(tempmessage, 0);

    int count = 0;
    int i = 0;
    int n = 0;

    while (count < squares)
    {
      if (i >= scroll_row_start && i <= scroll_row_end)
      {
        int color = 0;
        int bad_head = 0;
        int good_data = 0;
        long long position = count * blocks_per_square;
        int status_bits = get_block_status(position, blocks_per_square);
        int time_bits = get_block_timing(position, blocks_per_square);
        int in_domain = process_domain(position, blocks_per_square, FINISHED, FINISHED);
        if (status_bits & NONTRIMMED_BIT)
        {
          color = nontrimmed_color;
        }
        else if (status_bits & NONDIVIDED_BIT)
        {
          color = nondivided_color;
        }
        else if (status_bits & NONSCRAPED_BIT)
        {
          color = nonscraped_color;
        }
        else if (status_bits & BAD_BIT)
        {
          color = bad_color;
        }
        else if (status_bits & NONTRIED_BIT)
        {
          color = nontried_color;
        }
        else if (status_bits & FINISHED_BIT)
        {
          color = good_color;
        }
        else if (status_bits & UNKNOWN_BIT)
        {
          color = unknown_color;
        }
        if (status_bits & BAD_HEAD_BIT)
        {
          bad_head = 1;
        }
        if (status_bits & FINISHED_BIT)
        {
          good_data = 1;
        }
        get_rgb_color(color);
        r = rcolor;
        g = gcolor;
        b = bcolor;
        cairo_set_source_rgb(cr, r, g, b);
        cairo_rectangle(cr, (n * main_square_size) + 1 + square_adjust, (i * main_square_size) + 1 + square_adjust, main_square_size - 1 - (square_adjust * 2), main_square_size - 1 - (square_adjust * 2));
        cairo_fill(cr);

        if (bad_head && show_bad_head)
        {
          int spot_size = main_square_size - 3;
          int spot_adjust = 2;
          get_rgb_color(bad_head_color);
          r = rcolor;
          g = gcolor;
          b = bcolor;
          cairo_set_source_rgb(cr, r, g, b);
          cairo_rectangle(cr, (n * main_square_size) + spot_adjust, (i * main_square_size) + spot_adjust, spot_size, spot_size);
          cairo_stroke(cr);
        }

        if (good_data && show_good_data)
        {
          int spot_size = main_square_size - 3;
          int spot_adjust = 2;
          get_rgb_color(good_color);
          r = rcolor;
          g = gcolor;
          b = bcolor;
          cairo_set_source_rgb(cr, r, g, b);
          cairo_rectangle(cr, (n * main_square_size) + spot_adjust, (i * main_square_size) + spot_adjust, spot_size, spot_size);
          cairo_stroke(cr);
        }

        if ((time_bits >= show_timing) && show_timing)
        {
          int spot_size = main_square_size - 3;
          int spot_adjust = 2;
          get_rgb_color(time_color);
          r = rcolor;
          g = gcolor;
          b = bcolor;
          cairo_set_source_rgb(cr, r, g, b);
          cairo_rectangle(cr, (n * main_square_size) + spot_adjust, (i * main_square_size) + spot_adjust, spot_size, spot_size);
          cairo_stroke(cr);
        }

        if ((in_domain) && show_domain)
        {
          int spot_size = main_square_size - 3;
          int spot_adjust = 2;
          get_rgb_color(domain_color);
          r = rcolor;
          g = gcolor;
          b = bcolor;
          cairo_set_source_rgb(cr, r, g, b);
          cairo_rectangle(cr, (n * main_square_size) + spot_adjust, (i * main_square_size) + spot_adjust, spot_size, spot_size);
          cairo_stroke(cr);
        }
      }
      count++;
      if (count > squares)
      {
        snprintf(tempmessage, TEMP_MESSAGE_SIZE, "main count out of range\n");
        message_debug(tempmessage, 0);
        break;
      }
      n++;
      if (n >= columns)
      {
        n = 0;
        i++;
        if (i >= rows)
        {
          snprintf(tempmessage, TEMP_MESSAGE_SIZE, "main rows out of range\n");
          message_debug(tempmessage, 0);
          break;
        }
      }
    }

    long long position = current_position / blocks_per_square;
    count = 0;
    for (i = 0; i < rows; i++)
    {
      for (n = 0; n < columns; n++)
      {
        if (position == count)
        {
          int spot_size = main_square_size - 3;
          int spot_adjust = 2;
          get_rgb_color(current_color);
          r = rcolor;
          g = gcolor;
          b = bcolor;
          cairo_set_source_rgb(cr, r, g, b);
          cairo_rectangle(cr, (n * main_square_size) + spot_adjust, (i * main_square_size) + spot_adjust, spot_size, spot_size);
          cairo_stroke(cr);
        }

        // int xl = (n * main_square_size) + 1 + square_adjust;
        // int yl = (i * main_square_size) + 1 + square_adjust;
        // int xh = xl + main_square_size - 1 - (square_adjust * 2);
        // int yh = yl + main_square_size - 1 - (square_adjust * 2);
        int xl = (n * main_square_size) + square_adjust;
        int yl = (i * main_square_size) + square_adjust;
        int xh = xl + main_square_size - (square_adjust * 2);
        int yh = yl + main_square_size - (square_adjust * 2);
        if (mouse_x != mouse_x_old && mouse_y != mouse_y_old && mouse_x >= xl && mouse_x <= xh && mouse_y >= yl && mouse_y <= yh)
        {
          int spot_size = main_square_size - 3;
          int spot_adjust = 2;
          get_rgb_color(selected_color);
          r = rcolor;
          g = gcolor;
          b = bcolor;
          cairo_set_source_rgb(cr, r, g, b);
          cairo_rectangle(cr, (n * main_square_size) + spot_adjust, (i * main_square_size) + spot_adjust, spot_size, spot_size);
          cairo_stroke(cr);
          get_block_information(blocks_per_square * count, blocks_per_square);
          mouse_x_old = mouse_x;
          mouse_y_old = mouse_y;
        }

        count++;
      }
    }
  }

  return 0;
}

static gboolean left_drawing_expose_event(GtkWidget *self, cairo_t *cr, gpointer user_data)
{
  double x, y, w, l, r, g, b;

  get_rgb_color(WHITE);
  r = rcolor;
  g = gcolor;
  b = bcolor;
  x = 0;
  y = 0;
  w = left_drawing_area_width;
  l = left_drawing_area_height;
  cairo_set_source_rgb(cr, r, g, b);
  cairo_rectangle(cr, x, y, w, l);
  cairo_fill(cr);

  if (total_size > 0)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "redrawing left width = %d, height = %d\n", left_drawing_area_width, left_drawing_area_height);
    message_debug(tempmessage, 0);
    int columns = left_drawing_area_width / left_square_size;
    int rows = left_drawing_area_height / left_square_size;
    int squares = columns * rows;
    long long blocks_per_square = total_size / (squares - 1);

    int count = 0;
    int i = 0;
    int n = 0;

    while (count < squares)
    {
      int color = 0;
      long long position = count * blocks_per_square;
      int status_bits = get_block_status(position, blocks_per_square);
      if (status_bits & NONTRIMMED_BIT)
      {
        color = nontrimmed_color;
      }
      else if (status_bits & NONDIVIDED_BIT)
      {
        color = nondivided_color;
      }
      else if (status_bits & NONSCRAPED_BIT)
      {
        color = nonscraped_color;
      }
      else if (status_bits & BAD_BIT)
      {
        color = bad_color;
      }
      else if (status_bits & NONTRIED_BIT)
      {
        color = nontried_color;
      }
      else if (status_bits & FINISHED_BIT)
      {
        color = good_color;
      }
      else if (status_bits & UNKNOWN_BIT)
      {
        color = unknown_color;
      }
      get_rgb_color(color);
      r = rcolor;
      g = gcolor;
      b = bcolor;
      cairo_set_source_rgb(cr, r, g, b);
      cairo_rectangle(cr, n * left_square_size, i * left_square_size, left_square_size, left_square_size);
      cairo_fill(cr);
      count++;
      if (count > squares)
      {
        snprintf(tempmessage, TEMP_MESSAGE_SIZE, "left count out of range\n");
        message_debug(tempmessage, 0);
        break;
      }
      n++;
      if (n >= columns)
      {
        n = 0;
        i++;
        if (i >= rows)
        {
          snprintf(tempmessage, TEMP_MESSAGE_SIZE, "left rows out of range\n");
          message_debug(tempmessage, 0);
          break;
        }
      }
      position = (count + 1) * blocks_per_square;
    }

    long long position = current_position / blocks_per_square;
    count = 0;
    for (i = 0; i < rows; i++)
    {
      for (n = 0; n < columns; n++)
      {
        if (position == count)
        {
          get_rgb_color(current_color);
          r = rcolor;
          g = gcolor;
          b = bcolor;
          cairo_set_source_rgb(cr, r, g, b);
          cairo_rectangle(cr, n * left_square_size, i * left_square_size, left_square_size, left_square_size);
          cairo_fill(cr);
        }
        count++;
      }
    }
  }

  return 0;
}

void getsize_top_drawing_area(GtkWidget *widget, GtkAllocation *allocation, void *data)
{
  snprintf(tempmessage, TEMP_MESSAGE_SIZE, "top_drawing_area width = %d, height = %d\n", allocation->width, allocation->height);
  message_debug(tempmessage, 0);
  top_drawing_area_width = allocation->width;
  top_drawing_area_height = allocation->height;
}

void getsize_main_drawing_area(GtkWidget *widget, GtkAllocation *allocation, void *data)
{
  snprintf(tempmessage, TEMP_MESSAGE_SIZE, "main drawing area width = %d, height = %d\n", allocation->width, allocation->height);
  message_debug(tempmessage, 0);
  main_drawing_area_width = allocation->width;
  main_drawing_area_height = allocation->height;
}

void getsize_main_drawing_vbox(GtkWidget *widget, GtkAllocation *allocation, void *data)
{
  snprintf(tempmessage, TEMP_MESSAGE_SIZE, "main drawing vbox width = %d, height = %d\n", allocation->width, allocation->height);
  message_debug(tempmessage, 0);
  main_drawing_vbox_width = allocation->width;
  main_drawing_vbox_height = allocation->height;
}

void getsize_main_scrolled_window(GtkWidget *widget, GtkAllocation *allocation, void *data)
{
  snprintf(tempmessage, TEMP_MESSAGE_SIZE, "main_scrolled_window width = %d, height = %d\n", allocation->width, allocation->height);
  message_debug(tempmessage, 0);
  main_scrolled_window_width = allocation->width;
  main_scrolled_window_height = allocation->height;
}

void getsize_left_drawing_area(GtkWidget *widget, GtkAllocation *allocation, void *data)
{
  snprintf(tempmessage, TEMP_MESSAGE_SIZE, "left drawing area width = %d, height = %d\n", allocation->width, allocation->height);
  message_debug(tempmessage, 0);
  left_drawing_area_width = allocation->width;
  left_drawing_area_height = allocation->height;
}

void select_file(void)
{
  GtkWidget *dialog;
  dialog = gtk_file_chooser_dialog_new("Log file selection",
                                       GTK_WINDOW(main_window),
                                       GTK_FILE_CHOOSER_ACTION_OPEN,
                                       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                       GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                       NULL);
  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
  {
    char *filename;
    filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    snprintf(log_file, sizeof(log_file), "%s", filename);
    printf("%s\n", log_file);
    g_free(filename);
    total_size = 0;
    int ret = read_log_file(log_file);
    if (ret != 0)
    {
      snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing log file\n");
      message_now(tempmessage);
    }
    else
    {
      ret = check_log();
      if (ret != 0)
      {
        snprintf(tempmessage, TEMP_MESSAGE_SIZE, "there were errors found in the log file\n");
        message_now(tempmessage);
      }
      else
      {
        char *filename = strrchr(log_file, '/') + 1;
        snprintf(tempmessage, TEMP_MESSAGE_SIZE, "%s [%s]", window_title, filename);
        gtk_window_set_title(GTK_WINDOW(main_window), tempmessage);

        snprintf(tempmessage, TEMP_MESSAGE_SIZE, "%s: %s", _("File"), log_file);
        gtk_label_set_text(GTK_LABEL(progress_log_label), tempmessage);
      }
    }

    gtk_widget_queue_draw(main_window);
  }
  gtk_widget_destroy(dialog);
}

void select_domain(void)
{
  GtkWidget *dialog;
  dialog = gtk_file_chooser_dialog_new("Domain file selection",
                                       GTK_WINDOW(main_window),
                                       GTK_FILE_CHOOSER_ACTION_OPEN,
                                       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                       GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                       NULL);
  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
  {
    char *filename;
    filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    snprintf(domain_file, sizeof(domain_file), "%s", filename);
    printf("%s\n", domain_file);
    g_free(filename);
    regular_domain = 1;
    int ret = read_domain_file(domain_file);
    if (ret != 0)
    {
      snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing domain file\n");
      message_now(tempmessage);
    }
    else
    {
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(showdomaincheck), TRUE);

      snprintf(tempmessage, TEMP_MESSAGE_SIZE, "%s: %s", _("Domain"), domain_file);
      gtk_label_set_text(GTK_LABEL(domain_log_label), tempmessage);
    }
    // else
    // {
    //   ret = check_domain();
    //   if (ret != 0)
    //   {
    //     snprintf(tempmessage, TEMP_MESSAGE_SIZE, "there were errors found in the domain file\n");
    //     message_now(tempmessage);
    //   }
    // }

    gtk_widget_queue_draw(main_window);
  }
  gtk_widget_destroy(dialog);
}

void select_dmde_domain(void)
{
  GtkWidget *dialog;
  dialog = gtk_file_chooser_dialog_new("DMDE bytes file selection",
                                       GTK_WINDOW(main_window),
                                       GTK_FILE_CHOOSER_ACTION_OPEN,
                                       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                       GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                       NULL);
  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
  {
    char *filename;
    filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    snprintf(domain_file, sizeof(domain_file), "%s", filename);
    printf("%s\n", domain_file);
    g_free(filename);
    regular_domain = 0;
    int ret = read_domain_dmde_file(domain_file);
    if (ret != 0)
    {
      snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing domain file\n");
      message_now(tempmessage);
    }
    else
    {
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(showdomaincheck), TRUE);

      snprintf(tempmessage, TEMP_MESSAGE_SIZE, "%s: %s", _("Domain"), domain_file);
      gtk_label_set_text(GTK_LABEL(domain_log_label), tempmessage);
    }

    gtk_widget_queue_draw(main_window);
  }
  gtk_widget_destroy(dialog);
}

gint reload_file(void)
{
  printf("%s\n", log_file);
  total_size = 0;
  int ret = read_log_file(log_file);
  if (ret != 0)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing log file\n");
    message_now(tempmessage);
  }
  else
  {
    ret = check_log();
    if (ret != 0)
    {
      snprintf(tempmessage, TEMP_MESSAGE_SIZE, "there were errors found in the log file\n");
      message_now(tempmessage);
    }
  }
  if (regular_domain)
  {
    printf("%s\n", domain_file);
    domain_lines = 0;
    int ret = read_domain_file(domain_file);
    if (ret != 0)
    {
      snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing domain file\n");
      message_now(tempmessage);
    }
  }

  gtk_widget_queue_draw(main_window);

  return 1;
}

void set_autoupdate_timer(GtkWidget *w, gpointer data)
{
  if (autotimer_on)
  {
    g_source_remove(timeout_tag);
    autotimer_on = 0;
  }
  int time = GPOINTER_TO_INT(data);
  if (time > 0)
  {
    timeout_tag = g_timeout_add(time, (GSourceFunc)reload_file, NULL);
    autotimer_on = 1;
  }

  if (!autotimer_on)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "%s: %s", _("Auto-Update"), _("Off"));
  }
  else if (time == 5000)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "%s: %s", _("Auto-Update"), _("5 seconds"));
  }
  else if (time == 10000)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "%s: %s", _("Auto-Update"), _("10 seconds"));
  }
  else if (time == 30000)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "%s: %s", _("Auto-Update"), _("30 seconds"));
  }
  else if (time == 60000)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "%s: %s", _("Auto-Update"), _("1 minute"));
  }
  else if (time == 120000)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "%s: %s", _("Auto-Update"), _("2 minutes"));
  }
  else if (time == 300000)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "%s: %s", _("Auto-Update"), _("5 minutes"));
  }
  gtk_label_set_text(GTK_LABEL(auto_update_label), tempmessage);
}

void toggle_showbad(GtkWidget *w, gpointer data)
{
  if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)))
  {
    show_bad_head = 1;
  }
  else
  {
    show_bad_head = 0;
  }

  gtk_widget_queue_draw(main_window);
}

void toggle_showgood(GtkWidget *w, gpointer data)
{
  if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)))
  {
    show_good_data = 1;
  }
  else
  {
    show_good_data = 0;
  }

  gtk_widget_queue_draw(main_window);
}

void set_show_timing(GtkWidget *w, gpointer data)
{
  show_timing = GPOINTER_TO_INT(data);

  gtk_widget_queue_draw(main_window);
}

void toggle_showdomain(GtkWidget *w, gpointer data)
{
  if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)))
  {
    show_domain = 1;
  }
  else
  {
    show_domain = 0;
  }

  gtk_widget_queue_draw(main_window);
}

void change_left_resolution(GtkWidget *w, gpointer data)
{
  g_print("%d\n", GPOINTER_TO_INT(data));
  left_square_size = GPOINTER_TO_INT(data);

  gtk_widget_queue_draw(main_window);
}

void change_main_resolution(GtkWidget *w, gpointer data)
{
  snprintf(tempmessage, TEMP_MESSAGE_SIZE, "%d\n", GPOINTER_TO_INT(data));
  message_debug(tempmessage, 0);
  main_square_size = GPOINTER_TO_INT(data);

  gtk_widget_queue_draw(main_window);
}

void change_main_grid_size(GtkWidget *w, gpointer data)
{
  snprintf(tempmessage, TEMP_MESSAGE_SIZE, "%d\n", GPOINTER_TO_INT(data));
  message_debug(tempmessage, 0);
  main_grid_size = GPOINTER_TO_INT(data);

  gtk_widget_queue_draw(main_window);
}

void get_rgb_color(int color)
{
  bcolor = (double)(color & 0xff) / 255;
  gcolor = (double)((color >> 8) & 0xff) / 255;
  rcolor = (double)((color >> 16) & 0xff) / 255;
}

int get_color_from_status(int status_bits)
{
  int count = 0;
  int color = 0;
  int red = 0;
  int green = 0;
  int blue = 0;
  if (status_bits & NONTRIED_BIT)
  {
    int new_color = nontried_color;
    blue += new_color & 0xff;
    green += (new_color >> 8) & 0xff;
    red += (new_color >> 16) & 0xff;
    count++;
  }
  if (status_bits & NONTRIMMED_BIT)
  {
    int new_color = nontrimmed_color;
    blue += new_color & 0xff;
    green += (new_color >> 8) & 0xff;
    red += (new_color >> 16) & 0xff;
    count++;
  }
  if (status_bits & NONDIVIDED_BIT)
  {
    int new_color = nondivided_color;
    blue += new_color & 0xff;
    green += (new_color >> 8) & 0xff;
    red += (new_color >> 16) & 0xff;
    count++;
  }
  if (status_bits & NONSCRAPED_BIT)
  {
    int new_color = nonscraped_color;
    blue += new_color & 0xff;
    green += (new_color >> 8) & 0xff;
    red += (new_color >> 16) & 0xff;
    count++;
  }
  if (status_bits & BAD_BIT)
  {
    int new_color = bad_color;
    blue += new_color & 0xff;
    green += (new_color >> 8) & 0xff;
    red += (new_color >> 16) & 0xff;
    count++;
  }
  if (status_bits & FINISHED_BIT)
  {
    int new_color = good_color;
    blue += new_color & 0xff;
    green += (new_color >> 8) & 0xff;
    red += (new_color >> 16) & 0xff;
    // printf ("good color= 0x%x blue= %d green = %d red= %d\n", new_color, blue, green, red);
    count++;
  }
  if (status_bits & UNKNOWN_BIT)
  {
    int new_color = unknown_color;
    blue += new_color & 0xff;
    green += (new_color >> 8) & 0xff;
    red += (new_color >> 16) & 0xff;
    count++;
  }
  if (count)
  {
    red = red / count;
    green = green / count;
    blue = blue / count;
  }
  if (status_bits & BAD_HEAD_BIT)
  {
    red = red / 2;
    green = green / 2;
    blue = blue / 2;
  }
  color = blue + (green << 8) + (red << 16);
  // printf ("status= 0x%x color= 0x%x\n", status_bits, color);
  return color;
}

// function to initialize needed memory
int initialize_memory(void)
{
  // assign memory for log
  lposition = malloc(sizeof(*lposition) * log_rows);
  if (lposition == NULL)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "Error allocating log memory (%s)\n", strerror(errno));
    message_now(tempmessage);
    return (-1);
  }
  lsize = malloc(sizeof(*lsize) * log_rows);
  if (lsize == NULL)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "Error allocating log memory (%s)\n", strerror(errno));
    message_now(tempmessage);
    return (-1);
  }
  lstatus = malloc(sizeof(*lstatus) * log_rows);
  if (lstatus == NULL)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "Error allocating log memory (%s)\n", strerror(errno));
    message_now(tempmessage);
    return (-1);
  }

  // assign memory for domain
  dposition = malloc(sizeof(*dposition) * domain_rows);
  if (dposition == NULL)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "Error allocating domain memory (%s)\n", strerror(errno));
    message_now(tempmessage);
    return (-1);
  }
  dsize = malloc(sizeof(*dsize) * domain_rows);
  if (dsize == NULL)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "Error allocating domain memory (%s)\n", strerror(errno));
    message_now(tempmessage);
    return (-1);
  }
  dstatus = malloc(sizeof(*dstatus) * domain_rows);
  if (dstatus == NULL)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "Error allocating domain memory (%s)\n", strerror(errno));
    message_now(tempmessage);
    return (-1);
  }

  return 0;
}

int increase_log_memory(int new_lines)
{
  int found_error = 0;
  log_rows += new_lines;
  temp_lposition = realloc(lposition, log_rows * sizeof(*lposition));
  if (temp_lposition == NULL)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "Error allocating log memory (%s)\n", strerror(errno));
    message_now(tempmessage);
    found_error = -2;
  }
  lposition = temp_lposition;

  temp_lsize = realloc(lsize, log_rows * sizeof(*lsize));
  if (temp_lsize == NULL)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "Error allocating log memory (%s)\n", strerror(errno));
    message_now(tempmessage);
    found_error = -2;
  }
  lsize = temp_lsize;

  temp_lstatus = realloc(lstatus, log_rows * sizeof(*lstatus));
  if (temp_lstatus == NULL)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "Error allocating log memory (%s)\n", strerror(errno));
    message_now(tempmessage);
    found_error = -2;
  }
  lstatus = temp_lstatus;
  return found_error;
}

int increase_domain_memory(int new_lines)
{
  int found_error = 0;
  domain_rows += new_lines;
  temp_dposition = realloc(dposition, domain_rows * sizeof(*dposition));
  if (temp_dposition == NULL)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "Error allocating domain memory (%s)\n", strerror(errno));
    message_now(tempmessage);
    found_error = -2;
  }
  dposition = temp_dposition;

  temp_dsize = realloc(dsize, domain_rows * sizeof(*dsize));
  if (temp_dsize == NULL)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "Error allocating domain memory (%s)\n", strerror(errno));
    message_now(tempmessage);
    found_error = -2;
  }
  dsize = temp_dsize;

  temp_dstatus = realloc(dstatus, domain_rows * sizeof(*dstatus));
  if (temp_dstatus == NULL)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "Error allocating domain memory (%s)\n", strerror(errno));
    message_now(tempmessage);
    found_error = -2;
  }
  dstatus = temp_dstatus;
  return found_error;
}

// function to read the log file into memory
int read_log_file(char *log_file)
{
  if (log_file == NULL)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "Error: No log file specified.\n");
    message_now(tempmessage);
    return (1);
  }

  FILE *readfile;
  readfile = fopen(log_file, "r");
  if (readfile == NULL)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "Cannot open %s for reading (%s).\n", log_file, strerror(errno));
    message_now(tempmessage);
    return (1);
  }

  char line[MAX_LINE_LENGTH];
  int i = 0;
  int real_line_number = 0;
  int found_current = 0;
  int found_error = 0;
  ddrescue_log = 0;
  while (fgets(line, sizeof line, readfile))
  {
    real_line_number++;
    // process the line here
    int scanline;
    long long temp_position = 0;
    long long temp_size = 0;
    unsigned char temp_status = 0;
    char raw_position[MAX_LINE_LENGTH];
    char raw_size[MAX_LINE_LENGTH];
    char raw_status[MAX_LINE_LENGTH];
    char raw_info[MAX_LINE_LENGTH];
    char raw_errstat[MAX_LINE_LENGTH];
    char rest_of_line[MAX_LINE_LENGTH];
    raw_position[0] = '\0';
    raw_size[0] = '\0';
    raw_status[0] = '\0';
    rest_of_line[0] = '\0';
    scanline = sscanf(line, "%s %s %s %s %s %[^\n]", raw_position, raw_size, raw_status, raw_info, raw_errstat, rest_of_line);
    // ignore blank lines
    if (scanline > 0)
    {
      // if the line doesn't start with a comment then process it
      if (raw_position[0] != '#')
      {
        // the first line should be the current position and status
        // new ddrescue format has copy pass added to first line
        if ((!found_current && scanline == 2) || (!found_current && scanline == 3))
        {
          char *endptr;
          current_position = strtoull(raw_position, &endptr, 0);
          if (*endptr)
          {
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing position on line %d\n", real_line_number);
            message_now(tempmessage);
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "line%d= %s", real_line_number, line);
            message_now(tempmessage);
            found_error = 1;
          }
          current_status = strtoull(raw_size, &endptr, 0);
          if (*endptr)
          {
            // check if ddrescue format
            scanline = sscanf(raw_size, "%c %[^\n]", &temp_status, rest_of_line);
            if (scanline == 1)
            {
              if (temp_status == '?')
              {
                current_status = NONTRIED;
              }
              else if (temp_status == '+')
              {
                current_status = FINISHED;
              }
              else if (temp_status == '*')
              {
                current_status = NONTRIMMED;
              }
              else if (temp_status == '/')
              {
                current_status = NONSCRAPED;
              }
              else if (temp_status == '-')
              {
                current_status = BAD;
              }
              else
              {
                snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing line %d, status not recognized\n", real_line_number);
                message_now(tempmessage);
                snprintf(tempmessage, TEMP_MESSAGE_SIZE, "line%d= %s", real_line_number, line);
                message_now(tempmessage);
                found_error = -1;
              }
            }
            else
            {
              snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing status on line %d\n", real_line_number);
              message_now(tempmessage);
              snprintf(tempmessage, TEMP_MESSAGE_SIZE, "line%d= %s", real_line_number, line);
              message_now(tempmessage);
              found_error = 1;
            }
          }
          found_current = 1;
          // fprintf (stdout, "%d=  %d  0x%08llx  0x%08llx\n", real_line_number, i, current_position, current_status);    //debug
        }
        else if (!found_current)
        {
          snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing progress log file line %d, expecting current status\n", real_line_number);
          message_now(tempmessage);
          snprintf(tempmessage, TEMP_MESSAGE_SIZE, "line%d= %s", real_line_number, line);
          message_now(tempmessage);
          found_error = 1;
        }

        // hddclone progress file format
        else if (scanline == 5)
        {
          char *endptr;
          lposition[i] = strtoull(raw_position, &endptr, 0);
          if (*endptr)
          {
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing position on line %d\n", real_line_number);
            message_now(tempmessage);
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "line%d= %s", real_line_number, line);
            message_now(tempmessage);
            found_error = 1;
          }
          lsize[i] = strtoull(raw_size, &endptr, 0);
          if (*endptr)
          {
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing size on line %d\n", real_line_number);
            message_now(tempmessage);
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "line%d= %s", real_line_number, line);
            message_now(tempmessage);
            found_error = 1;
          }
          lstatus[i] = strtoll(raw_status, &endptr, 0);
          if (*endptr)
          {
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing status on line %d\n", real_line_number);
            message_now(tempmessage);
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "line%d= %s", real_line_number, line);
            message_now(tempmessage);
            found_error = 1;
          }
          lstatus[i] += (strtoll(raw_info, &endptr, 0)) << 8;
          if (*endptr)
          {
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing info on line %d\n", real_line_number);
            message_now(tempmessage);
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "line%d= %s", real_line_number, line);
            message_now(tempmessage);
            found_error = 1;
          }
          lstatus[i] += (strtoll(raw_errstat, &endptr, 0)) << 32;
          if (*endptr)
          {
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing err/stat on line %d\n", real_line_number);
            message_now(tempmessage);
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "line%d= %s", real_line_number, line);
            message_now(tempmessage);
            found_error = 1;
          }
          // fprintf (stdout, "%d=  %d  0x%08llx  0x%08llx  0x%08llx\n", real_line_number, i, lposition[i], lsize[i], lstatus[i]);    //debug
          i++;
        }

        // ddrescue log format
        else if (scanline == 3)
        {
          char *endptr;
          temp_position = strtoull(raw_position, &endptr, 0);
          if (*endptr)
          {
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing position on line %d\n", real_line_number);
            message_now(tempmessage);
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "line%d= %s", real_line_number, line);
            message_now(tempmessage);
            found_error = -1;
          }
          temp_size = strtoull(raw_size, &endptr, 0);
          if (*endptr)
          {
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing size on line %d\n", real_line_number);
            message_now(tempmessage);
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "line%d= %s", real_line_number, line);
            message_now(tempmessage);
            found_error = -1;
          }
          scanline = sscanf(raw_status, "%c %[^\n]", &temp_status, rest_of_line);
          if (scanline != 1)
          {
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing status on line %d\n", real_line_number);
            message_now(tempmessage);
            found_error = -1;
          }

          if (!found_error)
          {
            lposition[i] = temp_position;
            lsize[i] = temp_size;

            if (temp_status == '?')
            {
              lstatus[i] = NONTRIED;
            }
            else if (temp_status == '+')
            {
              lstatus[i] = FINISHED;
            }
            else if (temp_status == '*')
            {
              lstatus[i] = NONTRIMMED;
            }
            else if (temp_status == '/')
            {
              lstatus[i] = NONSCRAPED;
            }
            else if (temp_status == '-')
            {
              lstatus[i] = BAD;
            }
            else
            {
              snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing line %d, status not recognized\n", real_line_number);
              message_now(tempmessage);
              snprintf(tempmessage, TEMP_MESSAGE_SIZE, "line%d= %s", real_line_number, line);
              message_now(tempmessage);
              found_error = -1;
            }
            if (!found_error)
            {
              ddrescue_log = 1;
            }
          }

          // fprintf (stdout, "%d=  %d  0x%08llx  0x%08llx  0x%08llx\n", real_line_number, i, lposition[i], lsize[i], lstatus[i]);    //debug
          i++;
        }

        else
        {
          snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing progress log file line %d\n", real_line_number);
          message_now(tempmessage);
          snprintf(tempmessage, TEMP_MESSAGE_SIZE, "line%d= %s", real_line_number, line);
          message_now(tempmessage);
          found_error = 1;
        }
      }
      // line starts with # so check if sector size
      else
      {
        if (strcmp(raw_size, "sectorsize") == 0)
        {
          sector_size = strtoll(raw_status, NULL, 0);
          // fprintf (stdout, "found sector size of %d\n", sector_size);
        }
      }
    }

    // if used up allocated memory then increase it
    if (i >= log_rows)
    {
      increase_log_memory(1000);
    }

    if (found_error)
    {
      break;
    }
  }
  total_lines = i;

  fclose(readfile);
  return (found_error);
}

// function to read the domain file into memory
int read_domain_file(char *domain_file)
{
  if (domain_file == NULL)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "Error: No domain file specified.\n");
    message_now(tempmessage);
    return (1);
  }

  FILE *readfile;
  readfile = fopen(domain_file, "r");
  if (readfile == NULL)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "Cannot open %s for reading (%s).\n", domain_file, strerror(errno));
    message_now(tempmessage);
    return (1);
  }

  char line[MAX_LINE_LENGTH];
  int i = 0;
  int real_line_number = 0;
  int found_current = 0;
  int found_error = 0;
  while (fgets(line, sizeof line, readfile))
  {
    found_error = 0;
    real_line_number++;
    // process the line here
    // fprintf (stdout, "line%d= %s", real_line_number, line);    //debug

    int scanline;
    long long temp_position = 0;
    long long temp_size = 0;
    unsigned char temp_status = 0;
    char raw_position[MAX_LINE_LENGTH];
    char raw_size[MAX_LINE_LENGTH];
    char raw_status[MAX_LINE_LENGTH];
    char raw_info[MAX_LINE_LENGTH];
    char raw_errstat[MAX_LINE_LENGTH];
    char rest_of_line[MAX_LINE_LENGTH];
    raw_position[0] = '\0';
    raw_size[0] = '\0';
    raw_status[0] = '\0';
    rest_of_line[0] = '\0';
    scanline = sscanf(line, "%s %s %s %s %s %[^\n]", raw_position, raw_size, raw_status, raw_info, raw_errstat, rest_of_line);
    // ignore blank lines
    if (scanline > 0)
    {
      // if the line doesn't start with a comment then process it
      if (raw_position[0] != '#')
      {
        // the first line should be the current position and status
        // new ddrescue format has copy pass added to first line
        if ((!found_current && scanline == 2) || (!found_current && scanline == 3))
        {
          found_current = 1;
        }

        // hddclone progress file format
        else if (scanline == 5)
        {
          char *endptr;
          dposition[i] = strtoull(raw_position, &endptr, 0);
          if (*endptr)
          {
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing domain position on line %d\n", real_line_number);
            message_now(tempmessage);
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "line%d= %s", real_line_number, line);
            message_now(tempmessage);
            found_error = -1;
          }
          dsize[i] = strtoull(raw_size, &endptr, 0);
          if (*endptr)
          {
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing domain size on line %d\n", real_line_number);
            message_now(tempmessage);
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "line%d= %s", real_line_number, line);
            message_now(tempmessage);
            found_error = -1;
          }
          dstatus[i] = strtoll(raw_status, &endptr, 0);
          if (*endptr)
          {
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing domain status on line %d\n", real_line_number);
            message_now(tempmessage);
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "line%d= %s", real_line_number, line);
            message_now(tempmessage);
            found_error = -1;
          }
          dstatus[i] += (strtoll(raw_info, &endptr, 0)) << 8;
          if (*endptr)
          {
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing domain info on line %d\n", real_line_number);
            message_now(tempmessage);
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "line%d= %s", real_line_number, line);
            message_now(tempmessage);
            found_error = -1;
          }
          dstatus[i] += (strtoll(raw_errstat, &endptr, 0)) << 32;
          if (*endptr)
          {
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing domain err/stat on line %d\n", real_line_number);
            message_now(tempmessage);
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "line%d= %s", real_line_number, line);
            message_now(tempmessage);
            found_error = -1;
          }
          // fprintf (stdout, "%d=  %d  0x%08llx  0x%08llx  0x%08llx\n", real_line_number, i, lposition[i], lsize[i], lstatus[i]);    //debug
          i++;
        }

        // ddrescue log format
        else if (scanline == 3)
        {
          char *endptr;
          temp_position = strtoull(raw_position, &endptr, 0);
          if (*endptr)
          {
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing domain position on line %d\n", real_line_number);
            message_now(tempmessage);
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "line%d= %s", real_line_number, line);
            message_now(tempmessage);
            found_error = -1;
          }
          temp_size = strtoull(raw_size, &endptr, 0);
          if (*endptr)
          {
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing domain size on line %d\n", real_line_number);
            message_now(tempmessage);
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "line%d= %s", real_line_number, line);
            message_now(tempmessage);
            found_error = -1;
          }
          scanline = sscanf(raw_status, "%c %[^\n]", &temp_status, rest_of_line);
          if (scanline != 1)
          {
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing domain status on line %d\n", real_line_number);
            message_now(tempmessage);
            found_error = -1;
          }

          if (!found_error)
          {
            dposition[i] = temp_position;
            dsize[i] = temp_size;

            if (temp_status == '?')
            {
              dstatus[i] = NONTRIED;
            }
            else if (temp_status == '+')
            {
              dstatus[i] = FINISHED;
            }
            else if (temp_status == '*')
            {
              dstatus[i] = NONTRIMMED;
            }
            else if (temp_status == '/')
            {
              dstatus[i] = NONSCRAPED;
            }
            else if (temp_status == '-')
            {
              dstatus[i] = BAD;
            }
            else
            {
              snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing line %d, domain status not recognized\n", real_line_number);
              message_now(tempmessage);
              snprintf(tempmessage, TEMP_MESSAGE_SIZE, "line%d= %s", real_line_number, line);
              message_now(tempmessage);
              found_error = -1;
            }

            // if position did not align then increase position and also decrease size if it is not 0
            if ((found_error & 1))
            {
              dposition[i]++;
              if (dsize[i] > 0)
              {
                dsize[i]--;
              }
            }
            // if only size did not align then do nothing as it was already cut by the divide
          }

          // fprintf (stdout, "%d=  %d  0x%08llx  0x%08llx  0x%08llx\n", real_line_number, i, lposition[i], lsize[i], lstatus[i]);    //debug
          i++;
        }

        else
        {
          snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing domain file line %d\n", real_line_number);
          message_now(tempmessage);
          snprintf(tempmessage, TEMP_MESSAGE_SIZE, "line%d= %s", real_line_number, line);
          message_now(tempmessage);
          found_error = -1;
        }
        found_current = 1;
      }
    }

    // if used up allocated memory then increase it
    if (i >= domain_rows)
    {
      increase_domain_memory(1000);
    }

    if (found_error < 0)
    {
      break;
    }
  }
  domain_lines = i;
  if (found_error < 0)
  {
    domain_lines = 0;
  }

  fclose(readfile);
  return (found_error);
}

// function to read a dmde file as domain
int read_domain_dmde_file(char *dmde_file)
{
  if (dmde_file == NULL)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "Error: No file specified.\n");
    message_error(tempmessage);
    return (1);
  }
  // snprintf (tempmessage, TEMP_MESSAGE_SIZE, "Reading %s into memory...\n", dmde_file);    //debug

  FILE *readfile;
  readfile = fopen(dmde_file, "r");
  if (readfile == NULL)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "Cannot open %s for reading (%s).\n", dmde_file, strerror(errno));
    message_error(tempmessage);
    return (1);
  }

  domain_lines = 1;
  dposition[0] = 0;
  dsize[0] = total_size;
  dstatus[0] = 0;

  char line[MAX_LINE_LENGTH];
  int real_line_number = 0;
  int found_error = 0;
  while (fgets(line, sizeof line, readfile))
  {
    found_error = 0;
    real_line_number++;
    // process the line here
    // snprintf (tempmessage, TEMP_MESSAGE_SIZE, "line%d= %s", real_line_number, line);    //debug
    int scanline;
    long long temp_position = 0;
    long long temp_size = 0;
    char raw_position[MAX_LINE_LENGTH];
    char raw_size[MAX_LINE_LENGTH];
    char rest_of_line[MAX_LINE_LENGTH];
    raw_position[0] = '\0';
    raw_size[0] = '\0';
    rest_of_line[0] = '\0';
    scanline = sscanf(line, "%s %s %[^\n]", raw_position, raw_size, rest_of_line);
    // ignore blank lines
    if (scanline > 0)
    {
      if (scanline == 2)
      {
        int good = 1;
        char *endptr;
        temp_position = strtoull(raw_position, &endptr, 0);
        if (*endptr)
        {
          snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing position on line %d\n", real_line_number);
          message_now(tempmessage);
          snprintf(tempmessage, TEMP_MESSAGE_SIZE, "line%d= %s", real_line_number, line);
          message_now(tempmessage);
          found_error = -1;
          good = 0;
        }
        temp_size = strtoull(raw_size, &endptr, 0);
        if (*endptr)
        {
          snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error processing size on line %d\n", real_line_number);
          message_now(tempmessage);
          snprintf(tempmessage, TEMP_MESSAGE_SIZE, "line%d= %s", real_line_number, line);
          message_now(tempmessage);
          found_error = -1;
          good = 0;
        }
        if (good)
        {
          long long position = temp_position / sector_size;
          if (temp_position % sector_size)
          {
            position = position + 1;
          }
          long long size = temp_size / sector_size;
          if (temp_size % sector_size)
          {
            size = size + 1;
          }
          if (position + size > total_size)
          {
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "end position greater than source size on line %d\n", real_line_number);
            message_now(tempmessage);
            snprintf(tempmessage, TEMP_MESSAGE_SIZE, "line%d= %s", real_line_number, line);
            message_now(tempmessage);
            found_error = -1;
          }
          else if (position > 0 && size > 0)
          {
            int ret = add_to_domain(position, size);
            if (ret)
            {
              snprintf(tempmessage, TEMP_MESSAGE_SIZE, "error adding to domain from line %d\n", real_line_number);
              message_now(tempmessage);
              snprintf(tempmessage, TEMP_MESSAGE_SIZE, "line%d= %s", real_line_number, line);
              message_now(tempmessage);
              found_error = -1;
            }
          }
        }
      }
      else
      {
        snprintf(tempmessage, TEMP_MESSAGE_SIZE, "format error on line %d\n", real_line_number);
        message_now(tempmessage);
        snprintf(tempmessage, TEMP_MESSAGE_SIZE, "line%d= %s", real_line_number, line);
        message_now(tempmessage);
        found_error = -1;
      }
    }
  }

  fclose(readfile);
  return (found_error);
}

int add_to_domain(long long position, long long size)
{
  // fprintf (debug_file, "change_domain_chunk_status 0x%06llx  0x%06llx\n", position, size);    //debug
  int line_number;
  // fprintf (debug_file,"before\n");
  for (line_number = 0; line_number < domain_lines; line_number++)
  {
    // fprintf (debug_file, "0x%06llx \t0x%06llx \t0x%llx \t0x%llx \t0x%llx\n", dposition[line_number], dsize[line_number], (dstatus[line_number] & STATUS_MASK), ((dstatus[line_number] & INFO_MASK) >> 8), (dstatus[line_number] >> 32) );
  }

  int block;
  while (1)
  {
    // fprintf (debug_file,"while start position=%06llx  size=0x%06llx\n", position, size);
    block = find_domain_block(position);
    if (block == -1)
    {
      snprintf(tempmessage, TEMP_MESSAGE_SIZE, _("Domain block not found"));
      message_error(tempmessage);
      snprintf(tempmessage, TEMP_MESSAGE_SIZE, "\nposition=%06llx  size=0x%06llx", position, size);
      message_error(tempmessage);
      print_gui_error_message(error_message, _("Error!"), 1);
      clear_error_message();

      // fprintf (debug_file,"error1 position=%06llx  size=0x%06llx\n", position, size);
      for (line_number = 0; line_number < domain_lines; line_number++)
      {
        // fprintf (debug_file, "0x%06llx \t0x%06llx \t0x%llx \t0x%llx \t0x%llx\n", dposition[line_number], dsize[line_number], (dstatus[line_number] & STATUS_MASK), ((dstatus[line_number] & INFO_MASK) >> 8), (dstatus[line_number] >> 32) );
      }

      return -1;
    }
    // see if chunk fits within the block
    if (position >= dposition[block] && (position + size) <= (dposition[block] + dsize[block]))
    {
      // check if already finished status, if so then nothing to do
      // we are using status mask in case of imported domain file, but extra status will not be kept
      if ((dstatus[block] & STATUS_MASK) == (FINISHED & STATUS_MASK))
      {
        return 0;
      }
      // it fits so break to allow that processing below
      else
      {
        break;
      }
    }
    else
    {
      // chunk is bigger than block, need to process
      // check if position matches
      if (position == dposition[block])
      {
        // fprintf (debug_file,"while1\n");
        //  if position matches then we can mark the block, then move position forward to next block reduce size and process
        dstatus[block] = FINISHED;
        position += dsize[block];
        size -= dsize[block];
        for (line_number = 0; line_number < 6; line_number++)
        {
          // fprintf (debug_file, "0x%06llx \t0x%06llx \t0x%llx \t0x%llx \t0x%llx\n", dposition[line_number], dsize[line_number], (dstatus[line_number] & STATUS_MASK), ((dstatus[line_number] & INFO_MASK) >> 8), (dstatus[line_number] >> 32) );
        }

        // check for same status on next block and merge if needed
        if (block < domain_lines - 1)
        {
          // fprintf (debug_file,"while2\n");
          if ((dstatus[block] & STATUS_MASK) == (dstatus[block + 1] & STATUS_MASK))
          {
            // fprintf (debug_file,"while3\n");
            dposition[block + 1] -= dsize[block];
            dsize[block + 1] += dsize[block];
            delete_domain_line(block);
            for (line_number = 0; line_number < 6; line_number++)
            {
              // fprintf (debug_file, "0x%06llx \t0x%06llx \t0x%llx \t0x%llx \t0x%llx\n", dposition[line_number], dsize[line_number], (dstatus[line_number] & STATUS_MASK), ((dstatus[line_number] & INFO_MASK) >> 8), (dstatus[line_number] >> 32) );
            }
          }
        }
        // check for same status on previous block and merge if needed
        if (block > 0 && (FINISHED & STATUS_MASK) == (dstatus[block - 1] & STATUS_MASK))
        {
          // fprintf (debug_file,"while4\n");
          dsize[block - 1] += dsize[block];
          delete_domain_line(block);
          for (line_number = 0; line_number < 6; line_number++)
          {
            // fprintf (debug_file, "0x%06llx \t0x%06llx \t0x%llx \t0x%llx \t0x%llx\n", dposition[line_number], dsize[line_number], (dstatus[line_number] & STATUS_MASK), ((dstatus[line_number] & INFO_MASK) >> 8), (dstatus[line_number] >> 32) );
          }
        }
      }
      // check if position is in middle of the block
      else if (position > dposition[block])
      {
        // fprintf (debug_file,"while5\n");
        //  check for same status on next block and merge if needed
        if (block < domain_lines - 1)
        {
          // fprintf (debug_file,"while6\n");
          if ((FINISHED & STATUS_MASK) == (dstatus[block + 1] & STATUS_MASK))
          {
            // fprintf (debug_file,"while7\n");
            dsize[block + 1] += dposition[block + 1] - position;
            dposition[block + 1] = position;
          }
        }
        // move position ahead reduce size and shorten size
        long long diff = (dposition[block] + dsize[block]) - position;
        // dsize[block] -= diff;
        position += diff;
        size -= diff;
        for (line_number = 0; line_number < 6; line_number++)
        {
          // fprintf (debug_file, "0x%06llx \t0x%06llx \t0x%llx \t0x%llx \t0x%llx\n", dposition[line_number], dsize[line_number], (dstatus[line_number] & STATUS_MASK), ((dstatus[line_number] & INFO_MASK) >> 8), (dstatus[line_number] >> 32) );
        }
      }
      // the position is before the block which should not be possible
      else
      {
        snprintf(tempmessage, TEMP_MESSAGE_SIZE, _("Domain block not found"));
        message_error(tempmessage);
        snprintf(tempmessage, TEMP_MESSAGE_SIZE, "\nposition=%06llx  size=0x%06llx", position, size);
        message_error(tempmessage);
        print_gui_error_message(error_message, _("Error!"), 1);
        clear_error_message();

        // fprintf (debug_file,"error2 position=%06llx  size=0x%06llx\n", position, size);
        for (line_number = 0; line_number < domain_lines; line_number++)
        {
          // fprintf (debug_file, "0x%06llx \t0x%06llx \t0x%llx \t0x%llx \t0x%llx\n", dposition[line_number], dsize[line_number], (dstatus[line_number] & STATUS_MASK), ((dstatus[line_number] & INFO_MASK) >> 8), (dstatus[line_number] >> 32) );
        }

        return -1;
      }
    }
    // fprintf (debug_file,"while end position=%06llx  size=0x%06llx\n", position, size);
  }

  // the chunk fits in the block
  // if postion and size match the block then only need to change status
  if (position == dposition[block] && size == dsize[block])
  {
    dstatus[block] = FINISHED;
    if (block < domain_lines - 1)
    {
      if ((dstatus[block] & STATUS_MASK) == (dstatus[block + 1] & STATUS_MASK))
      {
        dposition[block + 1] -= size;
        dsize[block + 1] += size;
        delete_domain_line(block);
      }
    }
    if (block > 0)
    {
      if ((dstatus[block] & STATUS_MASK) == (dstatus[block - 1] & STATUS_MASK))
      {
        dsize[block - 1] += dsize[block];
        delete_domain_line(block);
      }
    }
    // return 0;
  }
  // if position matches then check line above and merge or insert new line
  else if (position == dposition[block])
  {
    if (block > 0 && (FINISHED & STATUS_MASK) == (dstatus[block - 1] & STATUS_MASK))
    {
      dsize[block - 1] += size;
      dposition[block] += size;
      dsize[block] -= size;
      // return 0;
    }
    else
    {
      insert_domain_line(block + 1, position + size, dsize[block] - size, dstatus[block]);
      dposition[block] = position;
      dsize[block] = size;
      dstatus[block] = FINISHED;
      // return 0;
    }
  }
  // if end position matches then check line below and merge or insert new line
  else if (position + size == dposition[block] + dsize[block])
  {
    if (block < domain_lines - 1 && (FINISHED & STATUS_MASK) == (dstatus[block + 1] & STATUS_MASK))
    {
      dposition[block + 1] -= size;
      dsize[block + 1] += size;
      dsize[block] -= size;
      // return 0;
    }
    else
    {
      insert_domain_line(block, dposition[block], dsize[block] - size, dstatus[block]);
      dposition[block + 1] = dposition[block] + dsize[block];
      dsize[block + 1] = size;
      dstatus[block + 1] = FINISHED;
      // return 0;
    }
  }
  // if chunk is in the middle of the block then insert 2 lines and adjust
  else
  {
    long long block_end = dposition[block] + dsize[block];
    insert_domain_line(block + 1, position + size, block_end - (position + size), dstatus[block]);
    insert_domain_line(block, dposition[block], position - dposition[block], dstatus[block]);
    dposition[block + 1] = position;
    dsize[block + 1] = size;
    dstatus[block + 1] = FINISHED;
  }

  // fprintf (debug_file,"after\n");
  for (line_number = 0; line_number < domain_lines; line_number++)
  {
    // fprintf (debug_file, "0x%06llx \t0x%06llx \t0x%llx \t0x%llx \t0x%llx\n", dposition[line_number], dsize[line_number], (dstatus[line_number] & STATUS_MASK), ((dstatus[line_number] & INFO_MASK) >> 8), (dstatus[line_number] >> 32) );
  }

  return 0;
}

int insert_domain_line(int line, long long position, long long size, long long status)
{
  if (domain_lines + 1 >= domain_rows)
  {
    int ret = increase_domain_memory(1000);
    if (ret)
    {
      return ret;
    }
  }
  int i;
  for (i = domain_lines; i > line; i--)
  {
    dposition[i] = dposition[i - 1];
    dsize[i] = dsize[i - 1];
    dstatus[i] = dstatus[i - 1];
  }
  dposition[i] = position;
  dsize[i] = size;
  dstatus[i] = status;
  domain_lines++;
  return (0);
}

int find_domain_block(long long position)
{
  int first = 0;
  int last = domain_lines - 1;
  int middle = (first + last) / 2;
  while (first <= last)
  {
    // fprintf (stdout, "pos=0x%llx\n", position);
    if (position >= dposition[middle] && position < dposition[middle] + dsize[middle])
    {
      return (middle);
    }
    else if (position > dposition[middle])
    {
      first = middle + 1;
    }
    else
    {
      last = middle - 1;
    }
    middle = (first + last) / 2;
  }
  return (-1);
}

int delete_domain_line(int line)
{
  int i;
  for (i = line; i < domain_lines; i++)
  {
    dposition[i] = dposition[i + 1];
    dsize[i] = dsize[i + 1];
    dstatus[i] = dstatus[i + 1];
  }
  domain_lines--;
  return (0);
}

int check_log(void)
{
  snprintf(tempmessage, TEMP_MESSAGE_SIZE, "Checking progress log file...\n");
  message_now(tempmessage);
  int i;
  int fail = 0;

  // check that first position is 0
  if (lposition[0] != 0)
  {
    snprintf(tempmessage, TEMP_MESSAGE_SIZE, "The first position is not 0\n");
    message_now(tempmessage);
    fail = 1;
  }

  for (i = 0; i < total_lines - 1; i++)
  {
    // check if there is an overlap
    if ((lposition[i] + lsize[i]) > lposition[i + 1])
    {
      snprintf(tempmessage, TEMP_MESSAGE_SIZE, "Overlap found on line %d\n", i + 1);
      message_now(tempmessage);
      fail = 2;
    }
    // check if size is 0
    if (lsize[i] == 0)
    {
      snprintf(tempmessage, TEMP_MESSAGE_SIZE, "Size of 0 found on line %d\n", i + 1);
      message_now(tempmessage);
      fail = 3;
    }
    // check if there is a gap
    if ((lposition[i] + lsize[i]) < lposition[i + 1])
    {
      snprintf(tempmessage, TEMP_MESSAGE_SIZE, "Gap found on line %d\n", i + 1);
      message_now(tempmessage);
      fail = 4;
    }
    // check if the next status is the same
    if (lstatus[i] == lstatus[i + 1])
    {
      snprintf(tempmessage, TEMP_MESSAGE_SIZE, "Same status found on line %d\n", i + 1);
      message_now(tempmessage);
      fail = 5;
    }
  }

  // get total size from last position
  total_size = lposition[total_lines - 1] + lsize[total_lines - 1];
  snprintf(tempmessage, TEMP_MESSAGE_SIZE, "total size = %lld\n", total_size);
  message_now(tempmessage);

  return fail;
}

int check_domain(void)
{
  snprintf(tempmessage, TEMP_MESSAGE_SIZE, "Checking domain file...\n");
  message_now(tempmessage);
  int i;
  int fail = 0;
  for (i = 0; i < domain_lines - 1; i++)
  {
    // check if there is an overlap
    if ((dposition[i] + dsize[i]) > dposition[i + 1])
    {
      snprintf(tempmessage, TEMP_MESSAGE_SIZE, "Overlap found on domain line %d\n", i + 1);
      message_now(tempmessage);
      fail = 2;
    }
  }
  return fail;
}

int get_block_status(long long position, long long size)
{
  int status = 0;
  int line = find_block(position);
  if (line == -1)
  {
    // snprintf (tempmessage, TEMP_MESSAGE_SIZE, "Error: Position 0x%llx not found in progress log file\n", position);
    // message_now(tempmessage);
    return -1;
  }
  // process current status
  status = status | process_status(line);
  // check if the chunk fits in the block and if not then get next block
  // but only get the status of up to 255 more chunks before moving on
  // otherwise it can take a long time with a large log file
  long long block_end = lposition[line] + lsize[line];
  size -= lposition[line] - position;
  int chunk_count = 255;
  while (chunk_count > 0)
  {
    chunk_count--;
    if (lposition[line] + size > block_end || lposition[line] > block_end)
    {
      size -= lsize[line];
      line++;
      if (line > total_lines - 1)
      {
        break;
      }
      block_end = lposition[line] + lsize[line];
      status = status | process_status(line);
    }
    else
    {
      break;
    }
    // break;
  }

  return status;
}

int get_block_timing(long long position, long long size)
{
  long long new_size = size;
  long long time = 0;
  int high_time = 0;
  int line = find_block(position);
  if (line == -1)
  {
    // snprintf (tempmessage, TEMP_MESSAGE_SIZE, "Error: Position 0x%llx not found in progress log file\n", position);
    // message_now(tempmessage);
    return -1;
  }
  // process current time
  time = (lstatus[line] >> 32) & 0xff;
  if (time > high_time)
  {
    high_time = time;
  }
  // check if the chunk fits in the block and if not then get next block
  // but only get the status of up to 255 more chunks before moving on
  // otherwise it can take a long time with a large log file
  long long block_end = lposition[line] + lsize[line];
  new_size -= lposition[line] - position;
  int chunk_count = 255;
  while (chunk_count > 0)
  {
    chunk_count--;
    if (lposition[line] + new_size > block_end || lposition[line] > block_end)
    {
      new_size -= lsize[line];
      line++;
      if (line > total_lines - 1)
      {
        break;
      }
      block_end = lposition[line] + lsize[line];
      time = (lstatus[line] >> 32) & 0xff;
      if (time > high_time)
      {
        high_time = time;
      }
    }
    else
    {
      break;
    }
    // break;
  }

  return high_time;
}

int get_block_information(long long position, long long size)
{
  long long new_size = size;
  long long ret = 0;
  int line = find_block(position);
  if (line == -1)
  {
    // snprintf (tempmessage, TEMP_MESSAGE_SIZE, "Error: Position 0x%llx not found in progress log file\n", position);
    // message_now(tempmessage);
    return -1;
  }
  int maxcount = 4096;
  long long nontried = 0;
  long long nontrimmed = 0;
  long long nondivided = 0;
  long long nonscraped = 0;
  long long bad = 0;
  long long finished = 0;
  long long hightime = 0;
  char tempchar[384];
  char lines[maxcount * sizeof(tempchar)];
  lines[0] = '\0';
  // process current status
  // fprintf (stdout, "1 pos=%lld siz=%lld nsiz=%lld lin=%d lpos=%lld lsiz=%lld\n", position, size, new_size, line, lposition[line], lsize[line]);    //debug
  ret = process_information(position, size, line);
  if (ret > 0)
  {
    nontried = nontried + temp_nontried;
    nontrimmed = nontrimmed + temp_nontrimmed;
    nondivided = nondivided + temp_nondivided;
    nonscraped = nonscraped + temp_nonscraped;
    bad = bad + temp_bad;
    finished = finished + temp_finished;
    if (temp_hightime > hightime)
    {
      hightime = temp_hightime;
    }
    if (ddrescue_log)
    {
      snprintf(tempchar, sizeof(tempchar), "0x%06llx \t0x%06llx \t%c\n", lposition[line], lsize[line], ddrstatus);
    }
    else
    {
      snprintf(tempchar, sizeof(tempchar), "0x%06llx \t0x%06llx \t0x%llx \t0x%llx \t0x%llx\n", lposition[line], lsize[line], (lstatus[line] & STATUS_MASK), ((lstatus[line] & INFO_MASK) >> 8), (lstatus[line] >> 32));
    }
    strncat(lines, tempchar, sizeof(lines) - strlen(lines) - 1);
  }
  // check if the chunk fits in the block and if not then get next block
  long long block_end = lposition[line] + lsize[line];
  new_size -= lposition[line] - position;
  int chunk_count = maxcount;
  while (chunk_count > 0)
  {
    // fprintf (stdout, "2 pos=%lld siz=%lld nsiz=%lld lin=%d lpos=%lld lsiz=%lld\n", position, size, new_size, line, lposition[line], lsize[line]);    //debug
    chunk_count--;
    if (lposition[line] + new_size > block_end || lposition[line] > block_end)
    {
      new_size -= lsize[line];
      line++;
      if (line > total_lines - 1)
      {
        break;
      }
      block_end = lposition[line] + lsize[line];
      // fprintf (stdout, "3 pos=%lld siz=%lld nsiz=%lld lin=%d lpos=%lld lsiz=%lld end=%lld\n", position, size, new_size, line, lposition[line], lsize[line], block_end);    //debug
      ret = process_information(position, size, line);
      if (ret > 0)
      {
        nontried = nontried + temp_nontried;
        nontrimmed = nontrimmed + temp_nontrimmed;
        nondivided = nondivided + temp_nondivided;
        nonscraped = nonscraped + temp_nonscraped;
        bad = bad + temp_bad;
        finished = finished + temp_finished;
        if (temp_hightime > hightime)
        {
          hightime = temp_hightime;
        }
        if (ddrescue_log)
        {
          snprintf(tempchar, sizeof(tempchar), "0x%06llx \t0x%06llx \t%c\n", lposition[line], lsize[line], ddrstatus);
        }
        else
        {
          snprintf(tempchar, sizeof(tempchar), "0x%06llx \t0x%06llx \t0x%llx \t0x%llx \t0x%llx\n", lposition[line], lsize[line], (lstatus[line] & STATUS_MASK), ((lstatus[line] & INFO_MASK) >> 8), (lstatus[line] >> 32));
        }
        strncat(lines, tempchar, sizeof(lines) - strlen(lines) - 1);
      }
    }
    else
    {
      break;
    }
    // break;
  }

  if (chunk_count == 0)
  {
    fprintf(stdout, "block line count exceeded\n");
  }
  long long total = nontried + nontrimmed + nondivided + nonscraped + bad + finished;
  if (total != size)
  {
    fprintf(stdout, "block sum %lld does not equal block total %lld\n", total, size);
  }

  char label[8092 + (maxcount * sizeof(tempchar))];
  label[0] = '\0';
  snprintf(tempchar, sizeof(tempchar), "0x%llx - 0x%llx  (%lld-%lld)\n", position, (position + size) - 1, position, (position + size) - 1);
  strncat(label, tempchar, sizeof(label) - strlen(label) - 1);
  snprintf(tempchar, sizeof(tempchar), "%s: %lld,   ", _("Finished"), finished);
  strncat(label, tempchar, sizeof(label) - strlen(label) - 1);
  snprintf(tempchar, sizeof(tempchar), "%s: %lld,\n", _("Bad"), bad);
  strncat(label, tempchar, sizeof(label) - strlen(label) - 1);
  snprintf(tempchar, sizeof(tempchar), "%s: %lld,   ", _("Non-Tried"), nontried);
  strncat(label, tempchar, sizeof(label) - strlen(label) - 1);
  snprintf(tempchar, sizeof(tempchar), "%s: %lld,\n", _("Non-Trimmed"), nontrimmed);
  strncat(label, tempchar, sizeof(label) - strlen(label) - 1);
  snprintf(tempchar, sizeof(tempchar), "%s: %lld,   ", _("Non-Divided"), nondivided);
  strncat(label, tempchar, sizeof(label) - strlen(label) - 1);
  snprintf(tempchar, sizeof(tempchar), "%s: %lld,\n", _("Non-Scraped"), nonscraped);
  strncat(label, tempchar, sizeof(label) - strlen(label) - 1);
  snprintf(tempchar, sizeof(tempchar), "%s: %d,   ", _("Areas"), maxcount - chunk_count);
  strncat(label, tempchar, sizeof(label) - strlen(label) - 1);
  snprintf(tempchar, sizeof(tempchar), "%s: %lld\n", _("Timing"), hightime);
  strncat(label, tempchar, sizeof(label) - strlen(label) - 1);
  strncat(label, lines, sizeof(label) - strlen(label) - 1);
  // fprintf (stdout, "%s", label);
  // fprintf (stdout, "count=%d\n", maxcount - chunk_count);
  gtk_label_set_text(GTK_LABEL(block_information_label), label);

  return 0;
}

int process_domain(long long position, int size, int status, int status_mask)
{
  if (show_domain)
  {
    int first = 0;
    int last = domain_lines - 1;
    int middle = (first + last) / 2;
    while (first <= last)
    {
      // fprintf (stdout, "position=0x%llx size=0x%x dposition=0x%llx dsize=0x%llx\n", position, size, dposition[middle], dsize[middle]);    //debug
      // fprintf (stdout, "first=%d middle=%d end=%d\n", first, middle, last);    //debug
      //  check if any part of the chunk is in the domain
      if (((position >= dposition[middle] && position < dposition[middle] + dsize[middle]) || (dposition[middle] >= position && dposition[middle] < position + size)) && ((dstatus[middle] & status_mask) == status))
      {
        // found a matching block
        return 1;
      }
      else if (position > dposition[middle])
      {
        first = middle + 1;
      }
      else
      {
        last = middle - 1;
      }
      middle = (first + last) / 2;
    }
  }
  return 0;
}

int process_status(int line)
{
  int status = 0;
  int mask = 0x7f;
  if ((lstatus[line] & mask) == (NONTRIED & mask))
  {
    status = NONTRIED_BIT;
  }
  else if ((lstatus[line] & mask) == (NONTRIMMED & mask))
  {
    status = NONTRIMMED_BIT;
  }
  else if ((lstatus[line] & mask) == (NONDIVIDED & mask))
  {
    status = NONDIVIDED_BIT;
  }
  else if ((lstatus[line] & mask) == (NONSCRAPED & mask))
  {
    status = NONSCRAPED_BIT;
  }
  else if ((lstatus[line] & mask) == (BAD & mask))
  {
    status = BAD_BIT;
  }
  else if ((lstatus[line] & mask) == (FINISHED & mask))
  {
    status = FINISHED_BIT;
  }
  else
  {
    status = UNKNOWN_BIT;
  }
  if (lstatus[line] & BAD_HEAD)
  {
    status = status | BAD_HEAD_BIT;
  }
  return status;
}

long long process_information(long long position, long long size, int line)
{
  temp_nontried = 0;
  temp_nontrimmed = 0;
  temp_nondivided = 0;
  temp_nonscraped = 0;
  temp_bad = 0;
  temp_finished = 0;
  temp_hightime = 0;
  long long start = position;
  if (lposition[line] > position)
  {
    start = lposition[line];
  }
  long long end = (position + size);
  if ((lposition[line] + lsize[line]) < end)
  {
    end = (lposition[line] + lsize[line]);
  }
  long long total = end - start;
  if (total < 0)
  {
    total = 0;
  }
  ddrstatus = ' ';
  int mask = 0x7f;
  if ((lstatus[line] & mask) == (NONTRIED & mask))
  {
    temp_nontried = total;
    ddrstatus = '?';
    // fprintf (stdout, "temp_nontried=%lld\n", total);    // debug
  }
  else if ((lstatus[line] & mask) == (NONTRIMMED & mask))
  {
    temp_nontrimmed = total;
    ddrstatus = '*';
    // fprintf (stdout, "temp_nontrimmed=%lld\n", total);    // debug
  }
  else if ((lstatus[line] & mask) == (NONDIVIDED & mask))
  {
    temp_nondivided = total;
    ddrstatus = '*';
    // fprintf (stdout, "temp_nondivided=%lld\n", total);    // debug
  }
  else if ((lstatus[line] & mask) == (NONSCRAPED & mask))
  {
    temp_nonscraped = total;
    ddrstatus = '/';
    // fprintf (stdout, "temp_nonscraped=%lld\n", total);    // debug
  }
  else if ((lstatus[line] & mask) == (BAD & mask))
  {
    temp_bad = total;
    ddrstatus = '-';
    // fprintf (stdout, "temp_bad=%lld\n", total);    // debug
  }
  else if ((lstatus[line] & mask) == (FINISHED & mask))
  {
    temp_finished = total;
    ddrstatus = '+';
    // fprintf (stdout, "temp_finished=%lld\n", total);    // debug
  }
  temp_hightime = (lstatus[line] >> 32) & 0xff;
  return total;
}

int find_block(long long position)
{
  int first = 0;
  int last = total_lines - 1;
  int middle = (first + last) / 2;
  while (first <= last)
  {
    if (position >= lposition[middle] && position < lposition[middle] + lsize[middle])
    {
      return (middle);
    }
    else if (position > lposition[middle])
    {
      first = middle + 1;
    }
    else
    {
      last = middle - 1;
    }
    middle = (first + last) / 2;
  }
  return (-1);
}

int find_next_block(int line, long long status, long long mask)
{
  int i;
  for (i = line + 1; i < total_lines; i++)
  {
    if ((lstatus[i] & mask) == (status & mask))
    {
      return i;
    }
  }
  return (-1);
}

int message_exit(char *message)
{
  fprintf(stdout, "%s", message);
  strncat(exit_message, message, EXIT_MESSAGE_SIZE - strlen(exit_message) - 1);
  return 0;
}

int message_display(char *message)
{
  fprintf(stdout, "%s", message);
  strncat(display_message, message, DISPLAY_MESSAGE_SIZE - strlen(display_message) - 1);
  return 0;
}

int message_now(char *message)
{
  fprintf(stdout, "%s", message);
  return 0;
}

int message_debug(char *message, int level)
{
  if (debug > 0 && debug > level)
  {
    fprintf(stdout, "%s", message);
  }
  return 0;
}

int message_error(char *message)
{
  if (debug > 0)
  {
    fprintf(stdout, "%s", message);
  }
  strncat(error_message, message, ERROR_MESSAGE_SIZE - strlen(error_message) - 1);
  return 0;
}

int clear_error_message(void)
{
  error_message[0] = '\0';
  return 0;
}

void about(void)
{
  GtkWidget *dialog = gtk_about_dialog_new();
  char temp[1024];

  gtk_window_set_title(GTK_WINDOW(dialog), _("About OSCViewer"));

  gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dialog), title);

  snprintf(temp, sizeof(temp), "%s", OSC_VERSION); // Removed GIT_REVISION
  gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), temp);

  snprintf(temp, sizeof(temp), _("Copyright (C) %i Scott Dwyer and OpenSuperClone contributors"), COPYRIGHT_YEAR);
  gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(dialog), temp);

  snprintf(temp, sizeof(temp), _("License type: GPL2\nThere is NO WARRANTY, to the extent permitted by law."));
  gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog), temp);

  gtk_about_dialog_set_website_label(GTK_ABOUT_DIALOG(dialog), OSC_HOMEPAGE);
  gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(dialog), OSC_HOMEPAGE);

  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}

// function to display help
void help(void)
{
  fprintf(stdout, "\n");
  fprintf(stdout, "Usage: oscviewer [options]\n");
  fprintf(stdout, "\n");
  fprintf(stdout, "OPTIONS:\n");
  fprintf(stdout, " -v, --version              Show version and exit\n");
  fprintf(stdout, " -h, --help                 Show this help and exit\n");
  fprintf(stdout, " -l, --logfile <file>       Specify log file to open\n");
  fprintf(stdout, " -f, --domainfile <file>    Specify domain file to open\n");
  fprintf(stdout, "\n");
}

// function to display version
void version(void)
{
  fprintf(stdout, "%s %s\n", title, OSC_VERSION); // Removed GIT_REVISION
  fprintf(stdout, "Copyright (C) %d Scott Dwyer and OpenSuperClone contributors.\n", COPYRIGHT_YEAR);
  fprintf(stdout, "License type: GPL2\n");
  fprintf(stdout, "There is NO WARRANTY, to the extent permitted by law.\n");
}

int print_gui_error_message(char *message, char *title, int type)
{
  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  GtkWidget *dialog;
  GtkDialogFlags message_type;
  if (type)
  {
    message_type = (GtkDialogFlags)GTK_MESSAGE_WARNING;
  }
  else
  {
    message_type = (GtkDialogFlags)GTK_MESSAGE_INFO;
  }
  dialog = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_DESTROY_WITH_PARENT, (GtkMessageType)message_type, GTK_BUTTONS_OK, "%s", message);
  gtk_window_set_keep_above(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_title(GTK_WINDOW(dialog), title);
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
  gtk_widget_destroy(window);

  return 0;
}

void show_settings_window(void)
{
  gtk_widget_show_all(settings_window);

  GdkRGBA good_color_rgba;
  good_color_rgba.red = ((good_color >> 16) & 0xFF) / 255.0;
  good_color_rgba.green = ((good_color >> 8) & 0xFF) / 255.0;
  good_color_rgba.blue = (good_color & 0xFF) / 255.0;
  good_color_rgba.alpha = 1.0;
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(good_color_button), &good_color_rgba);

  GdkRGBA bad_color_rgba;
  bad_color_rgba.red = ((bad_color >> 16) & 0xFF) / 255.0;
  bad_color_rgba.green = ((bad_color >> 8) & 0xFF) / 255.0;
  bad_color_rgba.blue = (bad_color & 0xFF) / 255.0;
  bad_color_rgba.alpha = 1.0;
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(bad_color_button), &bad_color_rgba);

  GdkRGBA nontried_color_rgba;
  nontried_color_rgba.red = ((nontried_color >> 16) & 0xFF) / 255.0;
  nontried_color_rgba.green = ((nontried_color >> 8) & 0xFF) / 255.0;
  nontried_color_rgba.blue = (nontried_color & 0xFF) / 255.0;
  nontried_color_rgba.alpha = 1.0;
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(nontried_color_button), &nontried_color_rgba);

  GdkRGBA nontrimmed_color_rgba;
  nontrimmed_color_rgba.red = ((nontrimmed_color >> 16) & 0xFF) / 255.0;
  nontrimmed_color_rgba.green = ((nontrimmed_color >> 8) & 0xFF) / 255.0;
  nontrimmed_color_rgba.blue = (nontrimmed_color & 0xFF) / 255.0;
  nontrimmed_color_rgba.alpha = 1.0;
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(nontrimmed_color_button), &nontrimmed_color_rgba);

  GdkRGBA nondivided_color_rgba;
  nondivided_color_rgba.red = ((nondivided_color >> 16) & 0xFF) / 255.0;
  nondivided_color_rgba.green = ((nondivided_color >> 8) & 0xFF) / 255.0;
  nondivided_color_rgba.blue = (nondivided_color & 0xFF) / 255.0;
  nondivided_color_rgba.alpha = 1.0;
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(nondivided_color_button), &nondivided_color_rgba);

  GdkRGBA nonscraped_color_rgba;
  nonscraped_color_rgba.red = ((nonscraped_color >> 16) & 0xFF) / 255.0;
  nonscraped_color_rgba.green = ((nonscraped_color >> 8) & 0xFF) / 255.0;
  nonscraped_color_rgba.blue = (nonscraped_color & 0xFF) / 255.0;
  nonscraped_color_rgba.alpha = 1.0;
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(nonscraped_color_button), &nonscraped_color_rgba);

  GdkRGBA domain_color_rgba;
  domain_color_rgba.red = ((domain_color >> 16) & 0xFF) / 255.0;
  domain_color_rgba.green = ((domain_color >> 8) & 0xFF) / 255.0;
  domain_color_rgba.blue = (domain_color & 0xFF) / 255.0;
  domain_color_rgba.alpha = 1.0;
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(domain_color_button), &domain_color_rgba);

  GdkRGBA time_color_rgba;
  time_color_rgba.red = ((time_color >> 16) & 0xFF) / 255.0;
  time_color_rgba.green = ((time_color >> 8) & 0xFF) / 255.0;
  time_color_rgba.blue = (time_color & 0xFF) / 255.0;
  time_color_rgba.alpha = 1.0;
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(time_color_button), &time_color_rgba);

  GdkRGBA bad_head_color_rgba;
  bad_head_color_rgba.red = ((bad_head_color >> 16) & 0xFF) / 255.0;
  bad_head_color_rgba.green = ((bad_head_color >> 8) & 0xFF) / 255.0;
  bad_head_color_rgba.blue = (bad_head_color & 0xFF) / 255.0;
  bad_head_color_rgba.alpha = 1.0;
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(bad_head_color_button), &bad_head_color_rgba);

  GdkRGBA current_color_rgba;
  current_color_rgba.red = ((current_color >> 16) & 0xFF) / 255.0;
  current_color_rgba.green = ((current_color >> 8) & 0xFF) / 255.0;
  current_color_rgba.blue = (current_color & 0xFF) / 255.0;
  current_color_rgba.alpha = 1.0;
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(current_color_button), &current_color_rgba);

  GdkRGBA selected_color_rgba;
  selected_color_rgba.red = ((selected_color >> 16) & 0xFF) / 255.0;
  selected_color_rgba.green = ((selected_color >> 8) & 0xFF) / 255.0;
  selected_color_rgba.blue = (selected_color & 0xFF) / 255.0;
  selected_color_rgba.alpha = 1.0;
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(selected_color_button), &selected_color_rgba);
}

void set_color(GtkWidget *widget, gpointer data)
{
  GdkRGBA color;
  gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(widget), &color);
  int color_int = ((int)(color.red * 255) << 16) + ((int)(color.green * 255) << 8) + (int)(color.blue * 255);
  if (data == (gpointer)1)
  {
    good_color = color_int;
  }
  else if (data == (gpointer)2)
  {
    bad_color = color_int;
  }
  else if (data == (gpointer)3)
  {
    nontried_color = color_int;
  }
  else if (data == (gpointer)4)
  {
    nontrimmed_color = color_int;
  }
  else if (data == (gpointer)5)
  {
    nondivided_color = color_int;
  }
  else if (data == (gpointer)6)
  {
    nonscraped_color = color_int;
  }
  else if (data == (gpointer)7)
  {
    domain_color = color_int;
  }
  else if (data == (gpointer)8)
  {
    time_color = color_int;
  }
  else if (data == (gpointer)9)
  {
    bad_head_color = color_int;
  }
  else if (data == (gpointer)10)
  {
    current_color = color_int;
  }
  else if (data == (gpointer)11)
  {
    selected_color = color_int;
  }

  write_config_file();
}

void read_config_file(void)
{
  char config_file_name[256];
  snprintf(config_file_name, sizeof(config_file_name), "%s/.oscviewer.cfg", getenv("HOME"));

  FILE *config_file = fopen(config_file_name, "r");
  if (config_file == NULL)
  {
    // no config file, use defaults
    return;
  }

  config_t config;
  config_init(&config);

  if (config_read(&config, config_file) == CONFIG_FALSE)
  {
    fprintf(stderr, "Error reading config file: %s", config_error_text(&config));
    config_destroy(&config);
    fclose(config_file);
    return;
  }

  config_setting_t *root;
  config_setting_t *setting;
  config_setting_t *group;

  root = config_root_setting(&config);

  // color settings
  group = config_setting_get_member(root, "colors");
  if (group != NULL)
  {

    setting = config_setting_get_member(group, "good_color");
    if (setting != NULL)
    {
      good_color = config_setting_get_int(setting);
    }

    setting = config_setting_get_member(group, "bad_color");
    if (setting != NULL)
    {
      bad_color = config_setting_get_int(setting);
    }

    setting = config_setting_get_member(group, "nontried_color");
    if (setting != NULL)
    {
      nontried_color = config_setting_get_int(setting);
    }

    setting = config_setting_get_member(group, "nontrimmed_color");
    if (setting != NULL)
    {
      nontrimmed_color = config_setting_get_int(setting);
    }

    setting = config_setting_get_member(group, "nondivided_color");
    if (setting != NULL)
    {
      nondivided_color = config_setting_get_int(setting);
    }

    setting = config_setting_get_member(group, "nonscraped_color");
    if (setting != NULL)
    {
      nonscraped_color = config_setting_get_int(setting);
    }

    setting = config_setting_get_member(group, "domain_color");
    if (setting != NULL)
    {
      domain_color = config_setting_get_int(setting);
    }

    setting = config_setting_get_member(group, "time_color");
    if (setting != NULL)
    {
      time_color = config_setting_get_int(setting);
    }

    setting = config_setting_get_member(group, "bad_head_color");
    if (setting != NULL)
    {
      bad_head_color = config_setting_get_int(setting);
    }

    setting = config_setting_get_member(group, "current_color");
    if (setting != NULL)
    {
      current_color = config_setting_get_int(setting);
    }

    setting = config_setting_get_member(group, "selected_color");
    if (setting != NULL)
    {
      selected_color = config_setting_get_int(setting);
    }
  }

  config_destroy(&config);

  fclose(config_file);
}

void write_config_file(void)
{
  char config_file_name[256];
  snprintf(config_file_name, sizeof(config_file_name), "%s/.oscviewer.cfg", getenv("HOME"));

  FILE *config_file = fopen(config_file_name, "w");
  if (config_file == NULL)
  {
    fprintf(stderr, "Error opening config file for writing: %s", strerror(errno));
    return;
  }

  config_t config;
  config_init(&config);

  config_setting_t *root;
  config_setting_t *setting;
  config_setting_t *group;

  root = config_root_setting(&config);

  // color settings
  group = config_setting_add(root, "colors", CONFIG_TYPE_GROUP);

  setting = config_setting_add(group, "good_color", CONFIG_TYPE_INT);
  config_setting_set_format(setting, CONFIG_FORMAT_HEX);
  config_setting_set_int(setting, good_color);

  setting = config_setting_add(group, "bad_color", CONFIG_TYPE_INT);
  config_setting_set_format(setting, CONFIG_FORMAT_HEX);
  config_setting_set_int(setting, bad_color);

  setting = config_setting_add(group, "nontried_color", CONFIG_TYPE_INT);
  config_setting_set_format(setting, CONFIG_FORMAT_HEX);
  config_setting_set_int(setting, nontried_color);

  setting = config_setting_add(group, "nontrimmed_color", CONFIG_TYPE_INT);
  config_setting_set_format(setting, CONFIG_FORMAT_HEX);
  config_setting_set_int(setting, nontrimmed_color);

  setting = config_setting_add(group, "nondivided_color", CONFIG_TYPE_INT);
  config_setting_set_format(setting, CONFIG_FORMAT_HEX);
  config_setting_set_int(setting, nondivided_color);

  setting = config_setting_add(group, "nonscraped_color", CONFIG_TYPE_INT);
  config_setting_set_format(setting, CONFIG_FORMAT_HEX);
  config_setting_set_int(setting, nonscraped_color);

  setting = config_setting_add(group, "domain_color", CONFIG_TYPE_INT);
  config_setting_set_format(setting, CONFIG_FORMAT_HEX);
  config_setting_set_int(setting, domain_color);

  setting = config_setting_add(group, "time_color", CONFIG_TYPE_INT);
  config_setting_set_format(setting, CONFIG_FORMAT_HEX);
  config_setting_set_int(setting, time_color);

  setting = config_setting_add(group, "bad_head_color", CONFIG_TYPE_INT);
  config_setting_set_format(setting, CONFIG_FORMAT_HEX);
  config_setting_set_int(setting, bad_head_color);

  setting = config_setting_add(group, "current_color", CONFIG_TYPE_INT);
  config_setting_set_format(setting, CONFIG_FORMAT_HEX);
  config_setting_set_int(setting, current_color);

  setting = config_setting_add(group, "selected_color", CONFIG_TYPE_INT);
  config_setting_set_format(setting, CONFIG_FORMAT_HEX);
  config_setting_set_int(setting, selected_color);

  config_write(&config, config_file);

  config_destroy(&config);

  fclose(config_file);
}
