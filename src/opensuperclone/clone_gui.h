// Copyright (C) Scott Dwyer and OpenSuperClone contributors.
// You may use/distribute/modify this freely, under the terms of
// the GNU General Public License version 2 or later version.
// This software is distributed WITHOUT ANY WARRANTY.

#ifndef CLONE_GUI_H
#define CLONE_GUI_H

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libconfig.h>

#define TRANSLATETIMERALL 1000000000   // nanosecond delay for every translate call
#define TRANSLATETIMERFAST 10000000000 // nanosecond delay for each normal translate call
#define TRANSLATETIMERSLOW 2000000000  // nanosecond delay for each slow translate call

#define GTKCOLOR_BLACK "#000000"     // (0,0,0)
#define GTKCOLOR_WHITE "#FFFFFF"     // (255,255,255)
#define GTKCOLOR_RED "#FF0000"       // (255,0,0)
#define GTKCOLOR_LIME "#00FF00"      // (0,255,0)
#define GTKCOLOR_BLUE "#0000FF"      // (0,0,255)
#define GTKCOLOR_YELLOW "#FFFF00"    // (255,255,0)
#define GTKCOLOR_CYAN "#00FFFF"      // (0,255,255)
#define GTKCOLOR_MAGENTA "#FF00FF"   // (255,0,255)
#define GTKCOLOR_SILVER "#C0C0C0"    // (192,192,192)
#define GTKCOLOR_LIGHTGRAY "#D3D3D3" // (211, 211, 211)
#define GTKCOLOR_DARKGREY "#A9A9A9"  // (169, 169, 169)
#define GTKCOLOR_GRAY "#808080"      // (128,128,128)
#define GTKCOLOR_MAROON "#800000"    // (128,0,0)
#define GTKCOLOR_OLIVE "#808000"     // (128,128,0)
#define GTKCOLOR_GREEN "#008000"     // (0,128,0)
#define GTKCOLOR_PURPLE "#800080"    // (128,0,128)
#define GTKCOLOR_TEAL "#008080"      // (0,128,128)
#define GTKCOLOR_NAVY "#000080"      // (0,0,128)

#define MAX_BUTTON_LABEL_SIZE 2048

#define BUTTONID_PHASE1 1
#define BUTTONID_PHASE2 2
#define BUTTONID_PHASE3 3
#define BUTTONID_PHASE4 4
#define BUTTONID_DIVIDE 5
#define BUTTONID_DIVIDE2 6
#define BUTTONID_TRIM 7
#define BUTTONID_SCRAPE 8
#define BUTTONID_MARKBAD 9
#define BUTTONID_READBAD 10
#define BUTTONID_REVERSE 11
#define BUTTONID_SKIPFAST 12
#define BUTTONID_NOLOGBACK 13
#define BUTTONID_FORCE 14
#define BUTTONID_OFFSET 15
#define BUTTONID_FORCEDANGEROUS 16
#define BUTTONID_PIOMODE 17
#define BUTTONID_PHASETIMERS 18
#define BUTTONID_ENABLEPROCESSCHUNK 19
#define BUTTONID_ENABLEPHASELOGS 20
#define BUTTONID_ENABLESCSIWRITE 21
#define BUTTONID_ENABLEOUTPUTSECTOR 22
#define BUTTONID_USEPHYSICALSECTORSIZE 23
#define BUTTONID_COLORSTATUSBAR 24
#define BUTTONID_ENABLEREADTWICE 25
#define BUTTONID_POSITION 26
#define BUTTONID_IOSCSIONLY 27
#define BUTTONID_ENABLEREBUILDASSIST 28
#define BUTTONID_REBUILDASSIST 29
#define BUTTONID_ENABLERETRYCONNECTING 30
#define BUTTONID_DONTIDENTIFY 31

extern GtkWidget *language_window_ccc;
extern GtkWidget *main_frame;
extern GtkWidget *bottom_status_bar_ccc;

extern GtkWidget *main_window_ccc;
extern GtkWidget *main_vbox_ccc;
extern GtkWidget *main_hbox_ccc;

// status and error register
extern GtkWidget *status_register_label;
extern GtkWidget *error_register_label;
extern GtkWidget *bsy_status_icon;
extern GtkWidget *drdy_status_icon;
extern GtkWidget *df_status_icon;
extern GtkWidget *dsc_status_icon;
extern GtkWidget *drq_status_icon;
extern GtkWidget *corr_status_icon;
extern GtkWidget *idx_status_icon;
extern GtkWidget *err_status_icon;
extern GtkWidget *bbk_status_icon;
extern GtkWidget *unc_status_icon;
extern GtkWidget *mc_status_icon;
extern GtkWidget *idnf_status_icon;
extern GtkWidget *mcr_status_icon;
extern GtkWidget *abrt_status_icon;
extern GtkWidget *tk0nf_status_icon;
extern GtkWidget *amnf_status_icon;
extern GtkWidget *bsy_status_label;
extern GtkWidget *drdy_status_label;
extern GtkWidget *df_status_label;
extern GtkWidget *dsc_status_label;
extern GtkWidget *drq_status_label;
extern GtkWidget *corr_status_label;
extern GtkWidget *idx_status_label;
extern GtkWidget *err_status_label;
extern GtkWidget *bbk_status_label;
extern GtkWidget *unc_status_label;
extern GtkWidget *mc_status_label;
extern GtkWidget *idnf_status_label;
extern GtkWidget *mcr_status_label;
extern GtkWidget *abrt_status_label;
extern GtkWidget *tk0nf_status_label;
extern GtkWidget *amnf_status_label;

// asclepius status
extern GtkWidget *asclepius_12v_icon;
extern GtkWidget *asclepius_5v_icon;
extern GtkWidget *asclepius_usb_icon;
extern GtkWidget *asclepius_12v_label;
extern GtkWidget *asclepius_5v_label;
extern GtkWidget *asclepius_usb_label;
extern GtkWidget *asclepius_sata_power_label;
extern GtkWidget *asclepius_usb_power_label;

// file menu
extern GtkWidget *topmenubar_ccc;
extern GtkWidget *filemenu_ccc;
extern GtkWidget *filemi_ccc;
extern GtkWidget *quitmi_ccc;
extern GtkWidget *openmi_ccc;
extern GtkWidget *newmi_ccc;
extern GtkWidget *sourcemi_ccc;
extern GtkWidget *destinationmi_ccc;
extern GtkWidget *drivesmi_ccc;
extern GtkWidget *blockmi_ccc;
extern GtkWidget *drivemi_ccc;
extern GtkWidget *imagemi_ccc;
extern GtkWidget *devnullmi_ccc;
extern GtkWidget *filew_ccc;
extern GtkWidget *modemi_ccc;
extern GtkWidget *copyimagemi_ccc;
extern GtkWidget *genericmi_ccc;
extern GtkWidget *pass_automi_ccc;
extern GtkWidget *pass_scsimi_ccc;
extern GtkWidget *pass_atrami_ccc;
extern GtkWidget *directmi_ccc;
extern GtkWidget *ahcimi_ccc;
extern GtkWidget *usbmi_ccc;
extern GtkWidget *usbatami_ccc;
extern GtkWidget *settingsmi_ccc;
extern GtkWidget *advancedmi_ccc;
extern GtkWidget *clonemi_ccc;
extern GtkWidget *savemi_ccc;
extern GtkWidget *saveasmi_ccc;
extern GtkWidget *resetstatusmi_ccc;
extern GtkWidget *toolsmi_ccc;
extern GtkWidget *resetlogmi_ccc;
extern GtkWidget *repairlogmi_ccc;
extern GtkWidget *fillzeromi_ccc;
extern GtkWidget *fillmarkmi_ccc;
extern GtkWidget *fillerasemi_ccc;
extern GtkWidget *domainmi_ccc;
extern GtkWidget *importddmi_ccc;
extern GtkWidget *exportddmi_ccc;
extern GtkWidget *cleardomainmi_ccc;
extern GtkWidget *helpmi_ccc;
extern GtkWidget *aboutmi_ccc;
extern GtkWidget *timermi_ccc;
extern GtkWidget *installdrivermi_ccc;
extern GtkWidget *uninstalldrivermi_ccc;
extern GtkWidget *fixdrivermemorymi_ccc;
extern GtkWidget *driveronlymi_ccc;
extern GtkWidget *adddomainmi_ccc;
extern GtkWidget *newdomainmi_ccc;
extern GtkWidget *savedomainmi_ccc;
extern GtkWidget *savedomainasmi_ccc;
extern GtkWidget *activatewithfilemi_ccc;
extern GtkWidget *removeactivationmi_ccc;
extern GtkWidget *disableportsmi_ccc;
extern GtkWidget *restoreportsmi_ccc;
extern GtkWidget *displayidentifymi_ccc;
extern GtkWidget *displayanalyzemi_ccc;
extern GtkWidget *displaysmartmi_ccc;
extern GtkWidget *primaryrelaymi_ccc;
extern GtkWidget *chooseprimaryrelaymi_ccc;
extern GtkWidget *asclepiusextmi_ccc;
extern GtkWidget *disableusbmassmi_ccc;
extern GtkWidget *restoreusbmassmi_ccc;
extern GtkWidget *loadconfigmi_ccc;
extern GtkWidget *writeconfigmi_ccc;

extern GtkWidget *choose_source_dialog_window_ccc;
extern GtkWidget *choose_source_scrolled_window_ccc;
extern GtkWidget *choose_source_vbox_ccc;

extern GtkWidget *choose_destination_dialog_window_ccc;
extern GtkWidget *choose_destination_scrolled_window_ccc;
extern GtkWidget *choose_destination_vbox_ccc;

extern GtkWidget *choose_usb_dialog_window_ccc;
extern GtkWidget *choose_usb_scrolled_window_ccc;
extern GtkWidget *choose_usb_vbox_ccc;

extern GtkWidget *connect_button_ccc;
extern GtkWidget *start_button_ccc;
extern GtkWidget *stop_button_ccc;
extern GtkWidget *analyze_button_ccc;
extern GtkWidget *analyze_long_button_ccc;
extern GtkWidget *smart_button_ccc;
extern GtkWidget *clone_mode_button_ccc;
extern GtkWidget *agressive_driver_checkbutton_ccc;
extern GtkWidget *driver_mode1_button_ccc;
extern GtkWidget *driver_mode2_button_ccc;
extern GtkWidget *driver_mode3_button_ccc;
extern GtkWidget *driver_mode4_button_ccc;
extern GtkWidget *driver_mode5_button_ccc;
extern GtkWidget *soft_reset_button_ccc;
extern GtkWidget *hard_reset_button_ccc;

extern GtkWidget *phase_control_label_ccc;
extern GtkWidget *phase1_check_button_ccc;
extern GtkWidget *phase2_check_button_ccc;
extern GtkWidget *phase3_check_button_ccc;
extern GtkWidget *phase4_check_button_ccc;
extern GtkWidget *divide_check_button_ccc;
extern GtkWidget *divide2_check_button_ccc;
extern GtkWidget *trim_check_button_ccc;
extern GtkWidget *scrape_check_button_ccc;
extern GtkWidget *markbad_check_button_ccc;
extern GtkWidget *readbad_check_button_ccc;
extern GtkWidget *rebuild_assist_check_button_ccc;
extern GtkWidget *reverse_check_button_ccc;
extern GtkWidget *skipfast_check_button_ccc;
extern GtkWidget *retries_button_label_ccc;
extern GtkWidget *retries_spin_button_ccc;
extern GtkWidget *clustersize_button_label_ccc;
extern GtkWidget *clustersize_spin_button_ccc;
extern GtkWidget *inputoffset_button_label_ccc;
extern GtkWidget *inputoffset_spin_button_ccc;
extern GtkWidget *size_button_label_ccc;
extern GtkWidget *size_spin_button_ccc;
extern GtkWidget *blocksize_button_label_ccc;
extern GtkWidget *blocksize_spin_button_ccc;
extern GtkWidget *skipsize_button_label_ccc;
extern GtkWidget *skipsize_spin_button_ccc;
extern GtkWidget *maxskipsize_button_label_ccc;
extern GtkWidget *maxskipsize_spin_button_ccc;
extern GtkWidget *skipthreshold_button_label_ccc;
extern GtkWidget *skipthreshold_spin_button_ccc;
extern GtkWidget *rateskip_button_label_ccc;
extern GtkWidget *rateskip_spin_button_ccc;
extern GtkWidget *exitonslow_button_label_ccc;
extern GtkWidget *exitonslow_spin_button_ccc;
extern GtkWidget *exitonslowtime_button_label_ccc;
extern GtkWidget *exitonslowtime_spin_button_ccc;
extern GtkWidget *no_log_backup_checkbutton_ccc;
extern GtkWidget *force_mounted_checkbutton_ccc;
extern GtkWidget *force_dangerous_checkbutton_ccc;
extern GtkWidget *enable_output_offset_checkbutton_ccc;
extern GtkWidget *output_offset_spinbutton_ccc;
extern GtkWidget *output_offset_button_label_ccc;
extern GtkWidget *enable_current_position_checkbutton_ccc;
extern GtkWidget *current_position_spinbutton_ccc;
extern GtkWidget *current_position_button_label_ccc;
extern GtkWidget *action_on_error_label_ccc;
extern GtkWidget *stop_on_error_radio_button_ccc;
extern GtkWidget *call_command_radio_button_ccc;
extern GtkWidget *command_to_call_text_ccc;
extern GtkWidget *test_command_button_ccc;
extern GtkWidget *primary_relay_on_error_radio_button_ccc;
extern GtkWidget *write_buffer_disable_radio_button_ccc;
extern GtkWidget *write_buffer_enable_radio_button_ccc;
extern GtkWidget *write_buffer_label_ccc;
extern GtkWidget *timer_settings_label_ccc;
extern GtkWidget *timer_settings_info_label_ccc;
extern GtkWidget *initial_busy_wait_time_button_label_ccc;
extern GtkWidget *initial_busy_wait_time_spinbutton_ccc;
extern GtkWidget *busy_wait_time_button_label_ccc;
extern GtkWidget *busy_wait_time_spinbutton_ccc;
extern GtkWidget *soft_reset_time_button_label_ccc;
extern GtkWidget *soft_reset_time_spinbutton_ccc;
extern GtkWidget *hard_reset_time_button_label_ccc;
extern GtkWidget *hard_reset_time_spinbutton_ccc;
extern GtkWidget *power_cycle_time_button_label_ccc;
extern GtkWidget *power_cycle_time_spinbutton_ccc;
extern GtkWidget *general_timout_button_label_ccc;
extern GtkWidget *general_timeout_spinbutton_ccc;
extern GtkWidget *action_power_cycle_label_ccc;
extern GtkWidget *stop_on_power_cycle_radio_button_ccc;
extern GtkWidget *call_power_command_radio_button_ccc;
extern GtkWidget *power_command_to_call_text_ccc;
extern GtkWidget *primary_relay_power_cycle_radio_button_ccc;
extern GtkWidget *test_power_command_button_ccc;
extern GtkWidget *pio_mode_checkbutton_ccc;
extern GtkWidget *phase_timers_label_ccc;
extern GtkWidget *phase_timers_checkbutton_ccc;
extern GtkWidget *p12_soft_reset_time_button_label_ccc;
extern GtkWidget *p12_soft_reset_time_spinbutton_ccc;
extern GtkWidget *p3_soft_reset_time_button_label_ccc;
extern GtkWidget *p3_soft_reset_time_spinbutton_ccc;
extern GtkWidget *p4_soft_reset_time_button_label_ccc;
extern GtkWidget *p4_soft_reset_time_spinbutton_ccc;
extern GtkWidget *td_soft_reset_time_button_label_ccc;
extern GtkWidget *td_soft_reset_time_spinbutton_ccc;
extern GtkWidget *d2_soft_reset_time_button_label_ccc;
extern GtkWidget *d2_soft_reset_time_spinbutton_ccc;
extern GtkWidget *sc_soft_reset_time_button_label_ccc;
extern GtkWidget *sc_soft_reset_time_spinbutton_ccc;
extern GtkWidget *rt_soft_reset_time_button_label_ccc;
extern GtkWidget *rt_soft_reset_time_spinbutton_ccc;
extern GtkWidget *always_wait_for_reset_timers_checkbutton_ccc;
extern GtkWidget *usb_resets_label_ccc;
extern GtkWidget *enable_usb_bulk_reset_checkbutton_ccc;
extern GtkWidget *enable_usb_soft_reset_checkbutton_ccc;
extern GtkWidget *enable_usb_hard_reset_checkbutton_ccc;
extern GtkWidget *enable_usb_port_reset_checkbutton_ccc;
extern GtkWidget *sectorsize_button_label_ccc;
extern GtkWidget *sectorsize_spinbutton_ccc;
extern GtkWidget *alignment_button_label_ccc;
extern GtkWidget *alignment_spinbutton_ccc;
extern GtkWidget *maxreadrate_button_label_ccc;
extern GtkWidget *maxreadrate_spinbutton_ccc;
extern GtkWidget *enable_rebuild_assist_checkbutton_ccc;
extern GtkWidget *enable_process_chunk_checkbutton_ccc;
extern GtkWidget *enable_read_twice_checkbutton_ccc;
extern GtkWidget *enable_retry_connecting_checkbutton_ccc;
extern GtkWidget *enable_phase_log_checkbutton_ccc;
extern GtkWidget *confirmation_box_label_ccc;
extern GtkWidget *disable_ports_label_ccc;
extern GtkWidget *disable_ports_info_label_ccc;
extern GtkWidget *current_ports_label_ccc;
extern GtkWidget *current_ports_info_label_ccc;
extern GtkWidget *set_disabled_ports_label_ccc;
extern GtkWidget *set_disabled_ports_text_ccc;
extern GtkWidget *updated_ports_label_ccc;
extern GtkWidget *updated_ports_info_label_ccc;
extern GtkWidget *update_ports_button_ccc;
extern GtkWidget *select_lun_label_ccc;
extern GtkWidget *select_lun_info_label_ccc;
extern GtkWidget *lun_values_label_ccc;
extern GtkWidget *lun_values_info_label_ccc;
extern GtkWidget *set_current_lun_label_ccc;
extern GtkWidget *current_lun_spinbutton_ccc;
extern GtkWidget *enable_scsi_write_checkbutton_ccc;
extern GtkWidget *enable_output_sector_size_checkbutton_ccc;
extern GtkWidget *output_sector_size_button_label_ccc;
extern GtkWidget *driver_io_scsi_only_checkbutton_ccc;
extern GtkWidget *use_physical_sector_size_for_virtual_checkbutton_ccc;
extern GtkWidget *output_sector_size_spinbutton_ccc;
extern GtkWidget *driver_error_options_label_ccc;
extern GtkWidget *driver_return_error_radio_button_ccc;
extern GtkWidget *driver_return_zeros_radio_button_ccc;
extern GtkWidget *driver_return_marked_radio_button_ccc;
extern GtkWidget *virtual_disk_device_name_label_ccc;
extern GtkWidget *virtual_disk_device_name_text_ccc;
extern GtkWidget *driver_minimum_cluster_size_button_label_ccc;
extern GtkWidget *driver_minimum_cluster_size_spinbutton_ccc;
extern GtkWidget *enable_extended_analyze_checkbutton_ccc;
extern GtkWidget *primary_relay_settings_label_ccc;
extern GtkWidget *primary_relay_settings_info_label_ccc;
extern GtkWidget *label_current_primary_relay_board_ccc;
extern GtkWidget *data_current_relay_board_a_ccc;
extern GtkWidget *asclepius_relay_source_label_ccc;
extern GtkWidget *asclepius_relay_source_combobox_ccc;
extern GtkWidget *asclepius_tty_device_text_ccc;
extern GtkWidget *asclepius_connect_button_ccc;
extern GtkWidget *asclepius_disconnect_button_ccc;
extern GtkWidget *primary_relay_board_settings_label_ccc;
extern GtkWidget *primary_relay_activation_label_ccc;
extern GtkWidget *activate_primary_relay1_checkbutton_ccc;
extern GtkWidget *activate_primary_relay2_checkbutton_ccc;
extern GtkWidget *activate_primary_relay3_checkbutton_ccc;
extern GtkWidget *activate_primary_relay4_checkbutton_ccc;
extern GtkWidget *activate_primary_relay5_checkbutton_ccc;
extern GtkWidget *activate_primary_relay6_checkbutton_ccc;
extern GtkWidget *activate_primary_relay7_checkbutton_ccc;
extern GtkWidget *activate_primary_relay8_checkbutton_ccc;
extern GtkWidget *primary_relay_deactivation_label_ccc;
extern GtkWidget *deactivate_primary_relay1_checkbutton_ccc;
extern GtkWidget *deactivate_primary_relay2_checkbutton_ccc;
extern GtkWidget *deactivate_primary_relay3_checkbutton_ccc;
extern GtkWidget *deactivate_primary_relay4_checkbutton_ccc;
extern GtkWidget *deactivate_primary_relay5_checkbutton_ccc;
extern GtkWidget *deactivate_primary_relay6_checkbutton_ccc;
extern GtkWidget *deactivate_primary_relay7_checkbutton_ccc;
extern GtkWidget *deactivate_primary_relay8_checkbutton_ccc;
extern GtkWidget *primary_relay_activate_time_label_ccc;
extern GtkWidget *primary_relay_energize_time_spinbutton_ccc;
extern GtkWidget *primary_relay_delay_time_label_ccc;
extern GtkWidget *primary_relay_delay_time_spinbutton_ccc;
extern GtkWidget *test_primary_relay_button_ccc;
extern GtkWidget *activate_primary_relay_button_ccc;
extern GtkWidget *deactivate_primary_relay_button_ccc;
extern GtkWidget *activate_primary_relay_button_main_ccc;
extern GtkWidget *deactivate_primary_relay_button_main_ccc;
extern GtkWidget *disable_identify_checkbutton_ccc;

extern GtkWidget *label_project_ccc;
extern GtkWidget *data_project_ccc;
extern GtkWidget *label_domain_ccc;
extern GtkWidget *data_domain_ccc;
extern GtkWidget *label_source_ccc;
extern GtkWidget *data_source_ccc;
extern GtkWidget *label_destination_ccc;
extern GtkWidget *data_destination_ccc;
extern GtkWidget *label_totallba_ccc;
extern GtkWidget *data_totallba_ccc;
extern GtkWidget *label_lbatoread_ccc;
extern GtkWidget *data_lbatoread_ccc;
extern GtkWidget *label_domainsize_ccc;
extern GtkWidget *data_domainsize_ccc;
extern GtkWidget *label_runtime_ccc;
extern GtkWidget *data_runtime_ccc;
extern GtkWidget *label_remainingtime_ccc;
extern GtkWidget *data_remainingtime_ccc;
extern GtkWidget *label_currentposition_ccc;
extern GtkWidget *data_currentposition_ccc;
extern GtkWidget *label_currentstatus_ccc;
extern GtkWidget *data_currentstatus_ccc;
extern GtkWidget *label_currentrate_ccc;
extern GtkWidget *data_currentrate_ccc;
extern GtkWidget *label_recentrate_ccc;
extern GtkWidget *data_recentrate_ccc;
extern GtkWidget *label_totalrate_ccc;
extern GtkWidget *data_totalrate_ccc;
extern GtkWidget *label_skipsize_ccc;
extern GtkWidget *data_skipsize_ccc;
extern GtkWidget *label_slowskips_ccc;
extern GtkWidget *data_slowskips_ccc;
extern GtkWidget *label_skips_ccc;
extern GtkWidget *data_skips_ccc;
extern GtkWidget *label_skipruns_ccc;
extern GtkWidget *data_skipruns_ccc;
extern GtkWidget *label_skipresets_ccc;
extern GtkWidget *data_skipresets_ccc;
extern GtkWidget *label_runsize_ccc;
extern GtkWidget *data_runsize_ccc;
extern GtkWidget *label_finished_ccc;
extern GtkWidget *data_finished_ccc;
extern GtkWidget *label_nontried_ccc;
extern GtkWidget *data_nontried_ccc;
extern GtkWidget *label_nontrimmed_ccc;
extern GtkWidget *data_nontrimmed_ccc;
extern GtkWidget *label_nondivided_ccc;
extern GtkWidget *data_nondivided_ccc;
extern GtkWidget *label_nonscraped_ccc;
extern GtkWidget *data_nonscraped_ccc;
extern GtkWidget *label_bad_ccc;
extern GtkWidget *data_bad_ccc;
extern GtkWidget *label_datapreview_ccc;
extern GtkWidget *data_datapreview_ccc;
extern GtkWidget *label_longestreadtime_ccc;
extern GtkWidget *data_longestreadtime_ccc;
extern GtkWidget *label_filled_ccc;
extern GtkWidget *data_filled_ccc;
extern GtkWidget *label_retried_ccc;
extern GtkWidget *data_retried_ccc;
extern GtkWidget *label_retrypasses_ccc;
extern GtkWidget *data_retrypasses_ccc;
extern GtkWidget *label_drivermode_ccc;
extern GtkWidget *data_drivermode_ccc;

extern GtkAdjustment *gtk_adjustment_ccc;

extern gint display_status_timer_ccc;
extern int display_status_timer_running_ccc;

extern int source_chosen_ccc;
extern int destination_chosen_ccc;
extern int destination_size_valid_ccc;
extern int project_chosen_ccc;
extern int connected_ccc;
extern int running_ccc;
extern int new_project_ccc;
extern int color_switch_flag_ccc;
extern bool destination_is_generic_ccc;
extern int disabled_ports_changed_ccc;
extern int usbr1_chosen_ccc;

extern bool button_labeled_start;

extern char *status_icon_off_path;
extern char *status_icon_on_path;
extern char *error_icon_on_path;
extern char *status_warn_icon_path;
extern char *smart_info_icon_path;
extern char *smart_warning_icon_path;
extern char *smart_error_icon_path;

extern char *template_directory;
extern char *default_config_filename;
extern char *default_ssd_config_filename;
extern char *default_asclepius_config_filename;

extern int default_window_width;
extern int default_window_height;

void select_file_ccc(void);

void select_ddrescue_file_ccc(void);

void export_ddrescue_file_ccc(void);

void new_file_ccc(void);

void save_file_ccc(void);

void save_file_as_ccc(void);

void save_domain_ccc(void);

void save_domain_as_ccc(void);

static void new_domain_file_ccc(char *log_file);

static void load_log_file_ccc(char *log_file);

static void add_domain_file_ccc(char *log_file);

static void load_domain_file_ccc(char *log_file);

static void new_log_file_ccc(char *log_file);

static void load_ddrescue_log_file_ccc(char *log_file);

void choose_source_ccc(void);

void get_source_from_button_ccc(GtkWidget *widget, gpointer data);

void choose_destination_ccc(void);

void get_destination_from_button_ccc(GtkWidget *widget, gpointer data);

void choose_image_ccc(void);

void choose_null_ccc(void);

void set_connected_ccc(void);

void set_disconnected_ccc(void);

void clear_destination_ccc(void);

void set_stop_signal_ccc(void);

void start_cloning_ccc(void);

void quit_and_close_ccc(void);

void open_clone_settings_dialog_ccc(void);

void open_advanced_settings_dialog_ccc(void);

void open_timer_settings_dialog_ccc(void);

void activate_status_buttons_ccc(void);

void deactivate_status_buttons_ccc(void);

void set_mode_ccc(void);

void update_status_buttons_ccc(void);

void update_asclepius_status(void);

void set_state_from_button_ccc(GtkWidget *widget, gpointer data);

void do_reset_status_ccc(void);

void do_reset_log_status_ccc(void);

void do_repair_log_ccc(void);

void about_ccc(void);

void start_display_status_update_timer_ccc(void);

gint display_status_update_action_ccc(gpointer data);

gint display_status_update_asclepius_action(gpointer data);

void set_driver_mode_button_status_ccc(bool active);

void check_driver_null_condition_ccc(void);

void update_ports_ccc(void);

void start_analyzing_ccc(void);

void display_analyze_results_ccc(void);

void get_smart_data_ccc(void);

void display_smart_data_ccc(void);

void display_identify_data_ccc(void);

void choose_primary_usb_ccc(void);

void get_usb_from_button_ccc(GtkWidget *w, gpointer data);

void clear_usbr1_ccc(void);

void clear_usbd1_ccc(void);

void map_heads_ccc(void);

int set_data_dump_filename_ccc(void);

void get_data_dump_filename_ccc(void);

void analyze_quick_ccc(void);

void analyze_long_ccc(void);

void open_in_oscviewer_ccc(void);

void read_config_file_with_name_ccc(char *filename);

void read_config_file_ccc(void);

void write_config_file_with_name_ccc(char *filename);

void write_config_file_ccc(void);

void check_source_against_templates_ccc(void);

void open_asclepius_ext_terminal_ccc(void);

#endif
