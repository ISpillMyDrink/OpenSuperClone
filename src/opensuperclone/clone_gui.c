// Copyright (C) Scott Dwyer and OpenSuperClone contributors.
// You may use/distribute/modify this freely, under the terms of
// the GNU General Public License version 2 or later version.
// This software is distributed WITHOUT ANY WARRANTY.

#include "config.h"
#include "clone_gui_common.h"
#include "clone_gui.h"
#include "common.h"
#include "opensuperclone_glade.h"
#include "asclepius.h"
#include "wd_royl.h"
#include <ctype.h>
#include <glib/gstdio.h>

extern int process_lines_ccc(void);
#include <libconfig.h>

// Definitions for clone_gui_common.h globals
char *title_ccc = NULL;
char *version_number_ccc = NULL;
char *copyright_year_ccc = NULL;
char *log_file_ccc = NULL;
char *ddilog_file_ccc = NULL;
char *ddelog_file_ccc = NULL;
char *domain_file_ccc = NULL;
int check_log_size_ccc = 0;
bool do_domain_ccc = false;
bool create_domain_ccc = false;
bool domain_dd_ccc = false;
bool importdd_ccc = false;
char level4_middle_text_ccc[4096] = "";
int running_clone_ccc = 0;
char command_to_call_ccc[MAX_CALL_LENGTH] = "";
char power_command_to_call_ccc[MAX_CALL_LENGTH] = "";
bool stop_on_error_ccc = false;
bool do_call_command_ccc = false;
bool activate_primary_relay_on_error_ccc = false;
bool stop_on_power_cycle_ccc = false;
bool call_power_command_ccc = false;
bool power_cycle_primary_relay_ccc = false;
bool check_message_ccc = false;
bool write_buffer_disable_ccc = false;
bool write_buffer_enable_ccc = false;
bool driver_return_error_ccc = false;
bool driver_return_zeros_ccc = false;
bool driver_return_marked_ccc = false;
int memory_used_ccc = 0;
bool data_read_from_log_ccc = false;
int driver_mode_ccc = 0;
int driver_running_ccc = 0;
char virtual_driver_name_ccc[MAX_DEVICE_NAME_LENGTH] = "";
long long last_small_read_size_ccc = 0;
long long last_big_read_size_ccc = 0;
long long last_actual_read_size_ccc = 0;
long long driver_read_rate_size_ccc = 0;
bool no_io_error_ccc = false;
char new_ports_ccc[256] = "";
int running_analyze_ccc = 0;
long long analyze_stoptime_ccc = 0;
char analyze_text_ccc[MAX_ANALYZE_TEXT_LENGTH] = "";
struct analyze_data_t analyze_data_ccc;
int analyze_read_attempts_ccc[MAXANALYZESECTIONS];
int analyze_good_reads_ccc[MAXANALYZESECTIONS];
int analyze_bad_reads_ccc[MAXANALYZESECTIONS];
int analyze_slow_reads_ccc[MAXANALYZESECTIONS];
int analyze_timeouts_ccc[MAXANALYZESECTIONS];
long long analyze_read_time_ccc[MAXANALYZESECTIONS];
long long analyze_low_time_ccc[MAXANALYZESECTIONS];
long long analyze_high_time_ccc[MAXANALYZESECTIONS];
bool enable_extended_analyze_ccc = false;
long long analyze_slow_position_ccc[MAXANALYZESLOW];
long long analyze_slow_size_ccc[MAXANALYZESLOW];
long long analyze_slow_low_ccc[MAXANALYZESLOW];
long long analyze_slow_high_ccc[MAXANALYZESLOW];
long long analyze_slow_section_ccc[MAXANALYZESLOW];
int analyze_slow_total_reads_ccc = 0;
long long skip_timeout_copy_ccc = 0;
int cluster_size_copy_ccc = 0;
char smart_data_text_ccc[MAX_SMART_DATA_TEXT_LENGTH] = "";
struct smart_data_t smart_data_ccc;
bool aggressive_driver_ccc = false;
bool color_statusbar_ccc = false;
char primary_relay_name_ccc[MAX_RELAY_NAME_LENGTH] = "";
unsigned long long primary_relay_activation_time_ccc = 0;
unsigned long long primary_relay_delay_time_ccc = 0;
int primary_relay_board_type_ccc = PRIMARY_RELAY_BOARD_GENERIC;

int primary_relay_board_type_valid(int board_type)
{
  if (board_type != PRIMARY_RELAY_BOARD_GENERIC && board_type != PRIMARY_RELAY_BOARD_ASCLEPIUS)
  {
    return PRIMARY_RELAY_BOARD_GENERIC;
  }

  return board_type;
}

// True when the board the user selected is actually reachable, so the relay
// controls can say so instead of silently doing nothing.
bool primary_relay_board_is_connected_ccc(int board_type)
{
  if (primary_relay_board_type_valid(board_type) == PRIMARY_RELAY_BOARD_ASCLEPIUS)
  {
    return asclepius_get_connection_status();
  }

  return usbr1_chosen_ccc ? true : false;
}

// True when a relay operation should be sent to Asclepius, i.e. the user has
// Asclepius selected as the relay board.
//
// Dispatching on the selection rather than on "is Asclepius connected" keeps
// the choice visible: when Asclepius is selected but not connected the command
// below fails and reports an error, rather than quietly driving the other
// board instead.
static bool relay_controls_use_asclepius_ccc(void)
{
  return primary_relay_board_type_ccc == PRIMARY_RELAY_BOARD_ASCLEPIUS;
}

// Record a successful Asclepius connection as the selected primary relay board.
// Both the startup auto-connect and the relay dialog's Connect button call
// asclepius_connect() directly, so this bookkeeping cannot live in just one of
// them: without it an auto-connected board leaves the saved board type in place
// and the relay controls keep driving the other board.
static void select_connected_asclepius_ccc(void)
{
  // Name the board like a generic USB relay so the settings window shows
  // which board the relay controls are actually driving.
  snprintf(primary_relay_settings_ccc.primary_relay_name, MAX_RELAY_NAME_LENGTH, "%s", _("Asclepius"));
  strcpy(primary_relay_name_ccc, primary_relay_settings_ccc.primary_relay_name);

  // The port that just answered is authoritative, so keep the settings buffer
  // in step before applying it; an empty saved device must not blank the one
  // the auto-connect actually opened.
  snprintf(primary_relay_settings_ccc.asclepius_tty_device, sizeof(primary_relay_settings_ccc.asclepius_tty_device), "%s", asclepius_tty_device_ccc);

  // Attaching a board selects it, so a freshly connected Asclepius takes over
  // the relay controls exactly as it did before the choice became explicit.
  primary_relay_board_type_ccc = PRIMARY_RELAY_BOARD_ASCLEPIUS;
  primary_relay_settings_ccc.primary_relay_board_type = PRIMARY_RELAY_BOARD_ASCLEPIUS;
  update_primary_relay_settings_ccc();

  gtk_widget_set_sensitive(GTK_WIDGET(activate_primary_relay_button_main_ccc), TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(deactivate_primary_relay_button_main_ccc), TRUE);
}

// asclepius_tty_device_ccc, asclepius_ext_tty_device_ccc, asclepius_ext_baud_rate_ccc
// are defined in asclepius.c
bool rebuild_assist_enabled_ccc = false;
bool use_fpdma_ccc = false;
bool wait_for_ds_bit_ccc = false;
char identify_device_raw_text_ccc[65536] = "";
struct identify_device_data_t identify_device_data_ccc;
struct clone_settings_t clone_settings_ccc;
struct advanced_settings_t advanced_settings_ccc;
struct timer_settings_t timer_settings_ccc;
struct primary_relay_settings_t primary_relay_settings_ccc;
struct display_output_t display_output_ccc;

// Definitions for clone_gui.h globals
GtkWidget *language_window_ccc = NULL;
GtkWidget *main_frame = NULL;
GtkWidget *bottom_status_bar_ccc = NULL;
GtkWidget *main_window_ccc = NULL;
GtkWidget *main_vbox_ccc = NULL;
GtkWidget *main_hbox_ccc = NULL;
GtkWidget *status_register_label = NULL;
GtkWidget *error_register_label = NULL;
GtkWidget *bsy_status_icon = NULL;
GtkWidget *drdy_status_icon = NULL;
GtkWidget *df_status_icon = NULL;
GtkWidget *dsc_status_icon = NULL;
GtkWidget *drq_status_icon = NULL;
GtkWidget *corr_status_icon = NULL;
GtkWidget *idx_status_icon = NULL;
GtkWidget *err_status_icon = NULL;
GtkWidget *bbk_status_icon = NULL;
GtkWidget *unc_status_icon = NULL;
GtkWidget *mc_status_icon = NULL;
GtkWidget *idnf_status_icon = NULL;
GtkWidget *mcr_status_icon = NULL;
GtkWidget *abrt_status_icon = NULL;
GtkWidget *tk0nf_status_icon = NULL;
GtkWidget *amnf_status_icon = NULL;
GtkWidget *bsy_status_label = NULL;
GtkWidget *drdy_status_label = NULL;
GtkWidget *df_status_label = NULL;
GtkWidget *dsc_status_label = NULL;
GtkWidget *drq_status_label = NULL;
GtkWidget *corr_status_label = NULL;
GtkWidget *idx_status_label = NULL;
GtkWidget *err_status_label = NULL;
GtkWidget *bbk_status_label = NULL;
GtkWidget *unc_status_label = NULL;
GtkWidget *mc_status_label = NULL;
GtkWidget *idnf_status_label = NULL;
GtkWidget *mcr_status_label = NULL;
GtkWidget *abrt_status_label = NULL;
GtkWidget *tk0nf_status_label = NULL;
GtkWidget *amnf_status_label = NULL;
GtkWidget *asclepius_12v_icon = NULL;
GtkWidget *asclepius_5v_icon = NULL;
GtkWidget *asclepius_usb_icon = NULL;
GtkWidget *asclepius_12v_label = NULL;
GtkWidget *asclepius_5v_label = NULL;
GtkWidget *asclepius_usb_label = NULL;
GtkWidget *asclepius_sata_power_label = NULL;
GtkWidget *asclepius_usb_power_label = NULL;
GtkWidget *topmenubar_ccc = NULL;
GtkWidget *filemenu_ccc = NULL;
GtkWidget *filemi_ccc = NULL;
GtkWidget *quitmi_ccc = NULL;
GtkWidget *openmi_ccc = NULL;
GtkWidget *newmi_ccc = NULL;
GtkWidget *sourcemi_ccc = NULL;
GtkWidget *destinationmi_ccc = NULL;
GtkWidget *drivesmi_ccc = NULL;
GtkWidget *blockmi_ccc = NULL;
GtkWidget *drivemi_ccc = NULL;
GtkWidget *imagemi_ccc = NULL;
GtkWidget *devnullmi_ccc = NULL;
GtkWidget *filew_ccc = NULL;
GtkWidget *modemi_ccc = NULL;
GtkWidget *copyimagemi_ccc = NULL;
GtkWidget *genericmi_ccc = NULL;
GtkWidget *pass_automi_ccc = NULL;
GtkWidget *pass_scsimi_ccc = NULL;
GtkWidget *pass_atrami_ccc = NULL;
GtkWidget *directmi_ccc = NULL;
GtkWidget *ahcimi_ccc = NULL;
GtkWidget *usbmi_ccc = NULL;
GtkWidget *usbatami_ccc = NULL;
GtkWidget *settingsmi_ccc = NULL;
GtkWidget *advancedmi_ccc = NULL;
GtkWidget *clonemi_ccc = NULL;
GtkWidget *savemi_ccc = NULL;
GtkWidget *saveasmi_ccc = NULL;
GtkWidget *resetstatusmi_ccc = NULL;
GtkWidget *toolsmi_ccc = NULL;
GtkWidget *resetlogmi_ccc = NULL;
GtkWidget *repairlogmi_ccc = NULL;
GtkWidget *fillzeromi_ccc = NULL;
GtkWidget *fillmarkmi_ccc = NULL;
GtkWidget *fillerasemi_ccc = NULL;
GtkWidget *domainmi_ccc = NULL;
GtkWidget *importddmi_ccc = NULL;
GtkWidget *exportddmi_ccc = NULL;
GtkWidget *cleardomainmi_ccc = NULL;
GtkWidget *helpmi_ccc = NULL;
GtkWidget *aboutmi_ccc = NULL;
GtkWidget *timermi_ccc = NULL;
GtkWidget *installdrivermi_ccc = NULL;
GtkWidget *uninstalldrivermi_ccc = NULL;
GtkWidget *fixdrivermemorymi_ccc = NULL;
GtkWidget *driveronlymi_ccc = NULL;
GtkWidget *adddomainmi_ccc = NULL;
GtkWidget *newdomainmi_ccc = NULL;
GtkWidget *savedomainmi_ccc = NULL;
GtkWidget *savedomainasmi_ccc = NULL;
GtkWidget *activatewithfilemi_ccc = NULL;
GtkWidget *removeactivationmi_ccc = NULL;
GtkWidget *disableportsmi_ccc = NULL;
GtkWidget *restoreportsmi_ccc = NULL;
GtkWidget *displayidentifymi_ccc = NULL;
GtkWidget *displayanalyzemi_ccc = NULL;
GtkWidget *displaysmartmi_ccc = NULL;
GtkWidget *primaryrelaymi_ccc = NULL;
GtkWidget *chooseprimaryrelaymi_ccc = NULL;
GtkWidget *asclepiusextmi_ccc = NULL;
GtkWidget *asclepiusmonitormi_ccc = NULL;
GtkWidget *disableusbmassmi_ccc = NULL;
GtkWidget *restoreusbmassmi_ccc = NULL;
GtkWidget *loadconfigmi_ccc = NULL;
GtkWidget *writeconfigmi_ccc = NULL;
GtkWidget *choose_source_dialog_window_ccc = NULL;
GtkWidget *choose_source_scrolled_window_ccc = NULL;
GtkWidget *choose_source_vbox_ccc = NULL;
GtkWidget *choose_destination_dialog_window_ccc = NULL;
GtkWidget *choose_destination_scrolled_window_ccc = NULL;
GtkWidget *choose_destination_vbox_ccc = NULL;
GtkWidget *choose_usb_dialog_window_ccc = NULL;
GtkWidget *choose_usb_scrolled_window_ccc = NULL;
GtkWidget *choose_usb_vbox_ccc = NULL;
GtkWidget *connect_button_ccc = NULL;
GtkWidget *start_button_ccc = NULL;
GtkWidget *stop_button_ccc = NULL;
GtkWidget *analyze_button_ccc = NULL;
GtkWidget *analyze_long_button_ccc = NULL;
GtkWidget *smart_button_ccc = NULL;
GtkWidget *clone_mode_button_ccc = NULL;
GtkWidget *agressive_driver_checkbutton_ccc = NULL;
GtkWidget *driver_mode1_button_ccc = NULL;
GtkWidget *driver_mode2_button_ccc = NULL;
GtkWidget *driver_mode3_button_ccc = NULL;
GtkWidget *driver_mode4_button_ccc = NULL;
GtkWidget *driver_mode5_button_ccc = NULL;
GtkWidget *soft_reset_button_ccc = NULL;
GtkWidget *hard_reset_button_ccc = NULL;
GtkWidget *phase_control_label_ccc = NULL;
GtkWidget *phase1_check_button_ccc = NULL;
GtkWidget *phase2_check_button_ccc = NULL;
GtkWidget *phase3_check_button_ccc = NULL;
GtkWidget *phase4_check_button_ccc = NULL;
GtkWidget *divide_check_button_ccc = NULL;
GtkWidget *divide2_check_button_ccc = NULL;
GtkWidget *trim_check_button_ccc = NULL;
GtkWidget *scrape_check_button_ccc = NULL;
GtkWidget *markbad_check_button_ccc = NULL;
GtkWidget *readbad_check_button_ccc = NULL;
GtkWidget *rebuild_assist_check_button_ccc = NULL;
GtkWidget *reverse_check_button_ccc = NULL;
GtkWidget *skipfast_check_button_ccc = NULL;
GtkWidget *retries_button_label_ccc = NULL;
GtkWidget *retries_spin_button_ccc = NULL;
GtkWidget *clustersize_button_label_ccc = NULL;
GtkWidget *clustersize_spin_button_ccc = NULL;
GtkWidget *inputoffset_button_label_ccc = NULL;
GtkWidget *inputoffset_spin_button_ccc = NULL;
GtkWidget *size_button_label_ccc = NULL;
GtkWidget *size_spin_button_ccc = NULL;
GtkWidget *blocksize_button_label_ccc = NULL;
GtkWidget *blocksize_spin_button_ccc = NULL;
GtkWidget *skipsize_button_label_ccc = NULL;
GtkWidget *skipsize_spin_button_ccc = NULL;
GtkWidget *maxskipsize_button_label_ccc = NULL;
GtkWidget *maxskipsize_spin_button_ccc = NULL;
GtkWidget *skipthreshold_button_label_ccc = NULL;
GtkWidget *skipthreshold_spin_button_ccc = NULL;
GtkWidget *rateskip_button_label_ccc = NULL;
GtkWidget *rateskip_spin_button_ccc = NULL;
GtkWidget *exitonslow_button_label_ccc = NULL;
GtkWidget *exitonslow_spin_button_ccc = NULL;
GtkWidget *exitonslowtime_button_label_ccc = NULL;
GtkWidget *exitonslowtime_spin_button_ccc = NULL;
GtkWidget *no_log_backup_checkbutton_ccc = NULL;
GtkWidget *force_mounted_checkbutton_ccc = NULL;
GtkWidget *force_dangerous_checkbutton_ccc = NULL;
GtkWidget *enable_output_offset_checkbutton_ccc = NULL;
GtkWidget *output_offset_spinbutton_ccc = NULL;
GtkWidget *output_offset_button_label_ccc = NULL;
GtkWidget *enable_current_position_checkbutton_ccc = NULL;
GtkWidget *current_position_spinbutton_ccc = NULL;
GtkWidget *current_position_button_label_ccc = NULL;
GtkWidget *action_on_error_label_ccc = NULL;
GtkWidget *stop_on_error_radio_button_ccc = NULL;
GtkWidget *call_command_radio_button_ccc = NULL;
GtkWidget *command_to_call_text_ccc = NULL;
GtkWidget *test_command_button_ccc = NULL;
GtkWidget *primary_relay_on_error_radio_button_ccc = NULL;
GtkWidget *write_buffer_disable_radio_button_ccc = NULL;
GtkWidget *write_buffer_enable_radio_button_ccc = NULL;
GtkWidget *write_buffer_label_ccc = NULL;
GtkWidget *timer_settings_label_ccc = NULL;
GtkWidget *timer_settings_info_label_ccc = NULL;
GtkWidget *initial_busy_wait_time_button_label_ccc = NULL;
GtkWidget *initial_busy_wait_time_spinbutton_ccc = NULL;
GtkWidget *busy_wait_time_button_label_ccc = NULL;
GtkWidget *busy_wait_time_spinbutton_ccc = NULL;
GtkWidget *soft_reset_time_button_label_ccc = NULL;
GtkWidget *soft_reset_time_spinbutton_ccc = NULL;
GtkWidget *hard_reset_time_button_label_ccc = NULL;
GtkWidget *hard_reset_time_spinbutton_ccc = NULL;
GtkWidget *power_cycle_time_button_label_ccc = NULL;
GtkWidget *power_cycle_time_spinbutton_ccc = NULL;
GtkWidget *general_timout_button_label_ccc = NULL;
GtkWidget *general_timeout_spinbutton_ccc = NULL;
GtkWidget *action_power_cycle_label_ccc = NULL;
GtkWidget *stop_on_power_cycle_radio_button_ccc = NULL;
GtkWidget *call_power_command_radio_button_ccc = NULL;
GtkWidget *power_command_to_call_text_ccc = NULL;
GtkWidget *primary_relay_power_cycle_radio_button_ccc = NULL;
GtkWidget *test_power_command_button_ccc = NULL;
GtkWidget *pio_mode_checkbutton_ccc = NULL;
GtkWidget *phase_timers_label_ccc = NULL;
GtkWidget *phase_timers_checkbutton_ccc = NULL;
GtkWidget *p12_soft_reset_time_button_label_ccc = NULL;
GtkWidget *p12_soft_reset_time_spinbutton_ccc = NULL;
GtkWidget *p3_soft_reset_time_button_label_ccc = NULL;
GtkWidget *p3_soft_reset_time_spinbutton_ccc = NULL;
GtkWidget *p4_soft_reset_time_button_label_ccc = NULL;
GtkWidget *p4_soft_reset_time_spinbutton_ccc = NULL;
GtkWidget *td_soft_reset_time_button_label_ccc = NULL;
GtkWidget *td_soft_reset_time_spinbutton_ccc = NULL;
GtkWidget *d2_soft_reset_time_button_label_ccc = NULL;
GtkWidget *d2_soft_reset_time_spinbutton_ccc = NULL;
GtkWidget *sc_soft_reset_time_button_label_ccc = NULL;
GtkWidget *sc_soft_reset_time_spinbutton_ccc = NULL;
GtkWidget *rt_soft_reset_time_button_label_ccc = NULL;
GtkWidget *rt_soft_reset_time_spinbutton_ccc = NULL;
GtkWidget *always_wait_for_reset_timers_checkbutton_ccc = NULL;
GtkWidget *usb_resets_label_ccc = NULL;
GtkWidget *enable_usb_bulk_reset_checkbutton_ccc = NULL;
GtkWidget *enable_usb_soft_reset_checkbutton_ccc = NULL;
GtkWidget *enable_usb_hard_reset_checkbutton_ccc = NULL;
GtkWidget *enable_usb_port_reset_checkbutton_ccc = NULL;
GtkWidget *sectorsize_button_label_ccc = NULL;
GtkWidget *sectorsize_spinbutton_ccc = NULL;
GtkWidget *alignment_button_label_ccc = NULL;
GtkWidget *alignment_spinbutton_ccc = NULL;
GtkWidget *maxreadrate_button_label_ccc = NULL;
GtkWidget *maxreadrate_spinbutton_ccc = NULL;
GtkWidget *enable_rebuild_assist_checkbutton_ccc = NULL;
GtkWidget *enable_process_chunk_checkbutton_ccc = NULL;
GtkWidget *enable_read_twice_checkbutton_ccc = NULL;
GtkWidget *enable_retry_connecting_checkbutton_ccc = NULL;
GtkWidget *enable_phase_log_checkbutton_ccc = NULL;
GtkWidget *confirmation_box_label_ccc = NULL;
GtkWidget *disable_ports_label_ccc = NULL;
GtkWidget *disable_ports_info_label_ccc = NULL;
GtkWidget *current_ports_label_ccc = NULL;
GtkWidget *current_ports_info_label_ccc = NULL;
GtkWidget *set_disabled_ports_label_ccc = NULL;
GtkWidget *set_disabled_ports_text_ccc = NULL;
GtkWidget *updated_ports_label_ccc = NULL;
GtkWidget *updated_ports_info_label_ccc = NULL;
GtkWidget *update_ports_button_ccc = NULL;
GtkWidget *select_lun_label_ccc = NULL;
GtkWidget *select_lun_info_label_ccc = NULL;
GtkWidget *lun_values_label_ccc = NULL;
GtkWidget *lun_values_info_label_ccc = NULL;
GtkWidget *set_current_lun_label_ccc = NULL;
GtkWidget *current_lun_spinbutton_ccc = NULL;
GtkWidget *enable_scsi_write_checkbutton_ccc = NULL;
GtkWidget *enable_output_sector_size_checkbutton_ccc = NULL;
GtkWidget *output_sector_size_button_label_ccc = NULL;
GtkWidget *driver_io_scsi_only_checkbutton_ccc = NULL;
GtkWidget *use_physical_sector_size_for_virtual_checkbutton_ccc = NULL;
GtkWidget *output_sector_size_spinbutton_ccc = NULL;
GtkWidget *driver_error_options_label_ccc = NULL;
GtkWidget *driver_return_error_radio_button_ccc = NULL;
GtkWidget *driver_return_zeros_radio_button_ccc = NULL;
GtkWidget *driver_return_marked_radio_button_ccc = NULL;
GtkWidget *virtual_disk_device_name_label_ccc = NULL;
GtkWidget *virtual_disk_device_name_text_ccc = NULL;
GtkWidget *driver_minimum_cluster_size_button_label_ccc = NULL;
GtkWidget *driver_minimum_cluster_size_spinbutton_ccc = NULL;
GtkWidget *enable_extended_analyze_checkbutton_ccc = NULL;
GtkWidget *primary_relay_settings_label_ccc = NULL;
GtkWidget *primary_relay_settings_info_label_ccc = NULL;
GtkWidget *label_current_primary_relay_board_ccc = NULL;
GtkWidget *data_current_relay_board_a_ccc = NULL;
GtkWidget *asclepius_relay_source_label_ccc = NULL;
GtkWidget *asclepius_relay_source_combobox_ccc = NULL;
GtkWidget *asclepius_soft_ocp_label_ccc = NULL;
GtkWidget *asclepius_poll_interval_label_ccc = NULL;
GtkWidget *asclepius_poll_interval_combobox_ccc = NULL;
GtkWidget *asclepius_ocp_12v_label_ccc = NULL;
GtkWidget *asclepius_ocp_5v_label_ccc = NULL;
GtkWidget *asclepius_ocp_usb_label_ccc = NULL;
GtkWidget *asclepius_ocp_action_12v_combobox_ccc = NULL;
GtkWidget *asclepius_ocp_action_5v_combobox_ccc = NULL;
GtkWidget *asclepius_ocp_action_usb_combobox_ccc = NULL;
GtkWidget *asclepius_ocp_limit_12v_combobox_ccc = NULL;
GtkWidget *asclepius_ocp_limit_5v_combobox_ccc = NULL;
GtkWidget *asclepius_ocp_limit_usb_combobox_ccc = NULL;
GtkWidget *asclepius_tty_device_text_ccc = NULL;
GtkWidget *asclepius_connect_button_ccc = NULL;
GtkWidget *asclepius_disconnect_button_ccc = NULL;
GtkWidget *primary_relay_board_settings_label_ccc = NULL;
GtkWidget *primary_relay_board_type_frame_ccc = NULL;
GtkWidget *primary_relay_board_type_label_ccc = NULL;
GtkWidget *primary_relay_board_type_frame_label_ccc = NULL;
GtkWidget *primary_relay_board_generic_radio_ccc = NULL;
GtkWidget *primary_relay_board_asclepius_radio_ccc = NULL;

// Frames that only make sense for one board type. "Relay board settings"
// mixes the generic USB relay's Energize 1..8 sequencing with the Asclepius
// device settings, so the two groups are shown according to the selection
// instead of both appearing at once.
static GtkWidget *primary_relay_generic_frames_ccc[4] = { NULL, NULL, NULL, NULL };
static GtkWidget *primary_relay_asclepius_frames_ccc[4] = { NULL, NULL, NULL, NULL };
GtkWidget *primary_relay_activation_label_ccc = NULL;
GtkWidget *activate_primary_relay1_checkbutton_ccc = NULL;
GtkWidget *activate_primary_relay2_checkbutton_ccc = NULL;
GtkWidget *activate_primary_relay3_checkbutton_ccc = NULL;
GtkWidget *activate_primary_relay4_checkbutton_ccc = NULL;
GtkWidget *activate_primary_relay5_checkbutton_ccc = NULL;
GtkWidget *activate_primary_relay6_checkbutton_ccc = NULL;
GtkWidget *activate_primary_relay7_checkbutton_ccc = NULL;
GtkWidget *activate_primary_relay8_checkbutton_ccc = NULL;
GtkWidget *primary_relay_deactivation_label_ccc = NULL;
GtkWidget *deactivate_primary_relay1_checkbutton_ccc = NULL;
GtkWidget *deactivate_primary_relay2_checkbutton_ccc = NULL;
GtkWidget *deactivate_primary_relay3_checkbutton_ccc = NULL;
GtkWidget *deactivate_primary_relay4_checkbutton_ccc = NULL;
GtkWidget *deactivate_primary_relay5_checkbutton_ccc = NULL;
GtkWidget *deactivate_primary_relay6_checkbutton_ccc = NULL;
GtkWidget *deactivate_primary_relay7_checkbutton_ccc = NULL;
GtkWidget *deactivate_primary_relay8_checkbutton_ccc = NULL;
GtkWidget *primary_relay_activate_time_label_ccc = NULL;
GtkWidget *primary_relay_energize_time_spinbutton_ccc = NULL;
GtkWidget *primary_relay_delay_time_label_ccc = NULL;
GtkWidget *primary_relay_delay_time_spinbutton_ccc = NULL;
GtkWidget *test_primary_relay_button_ccc = NULL;
GtkWidget *activate_primary_relay_button_ccc = NULL;
GtkWidget *deactivate_primary_relay_button_ccc = NULL;
GtkWidget *activate_primary_relay_button_main_ccc = NULL;
GtkWidget *deactivate_primary_relay_button_main_ccc = NULL;
GtkWidget *disable_identify_checkbutton_ccc = NULL;
GtkWidget *label_project_ccc = NULL;
GtkWidget *data_project_ccc = NULL;
GtkWidget *label_domain_ccc = NULL;
GtkWidget *data_domain_ccc = NULL;
GtkWidget *label_source_ccc = NULL;
GtkWidget *data_source_ccc = NULL;
GtkWidget *label_destination_ccc = NULL;
GtkWidget *data_destination_ccc = NULL;
GtkWidget *label_totallba_ccc = NULL;
GtkWidget *data_totallba_ccc = NULL;
GtkWidget *label_lbatoread_ccc = NULL;
GtkWidget *data_lbatoread_ccc = NULL;
GtkWidget *label_domainsize_ccc = NULL;
GtkWidget *data_domainsize_ccc = NULL;
GtkWidget *label_runtime_ccc = NULL;
GtkWidget *data_runtime_ccc = NULL;
GtkWidget *label_remainingtime_ccc = NULL;
GtkWidget *data_remainingtime_ccc = NULL;
GtkWidget *label_currentposition_ccc = NULL;
GtkWidget *data_currentposition_ccc = NULL;
GtkWidget *label_currentstatus_ccc = NULL;
GtkWidget *data_currentstatus_ccc = NULL;
GtkWidget *label_currentrate_ccc = NULL;
GtkWidget *data_currentrate_ccc = NULL;
GtkWidget *label_recentrate_ccc = NULL;
GtkWidget *data_recentrate_ccc = NULL;
GtkWidget *label_totalrate_ccc = NULL;
GtkWidget *data_totalrate_ccc = NULL;
GtkWidget *label_skipsize_ccc = NULL;
GtkWidget *data_skipsize_ccc = NULL;
GtkWidget *label_slowskips_ccc = NULL;
GtkWidget *data_slowskips_ccc = NULL;
GtkWidget *label_skips_ccc = NULL;
GtkWidget *data_skips_ccc = NULL;
GtkWidget *label_skipruns_ccc = NULL;
GtkWidget *data_skipruns_ccc = NULL;
GtkWidget *label_skipresets_ccc = NULL;
GtkWidget *data_skipresets_ccc = NULL;
GtkWidget *label_runsize_ccc = NULL;
GtkWidget *data_runsize_ccc = NULL;
GtkWidget *label_finished_ccc = NULL;
GtkWidget *data_finished_ccc = NULL;
GtkWidget *label_nontried_ccc = NULL;
GtkWidget *data_nontried_ccc = NULL;
GtkWidget *label_nontrimmed_ccc = NULL;
GtkWidget *data_nontrimmed_ccc = NULL;
GtkWidget *label_nondivided_ccc = NULL;
GtkWidget *data_nondivided_ccc = NULL;
GtkWidget *label_nonscraped_ccc = NULL;
GtkWidget *data_nonscraped_ccc = NULL;
GtkWidget *label_bad_ccc = NULL;
GtkWidget *data_bad_ccc = NULL;
GtkWidget *label_datapreview_ccc = NULL;
GtkWidget *data_datapreview_ccc = NULL;
GtkWidget *label_longestreadtime_ccc = NULL;
GtkWidget *data_longestreadtime_ccc = NULL;
GtkWidget *label_filled_ccc = NULL;
GtkWidget *data_filled_ccc = NULL;
GtkWidget *label_retried_ccc = NULL;
GtkWidget *data_retried_ccc = NULL;
GtkWidget *label_retrypasses_ccc = NULL;
GtkWidget *data_retrypasses_ccc = NULL;
GtkWidget *label_drivermode_ccc = NULL;
GtkWidget *data_drivermode_ccc = NULL;
GtkAdjustment *gtk_adjustment_ccc = NULL;
gint display_status_timer_ccc = 0;
int display_status_timer_running_ccc = 0;
int source_chosen_ccc = 0;
int destination_chosen_ccc = 0;
int destination_size_valid_ccc = 0;
int project_chosen_ccc = 0;
int connected_ccc = 0;
int running_ccc = 0;
int new_project_ccc = 0;
int color_switch_flag_ccc = 0;
bool destination_is_generic_ccc = false;
int disabled_ports_changed_ccc = 0;
int usbr1_chosen_ccc = 0;
bool button_labeled_start = true;
char *status_icon_off_path = OSC_RESOURCE_PATH "/img/status_off.png";
char *status_icon_on_path = OSC_RESOURCE_PATH "/img/status_on.png";
char *error_icon_on_path = OSC_RESOURCE_PATH "/img/error_on.png";
char *status_warn_icon_path = OSC_RESOURCE_PATH "/img/status_warn.png";
char *smart_info_icon_path = OSC_RESOURCE_PATH "/img/fontawesome/circle-info.svg";
char *smart_warning_icon_path = OSC_RESOURCE_PATH "/img/fontawesome/triangle-exclamation.svg";
char *smart_error_icon_path = OSC_RESOURCE_PATH "/img/fontawesome/xmark.svg";
char *template_directory = OSC_RESOURCE_PATH "/templates/";
char *default_config_filename = "default.cfg";
char *default_ssd_config_filename = "default_ssd.cfg";
char *default_asclepius_config_filename = "asclepius.cfg";
int default_window_width = 1350;
int default_window_height = 670;

int start_gtk_ccc(int argc, char **argv, char *title, char *version)
{
  // initialize memory so it is ready
  return_value_ccc = initialize_logfile_memory_ccc();
  if (return_value_ccc != 0)
  {
    exitcode_ccc = GENERAL_ERROR_EXIT_CODE;
    return -1;
  }
  return_value_ccc = initialize_domainfile_memory_ccc();
  if (return_value_ccc != 0)
  {
    exitcode_ccc = GENERAL_ERROR_EXIT_CODE;
    return -1;
  }
  return_value_ccc = initialize_memory_ccc();
  if (return_value_ccc != 0)
  {
    exitcode_ccc = GENERAL_ERROR_EXIT_CODE;
    return -1;
  }

  initialize_script_variables_ccc();

  char window_title[256];
  snprintf(window_title, sizeof(window_title), "%s %s", title, version);

  gtk_init(&argc, &argv);

  if (enable_data_dump_ccc || enable_dump_identify_on_check_ccc)
  {
    set_data_dump_filename_ccc();
    char temp_string[256];
    dump_info_to_filename_ccc(data_dump_filename_ccc, "*****************************************************************\n");
    snprintf(temp_string, sizeof(temp_string), "********** %s %s data dump file **********\n", title_ccc, version_number_ccc);
    dump_info_to_filename_ccc(data_dump_filename_ccc, temp_string);
  }

  GtkBuilder *builder = gtk_builder_new();
  GError *err = NULL; // It is mandatory to initialize to NULL
  if (0 == gtk_builder_add_from_string(builder, (const gchar *)opensuperclone_glade, opensuperclone_glade_len, &err))
  {
    fprintf(stderr, "Error adding build from file. Error: %s\n", err->message);
  }

  main_window_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "window_main"));
  if (NULL == main_window_ccc)
  {
    fprintf(stderr, "Object error: window_main \n");
  }

  bottom_status_bar_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "bottom_status_bar"));

  gtk_builder_connect_signals(builder, NULL);

  gtk_window_set_title(GTK_WINDOW(main_window_ccc), window_title);

  topmenubar_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "topmenubar"));
  filemi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "filemi"));
  quitmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "quitmi"));
  openmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "openmi"));
  newmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "newmi"));
  sourcemi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "sourcemi"));
  destinationmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "destinationmi"));
  drivesmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "drivesmi"));
  blockmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "blockmi"));
  drivemi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "drivemi"));
  imagemi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "imagemi"));
  devnullmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "devnullmi"));
  modemi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "modemi"));
  copyimagemi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "copyimagemi"));
  genericmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "genericmi"));
  pass_automi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "pass_automi"));
  pass_scsimi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "pass_scsimi"));
  pass_atrami_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "pass_atami"));
  directmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "directmi"));
  ahcimi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "ahcimi"));
  usbmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "usbmi"));
  usbatami_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "usbatami"));
  settingsmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "settingsmi"));
  advancedmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "advancedmi"));
  clonemi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "clonemi"));
  savemi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "savemi"));
  saveasmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "saveasmi"));
  resetstatusmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "resetstatusmi"));
  toolsmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "toolsmi"));
  resetlogmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "resetlogmi"));
  repairlogmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "repairlogmi"));
  fillzeromi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "fillzeromi"));
  fillmarkmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "fillmarkmi"));
  fillerasemi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "fillerasemi"));
  domainmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "domainmi"));
  importddmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "importddmi"));
  exportddmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "exportddmi"));
  label_project_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_project"));
  data_project_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_project"));
  label_domain_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_domain"));
  data_domain_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_domain"));
  label_source_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_source"));
  data_source_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_source"));
  label_destination_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_destination"));
  data_destination_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_destination"));
  label_totallba_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_totallba"));
  data_totallba_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_totallba"));
  label_lbatoread_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_lbatoread"));
  data_lbatoread_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_lbatoread"));
  label_domainsize_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_domainsize"));
  data_domainsize_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_domainsize"));
  label_runtime_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_runtime"));
  data_runtime_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_runtime"));
  label_remainingtime_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_remainingtime"));
  data_remainingtime_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_remainingtime"));
  label_currentposition_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_currentposition"));
  data_currentposition_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_currentposition"));
  label_currentstatus_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_currentstatus"));
  data_currentstatus_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_currentstatus"));
  label_currentrate_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_currentrate"));
  data_currentrate_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_currentrate"));
  label_recentrate_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_recentrate"));
  data_recentrate_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_recentrate"));
  label_totalrate_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_totalrate"));
  data_totalrate_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_totalrate"));
  label_skipsize_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_skipsize"));
  data_skipsize_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_skipsize"));
  label_skips_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_skips"));
  data_skips_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_skips"));
  label_slowskips_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_slowskips"));
  data_slowskips_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_slowskips"));
  label_skipruns_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_skipruns"));
  data_skipruns_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_skipruns"));
  label_skipresets_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_skipresets"));
  data_skipresets_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_skipresets"));
  label_runsize_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_runsize"));
  data_runsize_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_runsize"));
  label_finished_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_finished"));
  data_finished_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_finished"));
  label_nontried_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_nontried"));
  data_nontried_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_nontried"));
  label_nontrimmed_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_nontrimmed"));
  data_nontrimmed_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_nontrimmed"));
  label_nondivided_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_nondivided"));
  data_nondivided_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_nondivided"));
  label_nonscraped_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_nonscraped"));
  data_nonscraped_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_nonscraped"));
  label_bad_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_bad"));
  data_bad_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_bad"));
  label_datapreview_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_datapreview"));
  data_datapreview_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_datapreview"));
  label_longestreadtime_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_longestreadtime"));
  data_longestreadtime_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_longestreadtime"));
  label_filled_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_filled"));
  data_filled_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_filled"));
  label_retried_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_retried"));
  data_retried_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_retried"));
  label_retrypasses_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_retrypasses"));
  data_retrypasses_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_retrypasses"));
  cleardomainmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "cleardomainmi"));
  helpmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "helpmi"));
  aboutmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "aboutmi"));
  timermi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "timermi"));
  label_drivermode_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_drivermode"));
  data_drivermode_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_drivermode"));
  installdrivermi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "installdrivermi"));
  uninstalldrivermi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "uninstalldrivermi"));
  fixdrivermemorymi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "fixdrivermemorymi"));
  driveronlymi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "driveronlymi"));
  adddomainmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "adddomainmi"));
  newdomainmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "newdomainmi"));
  savedomainmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "savedomainmi"));
  savedomainasmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "savedomainasmi"));
  activatewithfilemi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "activatewithfilemi"));
  removeactivationmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "removeactivationmi"));
  disableportsmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "disableportsmi"));
  restoreportsmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "restoreportsmi"));
  displayidentifymi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "displayidentifymi"));
  displayanalyzemi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "displayanalyzemi"));
  displaysmartmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "displaysmartmi"));
  primaryrelaymi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "primaryrelaymi"));
  chooseprimaryrelaymi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "chooseprimaryrelaymi"));
  asclepiusextmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "asclepiusextmi"));
  asclepiusmonitormi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "asclepiusmonitormi"));
  disableusbmassmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "disableusbmassmi"));
  restoreusbmassmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "restoreusbmassmi"));
  loadconfigmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "loadconfigmi"));
  writeconfigmi_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "writeconfigmi"));
  activate_primary_relay_button_main_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "activate_primary_relay_button_main"));
  deactivate_primary_relay_button_main_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "deactivate_primary_relay_button_main"));

  gtk_menu_item_set_label(GTK_MENU_ITEM(filemi_ccc), _("File"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(openmi_ccc), _("Open Project"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(newmi_ccc), _("New Project"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(quitmi_ccc), _("Quit"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(sourcemi_ccc), _("Choose Source Drive"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(destinationmi_ccc), _("Choose Destination"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(blockmi_ccc), _("Generic Block Device"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(drivemi_ccc), _("Drive"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(drivesmi_ccc), _("Drives"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(imagemi_ccc), _("Image File"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(devnullmi_ccc), _("NULL (No Destination)"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(modemi_ccc), _("Mode"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(copyimagemi_ccc), _("Generic Source File"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(genericmi_ccc), _("Generic Source Device"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(pass_automi_ccc), _("Passthrough Auto-Detect"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(pass_scsimi_ccc), _("Passthrough SCSI"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(pass_atrami_ccc), _("Passthrough ATA"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(directmi_ccc), _("Direct IDE"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(ahcimi_ccc), _("Direct AHCI"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(usbmi_ccc), _("Direct USB"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(usbatami_ccc), _("Direct USB ATA"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(settingsmi_ccc), _("Settings"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(advancedmi_ccc), _("Advanced Settings"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(clonemi_ccc), _("Clone Settings"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(savemi_ccc), _("Save Project"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(saveasmi_ccc), _("Save Project As"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(resetstatusmi_ccc), _("Reset Current Status"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(toolsmi_ccc), _("Tools"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(resetlogmi_ccc), _("Reset Log"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(repairlogmi_ccc), _("Repair Log"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(fillzeromi_ccc), _("Fill Zero"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(fillmarkmi_ccc), _("Fill Mark"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(fillerasemi_ccc), _("Erase Destination"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(domainmi_ccc), _("Load Domain File"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(importddmi_ccc), _("Import ddrescue Log (Map) File"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(exportddmi_ccc), _("Export ddrescue Log (Map) File"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(cleardomainmi_ccc), _("Clear Domain"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(helpmi_ccc), _("Help"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(aboutmi_ccc), _("About"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(timermi_ccc), _("Timer Settings"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(installdrivermi_ccc), _("Start Virtual Disk Driver"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(uninstalldrivermi_ccc), _("Stop Virtual Disk Driver"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(fixdrivermemorymi_ccc), _("Fix Driver Memory Error"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(driveronlymi_ccc), _("Virtual Driver Only"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(adddomainmi_ccc), _("Add to Domain from DMDE Bytes File"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(newdomainmi_ccc), _("New Domain File"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(savedomainmi_ccc), _("Save Domain File"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(savedomainasmi_ccc), _("Save Domain File As"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(disableportsmi_ccc), _("Disable SATA Ports"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(restoreportsmi_ccc), _("Restore Original Startup Port Configuration"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(displayidentifymi_ccc), _("Display Identify Results"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(displayanalyzemi_ccc), _("Display Analysis Results"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(displaysmartmi_ccc), _("Display SMART Results"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(primaryrelaymi_ccc), _("Primary Relay Settings"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(chooseprimaryrelaymi_ccc), _("Choose Primary Relay Board"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(asclepiusextmi_ccc), _("Open Terminal (UART)"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(asclepiusmonitormi_ccc), _("Open Monitor (Asclepius)"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(disableusbmassmi_ccc), _("Disable USB Mass Storage Driver"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(restoreusbmassmi_ccc), _("Restore USB Mass Storage Driver"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(loadconfigmi_ccc), _("Open Settings File"));
  gtk_menu_item_set_label(GTK_MENU_ITEM(writeconfigmi_ccc), _("Save Settings File"));
  gtk_button_set_label(GTK_BUTTON(activate_primary_relay_button_main_ccc), _("Power Off"));
  gtk_button_set_label(GTK_BUTTON(deactivate_primary_relay_button_main_ccc), _("Power On"));

  // add tooltips for relay buttons
  gtk_widget_set_tooltip_text(activate_primary_relay_button_main_ccc, _("Power off the drive with the selected relay board"));
  gtk_widget_set_tooltip_text(deactivate_primary_relay_button_main_ccc, _("Power on the drive with the selected relay board"));

  // add keyboard shortcuts
  GtkAccelGroup *accel_group = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(main_window_ccc), accel_group);

  gtk_widget_add_accelerator(openmi_ccc, "activate", accel_group, GDK_KEY_O, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
  gtk_widget_add_accelerator(savemi_ccc, "activate", accel_group, GDK_KEY_S, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
  gtk_widget_add_accelerator(saveasmi_ccc, "activate", accel_group, GDK_KEY_S, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
  gtk_widget_add_accelerator(quitmi_ccc, "activate", accel_group, GDK_KEY_Q, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
  gtk_widget_add_accelerator(newmi_ccc, "activate", accel_group, GDK_KEY_N, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
  // gtk_widget_add_accelerator(importddmi_ccc, "activate", accel_group, GDK_KEY_I, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
  // gtk_widget_add_accelerator(exportddmi_ccc, "activate", accel_group, GDK_KEY_E, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

  gtk_widget_add_accelerator(domainmi_ccc, "activate", accel_group, GDK_KEY_D, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
  gtk_widget_add_accelerator(newdomainmi_ccc, "activate", accel_group, GDK_KEY_N, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
  gtk_widget_add_accelerator(cleardomainmi_ccc, "activate", accel_group, GDK_KEY_C, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
  // gtk_widget_add_accelerator(adddomainmi_ccc, "activate", accel_group, GDK_KEY_M, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
  gtk_widget_add_accelerator(savedomainmi_ccc, "activate", accel_group, GDK_KEY_E, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);

  gtk_widget_add_accelerator(asclepiusextmi_ccc,"activate", accel_group, GDK_KEY_T, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);

  connect_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "connect_button"));
  start_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "start_button"));
  analyze_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "analyze_button"));
  analyze_long_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "analyze_long_button"));
  smart_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "smart_button"));
  clone_mode_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "clone_mode_button"));
  driver_mode1_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "driver_mode1_button"));
  agressive_driver_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "agressive_driver_checkbutton"));
  driver_mode2_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "driver_mode2_button"));
  driver_mode3_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "driver_mode3_button"));
  driver_mode4_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "driver_mode4_button"));
  driver_mode5_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "driver_mode5_button"));
  soft_reset_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "soft_reset_button"));
  hard_reset_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "hard_reset_button"));

  gtk_button_set_label(GTK_BUTTON(connect_button_ccc), _("Connect"));
  gtk_button_set_label(GTK_BUTTON(start_button_ccc), _("Start"));
  gtk_button_set_label(GTK_BUTTON(analyze_button_ccc), _("Analyze"));
  gtk_button_set_label(GTK_BUTTON(analyze_long_button_ccc), _("Extended\nAnalyze"));
  gtk_button_set_label(GTK_BUTTON(smart_button_ccc), _("SMART"));
  gtk_button_set_label(GTK_BUTTON(clone_mode_button_ccc), _("Clone\nMode"));
  gtk_button_set_label(GTK_BUTTON(agressive_driver_checkbutton_ccc), _("Slow\nDriver"));
  gtk_button_set_label(GTK_BUTTON(driver_mode1_button_ccc), _("Virtual\nMode 1"));
  gtk_button_set_label(GTK_BUTTON(driver_mode2_button_ccc), _("Virtual\nMode 2"));
  gtk_button_set_label(GTK_BUTTON(driver_mode3_button_ccc), _("Virtual\nMode 3"));
  gtk_button_set_label(GTK_BUTTON(driver_mode4_button_ccc), _("Virtual\nMode 4"));
  gtk_button_set_label(GTK_BUTTON(driver_mode5_button_ccc), _("Virtual\nMode 5"));
  gtk_button_set_label(GTK_BUTTON(soft_reset_button_ccc), _("Soft\nReset"));
  gtk_button_set_label(GTK_BUTTON(hard_reset_button_ccc), _("Hard\nReset"));

  // set button tooltips
  gtk_widget_set_tooltip_text(connect_button_ccc, _("Connect to the drive"));
  gtk_widget_set_tooltip_text(start_button_ccc, _("Start the clone/virtual driver"));
  gtk_widget_set_tooltip_text(analyze_button_ccc, _("Analyze the drive"));
  gtk_widget_set_tooltip_text(analyze_long_button_ccc, _("Analyze the drive (extended)"));
  gtk_widget_set_tooltip_text(smart_button_ccc, _("Retrieve SMART information"));
  gtk_widget_set_tooltip_text(clone_mode_button_ccc, _("Switch to clone mode"));
  gtk_widget_set_tooltip_text(agressive_driver_checkbutton_ccc, _("Activate slow driver"));
  gtk_widget_set_tooltip_text(driver_mode1_button_ccc, _("Switch to virtual mode 1: This mode will use all available Phases (starting with Phase 4) when reading from the source"));
  gtk_widget_set_tooltip_text(driver_mode2_button_ccc, _("Switch to virtual mode 2: This mode will only use Phase 4 and will not perform any further processing of a chunk"));
  gtk_widget_set_tooltip_text(driver_mode3_button_ccc, _("Switch to virtual mode 3: This mode is the same as mode 2, but it will increase the read size (up to the cluster size) when it detects sequential reads"));
  gtk_widget_set_tooltip_text(driver_mode4_button_ccc, _("Switch to virtual mode 4: This mode only reads from the destination, and will not read any data from the source"));
  gtk_widget_set_tooltip_text(driver_mode5_button_ccc, _("Switch to virtual mode 5: This mode does not read any data and will instead return zeroed data"));
  gtk_widget_set_tooltip_text(soft_reset_button_ccc, _("Soft reset the drive"));
  gtk_widget_set_tooltip_text(hard_reset_button_ccc, _("Hard reset the drive"));

  gtk_label_set_text(GTK_LABEL(label_project_ccc), _("Project"));
  gtk_label_set_text(GTK_LABEL(label_domain_ccc), _("Domain"));
  gtk_label_set_text(GTK_LABEL(label_source_ccc), _("Source"));
  gtk_label_set_text(GTK_LABEL(label_destination_ccc), _("Destination (All data will be overwritten!)"));
  gtk_label_set_text(GTK_LABEL(label_totallba_ccc), _("Total LBA"));
  gtk_label_set_text(GTK_LABEL(label_lbatoread_ccc), _("LBA to Read"));
  gtk_label_set_text(GTK_LABEL(label_domainsize_ccc), _("Domain Size"));
  gtk_label_set_text(GTK_LABEL(label_runtime_ccc), _("Run Time"));
  gtk_label_set_text(GTK_LABEL(label_remainingtime_ccc), _("Remaining Time"));
  gtk_label_set_text(GTK_LABEL(label_currentposition_ccc), _("Current Position"));
  gtk_label_set_text(GTK_LABEL(label_currentstatus_ccc), _("Current Status"));
  gtk_label_set_text(GTK_LABEL(label_currentrate_ccc), _("Current Rate"));
  gtk_label_set_text(GTK_LABEL(label_recentrate_ccc), _("Recent Rate"));
  gtk_label_set_text(GTK_LABEL(label_totalrate_ccc), _("Total Rate"));
  gtk_label_set_text(GTK_LABEL(label_skipsize_ccc), _("Base Skip Size"));
  gtk_label_set_text(GTK_LABEL(label_skips_ccc), _("Skips"));
  gtk_label_set_text(GTK_LABEL(label_slowskips_ccc), _("Slow Skips"));
  gtk_label_set_text(GTK_LABEL(label_skipruns_ccc), _("Skip Runs"));
  gtk_label_set_text(GTK_LABEL(label_skipresets_ccc), _("Skip Resets"));
  gtk_label_set_text(GTK_LABEL(label_runsize_ccc), _("Last Run Size"));
  gtk_label_set_text(GTK_LABEL(label_finished_ccc), _("Finished"));
  gtk_label_set_text(GTK_LABEL(label_nontried_ccc), _("Non-Tried"));
  gtk_label_set_text(GTK_LABEL(label_nontrimmed_ccc), _("Non-Trimmed"));
  gtk_label_set_text(GTK_LABEL(label_nondivided_ccc), _("Non-Divided"));
  gtk_label_set_text(GTK_LABEL(label_nonscraped_ccc), _("Non-Scraped"));
  gtk_label_set_text(GTK_LABEL(label_bad_ccc), _("Bad"));
  gtk_label_set_text(GTK_LABEL(label_datapreview_ccc), _("Data Preview"));
  gtk_label_set_text(GTK_LABEL(label_longestreadtime_ccc), _("Current / Recent / Longest"));
  gtk_label_set_text(GTK_LABEL(label_filled_ccc), _("Filled"));
  gtk_label_set_text(GTK_LABEL(label_retried_ccc), _("Retried"));
  gtk_label_set_text(GTK_LABEL(label_retrypasses_ccc), _("Remaining Retry Passes"));
  gtk_label_set_text(GTK_LABEL(label_drivermode_ccc), _("Clone/Driver Mode"));
  gtk_label_set_text(GTK_LABEL(data_drivermode_ccc), _("Clone"));

  // add label tooltips
  gtk_widget_set_tooltip_text(label_project_ccc, _("The project log file for the current recovery"));
  gtk_widget_set_tooltip_text(label_domain_ccc, _("The domain file for the current recovery"));
  gtk_widget_set_tooltip_text(label_source_ccc, _("The source drive to be recovered"));
  gtk_widget_set_tooltip_text(label_destination_ccc, _("The destination drive or image file to be recovered to"));
  gtk_widget_set_tooltip_text(label_totallba_ccc, _("The total number of LBAs in the source drive"));
  gtk_widget_set_tooltip_text(label_lbatoread_ccc, _("The number of LBAs to be read from the source drive"));
  gtk_widget_set_tooltip_text(label_domainsize_ccc, _("The size of the domain file in LBAs"));
  gtk_widget_set_tooltip_text(label_runtime_ccc, _("The total time taken to recover the drive"));
  gtk_widget_set_tooltip_text(label_remainingtime_ccc, _("The estimated time remaining to complete the recovery"));
  gtk_widget_set_tooltip_text(label_currentposition_ccc, _("The current position in the source drive in LBAs"));
  gtk_widget_set_tooltip_text(label_currentstatus_ccc, _("The current status of the recovery"));
  gtk_widget_set_tooltip_text(label_currentrate_ccc, _("The current rate of the recovery"));
  gtk_widget_set_tooltip_text(label_recentrate_ccc, _("The average rate of the last 5 minutes of the recovery"));
  gtk_widget_set_tooltip_text(label_totalrate_ccc, _("The average rate of the entire recovery"));
  gtk_widget_set_tooltip_text(label_skipsize_ccc, _("The base skip size"));
  gtk_widget_set_tooltip_text(label_skips_ccc, _("The total number of skips so far"));
  gtk_widget_set_tooltip_text(label_slowskips_ccc, _("The total number of skips due to slow reads so far"));
  gtk_widget_set_tooltip_text(label_skipruns_ccc, _("The total number of skip runs so far"));
  gtk_widget_set_tooltip_text(label_skipresets_ccc, _("The total number of times the skipping was reset due to exceeding the limit of 64 skips in a run"));
  gtk_widget_set_tooltip_text(label_runsize_ccc, _("The size of the last skip run"));
  gtk_widget_set_tooltip_text(label_finished_ccc, _("The total number of LBAs recovered so far"));
  gtk_widget_set_tooltip_text(label_nontried_ccc, _("The total number of LBAs that have not been tried to be recovered"));
  gtk_widget_set_tooltip_text(label_nontrimmed_ccc, _("The total number of LBAs that have not been trimmed"));
  gtk_widget_set_tooltip_text(label_nondivided_ccc, _("The total number of LBAs that have not been divided"));
  gtk_widget_set_tooltip_text(label_nonscraped_ccc, _("The total number of LBAs that have not been scraped"));
  gtk_widget_set_tooltip_text(label_bad_ccc, _("The total number of LBAs that have been marked as bad"));
  gtk_widget_set_tooltip_text(label_datapreview_ccc, _("A sample of data from the last good read since the last display update (will be all zeroes if no good reads have occurred since the last display update)"));
  gtk_widget_set_tooltip_text(label_longestreadtime_ccc, _("The longest read time since the last display update / within the last 256 display updates / since the start of the recovery"));
  gtk_widget_set_tooltip_text(label_filled_ccc, _("The total number of LBAs that have been filled either in Fill mode or Erase mode"));
  gtk_widget_set_tooltip_text(label_retried_ccc, _("The total number of LBAs that have been retried"));
  gtk_widget_set_tooltip_text(label_retrypasses_ccc, _("The total number of retry passes remaining"));
  gtk_widget_set_tooltip_text(label_drivermode_ccc, _("The current mode of the recovery"));

  // set status and error registers
  status_register_label = GTK_WIDGET(gtk_builder_get_object(builder, "status_register_label"));
  error_register_label = GTK_WIDGET(gtk_builder_get_object(builder, "error_register_label"));

  gtk_label_set_text(GTK_LABEL(status_register_label), _("Status Register"));
  gtk_label_set_text(GTK_LABEL(error_register_label), _("Error Register"));

  bsy_status_icon = GTK_WIDGET(gtk_builder_get_object(builder, "bsy_status_icon"));
  drdy_status_icon = GTK_WIDGET(gtk_builder_get_object(builder, "drdy_status_icon"));
  df_status_icon = GTK_WIDGET(gtk_builder_get_object(builder, "df_status_icon"));
  dsc_status_icon = GTK_WIDGET(gtk_builder_get_object(builder, "dsc_status_icon"));
  drq_status_icon = GTK_WIDGET(gtk_builder_get_object(builder, "drq_status_icon"));
  corr_status_icon = GTK_WIDGET(gtk_builder_get_object(builder, "corr_status_icon"));
  idx_status_icon = GTK_WIDGET(gtk_builder_get_object(builder, "idx_status_icon"));
  err_status_icon = GTK_WIDGET(gtk_builder_get_object(builder, "err_status_icon"));
  bbk_status_icon = GTK_WIDGET(gtk_builder_get_object(builder, "bbk_status_icon"));
  unc_status_icon = GTK_WIDGET(gtk_builder_get_object(builder, "unc_status_icon"));
  mc_status_icon = GTK_WIDGET(gtk_builder_get_object(builder, "mc_status_icon"));
  idnf_status_icon = GTK_WIDGET(gtk_builder_get_object(builder, "idnf_status_icon"));
  mcr_status_icon = GTK_WIDGET(gtk_builder_get_object(builder, "mcr_status_icon"));
  abrt_status_icon = GTK_WIDGET(gtk_builder_get_object(builder, "abrt_status_icon"));
  tk0nf_status_icon = GTK_WIDGET(gtk_builder_get_object(builder, "tk0nf_status_icon"));
  amnf_status_icon = GTK_WIDGET(gtk_builder_get_object(builder, "amnf_status_icon"));

  gtk_image_set_from_file(GTK_IMAGE(bsy_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(drdy_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(df_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(dsc_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(drq_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(corr_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(idx_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(err_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(bbk_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(unc_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(mc_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(idnf_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(mcr_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(abrt_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(tk0nf_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(amnf_status_icon), status_icon_off_path);

  // set status and error register tooltips
  gtk_widget_set_tooltip_text(bsy_status_icon, _("Busy (The device is busy)"));
  gtk_widget_set_tooltip_text(drdy_status_icon, _("Device Ready (The device is ready to accept commands)"));
  gtk_widget_set_tooltip_text(df_status_icon, _("Device Fault (The device has detected a fault)"));
  gtk_widget_set_tooltip_text(dsc_status_icon, _("Device Seek Complete (Head is settled on the track / Deferred write error)"));
  gtk_widget_set_tooltip_text(drq_status_icon, _("Data Request (The device is ready to transfer data)"));
  gtk_widget_set_tooltip_text(corr_status_icon, _("Corrected Data (The device has corrected data / Alignment error)"));
  gtk_widget_set_tooltip_text(idx_status_icon, _("Index (The device has reached the index mark / Sense data available)"));
  gtk_widget_set_tooltip_text(err_status_icon, _("Error (The device has detected an error)"));
  gtk_widget_set_tooltip_text(bbk_status_icon, _("Bad Block (The device has detected a bad block / Interface CRC error)"));
  gtk_widget_set_tooltip_text(unc_status_icon, _("Uncorrectable Data (The device has detected uncorrectable data)"));
  gtk_widget_set_tooltip_text(mc_status_icon, _("Media Change (The media has changed / Obsolete)"));
  gtk_widget_set_tooltip_text(idnf_status_icon, _("ID Not Found (The device could not find the sector's ID field)"));
  gtk_widget_set_tooltip_text(mcr_status_icon, _("Media Change Request (The device has requested a media change / Obsolete)"));
  gtk_widget_set_tooltip_text(abrt_status_icon, _("Command Aborted (The device has aborted the command)"));
  gtk_widget_set_tooltip_text(tk0nf_status_icon, _("Track 0 Not Found (The device could not find track 0 / End of media)"));
  gtk_widget_set_tooltip_text(amnf_status_icon, _("Address Mark Not Found (The device could not find the address mark / Command timeout)"));

  bsy_status_label = GTK_WIDGET(gtk_builder_get_object(builder, "bsy_status_label"));
  drdy_status_label = GTK_WIDGET(gtk_builder_get_object(builder, "drdy_status_label"));
  df_status_label = GTK_WIDGET(gtk_builder_get_object(builder, "df_status_label"));
  dsc_status_label = GTK_WIDGET(gtk_builder_get_object(builder, "dsc_status_label"));
  drq_status_label = GTK_WIDGET(gtk_builder_get_object(builder, "drq_status_label"));
  corr_status_label = GTK_WIDGET(gtk_builder_get_object(builder, "corr_status_label"));
  idx_status_label = GTK_WIDGET(gtk_builder_get_object(builder, "idx_status_label"));
  err_status_label = GTK_WIDGET(gtk_builder_get_object(builder, "err_status_label"));
  bbk_status_label = GTK_WIDGET(gtk_builder_get_object(builder, "bbk_status_label"));
  unc_status_label = GTK_WIDGET(gtk_builder_get_object(builder, "unc_status_label"));
  mc_status_label = GTK_WIDGET(gtk_builder_get_object(builder, "mc_status_label"));
  idnf_status_label = GTK_WIDGET(gtk_builder_get_object(builder, "idnf_status_label"));
  mcr_status_label = GTK_WIDGET(gtk_builder_get_object(builder, "mcr_status_label"));
  abrt_status_label = GTK_WIDGET(gtk_builder_get_object(builder, "abrt_status_label"));
  tk0nf_status_label = GTK_WIDGET(gtk_builder_get_object(builder, "tk0nf_status_label"));
  amnf_status_label = GTK_WIDGET(gtk_builder_get_object(builder, "amnf_status_label"));

  asclepius_sata_power_label = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_sata_power_label"));
  asclepius_usb_power_label = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_usb_power_label"));

  gtk_label_set_text(GTK_LABEL(asclepius_sata_power_label), _("SATA / IDE"));
  gtk_label_set_text(GTK_LABEL(asclepius_usb_power_label), _("USB"));

  asclepius_12v_icon = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_12v_icon"));
  asclepius_5v_icon = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_5v_icon"));
  asclepius_usb_icon = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_usb_icon"));

  gtk_image_set_from_file(GTK_IMAGE(asclepius_12v_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(asclepius_5v_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(asclepius_usb_icon), status_icon_off_path);

  gtk_widget_set_tooltip_text(asclepius_12v_icon, _("12V Channel"));
  gtk_widget_set_tooltip_text(asclepius_5v_icon, _("5V Channel"));
  gtk_widget_set_tooltip_text(asclepius_usb_icon, _("USB 5V Channel"));

  asclepius_12v_label = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_12v_label"));
  asclepius_5v_label = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_5v_label"));
  asclepius_usb_label = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_usb_label"));


  gtk_label_set_text(GTK_LABEL(asclepius_12v_label), _("0V / 0mA"));
  gtk_label_set_text(GTK_LABEL(asclepius_5v_label), _("0V / 0mA"));
  gtk_label_set_text(GTK_LABEL(asclepius_usb_label), _("0V / 0mA"));

  gtk_label_set_text(GTK_LABEL(bsy_status_label), _("BSY"));
  gtk_label_set_text(GTK_LABEL(drdy_status_label), _("DRDY"));
  gtk_label_set_text(GTK_LABEL(df_status_label), _("DF"));
  gtk_label_set_text(GTK_LABEL(dsc_status_label), _("DSC"));
  gtk_label_set_text(GTK_LABEL(drq_status_label), _("DRQ"));
  gtk_label_set_text(GTK_LABEL(corr_status_label), _("CORR"));
  gtk_label_set_text(GTK_LABEL(idx_status_label), _("IDX"));
  gtk_label_set_text(GTK_LABEL(err_status_label), _("ERR"));
  gtk_label_set_text(GTK_LABEL(bbk_status_label), _("BBK"));
  gtk_label_set_text(GTK_LABEL(unc_status_label), _("UNC"));
  gtk_label_set_text(GTK_LABEL(mc_status_label), _("MC"));
  gtk_label_set_text(GTK_LABEL(idnf_status_label), _("IDNF"));
  gtk_label_set_text(GTK_LABEL(mcr_status_label), _("MCR"));
  gtk_label_set_text(GTK_LABEL(abrt_status_label), _("ABRT"));
  gtk_label_set_text(GTK_LABEL(tk0nf_status_label), _("TK0NF"));
  gtk_label_set_text(GTK_LABEL(amnf_status_label), _("AMNF"));

  // set intial button states
  set_disconnected_ccc();

  set_mode_auto_passthrough_ccc();

  if (scsi_passthrough_ccc || generic_mode_ccc)
  {
    deactivate_status_buttons_ccc();
  }
  else
  {
    activate_status_buttons_ccc();
  }

  g_object_unref(builder);

  // load default settings from config file
  char filename[1024];
  snprintf(filename, 1024, "%s%s", template_directory, default_config_filename);

  FILE *file = fopen(filename, "r");
  if (file)
  {
    fclose(file);

    read_config_file_with_name_ccc(filename);
  }
  else
  {
    // write default config file if it does not exist
    write_config_file_with_name_ccc(filename);
  }

  gdk_threads_add_timeout(ASCLEPIUS_UI_UPDATE_TIMER, display_status_update_asclepius_action, NULL);
  asclepius_connect();

  if(asclepius_get_connection_status())
  {
    print_gui_error_message_ccc(_("Asclepius companion board was detected.\nEnabling default settings for Asclepius support."), _("Asclepius Connection"), 0);

    // load asclepius settings from config file
    char filename[1024];
    snprintf(filename, 1024, "%s%s", template_directory, default_asclepius_config_filename);

    FILE *file = fopen(filename, "r");
    if (file)
    {
      fclose(file);

      read_config_file_with_name_ccc(filename);
    }

    // The auto-connect has to leave the same state as connecting from the relay
    // dialog, so the board type and name reflect the board that just answered.
    select_connected_asclepius_ccc();
  }

  gtk_window_set_default_size(GTK_WINDOW(main_window_ccc), default_window_width, default_window_height);
  gtk_widget_show_all(main_window_ccc);
  gtk_main();

  return 0;
}

int set_data_dump_filename_ccc(void)
{
  language_window_ccc = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  // gtk_window_set_keep_above(GTK_WINDOW(language_window_ccc), TRUE);
  gtk_window_set_title(GTK_WINDOW(language_window_ccc), "Select Data Dump File");
  gtk_window_set_default_size(GTK_WINDOW(language_window_ccc), 400, 300);
  gtk_window_set_position(GTK_WINDOW(language_window_ccc), GTK_WIN_POS_CENTER);

  gint handler_id1 = g_signal_connect(G_OBJECT(language_window_ccc), "destroy", G_CALLBACK(gtk_main_quit), NULL);

  GtkWidget *box = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(language_window_ccc), box);

  GtkWidget *enbutton = gtk_button_new_with_label("Continue");
  GtkWidget *imbutton = gtk_button_new_with_label("Change Data Dump File");
  GtkWidget *okim = gtk_image_new_from_stock(GTK_STOCK_OK, GTK_ICON_SIZE_BUTTON);
  GtkWidget *openim = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image(GTK_BUTTON(enbutton), okim);
  gtk_button_set_image(GTK_BUTTON(imbutton), openim);

  gtk_box_pack_start(GTK_BOX(box), enbutton, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box), imbutton, TRUE, TRUE, 0);

  gint handler_id2 = g_signal_connect(enbutton, "clicked", G_CALLBACK(gtk_main_quit), NULL);
  gint handler_id4 = g_signal_connect(imbutton, "clicked", G_CALLBACK(get_data_dump_filename_ccc), NULL);

  gtk_widget_show_all(language_window_ccc);
  gtk_main();
  g_signal_handler_disconnect(language_window_ccc, handler_id1);
  g_signal_handler_disconnect(enbutton, handler_id2);
  g_signal_handler_disconnect(imbutton, handler_id4);
  gtk_widget_hide(language_window_ccc);
  gtk_widget_destroy(language_window_ccc);

  return 0;
}

void get_data_dump_filename_ccc(void)
{
  GtkWidget *dialog;
  dialog = gtk_file_chooser_dialog_new("Select Data Dump File",
                                       GTK_WINDOW(language_window_ccc),
                                       GTK_FILE_CHOOSER_ACTION_SAVE,
                                       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                       GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                       NULL);
  gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "debug_data_dump.log");
  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
  {
    char *filename;
    filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    fprintf(stdout, "%s\n", filename);
    strcpy(data_dump_filename_ccc, filename);
    g_free(filename);
  }
  gtk_widget_destroy(dialog);
}

int print_gui_error_message_ccc(char *message, char *title, int type)
{
  if (!superclone_ccc)
  {
    message_now_ccc(message);
  }
  else if (check_message_ccc && (do_call_command_ccc || activate_primary_relay_on_error_ccc))
  {
    g_print("%s\n", message);
  }
  else
  {
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWidget *dialog;
    GtkMessageType message_type;
    if (type)
    {
      message_type = GTK_MESSAGE_WARNING;
      // dialog = gtk_message_dialog_new(GTK_WINDOW(main_window_ccc), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, "%s", message);
    }
    else
    {
      message_type = GTK_MESSAGE_INFO;
      // dialog = gtk_message_dialog_new(GTK_WINDOW(main_window_ccc), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s", message);
    }
    dialog = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_DESTROY_WITH_PARENT, message_type, GTK_BUTTONS_OK, "%s", message);
    gtk_window_set_keep_above(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_title(GTK_WINDOW(dialog), title);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    gtk_widget_destroy(window);
  }

  check_message_ccc = false;
  return 0;
}

void select_file_ccc(void)
{
  if (connected_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("You must disconnect first."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
  else
  {
    GtkWidget *dialog;
    dialog = gtk_file_chooser_dialog_new(_("Load project (progress) file"),
                                         GTK_WINDOW(main_window_ccc),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                         NULL);
    if (log_file_ccc != NULL)
    {
      char *default_path = g_path_get_dirname(log_file_ccc);
      gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), default_path);
    }
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
      clear_source_ccc();
      clear_destination_ccc();
      project_chosen_ccc = 0;
      char *filename;
      filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
      fprintf(stdout, "%s\n", filename);
      load_log_file_ccc(filename);
      g_free(filename);
    }
    gtk_widget_destroy(dialog);
  }
}

void new_file_ccc(void)
{
  if (connected_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("You must disconnect first."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
  else
  {
    GtkWidget *dialog;
    dialog = gtk_file_chooser_dialog_new(_("New project (progress) file"),
                                         GTK_WINDOW(main_window_ccc),
                                         GTK_FILE_CHOOSER_ACTION_SAVE,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                         NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
    if (log_file_ccc != NULL)
    {
      char *default_path = g_path_get_dirname(log_file_ccc);
      gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), default_path);
    }
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
      clear_source_ccc();
      clear_destination_ccc();
      char *filename;
      filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
      fprintf(stdout, "%s\n", filename);
      new_log_file_ccc(filename);
      g_free(filename);
    }
    gtk_widget_destroy(dialog);
  }
}

void new_domain_ccc(void)
{
  if (connected_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("You must disconnect first."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
  else
  {
    GtkWidget *dialog;
    dialog = gtk_file_chooser_dialog_new(_("New Domain File"),
                                         GTK_WINDOW(main_window_ccc),
                                         GTK_FILE_CHOOSER_ACTION_SAVE,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                         NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
    if (log_file_ccc != NULL)
    {
      char *default_path = g_path_get_dirname(log_file_ccc);
      gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), default_path);
    }
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
      char *filename;
      filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
      fprintf(stdout, "%s\n", filename);
      new_domain_file_ccc(filename);
      g_free(filename);
    }
    gtk_widget_destroy(dialog);
  }
}

void select_ddrescue_file_ccc(void)
{
  if (connected_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("You must disconnect first."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
  else if (!new_project_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("You must start a new project before importing a ddrescue log file"));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
  else
  {
    GtkWidget *dialog;
    dialog = gtk_file_chooser_dialog_new(_("Import ddrescue log (map) file"),
                                         GTK_WINDOW(main_window_ccc),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                         NULL);
    if (log_file_ccc != NULL)
    {
      char *default_path = g_path_get_dirname(log_file_ccc);
      gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), default_path);
    }
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
      char *filename;
      filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
      fprintf(stdout, "%s\n", filename);
      load_ddrescue_log_file_ccc(filename);
      g_free(filename);
    }
    gtk_widget_destroy(dialog);
  }
}

void save_file_ccc(void)
{
  if (!project_chosen_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("You must choose a project first."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
  else
  {
    return_value_ccc = write_logfile_ccc(log_file_ccc, 0);
    if (return_value_ccc)
    {
      snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error writing progress log file"));
      message_error_ccc(tempmessage_ccc);
      print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
      clear_error_message_ccc();
    }
  }
}

void save_file_as_ccc(void)
{
  if (!project_chosen_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("You must choose a project first."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
  else
  {
    GtkWidget *dialog;
    dialog = gtk_file_chooser_dialog_new(_("Save Project File As"),
                                         GTK_WINDOW(main_window_ccc),
                                         GTK_FILE_CHOOSER_ACTION_SAVE,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                         NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
    if (log_file_ccc != NULL)
    {
      char *default_path = g_path_get_dirname(log_file_ccc);
      gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), default_path);
    }
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
      char *filename;
      filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
      fprintf(stdout, "%s\n", filename);
      return_value_ccc = write_logfile_ccc(filename, 0);
      if (return_value_ccc)
      {
        snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error writing progress log file"));
        message_error_ccc(tempmessage_ccc);
        print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
        clear_error_message_ccc();
      }
      g_free(filename);
    }
    gtk_widget_destroy(dialog);
  }
}

void save_domain_ccc(void)
{
  if (!project_chosen_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("You must choose a project first."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
  else
  {
    return_value_ccc = write_domainfile_ccc(domain_file_ccc, 0);
    if (return_value_ccc)
    {
      snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error writing progress log file"));
      message_error_ccc(tempmessage_ccc);
      print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
      clear_error_message_ccc();
    }
  }
}

void save_domain_as_ccc(void)
{
  if (!project_chosen_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("You must choose a project first."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
  else
  {
    GtkWidget *dialog;
    dialog = gtk_file_chooser_dialog_new(_("Save Domain File As"),
                                         GTK_WINDOW(main_window_ccc),
                                         GTK_FILE_CHOOSER_ACTION_SAVE,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                         NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
    if (log_file_ccc != NULL)
    {
      char *default_path = g_path_get_dirname(log_file_ccc);
      gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), default_path);
    }
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
      char *filename;
      filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
      fprintf(stdout, "%s\n", filename);
      return_value_ccc = write_domainfile_ccc(filename, 0);
      if (return_value_ccc)
      {
        snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error writing progress log file"));
        message_error_ccc(tempmessage_ccc);
        print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
        clear_error_message_ccc();
      }
      g_free(filename);
    }
    gtk_widget_destroy(dialog);
  }
}

void export_ddrescue_file_ccc(void)
{
  if (!project_chosen_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("You must choose a project first."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
  else
  {
    GtkWidget *dialog;
    dialog = gtk_file_chooser_dialog_new(_("Export ddrescue log (map) file"),
                                         GTK_WINDOW(main_window_ccc),
                                         GTK_FILE_CHOOSER_ACTION_SAVE,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                         NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
    if (log_file_ccc != NULL)
    {
      char *default_path = g_path_get_dirname(log_file_ccc);
      gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), default_path);
    }
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
      char *filename;
      filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
      fprintf(stdout, "%s\n", filename);
      return_value_ccc = write_ddrescue_logfile_ccc(filename);
      if (return_value_ccc)
      {
        snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error exporting ddrescue log (map) file"));
        message_error_ccc(tempmessage_ccc);
        print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
        clear_error_message_ccc();
      }
      g_free(filename);
    }
    gtk_widget_destroy(dialog);
  }
}

void select_domain_ccc(void)
{
  if (connected_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("You must disconnect first."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
  else
  {
    GtkWidget *dialog;
    dialog = gtk_file_chooser_dialog_new(_("Load Domain File"),
                                         GTK_WINDOW(main_window_ccc),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                         NULL);
    if (log_file_ccc != NULL)
    {
      char *default_path = g_path_get_dirname(log_file_ccc);
      gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), default_path);
    }
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
      char *filename;
      filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
      fprintf(stdout, "%s\n", filename);
      load_domain_file_ccc(filename);
      g_free(filename);
    }
    gtk_widget_destroy(dialog);
  }
}

void add_domain_ccc(void)
{
  if (connected_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("You must disconnect first."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
  else if (1 && !do_domain_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error: Domain is not active or no domain file loaded"));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
  else
  {
    GtkWidget *dialog;
    dialog = gtk_file_chooser_dialog_new(_("Choose Destination Image"),
                                         GTK_WINDOW(main_window_ccc),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                         NULL);
    if (log_file_ccc != NULL)
    {
      char *default_path = g_path_get_dirname(log_file_ccc);
      gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), default_path);
    }
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
      char *filename;
      filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
      fprintf(stdout, "%s\n", filename);
      // load_domain_file_ccc(filename);
      add_domain_file_ccc(filename);
      g_free(filename);
    }
    gtk_widget_destroy(dialog);
  }
}

// Get the selected filename and load it
static void load_log_file_ccc(char *log_file)
{
  // g_print ("%s\n", gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs)));
  log_file_ccc = malloc(1024);
  memset(log_file_ccc, 0, 1024);
  snprintf(log_file_ccc, 1024, "%s", log_file);
  printf("%s\n", log_file_ccc);

  // initialize memory
  return_value_ccc = initialize_logfile_memory_ccc();
  if (return_value_ccc != 0)
  {
    exitcode_ccc = GENERAL_ERROR_EXIT_CODE;
  }

  else
  {
    int ret = read_log_file_ccc(log_file_ccc);
    int ret2 = 0;
    if (ret != 0)
    {
      snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error processing log file!\nThe log file was not loaded.\n"));
      message_error_ccc(tempmessage_ccc);
      print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
      clear_error_message_ccc();
    }
    else
    {
      check_log_size_ccc = 0;
      ret = check_log_ccc();
      // if only same status then repair log automatically
      if (ret == 0x10)
      {
        ret2 = check_and_repair_log_ccc();
        clear_error_message_ccc();
        ret = check_log_ccc();
      }
      check_log_size_ccc = 1;
      if (ret != 0 || ret2 != 0)
      {
        snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("The log file has errors!\nThe log file will need to be repaired before it can be used.\nPlease reference console messages for more information.\n"));
        message_error_ccc(tempmessage_ccc);
        print_gui_error_message_ccc(error_message_ccc, _("Warning"), 1);
        clear_error_message_ccc();
      }

      project_chosen_ccc = 1;
      new_project_ccc = 0;
      data_read_from_log_ccc = 1;
      clear_destination_ccc();
      clear_source_ccc();
      set_initial_data_from_log_ccc(new_project_ccc);
      logfile_changed_ccc = true;
      update_display_ccc(0);
    }
  }
}

// Get the selected ddrescue filename and load it
static void load_ddrescue_log_file_ccc(char *log_file)
{
  // g_print ("%s\n", gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs)));
  ddilog_file_ccc = malloc(1024);
  memset(ddilog_file_ccc, 0, 1024);
  snprintf(ddilog_file_ccc, 1024, "%s", log_file);
  printf("%s\n", ddilog_file_ccc);

  // initialize memory
  return_value_ccc = initialize_logfile_memory_ccc();
  if (return_value_ccc != 0)
  {
    exitcode_ccc = GENERAL_ERROR_EXIT_CODE;
  }

  else
  {
    int ret = read_ddrescue_log_ccc(ddilog_file_ccc);
    int ret2 = 0;
    if (ret != 0)
    {
      snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error processing log file!\nThe log file was not loaded.\n"));
      message_error_ccc(tempmessage_ccc);
      print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
      clear_error_message_ccc();
    }
    else
    {
      importdd_ccc = true;
      ret = check_log_ccc();
      if (ret)
      {
        snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "There were errors found in the progress log file\n");
        message_now_ccc(tempmessage_ccc);
        ret2 = check_and_repair_log_ccc();
        snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "Checking the progress log file again to make sure it was fixed correctly.\n");
        message_now_ccc(tempmessage_ccc);
        ret = check_log_ccc();
        if (ret != 0 || ret2 != 0)
        {
          snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("The log file has errors!\nThe log file will need to be repaired before it can be used.\nPlease reference console messages for more information.\n"));
          message_error_ccc(tempmessage_ccc);
          print_gui_error_message_ccc(error_message_ccc, _("Warning"), 1);
          clear_error_message_ccc();
        }
      }
      importdd_ccc = false;

      project_chosen_ccc = 1;
      new_project_ccc = 0;
      clear_destination_ccc();
      clear_source_ccc();
      set_initial_data_from_log_ccc(new_project_ccc);
      logfile_changed_ccc = true;
      update_display_ccc(0);
    }
  }
}

// Get the selected domain and load it
static void load_domain_file_ccc(char *log_file)
{
  // g_print ("%s\n", gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs)));
  clear_domain_ccc();
  strncpy(domain_file_ccc, log_file, 1024);
  printf("%s\n", domain_file_ccc);

  // initialize memory
  return_value_ccc = initialize_domainfile_memory_ccc();
  if (return_value_ccc != 0)
  {
    exitcode_ccc = GENERAL_ERROR_EXIT_CODE;
  }

  else
  {
    int ret = read_domain_file_ccc(domain_file_ccc);
    if (ret < 0)
    {
      snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error processing log file!\nThe log file was not loaded.\n"));
      message_error_ccc(tempmessage_ccc);
      print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
      clear_error_message_ccc();
      do_domain_ccc = false;
    }
    else
    {
      do_domain_ccc = true;
      update_display_ccc(0);
    }
  }
}

// Get the selected domain and load it
static void add_domain_file_ccc(char *log_file)
{
  // g_print ("%s\n", gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs)));
  printf("%s\n", log_file);

  int ret = read_domain_add_file_ccc(log_file);
  if (ret < 0)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error processing log file!\nThe log file was not loaded.\n"));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
  update_display_ccc(0);
}

void clear_domain_ccc(void)
{
  if (connected_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("You must disconnect first."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
  else
  {
    initialize_domainfile_memory_ccc();
    if (domain_file_ccc != NULL)
    {
      free(domain_file_ccc);
    }
    domain_file_ccc = malloc(1024);
    memset(domain_file_ccc, 0, 1024);
    do_domain_ccc = false;
  }
  update_display_ccc(0);
}

// Get the selected filename and set it
static void new_log_file_ccc(char *log_file)
{
  log_file_ccc = malloc(1024);
  memset(log_file_ccc, 0, 1024);
  snprintf(log_file_ccc, 1024, "%s", log_file);
  printf("%s\n", log_file_ccc);

  clear_domain_ccc();
  strncpy(domain_file_ccc, log_file_ccc, 1016);
  strcat(domain_file_ccc, ".domain");
  remove(domain_file_ccc);
  clear_domain_ccc();

  // initialize memory
  return_value_ccc = initialize_logfile_memory_ccc();
  if (return_value_ccc != 0)
  {
    exitcode_ccc = GENERAL_ERROR_EXIT_CODE;
  }
  project_chosen_ccc = 1;
  new_project_ccc = 1;
  data_read_from_log_ccc = 0;
  enable_extended_analyze_ccc = false;
  clear_destination_ccc();
  clear_source_ccc();
  clear_variables_for_new_project_ccc();
  initialize_new_log_ccc();
  logfile_changed_ccc = true;
  set_initial_data_from_log_ccc(new_project_ccc);
  logfile_changed_ccc = true;
  update_display_ccc(0);
}

static void new_domain_file_ccc(char *log_file)
{
  clear_domain_ccc();
  strncpy(domain_file_ccc, log_file, 1024);
  printf("%s\n", domain_file_ccc);

  // initialize memory
  return_value_ccc = initialize_domainfile_memory_ccc();
  if (return_value_ccc != 0)
  {
    exitcode_ccc = GENERAL_ERROR_EXIT_CODE;
  }
  initialize_domain_log_ccc();
  do_domain_ccc = true;
  domainfile_changed_ccc = true;
  update_display_ccc(0);
}

// open choose source dialog window
void choose_source_ccc(void)
{
  usb_lun_set_ccc = 0;
  bool use_rebuild_assist_bak = use_rebuild_assist_ccc;
  use_rebuild_assist_ccc = false;
  if (!project_chosen_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("You must choose a project first."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
  else if (connected_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("You must disconnect first."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
  else if (memory_failed_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Unable to get buffer physical address in 32 bit range.\nPlease try Starting the Virtual Disk Driver.\n\n Alternative is to try to choose the mode again (switch to different mode and back), or restart the program."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
  else if (copy_image_mode_ccc)
  {
    GtkWidget *dialog;
    dialog = gtk_file_chooser_dialog_new(_("Choose image file"),
                                         GTK_WINDOW(main_window_ccc),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                         NULL);
    if (log_file_ccc != NULL)
    {
      char *default_path = g_path_get_dirname(log_file_ccc);
      gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), default_path);
    }
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
      char *filename;
      filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
      fprintf(stdout, "%s\n", filename);
      free(disk_1_ccc);
      disk_1_ccc = malloc(strlen(filename) + 2);
      strcpy(disk_1_ccc, filename);
      g_free(filename);
      current_source_model_ccc[0] = '\0';
      current_source_serial_ccc[0] = '\0';
      update_display_ccc(0);

      if (!connect_source_disk_ccc() && !process_source_ccc() && !check_and_adjust_variables_after_choosing_source_ccc(new_project_ccc))
      {
        source_chosen_ccc = 1;
        if (new_project_ccc)
        {
          initialize_new_log_ccc();
          logfile_changed_ccc = true;
          set_initial_data_from_log_ccc(new_project_ccc);
          logfile_changed_ccc = true;
        }
        clear_domain_ccc();
        strncpy(domain_file_ccc, log_file_ccc, 1016);
        strcat(domain_file_ccc, ".domain");
        printf("%s\n", domain_file_ccc);
        if (access(domain_file_ccc, F_OK) != -1)
        {
          int ret = read_domain_file_ccc(domain_file_ccc);
          if (ret < 0)
          {
            snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error processing log file!\nThe log file was not loaded.\n"));
            message_error_ccc(tempmessage_ccc);
            print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
            clear_error_message_ccc();
            clear_domain_ccc();
            // initialize_domainfile_memory_ccc();
            // initialize_domain_log_ccc();
          }
        }
        else
        {
          initialize_domain_log_ccc();
          // domainfile_changed_ccc = true;
        }
      }
      else
      {
        fprintf(stdout, "error selecting source\n");
        clear_source_ccc();
        current_source_model_ccc[0] = '\0';
        current_source_serial_ccc[0] = '\0';
        snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "%s", _("Error selecting source"));
        message_error_ccc(tempmessage_ccc);
        print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
        clear_error_message_ccc();
      }
      release_devices_ccc();
      update_display_ccc(0);
    }
    gtk_widget_destroy(dialog);
  }
  else
  {
    if (memory_used_ccc)
    {
      initialize_memory_ccc();
    }
    find_all_devices_ccc();

    choose_source_dialog_window_ccc = gtk_dialog_new_with_buttons(_("Choose Source Drive"), GTK_WINDOW(main_window_ccc), GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);
    // gtk_window_set_title (GTK_WINDOW (choose_source_dialog_window_ccc), "GtkScrolledWindow example");
    gtk_container_set_border_width(GTK_CONTAINER(choose_source_dialog_window_ccc), 0);
    gtk_widget_set_size_request(choose_source_dialog_window_ccc, 600, 400);

    choose_source_scrolled_window_ccc = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_set_border_width(GTK_CONTAINER(choose_source_scrolled_window_ccc), 10);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(choose_source_scrolled_window_ccc), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    // gtk_box_pack_start (GTK_BOX (GTK_DIALOG(choose_source_dialog_window_ccc)->vbox), choose_source_scrolled_window_ccc, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(choose_source_dialog_window_ccc))), choose_source_scrolled_window_ccc, TRUE, TRUE, 0);

    choose_source_vbox_ccc = gtk_vbox_new(FALSE, 0);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(choose_source_scrolled_window_ccc), choose_source_vbox_ccc);

    GtkWidget *button[MAX_DEVICES];

    if (ahci_mode_ccc)
    {
      int i;
      for (i = 0; i < device_count_ccc; i++)
      {
        char button_label[MAX_BUTTON_LABEL_SIZE] = "";
        if (verbose_ccc & DEBUG6)
        {
          snprintf(button_label, MAX_BUTTON_LABEL_SIZE, "%s %s %s %s %llx %d %llx (%lld) %s %s", device_driver_ccc[i], device_bus_ccc[i], device_reference_ccc[i], device_name_ccc[i], hba_address_ccc[i], port_number_ccc[i], port_address_ccc[i], drive_size_ccc[i], model_ccc[i], serial_ccc[i]);
        }
        else if (verbose_ccc & DEBUG5)
        {
          snprintf(button_label, MAX_BUTTON_LABEL_SIZE, "%s %s %llx %d %llx (%lld) %s %s", device_reference_ccc[i], device_name_ccc[i], hba_address_ccc[i], port_number_ccc[i], port_address_ccc[i], drive_size_ccc[i], model_ccc[i], serial_ccc[i]);
        }
        else
        {
          snprintf(button_label, MAX_BUTTON_LABEL_SIZE, "%s %s (%lld) %s %s", device_reference_ccc[i], device_name_ccc[i], drive_size_ccc[i], model_ccc[i], serial_ccc[i]);
        }
        button[i] = gtk_button_new_with_label(button_label);
        gtk_button_set_alignment(GTK_BUTTON(button[i]), 0, .5);
        g_signal_connect(button[i], "clicked", G_CALLBACK(get_source_from_button_ccc), GINT_TO_POINTER(i));
      }
    }
    else if (direct_mode_ccc)
    {
      int i;
      for (i = 0; i < device_count_ccc; i++)
      {
        char button_label[MAX_BUTTON_LABEL_SIZE] = "";
        if (verbose_ccc & DEBUG6)
        {
          snprintf(button_label, MAX_BUTTON_LABEL_SIZE, "%s %s %s %s %llx %llx %llx %d (%lld) %s %s", device_driver_ccc[i], device_bus_ccc[i], device_reference_ccc[i], device_name_ccc[i], reg_address_ccc[i], control_address_ccc[i], bus_address_ccc[i], device_select_ccc[i], drive_size_ccc[i], model_ccc[i], serial_ccc[i]);
        }
        else if (verbose_ccc & DEBUG5)
        {
          snprintf(button_label, MAX_BUTTON_LABEL_SIZE, "%s %s %llx %llx %llx %d (%lld) %s %s", device_reference_ccc[i], device_name_ccc[i], reg_address_ccc[i], control_address_ccc[i], bus_address_ccc[i], device_select_ccc[i], drive_size_ccc[i], model_ccc[i], serial_ccc[i]);
        }
        else
        {
          snprintf(button_label, MAX_BUTTON_LABEL_SIZE, "%s %s (%lld) %s %s", device_reference_ccc[i], device_name_ccc[i], drive_size_ccc[i], model_ccc[i], serial_ccc[i]);
        }
        button[i] = gtk_button_new_with_label(button_label);
        gtk_button_set_alignment(GTK_BUTTON(button[i]), 0, .5);
        g_signal_connect(button[i], "clicked", G_CALLBACK(get_source_from_button_ccc), GINT_TO_POINTER(i));
      }
    }
    else if (usb_mode_ccc)
    {
      int i;
      for (i = 0; i < usb_device_count_ccc; i++)
      {
        char button_label[MAX_BUTTON_LABEL_SIZE] = "";
        if (verbose_ccc & DEBUG6)
        {
          snprintf(button_label, MAX_BUTTON_LABEL_SIZE, "%d:%d %04x:%04x %02x %02x %d %s %s %s\n", usb_bus_real_number_ccc[i], usb_device_number_ccc[i], usb_vendor_id_ccc[i], usb_product_id_ccc[i], usb_bulk_in_endpoint_ccc[i], usb_bulk_out_endpoint_ccc[i], usb_mass_storage_ccc[i], usb_vendor_string_ccc[i], usb_product_string_ccc[i], usb_extra_id_string_ccc[i]);
        }
        else if (verbose_ccc & DEBUG5)
        {
          snprintf(button_label, MAX_BUTTON_LABEL_SIZE, "%d:%d %04x:%04x %s %s %s\n", usb_bus_real_number_ccc[i], usb_device_number_ccc[i], usb_vendor_id_ccc[i], usb_product_id_ccc[i], usb_vendor_string_ccc[i], usb_product_string_ccc[i], usb_extra_id_string_ccc[i]);
        }
        else
        {
          snprintf(button_label, MAX_BUTTON_LABEL_SIZE, "%d:%d %04x:%04x %s %s\n", usb_bus_real_number_ccc[i], usb_device_number_ccc[i], usb_vendor_id_ccc[i], usb_product_id_ccc[i], usb_vendor_string_ccc[i], usb_product_string_ccc[i]);
        }
        button[i] = gtk_button_new_with_label(button_label);
        gtk_button_set_alignment(GTK_BUTTON(button[i]), 0, .5);
        g_signal_connect(button[i], "clicked", G_CALLBACK(get_usb_from_button_ccc), GINT_TO_POINTER(i));
      }
    }
    else
    {
      int i;
      for (i = 0; i < device_count_ccc; i++)
      {
        char button_label[MAX_BUTTON_LABEL_SIZE] = "";
        snprintf(button_label, MAX_BUTTON_LABEL_SIZE, "%s (%lld) %s %s", drive_list_ccc[i], drive_size_ccc[i], model_ccc[i], serial_ccc[i]);
        button[i] = gtk_button_new_with_label(button_label);
        gtk_button_set_alignment(GTK_BUTTON(button[i]), 0, .5);
        g_signal_connect(button[i], "clicked", G_CALLBACK(get_source_from_button_ccc), GINT_TO_POINTER(i));
      }
    }

    int i;
    if (usb_mode_ccc)
    {
      for (i = 0; i < usb_device_count_ccc; i++)
      {
        if ((verbose_ccc & DEBUG6) || (verbose_ccc & DEBUG5) || usb_mass_storage_ccc[i])
        {
          gtk_box_pack_start(GTK_BOX(choose_source_vbox_ccc), button[i], FALSE, FALSE, 0);
        }
      }
    }
    else
    {
      for (i = 0; i < device_count_ccc; i++)
      {
        gtk_box_pack_start(GTK_BOX(choose_source_vbox_ccc), button[i], FALSE, FALSE, 0);
      }
    }

    gtk_widget_show_all(choose_source_dialog_window_ccc);
    gint result = gtk_dialog_run(GTK_DIALOG(choose_source_dialog_window_ccc));
    gtk_widget_destroy(choose_source_dialog_window_ccc);

    if (result == GTK_RESPONSE_ACCEPT)
    {
      // fprintf (stdout, "OK\n");

      // disable the window while it is processing the source
      gtk_widget_set_sensitive(GTK_WIDGET(main_window_ccc), FALSE);

      clear_source_ccc();
      int ret = choose_device_ccc();
      if (ahci_mode_ccc)
      {
        hba_reset_address_ccc = hba_base_address_ccc;
        port_reset_address_ccc = port_base_address_ccc;
      }
      if (ret)
      {
        if (ret == -100)
        {
          clear_source_ccc();
          current_source_model_ccc[0] = '\0';
          current_source_serial_ccc[0] = '\0';
          snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "%s", _("Error selecting source"));
          message_error_ccc(tempmessage_ccc);
          print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
          clear_error_message_ccc();
        }
        else if (ret == -200)
        {
          fprintf(stdout, "error selecting source, ret=%d\n", ret);
          clear_source_ccc();
          current_source_model_ccc[0] = '\0';
          current_source_serial_ccc[0] = '\0';
          snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "%s", _("Error selecting source"));
          message_error_ccc(tempmessage_ccc);
          print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
          clear_error_message_ccc();
        }
        else
        {
          fprintf(stdout, "error selecting source, ret=%d\n", ret);
          clear_source_ccc();
          current_source_model_ccc[0] = '\0';
          current_source_serial_ccc[0] = '\0';
          snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "%s", _("Error selecting source"));
          message_error_ccc(tempmessage_ccc);
          print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
          clear_error_message_ccc();
        }
      }
      else if (!connect_source_disk_ccc() && !process_source_ccc() && !check_and_adjust_variables_after_choosing_source_ccc(new_project_ccc))
      {
        source_chosen_ccc = 1;
        if (new_project_ccc)
        {
          initialize_new_log_ccc();
          logfile_changed_ccc = true;
          set_initial_data_from_log_ccc(new_project_ccc);
          logfile_changed_ccc = true;
        }
        clear_domain_ccc();
        strncpy(domain_file_ccc, log_file_ccc, 1016);
        strcat(domain_file_ccc, ".domain");
        printf("%s\n", domain_file_ccc);
        if (access(domain_file_ccc, F_OK) != -1)
        {
          int ret = read_domain_file_ccc(domain_file_ccc);
          if (ret < 0)
          {
            snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error processing log file!\nThe log file was not loaded.\n"));
            message_error_ccc(tempmessage_ccc);
            print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
            clear_error_message_ccc();
            clear_domain_ccc();
            // initialize_domainfile_memory_ccc();
            // initialize_domain_log_ccc();
          }
        }
        else
        {
          initialize_domain_log_ccc();
          // domainfile_changed_ccc = true;
        }
      }
      else
      {
        fprintf(stdout, "error selecting source\n");
        clear_source_ccc();
        current_source_model_ccc[0] = '\0';
        current_source_serial_ccc[0] = '\0';
        snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "%s", _("Error selecting source"));
        message_error_ccc(tempmessage_ccc);
        print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
        clear_error_message_ccc();
      }
      release_devices_ccc();
      update_display_ccc(0);

      // re-enable the window
      gtk_widget_set_sensitive(GTK_WIDGET(main_window_ccc), TRUE);

      check_source_against_templates_ccc();
    }
  }
  use_rebuild_assist_ccc = use_rebuild_assist_bak;
}

void get_source_from_button_ccc(GtkWidget *w, gpointer data)
{
  new_source_ccc = GPOINTER_TO_INT(data);
  g_print("selection=%d  \n", new_source_ccc);
}

void choose_destination_drive_ccc(void)
{
  destination_is_generic_ccc = false;
  choose_destination_ccc();
}

void choose_destination_block_ccc(void)
{
  destination_is_generic_ccc = true;
  choose_destination_ccc();
}

// open choose destination dialog window
void choose_destination_ccc(void)
{
  if (!project_chosen_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("You must choose a project first."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
  else if (connected_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("You must disconnect first."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
  else
  {
    bool direct_mode_bak = direct_mode_ccc;
    bool ahci_mode_bak = ahci_mode_ccc;
    bool ata_passthrough_bak = ata_passthrough_ccc;
    bool scsi_passthrough_bak = scsi_passthrough_ccc;
    bool generic_mode_ccc_bak = generic_mode_ccc;
    bool usb_mode_bak = usb_mode_ccc;

    direct_mode_ccc = false;
    ahci_mode_ccc = false;
    ata_passthrough_ccc = false;
    scsi_passthrough_ccc = !destination_is_generic_ccc;
    generic_mode_ccc = destination_is_generic_ccc;
    usb_mode_ccc = false;
    if (memory_used_ccc)
    {
      initialize_memory_ccc();
    }
    find_all_devices_ccc();
    direct_mode_ccc = direct_mode_bak;
    ahci_mode_ccc = ahci_mode_bak;
    ata_passthrough_ccc = ata_passthrough_bak;
    scsi_passthrough_ccc = scsi_passthrough_bak;
    generic_mode_ccc = generic_mode_ccc_bak;
    usb_mode_ccc = usb_mode_bak;

    choose_destination_dialog_window_ccc = gtk_dialog_new_with_buttons(_("Destination (All data will be overwritten!)"), GTK_WINDOW(main_window_ccc), GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);
    // gtk_window_set_title (GTK_WINDOW (choose_source_dialog_window_ccc), "GtkScrolledWindow example");
    gtk_container_set_border_width(GTK_CONTAINER(choose_destination_dialog_window_ccc), 0);
    gtk_widget_set_size_request(choose_destination_dialog_window_ccc, 600, 400);

    choose_destination_scrolled_window_ccc = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_set_border_width(GTK_CONTAINER(choose_destination_scrolled_window_ccc), 10);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(choose_destination_scrolled_window_ccc), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    // gtk_box_pack_start (GTK_BOX (GTK_DIALOG(choose_destination_dialog_window_ccc)->vbox), choose_destination_scrolled_window_ccc, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(choose_destination_dialog_window_ccc))), choose_destination_scrolled_window_ccc, TRUE, TRUE, 0);

    choose_destination_vbox_ccc = gtk_vbox_new(FALSE, 0);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(choose_destination_scrolled_window_ccc), choose_destination_vbox_ccc);

    GtkWidget *button[MAX_DEVICES];

    int i;
    for (i = 0; i < device_count_ccc; i++)
    {
      char button_label[MAX_BUTTON_LABEL_SIZE] = "";
      snprintf(button_label, MAX_BUTTON_LABEL_SIZE, "%s (%lld) %s %s", drive_list_ccc[i], drive_size_ccc[i], model_ccc[i], serial_ccc[i]);
      button[i] = gtk_button_new_with_label(button_label);
      gtk_button_set_alignment(GTK_BUTTON(button[i]), 0, .5);
      g_signal_connect(button[i], "clicked", G_CALLBACK(get_destination_from_button_ccc), GINT_TO_POINTER(i));
    }

    for (i = 0; i < device_count_ccc; i++)
    {
      gtk_box_pack_start(GTK_BOX(choose_destination_vbox_ccc), button[i], FALSE, FALSE, 0);
    }

    gtk_widget_show_all(choose_destination_dialog_window_ccc);
    gint result = gtk_dialog_run(GTK_DIALOG(choose_destination_dialog_window_ccc));
    gtk_widget_destroy(choose_destination_dialog_window_ccc);

    if (result == GTK_RESPONSE_ACCEPT)
    {
      // fprintf (stdout, "OK\n");
      generic_mode_ccc = destination_is_generic_ccc;
      int ret = choose_target_ccc();
      generic_mode_ccc = generic_mode_ccc_bak;
      if (ret)
      {
        fprintf(stdout, "error selecting destination, ret=%d\n", ret);
        clear_destination_ccc();
        snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "%s", _("Error selecting destination"));
        message_error_ccc(tempmessage_ccc);
        print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
        clear_error_message_ccc();
      }
      else
      {
        destination_chosen_ccc = 1;
        destination_size_valid_ccc = 1;
        logfile_changed_ccc = true;

        if (driver_only_ccc)
        {
          clear_domain_ccc();
          strncpy(domain_file_ccc, log_file_ccc, 1016);
          strcat(domain_file_ccc, ".domain");
          printf("%s\n", domain_file_ccc);
          if (access(domain_file_ccc, F_OK) != -1)
          {
            int ret = read_domain_file_ccc(domain_file_ccc);
            if (ret < 0)
            {
              snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error processing log file!\nThe log file was not loaded.\n"));
              message_error_ccc(tempmessage_ccc);
              print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
              clear_error_message_ccc();
              clear_domain_ccc();
              // initialize_domainfile_memory_ccc();
              // initialize_domain_log_ccc();
            }
          }
          else
          {
            initialize_domain_log_ccc();
            domainfile_changed_ccc = true;
          }
        }
      }
      update_display_ccc(0);
    }
  }
}

void get_destination_from_button_ccc(GtkWidget *w, gpointer data)
{
  new_destination_ccc = GPOINTER_TO_INT(data);
  g_print("selection=%d  \n", new_destination_ccc);
}

void choose_image_ccc(void)
{
  if (!project_chosen_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("You must choose a project first."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
  else if (connected_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("You must disconnect first."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
  else
  {
    GtkWidget *dialog;
    dialog = gtk_file_chooser_dialog_new(_("Choose image file"),
                                         GTK_WINDOW(main_window_ccc),
                                         GTK_FILE_CHOOSER_ACTION_SAVE,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                         NULL);
    // gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
    if (log_file_ccc != NULL)
    {
      char *default_path = g_path_get_dirname(log_file_ccc);
      gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), default_path);
    }
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
      int confirmed = 1;
      char *filename;
      filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
      if (access(filename, F_OK) == 0)
      {
        snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "   %s   \n%s", filename, _("The image file currently exists.\nThis will continue the recovery using this image file."));
        if (!open_confirmation_dialog_ccc(tempmessage_ccc))
        {
          confirmed = 0;
        }
      }
      if (confirmed)
      {
        fprintf(stdout, "%s\n", filename);
        free(disk_2_ccc);
        disk_2_ccc = malloc(strlen(filename) + 2);
        strcpy(disk_2_ccc, filename);
        destination_chosen_ccc = 1;
        destination_size_valid_ccc = 0;
        current_destination_model_ccc[0] = '\0';
        current_destination_serial_ccc[0] = '\0';
        logfile_changed_ccc = true;

        if (driver_only_ccc)
        {
          clear_domain_ccc();
          strncpy(domain_file_ccc, log_file_ccc, 1016);
          strcat(domain_file_ccc, ".domain");
          printf("%s\n", domain_file_ccc);
          if (access(domain_file_ccc, F_OK) != -1)
          {
            int ret = read_domain_file_ccc(domain_file_ccc);
            if (ret < 0)
            {
              snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error processing log file!\nThe log file was not loaded.\n"));
              message_error_ccc(tempmessage_ccc);
              print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
              clear_error_message_ccc();
              clear_domain_ccc();
              // initialize_domainfile_memory_ccc();
              // initialize_domain_log_ccc();
            }
          }
          else
          {
            initialize_domain_log_ccc();
            domainfile_changed_ccc = true;
          }
        }

        update_display_ccc(0);
      }
      g_free(filename);
      gtk_widget_destroy(dialog);
    }
  }
}

void choose_null_ccc(void)
{
  if (!project_chosen_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("You must choose a project first."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
  else if (connected_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("You must disconnect first."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
  else
  {
    fprintf(stdout, "/dev/null\n");
    free(disk_2_ccc);
    disk_2_ccc = malloc(16);
    strcpy(disk_2_ccc, "/dev/null");
    strcpy(current_destination_model_ccc, _("NULL (No destination)"));
    strcpy(current_destination_serial_ccc, "");
    logfile_changed_ccc = true;
    update_display_ccc(0);
    destination_chosen_ccc = 1;
    destination_size_valid_ccc = 0;
  }
}

void disconnect_devices_ccc(void)
{
  release_devices_ccc();
  connected_ccc = 0;
  // strcpy (tempmessage_ccc, _("Devices now disconnected"));
  // message_error_ccc(tempmessage_ccc);
  // print_gui_error_message_ccc(error_message_ccc, _("Success!"), 0);
  // clear_error_message_ccc();
  set_disconnected_ccc();
  ata_status_ccc = 0;
  ata_error_ccc = 0;
  update_display_ccc(0);
}

void connect_devices_ccc(void)
{
  // check if we're already connected
  if (connected_ccc)
  {
    disconnect_devices_ccc();
    return;
  }

  int relaycheck = 1;
  if ((activate_primary_relay_on_error_ccc || power_cycle_primary_relay_ccc) && !(fill_mode_ccc || driver_only_ccc) && !(usbr1_chosen_ccc || asclepius_get_connection_status()))
  {
    relaycheck = 0;
  }
  if (memory_failed_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Unable to get buffer physical address in 32 bit range.\nPlease try Starting the Virtual Disk Driver.\n\n Alternative is to try to choose the mode again (switch to different mode and back), or restart the program."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
  else if (relaycheck && project_chosen_ccc && destination_chosen_ccc && (fill_mode_ccc || driver_only_ccc))
  {
    check_log_size_ccc = 0;
    // check log and if only same status then repair automatically
    int ret = check_log_ccc();
    int ret2 = 0;
    if (ret == 0x10)
    {
      clear_error_message_ccc();
      ret2 = check_and_repair_log_ccc();
      ret = check_log_ccc();
    }
    if (new_project_ccc && !fill_mode_ccc)
    {
      snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("No source chosen\n"));
      message_error_ccc(tempmessage_ccc);
      print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
      clear_error_message_ccc();
      set_disconnected_ccc();
    }
    else if (ret || ret2)
    {
      snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("The log file has errors!\nThe log file will need to be repaired before it can be used.\nPlease reference console messages for more information.\n"));
      message_error_ccc(tempmessage_ccc);
      print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
      clear_error_message_ccc();
      set_disconnected_ccc();
    }
    else if (driver_only_ccc && !driver_installed_ccc)
    {
      snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("The virtual disk device driver is not running"));
      message_error_ccc(tempmessage_ccc);
      print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
      clear_error_message_ccc();
      set_disconnected_ccc();
    }
    else
    {
      if (open_target_destination_ccc())
      {
        release_devices_ccc();
        snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Unable to connect devices"));
        message_error_ccc(tempmessage_ccc);
        print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
        clear_error_message_ccc();
        set_disconnected_ccc();
      }
      else
      {
        // strcpy (tempmessage_ccc, _("Devices now connected"));
        // message_error_ccc(tempmessage_ccc);
        // print_gui_error_message_ccc(error_message_ccc, _("Success!"), 0);
        // clear_error_message_ccc();
        check_and_adjust_variables_after_connection_ccc();
        set_connected_ccc();
      }
    }
  }
  else if (relaycheck && project_chosen_ccc && source_chosen_ccc && destination_chosen_ccc)
  {
    check_log_size_ccc = 1;
    // check log and if only same status then repair automatically
    int ret = check_log_ccc();
    int ret2 = 0;
    if (ret == 0x10)
    {
      clear_error_message_ccc();
      ret2 = check_and_repair_log_ccc();
      ret = check_log_ccc();
    }
    if (check_same_device_ccc())
    {
      snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Source and Destination cannot be the same device"));
      message_error_ccc(tempmessage_ccc);
      print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
      clear_error_message_ccc();
      set_disconnected_ccc();
    }
    else if (ret || ret2)
    {
      snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("The log file has errors!\nThe log file will need to be repaired before it can be used.\nPlease reference console messages for more information.\n"));
      message_error_ccc(tempmessage_ccc);
      print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
      clear_error_message_ccc();
      set_disconnected_ccc();
    }
    else
    {
      if (connect_source_disk_ccc() || open_target_destination_ccc())
      {
        release_devices_ccc();
        snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Unable to connect devices"));
        message_error_ccc(tempmessage_ccc);
        print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
        clear_error_message_ccc();
        set_disconnected_ccc();
      }
      else
      {
        // strcpy (tempmessage_ccc, _("Devices now connected"));
        // message_error_ccc(tempmessage_ccc);
        // print_gui_error_message_ccc(error_message_ccc, _("Success!"), 0);
        // clear_error_message_ccc();
        if (!enable_scsi_write_ccc && destination_size_valid_ccc && source_total_size_ccc > target_total_size_ccc)
        {
          fprintf(stdout, "Warning! Destination is smaller than souce.\n");
          fprintf(stdout, "Source size= %lld\n", source_total_size_ccc);
          fprintf(stdout, "Target size= %lld\n", target_total_size_ccc);
          snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Warning! The destination is smaller than the source.\nSome data may not be copied."));
          message_error_ccc(tempmessage_ccc);
          print_gui_error_message_ccc(error_message_ccc, _("Warning"), 1);
          clear_error_message_ccc();
        }
        check_and_adjust_variables_after_connection_ccc();
        set_connected_ccc();
        new_project_ccc = 0;
      }
    }
  }
  else
  {
    if (!project_chosen_ccc)
    {
      snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("No project chosen\n"));
      message_error_ccc(tempmessage_ccc);
    }
    if (!source_chosen_ccc && !(fill_mode_ccc || driver_only_ccc))
    {
      snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("No source chosen\n"));
      message_error_ccc(tempmessage_ccc);
    }
    if (!destination_chosen_ccc)
    {
      snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("No destination chosen\n"));
      message_error_ccc(tempmessage_ccc);
    }
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("You must choose a project, source, and destination before connecting."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
    if (!relaycheck)
    {
      snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error: No relay has been chosen"));
      message_error_ccc(tempmessage_ccc);
      print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
      clear_error_message_ccc();
    }
  }
  ata_status_ccc = 0;
  ata_error_ccc = 0;
  update_display_ccc(0);
}

void start_display_status_update_timer_ccc(void)
{
  // g_print ("start=%d timer=%d\n", display_status_timer_ccc, display_status_timer_running_ccc);    //debug
  if (direct_mode_ccc && !display_status_timer_running_ccc)
  {
    display_status_timer_ccc = gdk_threads_add_timeout(500, display_status_update_action_ccc, NULL);
    display_status_timer_running_ccc = 1;
  }
}

void stop_display_status_update_timer_ccc(void)
{
  if (display_status_timer_running_ccc)
  {
    // gtk_timeout_remove (display_status_timer_ccc);
    display_status_timer_running_ccc = 0;
  }
}

gint display_status_update_action_ccc(gpointer data)
{
  if (fill_mode_ccc || !connected_ccc || running_clone_ccc || !display_status_timer_running_ccc)
  {
    display_status_timer_running_ccc = 0;
    return 0;
  }
  // g_print ("timer\n");    //debug
  refresh_status_ccc(current_disk_ccc);
  update_gui_status_buttons_ccc();
  data = data;
  return 1;
}

gint display_status_update_asclepius_action(gpointer data)
{
  update_asclepius_status();

  return 1;
}

void set_connected_ccc(void)
{
  if (driver_mode_ccc && (strcmp(disk_2_ccc, "/dev/null") == 0))
  {
    clear_error_message_ccc();
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Warning! Using driver mode with destination of null will clear the current progress log.\nOnce you click on Start, the log will be cleared.\nIf you wish to keep the current log, disconnect and make a copy."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Warning"), 0);
    clear_error_message_ccc();
  }
  connected_ccc = 1;
  gtk_button_set_label(GTK_BUTTON(connect_button_ccc), _("Disconnect"));
  gtk_widget_set_tooltip_text(GTK_WIDGET(connect_button_ccc), _("Disconnect from the device"));
  gtk_widget_set_sensitive(GTK_WIDGET(start_button_ccc), TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(modemi_ccc), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(drivesmi_ccc), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(primaryrelaymi_ccc), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(chooseprimaryrelaymi_ccc), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(soft_reset_button_ccc), TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(hard_reset_button_ccc), TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(clone_mode_button_ccc), driver_only_ccc ? FALSE : TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(analyze_button_ccc), TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(smart_button_ccc), TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(activate_primary_relay_button_main_ccc), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(deactivate_primary_relay_button_main_ccc), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(analyze_long_button_ccc), TRUE);
  if (driver_installed_ccc)
  {
    set_driver_mode_button_status_ccc(TRUE);
  }
  start_display_status_update_timer_ccc();
  stop_signal_ccc = false;
  process_dma_mode_ccc();
}

void set_disconnected_ccc(void)
{
  connected_ccc = 0;
  gtk_button_set_label(GTK_BUTTON(connect_button_ccc), _("Connect"));
  gtk_widget_set_tooltip_text(GTK_WIDGET(connect_button_ccc), _("Connect to the device"));
  gtk_widget_set_sensitive(GTK_WIDGET(start_button_ccc), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(analyze_button_ccc), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(analyze_long_button_ccc), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(smart_button_ccc), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(modemi_ccc), TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(drivesmi_ccc), TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(primaryrelaymi_ccc), TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(chooseprimaryrelaymi_ccc), TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(asclepiusextmi_ccc), TRUE);
  if(usbr1_chosen_ccc || asclepius_get_connection_status())
  {
    gtk_widget_set_sensitive(GTK_WIDGET(activate_primary_relay_button_main_ccc), TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(deactivate_primary_relay_button_main_ccc), TRUE);
  }
  else
  {
    gtk_widget_set_sensitive(GTK_WIDGET(activate_primary_relay_button_main_ccc), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(deactivate_primary_relay_button_main_ccc), FALSE);
  }
  gtk_widget_set_sensitive(GTK_WIDGET(soft_reset_button_ccc), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(hard_reset_button_ccc), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(clone_mode_button_ccc), FALSE);
  set_driver_mode_button_status_ccc(FALSE);
  stop_display_status_update_timer_ccc();
}

void clear_source_ccc(void)
{
  source_chosen_ccc = 0;
  disk_1_ccc = NULL;
  current_source_model_ccc[0] = '\0';
  current_source_serial_ccc[0] = '\0';
  source_total_size_ccc = 0;
}

void clear_destination_ccc(void)
{
  destination_chosen_ccc = 0;
  disk_2_ccc = NULL;
  current_destination_model_ccc[0] = '\0';
  current_destination_serial_ccc[0] = '\0';
}

void update_gui_display_ccc(void)
{
  gtk_label_set_text(GTK_LABEL(data_project_ccc), display_output_ccc.logfile);
  gtk_label_set_text(GTK_LABEL(data_domain_ccc), display_output_ccc.domainfile);
  gtk_label_set_text(GTK_LABEL(data_source_ccc), display_output_ccc.source);
  gtk_label_set_text(GTK_LABEL(data_destination_ccc), display_output_ccc.destination);
  gtk_label_set_text(GTK_LABEL(data_totallba_ccc), display_output_ccc.totallba);
  gtk_label_set_text(GTK_LABEL(data_lbatoread_ccc), display_output_ccc.lbatoread);
  gtk_label_set_text(GTK_LABEL(data_domainsize_ccc), display_output_ccc.domainsize);
  gtk_label_set_text(GTK_LABEL(data_runtime_ccc), display_output_ccc.runtime);
  gtk_label_set_text(GTK_LABEL(data_remainingtime_ccc), display_output_ccc.remainingtime);
  gtk_label_set_text(GTK_LABEL(data_currentposition_ccc), display_output_ccc.currentposition);
  gtk_label_set_text(GTK_LABEL(data_currentstatus_ccc), display_output_ccc.currentstatus);
  gtk_label_set_text(GTK_LABEL(data_currentrate_ccc), display_output_ccc.currentrate);
  gtk_label_set_text(GTK_LABEL(data_recentrate_ccc), display_output_ccc.recentrate);
  gtk_label_set_text(GTK_LABEL(data_totalrate_ccc), display_output_ccc.totalrate);
  gtk_label_set_text(GTK_LABEL(data_skipsize_ccc), display_output_ccc.skipsize);
  gtk_label_set_text(GTK_LABEL(data_slowskips_ccc), display_output_ccc.slowskips);
  gtk_label_set_text(GTK_LABEL(data_skips_ccc), display_output_ccc.skips);
  gtk_label_set_text(GTK_LABEL(data_skipruns_ccc), display_output_ccc.skipruns);
  gtk_label_set_text(GTK_LABEL(data_skipresets_ccc), display_output_ccc.skipresets);
  gtk_label_set_text(GTK_LABEL(data_runsize_ccc), display_output_ccc.runsize);
  gtk_label_set_text(GTK_LABEL(data_finished_ccc), display_output_ccc.finished);
  gtk_label_set_text(GTK_LABEL(data_nontried_ccc), display_output_ccc.nontried);
  gtk_label_set_text(GTK_LABEL(data_nontrimmed_ccc), display_output_ccc.nontrimmed);
  gtk_label_set_text(GTK_LABEL(data_nondivided_ccc), display_output_ccc.nondivided);
  gtk_label_set_text(GTK_LABEL(data_nonscraped_ccc), display_output_ccc.nonscraped);
  gtk_label_set_text(GTK_LABEL(data_bad_ccc), display_output_ccc.bad);
  gtk_label_set_text(GTK_LABEL(data_datapreview_ccc), display_output_ccc.datapreview);
  gtk_label_set_text(GTK_LABEL(data_longestreadtime_ccc), display_output_ccc.longestreadtime);
  gtk_label_set_text(GTK_LABEL(data_filled_ccc), display_output_ccc.totalfilled);
  gtk_label_set_text(GTK_LABEL(data_retried_ccc), display_output_ccc.retried);
  gtk_label_set_text(GTK_LABEL(data_retrypasses_ccc), display_output_ccc.retrypasses);
  // gtk_label_set_text(GTK_LABEL(), display_output_ccc.);
  // gtk_label_set_text(GTK_LABEL(), display_output_ccc.);
  // gtk_label_set_text(GTK_LABEL(), display_output_ccc.);
  // gtk_label_set_text(GTK_LABEL(), display_output_ccc.);

  update_status_buttons_ccc();
  update_asclepius_status();
  run_gtk_gui_update_ccc();
}

void update_gui_status_buttons_ccc(void)
{
  update_status_buttons_ccc();
  run_gtk_gui_update_ccc();
}

void run_gtk_gui_update_ccc(void)
{
  while (gtk_events_pending())
  {
    gtk_main_iteration();
  }
}

void set_stop_signal_ccc(void)
{
  stop_signal_ccc = true;
}

void set_clone_mode_ccc(void)
{
  if (driver_mode_ccc)
  {
    do_domain_ccc = true;
  }
  driver_mode_ccc = 0;
  gtk_label_set_text(GTK_LABEL(data_drivermode_ccc), _("Clone"));
  update_display_ccc(0);
}

void set_driver_mode1_ccc(void)
{
  do_domain_ccc = false;
  check_driver_null_condition_ccc();
  driver_mode_ccc = 1;
  gtk_label_set_text(GTK_LABEL(data_drivermode_ccc), _("Mode 1"));
  update_display_ccc(0);
}

void set_driver_mode2_ccc(void)
{
  do_domain_ccc = false;
  check_driver_null_condition_ccc();
  driver_mode_ccc = 2;
  gtk_label_set_text(GTK_LABEL(data_drivermode_ccc), _("Mode 2"));
  update_display_ccc(0);
}

void set_driver_mode3_ccc(void)
{
  do_domain_ccc = false;
  check_driver_null_condition_ccc();
  driver_mode_ccc = 3;
  gtk_label_set_text(GTK_LABEL(data_drivermode_ccc), _("Mode 3"));
  update_display_ccc(0);
}

void set_driver_mode4_ccc(void)
{
  do_domain_ccc = false;
  check_driver_null_condition_ccc();
  driver_mode_ccc = 4;
  gtk_label_set_text(GTK_LABEL(data_drivermode_ccc), _("Mode 4"));
  update_display_ccc(0);
}

void set_driver_mode5_ccc(void)
{
  do_domain_ccc = false;
  check_driver_null_condition_ccc();
  driver_mode_ccc = 5;
  gtk_label_set_text(GTK_LABEL(data_drivermode_ccc), _("Mode 5"));
  update_display_ccc(0);
}

void check_driver_null_condition_ccc(void)
{
  // only check if coming from clone mode and destination string is not NULL
  if (disk_2_ccc != NULL && driver_mode_ccc == 0 && (strcmp(disk_2_ccc, "/dev/null") == 0))
  {
    clear_error_message_ccc();
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Warning! Using driver mode with destination of null will clear the current progress log.\nOnce you click on Start, the log will be cleared.\nIf you wish to keep the current log, disconnect and make a copy."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Warning"), 0);
    clear_error_message_ccc();
  }
}

void set_driver_mode_button_status_ccc(bool active)
{
  if (driver_installed_ccc)
  {
    if (driver_only_ccc)
    {
      set_driver_mode4_ccc();
      gtk_widget_set_sensitive(GTK_WIDGET(driver_mode4_button_ccc), active);
      gtk_widget_set_sensitive(GTK_WIDGET(agressive_driver_checkbutton_ccc), active);
      gtk_widget_set_sensitive(GTK_WIDGET(driver_mode1_button_ccc), FALSE);
      gtk_widget_set_sensitive(GTK_WIDGET(driver_mode2_button_ccc), FALSE);
      gtk_widget_set_sensitive(GTK_WIDGET(driver_mode3_button_ccc), FALSE);
      gtk_widget_set_sensitive(GTK_WIDGET(driver_mode5_button_ccc), FALSE);
    }
    else
    {
      gtk_widget_set_sensitive(GTK_WIDGET(agressive_driver_checkbutton_ccc), active);
      gtk_widget_set_sensitive(GTK_WIDGET(driver_mode1_button_ccc), active);
      gtk_widget_set_sensitive(GTK_WIDGET(driver_mode2_button_ccc), active);
      gtk_widget_set_sensitive(GTK_WIDGET(driver_mode3_button_ccc), active);
      gtk_widget_set_sensitive(GTK_WIDGET(driver_mode4_button_ccc), active);
      gtk_widget_set_sensitive(GTK_WIDGET(driver_mode5_button_ccc), active);
    }
  }
  else
  {
    gtk_widget_set_sensitive(GTK_WIDGET(driver_mode4_button_ccc), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(agressive_driver_checkbutton_ccc), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(driver_mode1_button_ccc), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(driver_mode2_button_ccc), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(driver_mode3_button_ccc), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(driver_mode5_button_ccc), FALSE);
  }
}

void start_cloning_ccc(void)
{
  if (!button_labeled_start)
  {
    stop_signal_ccc = true;
    return;
  }

  int ret = 0;
  stop_display_status_update_timer_ccc();
  stop_signal_ccc = false;
  gtk_widget_set_sensitive(GTK_WIDGET(connect_button_ccc), FALSE);
  gtk_button_set_label(GTK_BUTTON(start_button_ccc), _("Stop"));
  gtk_widget_set_tooltip_text(GTK_WIDGET(start_button_ccc), _("Stop the clone/virtual driver"));
  button_labeled_start = false;
  gtk_widget_set_sensitive(GTK_WIDGET(analyze_button_ccc), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(analyze_long_button_ccc), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(smart_button_ccc), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(topmenubar_ccc), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(clone_mode_button_ccc), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(soft_reset_button_ccc), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(hard_reset_button_ccc), FALSE);
  if (driver_mode_ccc)
  {
    set_driver_mode_button_status_ccc(TRUE);
  }
  else
  {
    set_driver_mode_button_status_ccc(FALSE);
  }

  running_clone_ccc = 1;
  memory_used_ccc = 1;
  if (driver_mode_ccc == 0)
  {
    ret = begin_cloning_ccc();
  }
  else
  {
    gtk_label_set_text(GTK_LABEL(label_filled_ccc), _("Last read size"));
    ret = begin_driver_ccc();
    gtk_label_set_text(GTK_LABEL(label_filled_ccc), _("Filled"));
  }
  running_clone_ccc = 0;
  update_logfile_ccc(0);
  update_domainfile_ccc(0);

  gtk_widget_set_sensitive(GTK_WIDGET(connect_button_ccc), TRUE);
  gtk_button_set_label(GTK_BUTTON(start_button_ccc), _("Start"));
  gtk_widget_set_tooltip_text(GTK_WIDGET(start_button_ccc), _("Start the clone/virtual driver"));
  button_labeled_start = true;
  gtk_widget_set_sensitive(GTK_WIDGET(topmenubar_ccc), TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(clone_mode_button_ccc), driver_only_ccc ? FALSE : TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(soft_reset_button_ccc), TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(hard_reset_button_ccc), TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(analyze_button_ccc), TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(smart_button_ccc), TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(analyze_long_button_ccc), TRUE);
  set_driver_mode_button_status_ccc(TRUE);

  if (ret && ret != STOP_SIGNAL_RETURN_CODE)
  {
    disconnect_devices_ccc();
  }
  start_display_status_update_timer_ccc();
}

void analyze_quick_ccc(void)
{
  enable_extended_analyze_ccc = false;
  start_analyzing_ccc();
}

void analyze_long_ccc(void)
{
  enable_extended_analyze_ccc = true;
  start_analyzing_ccc();
}

void start_analyzing_ccc(void)
{
  int ret = 0;
  stop_display_status_update_timer_ccc();
  stop_signal_ccc = false;
  gtk_widget_set_sensitive(GTK_WIDGET(connect_button_ccc), FALSE);
  gtk_button_set_label(GTK_BUTTON(start_button_ccc), _("Stop"));
  gtk_widget_set_tooltip_text(GTK_WIDGET(start_button_ccc), _("Stop the clone/virtual driver"));
  button_labeled_start = false;
  gtk_widget_set_sensitive(GTK_WIDGET(analyze_button_ccc), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(analyze_long_button_ccc), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(smart_button_ccc), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(topmenubar_ccc), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(clone_mode_button_ccc), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(soft_reset_button_ccc), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(hard_reset_button_ccc), FALSE);

  set_driver_mode_button_status_ccc(FALSE);

  memory_used_ccc = 1;

  int extended = 0;
  int sections = MAXANALYZESECTIONS / 4;
  if (enable_extended_analyze_ccc)
  {
    extended = 1;
    sections = MAXANALYZESECTIONS;
  }

  analyze_data_ccc.sections = sections;

  memset(analyze_read_attempts_ccc, 0, sizeof(analyze_read_attempts_ccc));
  memset(analyze_good_reads_ccc, 0, sizeof(analyze_good_reads_ccc));
  memset(analyze_bad_reads_ccc, 0, sizeof(analyze_bad_reads_ccc));
  memset(analyze_slow_reads_ccc, 0, sizeof(analyze_slow_reads_ccc));
  memset(analyze_timeouts_ccc, 0, sizeof(analyze_timeouts_ccc));
  memset(analyze_read_time_ccc, 0, sizeof(analyze_read_time_ccc));
  // memset (analyze_low_time_ccc, 0x7f, sizeof(analyze_low_time_ccc));
  memset(analyze_high_time_ccc, 0, sizeof(analyze_high_time_ccc));
  memset(analyze_text_ccc, 0, sizeof(analyze_text_ccc));
  memset(analyze_slow_position_ccc, 0, sizeof(analyze_slow_position_ccc));
  memset(analyze_slow_size_ccc, 0, sizeof(analyze_slow_size_ccc));
  // memset (analyze_slow_low_ccc, 0x7f, sizeof(analyze_slow_low_ccc));
  memset(analyze_slow_high_ccc, 0, sizeof(analyze_slow_high_ccc));
  analyze_slow_total_reads_ccc = 0;
  int i;
  for (i = 0; i < MAXANALYZESECTIONS; i++)
  {
    analyze_low_time_ccc[i] = 999999999999;
    analyze_slow_low_ccc[i] = 999999999999;
  }
  running_analyze_ccc = 1;
  gtk_label_set_text(GTK_LABEL(label_filled_ccc), _("Total, Good, Bad, Slow"));
  ret = analyze_drive_ccc(sections, extended);
  gtk_label_set_text(GTK_LABEL(label_filled_ccc), _("Filled"));
  running_analyze_ccc = 0;

  memset(analyze_data_ccc.average_read_time, 0, sizeof(analyze_data_ccc.average_read_time));
  for (i = 0; i < sections; i++)
  {
    // prevent divid by 0 error
    if (analyze_read_attempts_ccc[i] != 0)
    {
      analyze_data_ccc.average_read_time[i] = analyze_read_time_ccc[i] / analyze_read_attempts_ccc[i];
    }
  }

  long long total_average_read_time = 0;
  for (i = 0; i < sections; i++)
  {
    total_average_read_time = total_average_read_time + analyze_data_ccc.average_read_time[i];
  }
  total_average_read_time = total_average_read_time / sections;

  analyze_data_ccc.total_average_read_time = total_average_read_time;

  long long total_low_time = analyze_low_time_ccc[0];
  for (i = 0; i < sections; i++)
  {
    if (analyze_low_time_ccc[i] < total_low_time)
    {
      total_low_time = analyze_low_time_ccc[i];
    }
  }

  analyze_data_ccc.total_low_time = total_low_time;

  long long total_high_time = analyze_high_time_ccc[0];
  for (i = 0; i < sections; i++)
  {
    if (analyze_high_time_ccc[i] > total_high_time)
    {
      total_high_time = analyze_high_time_ccc[i];
    }
  }

  analyze_data_ccc.total_high_time = total_high_time;

  int total_read_attempts = 0;
  for (i = 0; i < sections; i++)
  {
    total_read_attempts = total_read_attempts + analyze_read_attempts_ccc[i];
  }

  analyze_data_ccc.total_read_attempts = total_read_attempts;

  int total_good_reads = 0;
  for (i = 0; i < sections; i++)
  {
    total_good_reads = total_good_reads + analyze_good_reads_ccc[i];
  }

  analyze_data_ccc.total_good_reads = total_good_reads;

  int total_bad_reads = 0;
  for (i = 0; i < sections; i++)
  {
    total_bad_reads = total_bad_reads + analyze_bad_reads_ccc[i];
  }

  analyze_data_ccc.total_bad_reads = total_bad_reads;

  int total_slow_reads = 0;
  for (i = 0; i < sections; i++)
  {
    total_slow_reads = total_slow_reads + analyze_slow_reads_ccc[i];
  }

  analyze_data_ccc.total_slow_reads = total_slow_reads;

  int total_timeouts = 0;
  for (i = 0; i < sections; i++)
  {
    total_timeouts = total_timeouts + analyze_timeouts_ccc[i];
  }

  analyze_data_ccc.total_timeouts = total_timeouts;

  float good_percent;
  float bad_percent;
  float slow_percent;
  good_percent = 100.0f * total_good_reads / total_read_attempts;
  bad_percent = 100.0f * total_bad_reads / total_read_attempts;
  slow_percent = 100.0f * total_slow_reads / total_read_attempts;
  analyze_data_ccc.good_read_percent = good_percent;
  analyze_data_ccc.bad_read_percent = bad_percent;
  analyze_data_ccc.slow_read_percent = slow_percent;
  snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "# %s = %f%%\n", "Good", good_percent);
  strncat(analyze_text_ccc, tempmessage_ccc, MAX_ANALYZE_TEXT_LENGTH - strlen(analyze_text_ccc) - 1);
  snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "# %s = %f%%\n", "Bad", bad_percent);
  strncat(analyze_text_ccc, tempmessage_ccc, MAX_ANALYZE_TEXT_LENGTH - strlen(analyze_text_ccc) - 1);
  snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "# %s = %f%%\n", "Slow", slow_percent);
  strncat(analyze_text_ccc, tempmessage_ccc, MAX_ANALYZE_TEXT_LENGTH - strlen(analyze_text_ccc) - 1);

  snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "#\n");
  strncat(analyze_text_ccc, tempmessage_ccc, MAX_ANALYZE_TEXT_LENGTH - strlen(analyze_text_ccc) - 1);

  float slow_issue_percent;
  if (slow_percent > 50.0f)
  {
    slow_issue_percent = slow_percent;
  }
  else
  {
    slow_issue_percent = slow_percent / 2.0f;
  }
  float slow_variance_percent = 0.0f;
  if (1)
  {
    int slowsections = MAXANALYZESLOW / 4;
    if (extended)
    {
      slowsections = MAXANALYZESLOW;
    }
    int slow_variance_count = 0;
    for (i = 0; i < slowsections; i++)
    {
      if (analyze_slow_high_ccc[i] - analyze_slow_low_ccc[i] > skip_timeout_copy_ccc)
      {
        slow_variance_count++;
      }
    }
    slow_variance_percent = 100.0f * slow_variance_count / slowsections;
  }
  slow_issue_percent = slow_issue_percent + slow_variance_percent;

  analyze_data_ccc.slow_issue_percent = slow_issue_percent;

  snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "# %s = %f%%\n", "Slow Responding Firmware Issue", slow_issue_percent);
  strncat(analyze_text_ccc, tempmessage_ccc, MAX_ANALYZE_TEXT_LENGTH - strlen(analyze_text_ccc) - 1);

  int consecutive_no_read_sections = 0;
  int highest_no_read_sections = 0;
  long long highest_read_time_sections = 0;
  for (i = 0; i < sections; i++)
  {
    if (analyze_read_attempts_ccc[i] > 0 && analyze_good_reads_ccc[i] == 0)
    {
      consecutive_no_read_sections++;
      if (highest_no_read_sections < consecutive_no_read_sections)
      {
        highest_no_read_sections = consecutive_no_read_sections;
      }
      if (highest_read_time_sections < analyze_high_time_ccc[i])
      {
        highest_read_time_sections = analyze_high_time_ccc[i];
      }
    }
    else
    {
      consecutive_no_read_sections = 0;
    }
  }
  float partial_access_percent = 100.0f * consecutive_no_read_sections / sections;
  if (consecutive_no_read_sections > 0 && highest_read_time_sections < skip_timeout_copy_ccc)
  {
    partial_access_percent = partial_access_percent * 2.0f;
  }

  analyze_data_ccc.partial_access_percent = partial_access_percent;

  snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "# %s = %f%%\n", "Partial Access Issue", partial_access_percent);
  strncat(analyze_text_ccc, tempmessage_ccc, MAX_ANALYZE_TEXT_LENGTH - strlen(analyze_text_ccc) - 1);

  int bad_sections = 0;
  for (i = 0; i < sections; i++)
  {
    if (analyze_bad_reads_ccc[i] > 0)
    {
      bad_sections++;
    }
    else if (analyze_slow_reads_ccc[i] > 0 && slow_issue_percent < 50.0f)
    {
      bad_sections++;
    }
  }
  float bad_head_percent = 100.0f * bad_sections / sections;

  analyze_data_ccc.bad_head_percent = bad_head_percent;

  snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "# %s = %f%%\n", "Bad Or Weak Head", bad_head_percent);
  strncat(analyze_text_ccc, tempmessage_ccc, MAX_ANALYZE_TEXT_LENGTH - strlen(analyze_text_ccc) - 1);

  snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "#\n");
  strncat(analyze_text_ccc, tempmessage_ccc, MAX_ANALYZE_TEXT_LENGTH - strlen(analyze_text_ccc) - 1);

  if (1)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "# (%d) %s", analyze_slow_total_reads_ccc, "Variance read times low/high:");
    strncat(analyze_text_ccc, tempmessage_ccc, MAX_ANALYZE_TEXT_LENGTH - strlen(analyze_text_ccc) - 1);
    int slowsections = MAXANALYZESLOW / 4;
    if (extended)
    {
      slowsections = MAXANALYZESLOW;
    }

    analyze_data_ccc.slowsections = slowsections;

    for (i = 0; i < slowsections; i++)
    {
      if ((i % 8) == 0)
      {
        snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "\n#     ");
        strncat(analyze_text_ccc, tempmessage_ccc, MAX_ANALYZE_TEXT_LENGTH - strlen(analyze_text_ccc) - 1);
      }
      snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "%lld/%lld  ", analyze_slow_low_ccc[i] / 1000, analyze_slow_high_ccc[i] / 1000);
      strncat(analyze_text_ccc, tempmessage_ccc, MAX_ANALYZE_TEXT_LENGTH - strlen(analyze_text_ccc) - 1);
    }
    strncat(analyze_text_ccc, "\n", MAX_ANALYZE_TEXT_LENGTH - strlen(analyze_text_ccc) - 1);
  }

  snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "#\n");
  strncat(analyze_text_ccc, tempmessage_ccc, MAX_ANALYZE_TEXT_LENGTH - strlen(analyze_text_ccc) - 1);

  snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "# %s   %s %d    %s %d    %s %d (%d)    %s %d    %s %lld    %s %lld    %s %lld", "Zones", "Total", total_read_attempts, "Good", total_good_reads, "Bad", total_bad_reads, total_timeouts, "Slow", total_slow_reads, "Low", total_low_time / 1000, "High", total_high_time / 1000, "Average", total_average_read_time / 1000);
  strncat(analyze_text_ccc, tempmessage_ccc, MAX_ANALYZE_TEXT_LENGTH - strlen(analyze_text_ccc) - 1);

  for (i = 0; i < sections; i++)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "\n# %s %d    %s %d    %s %d    %s %d (%d)    %s %d    %s %lld    %s %lld    %s %lld", "Zone", i, "Total", analyze_read_attempts_ccc[i], "Good", analyze_good_reads_ccc[i], "Bad", analyze_bad_reads_ccc[i], analyze_timeouts_ccc[i], "Slow", analyze_slow_reads_ccc[i], "Low", analyze_low_time_ccc[i] / 1000, "High", analyze_high_time_ccc[i] / 1000, "Average", analyze_data_ccc.average_read_time[i] / 1000);
    strncat(analyze_text_ccc, tempmessage_ccc, MAX_ANALYZE_TEXT_LENGTH - strlen(analyze_text_ccc) - 1);
  }
  fprintf(stdout, "%s", analyze_text_ccc);

  display_analyze_results_ccc();

  update_logfile_ccc(0);

  gtk_widget_set_sensitive(GTK_WIDGET(connect_button_ccc), TRUE);
  gtk_button_set_label(GTK_BUTTON(start_button_ccc), _("Start"));
  gtk_widget_set_tooltip_text(GTK_WIDGET(start_button_ccc), _("Start the clone/virtual driver"));
  button_labeled_start = true;
  gtk_widget_set_sensitive(GTK_WIDGET(topmenubar_ccc), TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(clone_mode_button_ccc), driver_only_ccc ? FALSE : TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(soft_reset_button_ccc), TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(hard_reset_button_ccc), TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(smart_button_ccc), TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(analyze_button_ccc), TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(analyze_long_button_ccc), TRUE);
  if (driver_installed_ccc)
  {
    set_driver_mode_button_status_ccc(TRUE);
  }

  if (ret && ret != STOP_SIGNAL_RETURN_CODE)
  {
    disconnect_devices_ccc();
  }
  start_display_status_update_timer_ccc();
  logfile_changed_ccc = true;
}

void display_analyze_results_ccc(void)
{
  GtkBuilder *builder = gtk_builder_new();
  GError *err = NULL; // It is mandatory to initialize to NULL
  if (0 == gtk_builder_add_from_string(builder, (const gchar *)opensuperclone_glade, opensuperclone_glade_len, &err))
  {
    fprintf(stderr, "Error adding build from file. Error: %s\n", err->message);
  }
  gtk_builder_connect_signals(builder, NULL);
  GtkWidget *dialog = GTK_WIDGET(gtk_builder_get_object(builder, "analyze_results_dialog"));
  GtkWidget *analyze_reads_label = GTK_WIDGET(gtk_builder_get_object(builder, "analyze_reads_label"));
  GtkWidget *analyze_good_reads_label = GTK_WIDGET(gtk_builder_get_object(builder, "analyze_good_reads_label"));
  GtkWidget *analyze_bad_reads_label = GTK_WIDGET(gtk_builder_get_object(builder, "analyze_bad_reads_label"));
  GtkWidget *analyze_slow_reads_label = GTK_WIDGET(gtk_builder_get_object(builder, "analyze_slow_reads_label"));
  GtkWidget *analyze_issues_label = GTK_WIDGET(gtk_builder_get_object(builder, "analyze_issues_label"));
  GtkWidget *analyze_slow_issue_label = GTK_WIDGET(gtk_builder_get_object(builder, "analyze_slow_issue_label"));
  GtkWidget *analyze_partial_access_label = GTK_WIDGET(gtk_builder_get_object(builder, "analyze_partial_access_label"));
  GtkWidget *analyze_bad_head_label = GTK_WIDGET(gtk_builder_get_object(builder, "analyze_bad_head_label"));
  GtkWidget *analyze_variance_label = GTK_WIDGET(gtk_builder_get_object(builder, "analyze_variance_label"));
  GtkWidget *analyze_zones_label = GTK_WIDGET(gtk_builder_get_object(builder, "analyze_zones_label"));
  GtkWidget *analyze_slow_total_reads_label = GTK_WIDGET(gtk_builder_get_object(builder, "analyze_slow_total_reads_label"));

  GtkListStore *store;
  GtkCellRenderer *renderer;

  GtkTreeIter variance_iter;
  GtkTreeIter zones_iter;

  GtkWidget *analyze_variance_view = GTK_WIDGET(gtk_builder_get_object(builder, "analyze_variance_view"));
  GtkWidget *analyze_zone_view = GTK_WIDGET(gtk_builder_get_object(builder, "analyze_zones_view"));

  gtk_label_set_text(GTK_LABEL(analyze_reads_label), _("Read Statistics"));

  char temp[256];
  snprintf(temp, sizeof(temp), _("Good Reads: %f%% (%d / %d)"), analyze_data_ccc.good_read_percent, analyze_data_ccc.total_good_reads, analyze_data_ccc.total_read_attempts);
  gtk_label_set_text(GTK_LABEL(analyze_good_reads_label), temp);
  gtk_widget_set_tooltip_text(GTK_WIDGET(analyze_good_reads_label), _("Percentage of good reads"));

  snprintf(temp, sizeof(temp), _("Bad Reads: %f%% (%d / %d)"), analyze_data_ccc.bad_read_percent, analyze_data_ccc.total_bad_reads, analyze_data_ccc.total_read_attempts);
  gtk_label_set_text(GTK_LABEL(analyze_bad_reads_label), temp);
  gtk_widget_set_tooltip_text(GTK_WIDGET(analyze_bad_reads_label), _("Percentage of bad reads"));

  snprintf(temp, sizeof(temp), _("Slow Reads: %f%% (%d / %d)"), analyze_data_ccc.slow_read_percent, analyze_data_ccc.total_slow_reads, analyze_data_ccc.total_read_attempts);
  gtk_label_set_text(GTK_LABEL(analyze_slow_reads_label), temp);
  gtk_widget_set_tooltip_text(GTK_WIDGET(analyze_slow_reads_label), _("Percentage of slow reads"));

  gtk_label_set_text(GTK_LABEL(analyze_issues_label), _("Common Issues"));

  snprintf(temp, sizeof(temp), _("Slow Responding Firmware Issue: %f%%"), analyze_data_ccc.slow_issue_percent);
  gtk_label_set_text(GTK_LABEL(analyze_slow_issue_label), temp);
  gtk_widget_set_tooltip_text(GTK_WIDGET(analyze_slow_issue_label), _("Chance of an issue that causes good reads to be slow due to a firmware background operation. Values below 50% are not likely to be an issue. Values above 50% are a possible indication of an issue. Values above 100% are a likely indication of an issue."));

  snprintf(temp, sizeof(temp), _("Partial Access Issue: %f%%"), analyze_data_ccc.partial_access_percent);
  gtk_label_set_text(GTK_LABEL(analyze_partial_access_label), temp);
  gtk_widget_set_tooltip_text(GTK_WIDGET(analyze_partial_access_label), _("Chance of an issue with reads after a certain point, likely caused by a translator issue. Any value above 0% is a possible indication of this issue."));

  snprintf(temp, sizeof(temp), _("Bad Or Weak Head: %f%%"), analyze_data_ccc.bad_head_percent);
  gtk_label_set_text(GTK_LABEL(analyze_bad_head_label), temp);
  gtk_widget_set_tooltip_text(GTK_WIDGET(analyze_bad_head_label), _("Chance of a bad or weak head. Values are between 0% and 100% and are an indication of how widespread the damage across the drive is."));

  gtk_label_set_text(GTK_LABEL(analyze_variance_label), _("Variance Statistics"));


  snprintf(temp, sizeof(temp), _("Total Reads: %d"), analyze_slow_total_reads_ccc);
  gtk_label_set_text(GTK_LABEL(analyze_slow_total_reads_label), temp);
  gtk_widget_set_tooltip_text(GTK_WIDGET(analyze_slow_total_reads_label), _("Total number of reads performed during the variance test"));

  store = gtk_list_store_new(4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
  gtk_tree_view_set_model(GTK_TREE_VIEW(analyze_variance_view), GTK_TREE_MODEL(store));
  g_object_unref(store);

  renderer = gtk_cell_renderer_text_new();
  snprintf(temp, sizeof(temp), "%s (%d)", _("Section"), analyze_data_ccc.slowsections);
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(analyze_variance_view), -1, temp, renderer, "text", 0, NULL);

  renderer = gtk_cell_renderer_text_new();
  long long lowest = analyze_slow_low_ccc[0];
  for (int i = 0; i < analyze_data_ccc.slowsections; i++)
  {
    if (analyze_slow_low_ccc[i] < lowest)
    {
      lowest = analyze_slow_low_ccc[i];
    }
  }
  snprintf(temp, sizeof(temp), "%s (%lld %s)", _("Low"), lowest / 1000, _("ms"));
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(analyze_variance_view), -1, temp, renderer, "text", 1, NULL);

  renderer = gtk_cell_renderer_text_new();
  long long highest = analyze_slow_high_ccc[0];
  for (int i = 0; i < analyze_data_ccc.slowsections; i++)
  {
    if (analyze_slow_high_ccc[i] > highest)
    {
      highest = analyze_slow_high_ccc[i];
    }
  }
  snprintf(temp, sizeof(temp), "%s (%lld %s)", _("High"), highest / 1000, _("ms"));
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(analyze_variance_view), -1, temp, renderer, "text", 2, NULL);

  renderer = gtk_cell_renderer_text_new();
  long long maxvariance = 0;
  for (int i = 0; i < analyze_data_ccc.slowsections; i++)
  {
    if (maxvariance < analyze_slow_high_ccc[i] - analyze_slow_low_ccc[i])
    {
      maxvariance = analyze_slow_high_ccc[i] - analyze_slow_low_ccc[i];
    }
  }
  snprintf(temp, sizeof(temp), "%s (%lld %s)", _("Variance"), maxvariance / 1000, _("ms"));
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(analyze_variance_view), -1, temp, renderer, "text", 3, NULL);

  for (int i = 0; i < analyze_data_ccc.slowsections; i++)
  {
    char zone[128];
    char low[128];
    char high[128];
    char variance[128];

    snprintf(zone, sizeof(zone), "%d", i);
    snprintf(low, sizeof(low), "%lld %s", analyze_slow_low_ccc[i] / 1000, _("ms"));
    snprintf(high, sizeof(high), "%lld %s", analyze_slow_high_ccc[i] / 1000, _("ms"));
    snprintf(variance, sizeof(variance), "%lld %s", (analyze_slow_high_ccc[i] - analyze_slow_low_ccc[i]) / 1000, _("ms"));

    gtk_list_store_append(store, &variance_iter);
    gtk_list_store_set(store, &variance_iter, 0, zone, 1, low, 2, high, 3, variance, -1);
  }

  gtk_label_set_text(GTK_LABEL(analyze_zones_label), _("Zone Statistics"));

  store = gtk_list_store_new(9, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
  gtk_tree_view_set_model(GTK_TREE_VIEW(analyze_zone_view), GTK_TREE_MODEL(store));
  g_object_unref(store);

  renderer = gtk_cell_renderer_text_new();
  snprintf(temp, sizeof(temp), "%s (%d)", _("Zone"), analyze_data_ccc.sections);
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(analyze_zone_view), -1, temp, renderer, "text", 0, NULL);

  renderer = gtk_cell_renderer_text_new();
  snprintf(temp, sizeof(temp), "%s (%d)", _("Total"), analyze_data_ccc.total_read_attempts);
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(analyze_zone_view), -1, temp, renderer, "text", 1, NULL);

  renderer = gtk_cell_renderer_text_new();
  snprintf(temp, sizeof(temp), "%s (%d)", _("Good"), analyze_data_ccc.total_good_reads);
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(analyze_zone_view), -1, temp, renderer, "text", 2, NULL);

  renderer = gtk_cell_renderer_text_new();
  snprintf(temp, sizeof(temp), "%s (%d)", _("Bad"), analyze_data_ccc.total_bad_reads);
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(analyze_zone_view), -1, temp, renderer, "text", 3, NULL);

    renderer = gtk_cell_renderer_text_new();
  snprintf(temp, sizeof(temp), "%s (%d)", _("Timeout"), analyze_data_ccc.total_timeouts);
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(analyze_zone_view), -1, temp, renderer, "text", 4, NULL);

  renderer = gtk_cell_renderer_text_new();
  snprintf(temp, sizeof(temp), "%s (%d)", _("Slow"), analyze_data_ccc.total_slow_reads);
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(analyze_zone_view), -1, temp, renderer, "text", 5, NULL);

  renderer = gtk_cell_renderer_text_new();
  snprintf(temp, sizeof(temp), "%s (%lld %s)", _("Low"), analyze_data_ccc.total_low_time / 1000, _("ms"));
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(analyze_zone_view), -1, temp, renderer, "text", 6, NULL);

  renderer = gtk_cell_renderer_text_new();
  snprintf(temp, sizeof(temp), "%s (%lld %s)", _("High"), analyze_data_ccc.total_high_time / 1000, _("ms"));
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(analyze_zone_view), -1, temp, renderer, "text", 7, NULL);

  renderer = gtk_cell_renderer_text_new();
  snprintf(temp, sizeof(temp), "%s (%lld %s)", _("Average"), analyze_data_ccc.total_average_read_time / 1000, _("ms"));
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(analyze_zone_view), -1, temp, renderer, "text", 8, NULL);

  for (int i = 0; i < analyze_data_ccc.sections; i++)
  {
    char zone[128];
    char total[128];
    char good[128];
    char bad[128];
    char timeout[128];
    char slow[128];
    char low[128];
    char high[128];
    char average[128];

    snprintf(zone, sizeof(zone), "%d", i);
    snprintf(total, sizeof(total), "%d", analyze_read_attempts_ccc[i]);
    snprintf(good, sizeof(good), "%d", analyze_good_reads_ccc[i]);
    snprintf(bad, sizeof(bad), "%d", analyze_bad_reads_ccc[i]);
    snprintf(timeout, sizeof(timeout), "%d", analyze_timeouts_ccc[i]);
    snprintf(slow, sizeof(slow), "%d", analyze_slow_reads_ccc[i]);
    snprintf(low, sizeof(low), "%lld %s", analyze_low_time_ccc[i] / 1000, _("ms"));
    snprintf(high, sizeof(high), "%lld %s", analyze_high_time_ccc[i] / 1000, _("ms"));
    snprintf(average, sizeof(average), "%lld %s", analyze_data_ccc.average_read_time[i] / 1000, _("ms"));

    gtk_list_store_append(store, &zones_iter);
    gtk_list_store_set(store, &zones_iter, 0, zone, 1, total, 2, good, 3, bad, 4, timeout, 5, slow, 6, low, 7, high, 8, average, -1);
  }

  g_object_unref(builder);

  gtk_window_set_title(GTK_WINDOW(dialog), _("Analysis Results"));
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}

int get_smart_attribute_level_ccc(int id, long long value, int current, int worst, int threshold)
{
  int level = 0;

  switch (id)
  {
    // Reallocated Sectors Count
    case 5:
    // Current Pending Sectors
    case 197:
      if (value > 0) level = 2;
      break;

  	// Reported Uncorrectable Errors
    case 187:
    // Command Timeout
    case 188:
    // Off-line Uncorrectable
    case 198:
    // UDMA CRC Error Rate
    case 199:
      if (value > 0) level = 1;
      break;

    default:
      level = 0;
      break;
  }

  return level;
}

void get_smart_data_ccc(void)
{
  stop_signal_ccc = false;
  memset(smart_data_text_ccc, 0, sizeof(smart_data_text_ccc));
  for (int i = 0; i < 256; i++)
  {
    smart_data_ccc.id[i] = 0;
  }
  smart_data_ccc.smart_version = 0;
  smart_data_ccc.value_count = 0;
  int ret = extract_smart_data_ccc();
  if (ret == 0)
  {
    fprintf(stdout, "%s", smart_data_text_ccc);
    display_smart_data_ccc();
  }
  logfile_changed_ccc = true;
}

void display_smart_data_ccc(void)
{
  GtkBuilder *builder = gtk_builder_new();
  GError *err = NULL; // It is mandatory to initialize to NULL
  if (0 == gtk_builder_add_from_string(builder, (const gchar *)opensuperclone_glade, opensuperclone_glade_len, &err))
  {
    fprintf(stderr, "Error adding build from file. Error: %s\n", err->message);
  }
  gtk_builder_connect_signals(builder, NULL);
  GtkWidget *dialog = GTK_WIDGET(gtk_builder_get_object(builder, "smart_results_dialog"));
  GtkWidget *smart_results_box_label = GTK_WIDGET (gtk_builder_get_object (builder, "smart_results_box_label"));
  // GtkWidget *smart_update_button = GTK_WIDGET (gtk_builder_get_object (builder, "smart_update_button"));

  char smart_data_version_label[256];
  snprintf(smart_data_version_label, sizeof(smart_data_version_label), _("S.M.A.R.T. Data Structure Version: %d"), smart_data_ccc.smart_version);
  gtk_label_set_text(GTK_LABEL(smart_results_box_label), smart_data_version_label);

  // gtk_button_set_label(GTK_BUTTON(smart_update_button), _("Update"));

  GtkListStore *store;
  GtkTreeIter iter;
  GtkCellRenderer *renderer;
  GtkWidget *treeview;

  treeview = GTK_WIDGET(gtk_builder_get_object(builder, "smart_results_view"));
  store = gtk_list_store_new(8, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, GDK_TYPE_PIXBUF);
  gtk_tree_view_set_model(GTK_TREE_VIEW(treeview), GTK_TREE_MODEL(store));
  g_object_unref(store);

  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview), -1, _("ID"), renderer, "text", 0, NULL);
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview), -1, _("Name"), renderer, "text", 1, NULL);
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview), -1, _("Flags"), renderer, "text", 2, NULL);
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview), -1, _("Current"), renderer, "text", 3, NULL);
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview), -1, _("Worst"), renderer, "text", 4, NULL);
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview), -1, _("Threshold"), renderer, "text", 5, NULL);
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview), -1, _("Value"), renderer, "text", 6, NULL);
  renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview), -1, _("Status"), renderer, "pixbuf", 7, NULL);

  int i;
  for (i = 0; i < smart_data_ccc.value_count; i++)
  {
    if (smart_data_ccc.id[i] != 0)
    {
      char id[128];
      char name[128];
      char flags[128];
      char current[128];
      char worst[128];
      char threshold[128];
      char raw[128];
      int level = 0;

      snprintf(id, sizeof(id), "%03d", smart_data_ccc.id[i]);
      snprintf(name, sizeof(name), "%s", smart_data_ccc.name[i]);
      snprintf(flags, sizeof(flags), "0x%04x", smart_data_ccc.flags[i]);
      snprintf(current, sizeof(current), "%03d", smart_data_ccc.current[i]);
      snprintf(worst, sizeof(worst), "%03d", smart_data_ccc.worst[i]);
      snprintf(threshold, sizeof(threshold), "%03d", smart_data_ccc.threshold[i]);
      snprintf(raw, sizeof(raw), "0x%014llx", smart_data_ccc.raw[i]);

      GdkPixbuf *pixbuf = NULL;

      level = get_smart_attribute_level_ccc(smart_data_ccc.id[i], smart_data_ccc.raw[i], smart_data_ccc.current[i], smart_data_ccc.worst[i], smart_data_ccc.threshold[i]);

      if (level == 2)
      {
        pixbuf = gdk_pixbuf_new_from_file_at_size(smart_error_icon_path, 16, 16, NULL);
      }
      else if (level == 1)
      {
        pixbuf = gdk_pixbuf_new_from_file_at_size(smart_warning_icon_path, 16, 16, NULL);
      }
      else
      {
        pixbuf = gdk_pixbuf_new_from_file_at_size(smart_info_icon_path, 16, 16, NULL);
      }

      gtk_list_store_append(store, &iter);
      gtk_list_store_set(store, &iter, 0, id, 1, name, 2, flags, 3, current, 4, worst, 5, threshold, 6, raw, 7, pixbuf, -1);
    }
  }

  g_object_unref(builder);

  gtk_window_set_title(GTK_WINDOW(dialog), _("S.M.A.R.T."));
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}

void display_identify_data_ccc(void)
{
  GtkBuilder *builder = gtk_builder_new();
  GError *err = NULL; // It is mandatory to initialize to NULL
  if (0 == gtk_builder_add_from_string(builder, (const gchar *)opensuperclone_glade, opensuperclone_glade_len, &err))
  {
    fprintf(stderr, "Error adding build from file. Error: %s\n", err->message);
  }
  gtk_builder_connect_signals(builder, NULL);
  GtkWidget *dialog = GTK_WIDGET(gtk_builder_get_object(builder, "identify_results_dialog"));

  GtkWidget *identify_flags_label = GTK_WIDGET(gtk_builder_get_object(builder, "identify_flags_label"));
  GtkWidget *identify_lba_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_lba_checkbox"));
  GtkWidget *identify_extended_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_extended_checkbox"));
  GtkWidget *identify_drive_locked_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_drive_locked_checkbox"));
  GtkWidget *identify_smart_supported_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_smart_supported_checkbox"));
  GtkWidget *identify_smart_enabled_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_smart_enabled_checkbox"));
  GtkWidget *identify_rebuild_assist_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_rebuild_assist_checkbox"));
  GtkWidget *identify_ncq_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_ncq_checkbox"));
  GtkWidget *identify_sct_error_recovery_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_sct_error_recovery_checkbox"));
  GtkWidget *identify_trim_supported_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_trim_supported_checkbox"));

  GtkWidget *identify_mdma_label = GTK_WIDGET(gtk_builder_get_object(builder, "identify_mdma_label"));
  GtkWidget *identify_mdma_supported_label = GTK_WIDGET(gtk_builder_get_object(builder, "identify_mdma_supported_label"));
  GtkWidget *identify_mdma_selected_label = GTK_WIDGET(gtk_builder_get_object(builder, "identify_mdma_selected_label"));
  GtkWidget *identify_mdma_supported_0_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_mdma_supported_0_checkbox"));
  GtkWidget *identify_mdma_supported_1_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_mdma_supported_1_checkbox"));
  GtkWidget *identify_mdma_supported_2_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_mdma_supported_2_checkbox"));
  GtkWidget *identify_mdma_selected_0_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_mdma_selected_0_checkbox"));
  GtkWidget *identify_mdma_selected_1_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_mdma_selected_1_checkbox"));
  GtkWidget *identify_mdma_selected_2_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_mdma_selected_2_checkbox"));

  GtkWidget *identify_udma_label = GTK_WIDGET(gtk_builder_get_object(builder, "identify_udma_label"));
  GtkWidget *identify_udma_supported_label = GTK_WIDGET(gtk_builder_get_object(builder, "identify_udma_supported_label"));
  GtkWidget *identify_udma_selected_label = GTK_WIDGET(gtk_builder_get_object(builder, "identify_udma_selected_label"));
  GtkWidget *identify_udma_supported_0_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_udma_supported_0_checkbox"));
  GtkWidget *identify_udma_supported_1_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_udma_supported_1_checkbox"));
  GtkWidget *identify_udma_supported_2_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_udma_supported_2_checkbox"));
  GtkWidget *identify_udma_supported_3_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_udma_supported_3_checkbox"));
  GtkWidget *identify_udma_supported_4_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_udma_supported_4_checkbox"));
  GtkWidget *identify_udma_supported_5_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_udma_supported_5_checkbox"));
  GtkWidget *identify_udma_supported_6_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_udma_supported_6_checkbox"));
  GtkWidget *identify_udma_selected_0_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_udma_selected_0_checkbox"));
  GtkWidget *identify_udma_selected_1_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_udma_selected_1_checkbox"));
  GtkWidget *identify_udma_selected_2_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_udma_selected_2_checkbox"));
  GtkWidget *identify_udma_selected_3_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_udma_selected_3_checkbox"));
  GtkWidget *identify_udma_selected_4_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_udma_selected_4_checkbox"));
  GtkWidget *identify_udma_selected_5_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_udma_selected_5_checkbox"));
  GtkWidget *identify_udma_selected_6_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_udma_selected_6_checkbox"));

  GtkWidget *identify_raw_buffer_label = GTK_WIDGET(gtk_builder_get_object(builder, "identify_raw_buffer_label"));
  GtkWidget *identify_raw_data_textbox = GTK_WIDGET(gtk_builder_get_object(builder, "identify_raw_data_textbox"));

  g_object_unref(builder);

  gtk_label_set_text(GTK_LABEL(identify_flags_label), _("Identify Flags"));

  gtk_button_set_label(GTK_BUTTON(identify_lba_checkbox), _("LBA Supported"));
  gtk_button_set_label(GTK_BUTTON(identify_extended_checkbox), _("LBA48 Supported"));
  gtk_button_set_label(GTK_BUTTON(identify_drive_locked_checkbox), _("Security Locked"));
  gtk_button_set_label(GTK_BUTTON(identify_smart_supported_checkbox), _("S.M.A.R.T. Supported"));
  gtk_button_set_label(GTK_BUTTON(identify_smart_enabled_checkbox), _("S.M.A.R.T. Enabled"));
  gtk_button_set_label(GTK_BUTTON(identify_rebuild_assist_checkbox), _("Rebuild Assist Supported"));
  gtk_button_set_label(GTK_BUTTON(identify_ncq_checkbox), _("NCQ Supported"));
  gtk_button_set_label(GTK_BUTTON(identify_sct_error_recovery_checkbox), _("SCT Error Recovery Supported"));
  gtk_button_set_label(GTK_BUTTON(identify_trim_supported_checkbox), _("TRIM Supported"));
  
  gtk_label_set_text(GTK_LABEL(identify_mdma_label), _("Multiword DMA Support"));
  gtk_label_set_text(GTK_LABEL(identify_mdma_supported_label), _("Supported"));
  gtk_label_set_text(GTK_LABEL(identify_mdma_selected_label), _("Selected"));
  
  gtk_button_set_label(GTK_BUTTON(identify_mdma_supported_0_checkbox), _("0"));
  gtk_button_set_label(GTK_BUTTON(identify_mdma_supported_1_checkbox), _("1"));
  gtk_button_set_label(GTK_BUTTON(identify_mdma_supported_2_checkbox), _("2"));
  gtk_button_set_label(GTK_BUTTON(identify_mdma_selected_0_checkbox), _("0"));
  gtk_button_set_label(GTK_BUTTON(identify_mdma_selected_1_checkbox), _("1"));
  gtk_button_set_label(GTK_BUTTON(identify_mdma_selected_2_checkbox), _("2"));

  gtk_label_set_text(GTK_LABEL(identify_udma_label), _("Ultra DMA Support"));
  gtk_label_set_text(GTK_LABEL(identify_udma_supported_label), _("Supported"));
  gtk_label_set_text(GTK_LABEL(identify_udma_selected_label), _("Selected"));

  gtk_button_set_label(GTK_BUTTON(identify_udma_supported_0_checkbox), _("0"));
  gtk_button_set_label(GTK_BUTTON(identify_udma_supported_1_checkbox), _("1"));
  gtk_button_set_label(GTK_BUTTON(identify_udma_supported_2_checkbox), _("2"));
  gtk_button_set_label(GTK_BUTTON(identify_udma_supported_3_checkbox), _("3"));
  gtk_button_set_label(GTK_BUTTON(identify_udma_supported_4_checkbox), _("4"));
  gtk_button_set_label(GTK_BUTTON(identify_udma_supported_5_checkbox), _("5"));
  gtk_button_set_label(GTK_BUTTON(identify_udma_supported_6_checkbox), _("6"));
  gtk_button_set_label(GTK_BUTTON(identify_udma_selected_0_checkbox), _("0"));
  gtk_button_set_label(GTK_BUTTON(identify_udma_selected_1_checkbox), _("1"));
  gtk_button_set_label(GTK_BUTTON(identify_udma_selected_2_checkbox), _("2"));
  gtk_button_set_label(GTK_BUTTON(identify_udma_selected_3_checkbox), _("3"));
  gtk_button_set_label(GTK_BUTTON(identify_udma_selected_4_checkbox), _("4"));
  gtk_button_set_label(GTK_BUTTON(identify_udma_selected_5_checkbox), _("5"));
  gtk_button_set_label(GTK_BUTTON(identify_udma_selected_6_checkbox), _("6"));

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_lba_checkbox), identify_device_data_ccc.lba_supported);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_extended_checkbox), identify_device_data_ccc.extended_supported);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_drive_locked_checkbox), identify_device_data_ccc.drive_locked);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_smart_supported_checkbox), identify_device_data_ccc.smart_supported);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_smart_enabled_checkbox), identify_device_data_ccc.smart_enabled);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_rebuild_assist_checkbox), identify_device_data_ccc.rebuild_assist_supported);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_ncq_checkbox), identify_device_data_ccc.ncq_supported);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_sct_error_recovery_checkbox), identify_device_data_ccc.sct_error_recovery_control);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_trim_supported_checkbox), identify_device_data_ccc.trim_supported);

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_mdma_supported_0_checkbox), identify_device_data_ccc.mdma_mode_0_supported);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_mdma_supported_1_checkbox), identify_device_data_ccc.mdma_mode_1_supported);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_mdma_supported_2_checkbox), identify_device_data_ccc.mdma_mode_2_supported);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_mdma_selected_0_checkbox), identify_device_data_ccc.mdma_mode_0_selected);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_mdma_selected_1_checkbox), identify_device_data_ccc.mdma_mode_1_selected);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_mdma_selected_2_checkbox), identify_device_data_ccc.mdma_mode_2_selected);

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_udma_supported_0_checkbox), identify_device_data_ccc.udma_mode_0_supported);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_udma_supported_1_checkbox), identify_device_data_ccc.udma_mode_1_supported);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_udma_supported_2_checkbox), identify_device_data_ccc.udma_mode_2_supported);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_udma_supported_3_checkbox), identify_device_data_ccc.udma_mode_3_supported);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_udma_supported_4_checkbox), identify_device_data_ccc.udma_mode_4_supported);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_udma_supported_5_checkbox), identify_device_data_ccc.udma_mode_5_supported);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_udma_supported_6_checkbox), identify_device_data_ccc.udma_mode_6_supported);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_udma_selected_0_checkbox), identify_device_data_ccc.udma_mode_0_selected);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_udma_selected_1_checkbox), identify_device_data_ccc.udma_mode_1_selected);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_udma_selected_2_checkbox), identify_device_data_ccc.udma_mode_2_selected);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_udma_selected_3_checkbox), identify_device_data_ccc.udma_mode_3_selected);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_udma_selected_4_checkbox), identify_device_data_ccc.udma_mode_4_selected);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_udma_selected_5_checkbox), identify_device_data_ccc.udma_mode_5_selected);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(identify_udma_selected_6_checkbox), identify_device_data_ccc.udma_mode_6_selected);

  gtk_label_set_text(GTK_LABEL(identify_raw_buffer_label), _("Raw Data"));
  
  gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(identify_raw_data_textbox)), identify_device_raw_text_ccc, -1);

  gtk_window_set_title(GTK_WINDOW(dialog), _("Identify Results"));
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}

void quit_and_close_ccc(void)
{
  gtk_main_quit();
  quit_and_exit_ccc();
}

void open_clone_settings_dialog_ccc(void)
{
  GtkBuilder *builder = gtk_builder_new();
  GError *err = NULL; // It is mandatory to initialize to NULL
  if (0 == gtk_builder_add_from_string(builder, (const gchar *)opensuperclone_glade, opensuperclone_glade_len, &err))
  {
    fprintf(stderr, "Error adding build from file. Error: %s\n", err->message);
  }
  gtk_builder_connect_signals(builder, NULL);
  GtkWidget *dialog = GTK_WIDGET(gtk_builder_get_object(builder, "clone_settings_dialog"));
  phase_control_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "phase_control_label"));
  phase1_check_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "phase1_checkbutton"));
  phase2_check_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "phase2_checkbutton"));
  phase3_check_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "phase3_checkbutton"));
  phase4_check_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "phase4_checkbutton"));
  divide_check_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "divide_checkbutton"));
  divide2_check_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "divide2_checkbutton"));
  trim_check_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "trim_checkbutton"));
  scrape_check_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "scrape_checkbutton"));
  markbad_check_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "markbad_checkbutton"));
  readbad_check_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "readbad_checkbutton"));
  rebuild_assist_check_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "rebuild_assist_checkbutton"));
  reverse_check_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "reverse_checkbutton"));
  skipfast_check_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "skipfast_checkbutton"));
  retries_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "retries_button_label"));
  retries_spin_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "retries_spinbutton"));
  clustersize_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "clustersize_button_label"));
  clustersize_spin_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "clustersize_spinbutton"));
  inputoffset_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "inputoffset_button_label"));
  inputoffset_spin_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "inputoffset_spinbutton"));
  size_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "size_button_label"));
  size_spin_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "size_spinbutton"));
  blocksize_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "blocksize_button_label"));
  blocksize_spin_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "blocksize_spinbutton"));
  skipsize_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "skipsize_button_label"));
  skipsize_spin_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "skipsize_spinbutton"));
  maxskipsize_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "maxskipsize_button_label"));
  maxskipsize_spin_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "maxskipsize_spinbutton"));
  skipthreshold_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "skipthreshold_button_label"));
  skipthreshold_spin_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "skipthreshold_spinbutton"));
  rateskip_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "rateskip_button_label"));
  rateskip_spin_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "rateskip_spinbutton"));
  exitonslow_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "exitonslow_button_label"));
  exitonslow_spin_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "exitonslow_spinbutton"));
  exitonslowtime_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "exitonslowtime_button_label"));
  exitonslowtime_spin_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "exitonslowtime_spinbutton"));
  sectorsize_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "sectorsize_button_label"));
  sectorsize_spinbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "sectorsize_spinbutton"));
  alignment_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "alignment_button_label"));
  alignment_spinbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "alignment_spinbutton"));
  maxreadrate_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "maxreadrate_button_label"));
  maxreadrate_spinbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "maxreadrate_spinbutton"));
  g_object_unref(builder);

  gtk_label_set_text(GTK_LABEL(phase_control_label_ccc), _("Phase Control"));
  gtk_button_set_label(GTK_BUTTON(phase1_check_button_ccc), _("Phase 1"));
  gtk_button_set_label(GTK_BUTTON(phase2_check_button_ccc), _("Phase 2"));
  gtk_button_set_label(GTK_BUTTON(phase3_check_button_ccc), _("Phase 3"));
  gtk_button_set_label(GTK_BUTTON(phase4_check_button_ccc), _("Phase 4"));
  gtk_button_set_label(GTK_BUTTON(divide_check_button_ccc), _("Divide"));
  gtk_button_set_label(GTK_BUTTON(divide2_check_button_ccc), _("Divide 2"));
  gtk_button_set_label(GTK_BUTTON(trim_check_button_ccc), _("Trim (overrides dividing)"));
  gtk_button_set_label(GTK_BUTTON(scrape_check_button_ccc), _("Scrape"));
  gtk_button_set_label(GTK_BUTTON(markbad_check_button_ccc), _("Mark Bad Head"));
  gtk_button_set_label(GTK_BUTTON(readbad_check_button_ccc), _("Read Bad Head"));
  gtk_button_set_label(GTK_BUTTON(rebuild_assist_check_button_ccc), _("Rebuild Assist"));
  gtk_button_set_label(GTK_BUTTON(reverse_check_button_ccc), _("Reverse"));
  gtk_button_set_label(GTK_BUTTON(skipfast_check_button_ccc), _("Skip Fast"));

  gtk_label_set_text(GTK_LABEL(retries_button_label_ccc), _("Retries"));
  gtk_label_set_text(GTK_LABEL(clustersize_button_label_ccc), _("Cluster Size (LBA)"));
  gtk_label_set_text(GTK_LABEL(inputoffset_button_label_ccc), _("Input Offset (LBA)"));
  gtk_label_set_text(GTK_LABEL(size_button_label_ccc), _("Size (LBA)"));
  gtk_label_set_text(GTK_LABEL(blocksize_button_label_ccc), _("Block size (LBA)"));
  gtk_label_set_text(GTK_LABEL(skipsize_button_label_ccc), _("Skip Size (LBA) phase-1-2-3"));
  gtk_label_set_text(GTK_LABEL(maxskipsize_button_label_ccc), _("Max Skip Size (LBA) phase-1-2"));
  gtk_label_set_text(GTK_LABEL(skipthreshold_button_label_ccc), _("Skip Threshold (ms) phase-1-2"));
  gtk_label_set_text(GTK_LABEL(rateskip_button_label_ccc), _("Rate Skip (kB/s) phase-3"));
  gtk_label_set_text(GTK_LABEL(exitonslow_button_label_ccc), _("Exit on Slow (kB/s) phase-1-2"));
  gtk_label_set_text(GTK_LABEL(exitonslowtime_button_label_ccc), _("Exit on Slow Time (sec) phase-1-2"));
  gtk_label_set_text(GTK_LABEL(sectorsize_button_label_ccc), _("Sector size (bytes)"));
  gtk_label_set_text(GTK_LABEL(alignment_button_label_ccc), _("Alignment offset (LBA)"));
  gtk_label_set_text(GTK_LABEL(maxreadrate_button_label_ccc), _("Max read rate (kB/s)"));

  gtk_widget_set_tooltip_text(phase_control_label_ccc, _("Select the phases to be used for the clone operation"));
  gtk_widget_set_tooltip_text(phase1_check_button_ccc, _("Enable Phase 1 (copy pass forward with skipping based on read errors)"));
  gtk_widget_set_tooltip_text(phase2_check_button_ccc, _("Enable Phase 2 (copy pass backward with skipping based on read errors)"));
  gtk_widget_set_tooltip_text(phase3_check_button_ccc, _("Enable Phase 3 (copy pass forward with skipping based on read speed)"));
  gtk_widget_set_tooltip_text(phase4_check_button_ccc, _("Enable Phase 4 (copy pass forward without skipping)"));
  gtk_widget_set_tooltip_text(divide_check_button_ccc, _("Enable Dividing (divide non-trimmed blocks into smaller blocks)"));
  gtk_widget_set_tooltip_text(divide2_check_button_ccc, _("Perform two dividing passes instead of one"));
  gtk_widget_set_tooltip_text(trim_check_button_ccc, _("Enable Trimming (read non-trimmed blocks one sector at a time forwards and backwards)"));
  gtk_widget_set_tooltip_text(scrape_check_button_ccc, _("Enable Scrape (read non-scraped blocks one sector at a time forwards)"));
  gtk_widget_set_tooltip_text(markbad_check_button_ccc, _("Enable Mark Bad Head (uses the skipping information during phases 1-2 to mark what is considered a bad head)"));
  gtk_widget_set_tooltip_text(readbad_check_button_ccc, _("Enable Read Bad Head (read sectors from heads marked as bad)"));
  gtk_widget_set_tooltip_text(rebuild_assist_check_button_ccc, _("Enable Rebuild Assist (use the Rebuild Assist phase instead of phase 1)"));
  gtk_widget_set_tooltip_text(reverse_check_button_ccc, _("Enable Reverse (reverse the direction of all passes)"));
  gtk_widget_set_tooltip_text(skipfast_check_button_ccc, _("Enable Skip Fast (use a more aggressive skipping algorithm)"));

  gtk_widget_set_tooltip_text(retries_button_label_ccc, _("Number of retry passes to perform"));
  gtk_widget_set_tooltip_text(clustersize_button_label_ccc, _("Number of sectors per read block in phases 1-4"));
  gtk_widget_set_tooltip_text(inputoffset_button_label_ccc, _("Number of sectors to skip at the beginning"));
  gtk_widget_set_tooltip_text(size_button_label_ccc, _("Number of sectors to copy"));
  gtk_widget_set_tooltip_text(blocksize_button_label_ccc, _("Number of logical sectors per physical sector. All reads will be aligned to this value."));
  gtk_widget_set_tooltip_text(skipsize_button_label_ccc, _("Number of sectors to skip in LBAs during phases 1-3. This number can grow during phases 1-2 to adjust to the head skipping algorithm."));
  gtk_widget_set_tooltip_text(maxskipsize_button_label_ccc, _("Maximum number of sectors to skip in LBAs during phases 1-2. The default is calculated as drive size / 1000 or 1GB, whichever is smaller."));
  gtk_widget_set_tooltip_text(skipthreshold_button_label_ccc, _("Number of milliseconds to wait for a read before triggering a skip in phases 1-2"));
  gtk_widget_set_tooltip_text(rateskip_button_label_ccc, _("Lowest read rate in kB/s before triggering a skip in phase 3"));
  gtk_widget_set_tooltip_text(exitonslow_button_label_ccc, _("Lowest read rate in kB/s before triggering an exit in phases 1-2"));
  gtk_widget_set_tooltip_text(exitonslowtime_button_label_ccc, _("Number of seconds to wait for a slow read before triggering an exit in phases 1-2"));
  gtk_widget_set_tooltip_text(sectorsize_button_label_ccc, _("Number of bytes per sector. This value will be detected automatically when choosing a source."));
  gtk_widget_set_tooltip_text(alignment_button_label_ccc, _("Number of logical sectors to align the source to. This value will be detected automatically when choosing a source."));
  gtk_widget_set_tooltip_text(maxreadrate_button_label_ccc, _("Limit for the maximum read rate in kB/s"));

  g_signal_connect(G_OBJECT(phase1_check_button_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_PHASE1));
  g_signal_connect(G_OBJECT(phase2_check_button_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_PHASE2));
  g_signal_connect(G_OBJECT(phase3_check_button_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_PHASE3));
  g_signal_connect(G_OBJECT(phase4_check_button_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_PHASE4));
  g_signal_connect(G_OBJECT(divide_check_button_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_DIVIDE));
  g_signal_connect(G_OBJECT(divide2_check_button_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_DIVIDE2));
  g_signal_connect(G_OBJECT(trim_check_button_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_TRIM));
  g_signal_connect(G_OBJECT(scrape_check_button_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_SCRAPE));
  g_signal_connect(G_OBJECT(markbad_check_button_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_MARKBAD));
  g_signal_connect(G_OBJECT(readbad_check_button_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_READBAD));
  g_signal_connect(G_OBJECT(rebuild_assist_check_button_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_REBUILDASSIST));
  g_signal_connect(G_OBJECT(reverse_check_button_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_REVERSE));
  g_signal_connect(G_OBJECT(skipfast_check_button_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_SKIPFAST));

  load_clone_settings_ccc();
  update_clone_button_settings_ccc();

  gtk_window_set_title(GTK_WINDOW(dialog), _("Clone Settings"));
  if (gtk_dialog_run(GTK_DIALOG(dialog)) == 5)
  {
    clone_settings_ccc.retries = gtk_spin_button_get_value(GTK_SPIN_BUTTON(retries_spin_button_ccc));
    clone_settings_ccc.cluster_size = gtk_spin_button_get_value(GTK_SPIN_BUTTON(clustersize_spin_button_ccc));
    clone_settings_ccc.input_offset = gtk_spin_button_get_value(GTK_SPIN_BUTTON(inputoffset_spin_button_ccc));
    clone_settings_ccc.read_size = gtk_spin_button_get_value(GTK_SPIN_BUTTON(size_spin_button_ccc));
    clone_settings_ccc.block_size = gtk_spin_button_get_value(GTK_SPIN_BUTTON(blocksize_spin_button_ccc));
    clone_settings_ccc.min_skip_size = gtk_spin_button_get_value(GTK_SPIN_BUTTON(skipsize_spin_button_ccc));
    clone_settings_ccc.max_skip_size = gtk_spin_button_get_value(GTK_SPIN_BUTTON(maxskipsize_spin_button_ccc));
    clone_settings_ccc.skip_timeout = gtk_spin_button_get_value(GTK_SPIN_BUTTON(skipthreshold_spin_button_ccc)) * 1000;
    clone_settings_ccc.rate_skip = gtk_spin_button_get_value(GTK_SPIN_BUTTON(rateskip_spin_button_ccc)) * 1000;
    clone_settings_ccc.exit_on_slow = gtk_spin_button_get_value(GTK_SPIN_BUTTON(exitonslow_spin_button_ccc)) * 1000;
    clone_settings_ccc.exit_slow_time = gtk_spin_button_get_value(GTK_SPIN_BUTTON(exitonslowtime_spin_button_ccc)) * 1000;
    clone_settings_ccc.sector_size = gtk_spin_button_get_value(GTK_SPIN_BUTTON(sectorsize_spinbutton_ccc));
    clone_settings_ccc.block_offset = gtk_spin_button_get_value(GTK_SPIN_BUTTON(alignment_spinbutton_ccc));
    clone_settings_ccc.max_read_rate = gtk_spin_button_get_value(GTK_SPIN_BUTTON(maxreadrate_spinbutton_ccc)) * 1000;
    // clone_settings_ccc. = gtk_spin_button_get_value(GTK_SPIN_BUTTON());
    update_clone_settings_ccc();
    logfile_changed_ccc = true;
  }
  gtk_widget_destroy(dialog);
  update_display_ccc(0);
}

void open_advanced_settings_dialog_ccc(void)
{
  GtkBuilder *builder = gtk_builder_new();
  GError *err = NULL; // It is mandatory to initialize to NULL
  if (0 == gtk_builder_add_from_string(builder, (const gchar *)opensuperclone_glade, opensuperclone_glade_len, &err))
  {
    fprintf(stderr, "Error adding build from file. Error: %s\n", err->message);
  }
  gtk_builder_connect_signals(builder, NULL);
  GtkWidget *dialog = GTK_WIDGET(gtk_builder_get_object(builder, "advanced_settings_dialog"));
  no_log_backup_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "no_log_backup_checkbutton"));
  force_mounted_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "force_mounted_checkbutton"));
  force_dangerous_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "force_dangerous_checkbutton"));
  enable_output_offset_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "enable_output_offset_checkbutton"));
  output_offset_spinbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "output_offset_spinbutton"));
  output_offset_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "output_offset_button_label"));
  enable_current_position_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "enable_current_position_checkbutton"));
  current_position_spinbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "current_position_spinbutton"));
  current_position_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "current_position_button_label"));
  action_on_error_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "action_on_error_label"));
  stop_on_error_radio_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "stop_on_error_radio_button"));
  call_command_radio_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "call_command_radio_button"));
  command_to_call_text_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "command_to_call_text"));
  test_command_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "test_command_button"));
  primary_relay_on_error_radio_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "primary_relay_on_error_radio_button"));
  write_buffer_disable_radio_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "write_buffer_disable_radio_button"));
  write_buffer_enable_radio_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "write_buffer_enable_radio_button"));
  write_buffer_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "write_buffer_label"));
  disable_identify_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "disable_identify_checkbutton"));
  pio_mode_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "pio_mode_checkbutton"));
  enable_rebuild_assist_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "enable_rebuild_assist_checkbutton"));
  enable_process_chunk_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "enable_process_chunk_checkbutton"));
  enable_read_twice_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "enable_read_twice_checkbutton"));
  enable_retry_connecting_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "enable_retry_connecting_checkbutton"));
  enable_scsi_write_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "enable_scsi_write_checkbutton"));
  enable_phase_log_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "enable_phase_log_checkbutton"));
  enable_output_sector_size_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "enable_output_sector_size_checkbutton"));
  output_sector_size_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "output_sector_size_button_label"));
  driver_io_scsi_only_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "driver_io_scsi_only_checkbutton"));
  use_physical_sector_size_for_virtual_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "use_physical_sector_size_for_virtual_checkbutton"));
  output_sector_size_spinbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "output_sector_size_spinbutton"));
  driver_error_options_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "driver_error_options_label"));
  driver_return_error_radio_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "driver_return_error_radio_button"));
  driver_return_zeros_radio_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "driver_return_zeros_radio_button"));
  driver_return_marked_radio_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "driver_return_marked_radio_button"));
  virtual_disk_device_name_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "virtual_disk_device_name_label"));
  virtual_disk_device_name_text_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "virtual_disk_device_name_text"));
  driver_minimum_cluster_size_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "driver_minimum_cluster_size_button_label"));
  driver_minimum_cluster_size_spinbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "driver_minimum_cluster_size_spinbutton"));
  g_object_unref(builder);

  gtk_button_set_label(GTK_BUTTON(no_log_backup_checkbutton_ccc), _("Do not create backup log"));
  gtk_button_set_label(GTK_BUTTON(force_mounted_checkbutton_ccc), _("Force access of mounted disk (not recommended)"));
  gtk_button_set_label(GTK_BUTTON(force_dangerous_checkbutton_ccc), _("Force same controller / slave (not recommended)"));
  gtk_button_set_label(GTK_BUTTON(enable_output_offset_checkbutton_ccc), _("Enable changing output offset (use with caution)"));
  gtk_label_set_text(GTK_LABEL(output_offset_button_label_ccc), _("Output Offset (default value is -1, follow input offset)"));
  gtk_button_set_label(GTK_BUTTON(enable_current_position_checkbutton_ccc), _("Enable changing current position (can affect skipping algorithm)"));
  gtk_label_set_text(GTK_LABEL(current_position_button_label_ccc), _("Current position"));
  gtk_label_set_text(GTK_LABEL(action_on_error_label_ccc), _("Action to perform on major drive error:"));
  gtk_button_set_label(GTK_BUTTON(stop_on_error_radio_button_ccc), _("Stop cloning with error message"));
  gtk_button_set_label(GTK_BUTTON(call_command_radio_button_ccc), _("Call command:"));
  gtk_button_set_label(GTK_BUTTON(test_command_button_ccc), _("Test command"));
  gtk_button_set_label(GTK_BUTTON(primary_relay_on_error_radio_button_ccc), _("Activate primary relay"));
  gtk_button_set_label(GTK_BUTTON(write_buffer_disable_radio_button_ccc), _("Disabled (safer if there is a power loss)"));
  gtk_button_set_label(GTK_BUTTON(write_buffer_enable_radio_button_ccc), _("Enabled (faster)"));
  gtk_label_set_text(GTK_LABEL(write_buffer_label_ccc), _("Operating System write buffer (must disconnect to take effect)"));
  gtk_button_set_label(GTK_BUTTON(disable_identify_checkbutton_ccc), _("Do not perform identify when listing source devices"));
  gtk_button_set_label(GTK_BUTTON(pio_mode_checkbutton_ccc), _("PIO mode"));
  gtk_button_set_label(GTK_BUTTON(enable_rebuild_assist_checkbutton_ccc), _("Enable Rebuild Assist"));
  gtk_button_set_label(GTK_BUTTON(enable_process_chunk_checkbutton_ccc), _("Enable using ATA return to mark bad sectors"));
  gtk_button_set_label(GTK_BUTTON(enable_read_twice_checkbutton_ccc), _("Enable read twice on timeout"));
  gtk_button_set_label(GTK_BUTTON(enable_retry_connecting_checkbutton_ccc), _("Retry connecting on failure after relay power cycle"));
  gtk_button_set_label(GTK_BUTTON(enable_scsi_write_checkbutton_ccc), _("Enable SCSI write to destination (for odd sector size)"));
  gtk_button_set_label(GTK_BUTTON(enable_phase_log_checkbutton_ccc), _("Enable phase logs"));
  gtk_button_set_label(GTK_BUTTON(enable_output_sector_size_checkbutton_ccc), _("Enable output sector size adjustment"));
  gtk_label_set_text(GTK_LABEL(output_sector_size_button_label_ccc), _("Output sector size adjustment"));
  gtk_button_set_label(GTK_BUTTON(driver_io_scsi_only_checkbutton_ccc), _("Virtual Disk IO SCSI only"));
  gtk_button_set_label(GTK_BUTTON(use_physical_sector_size_for_virtual_checkbutton_ccc), _("Use physical sector size as logical for virtual disk"));
  gtk_button_set_label(GTK_BUTTON(driver_return_error_radio_button_ccc), _("Return read error (standard)"));
  gtk_button_set_label(GTK_BUTTON(driver_return_zeros_radio_button_ccc), _("Return zero filled data"));
  gtk_button_set_label(GTK_BUTTON(driver_return_marked_radio_button_ccc), _("Return mark filled data"));
  gtk_label_set_text(GTK_LABEL(driver_error_options_label_ccc), _("Virtual Disk Device Driver output options for read errors"));
  gtk_label_set_text(GTK_LABEL(driver_minimum_cluster_size_button_label_ccc), _("Virtual Disk minimum cluster read size (mode 3)"));
  gtk_label_set_text(GTK_LABEL(virtual_disk_device_name_label_ccc), _("Virtual Disk device name"));

  gtk_widget_set_tooltip_text(GTK_WIDGET(no_log_backup_checkbutton_ccc), _("If checked, no log backups will be created when writing to the progess log (not recommended)"));
  gtk_widget_set_tooltip_text(GTK_WIDGET(force_mounted_checkbutton_ccc), _("Force working with the source even when it is mounted (not recommended)"));
  gtk_widget_set_tooltip_text(GTK_WIDGET(force_dangerous_checkbutton_ccc), _("Force working with the source even if it is detected that there is another drive on the same controller (not recommended)"));
  gtk_widget_set_tooltip_text(GTK_WIDGET(enable_output_offset_checkbutton_ccc), _("Enable output offset adjustment. You must close and reopen the Advanced Settings to take effect."));
  gtk_widget_set_tooltip_text(GTK_WIDGET(output_offset_button_label_ccc), _("Offset in LBAs to start writing to the destination (default -1 follows input offset)"));
  gtk_widget_set_tooltip_text(GTK_WIDGET(enable_current_position_checkbutton_ccc), _("Enable current position adjustment. You must close and reopen the Advanced Settings to take effect."));
  gtk_widget_set_tooltip_text(GTK_WIDGET(current_position_button_label_ccc), _("Current position in the recovery (it is not recommended to change this)"));
  gtk_widget_set_tooltip_text(GTK_WIDGET(action_on_error_label_ccc), _("A major driver error is something other than a typical read error. The Exit or Slow condition is considered a major error."));
  gtk_widget_set_tooltip_text(GTK_WIDGET(stop_on_error_radio_button_ccc), _("Stop cloning and show an error message indicating the fault"));
  gtk_widget_set_tooltip_text(GTK_WIDGET(call_command_radio_button_ccc), _("Call the specified command and continue cloning. If the command returns an error, the clone will be stopped."));
  gtk_widget_set_tooltip_text(GTK_WIDGET(test_command_button_ccc), _("Test the specified command"));
  gtk_widget_set_tooltip_text(GTK_WIDGET(primary_relay_on_error_radio_button_ccc), _("Activate the primary relay board and continue cloning"));
  gtk_widget_set_tooltip_text(GTK_WIDGET(write_buffer_disable_radio_button_ccc), _("Disable the OS write buffer for writes to the destination. Safer in case of power loss, but slower."));
  gtk_widget_set_tooltip_text(GTK_WIDGET(write_buffer_enable_radio_button_ccc), _("Enable the OS write buffer for writes to the destination. Faster, but may cause corruption in case of power loss."));
  gtk_widget_set_tooltip_text(GTK_WIDGET(write_buffer_label_ccc), _("Enable or disable the OS write buffer for writes to the destination. Disabling it is safer in case of power loss, but slower."));
  gtk_widget_set_tooltip_text(GTK_WIDGET(disable_identify_checkbutton_ccc), _("Disable the identify device command when listing source drives in Direct AHCI and IDE modes. This is to prevent a system lockup or error when listing possible source drives."));
  gtk_widget_set_tooltip_text(GTK_WIDGET(pio_mode_checkbutton_ccc), _("Use PIO instead of DMA for reading the source drive."));
  gtk_widget_set_tooltip_text(GTK_WIDGET(enable_rebuild_assist_checkbutton_ccc), _("Enable Rebuild Assist. The Rebuild Assist selection in the Clone settings will have no effect if this is not checked."));
  gtk_widget_set_tooltip_text(GTK_WIDGET(enable_process_chunk_checkbutton_ccc), _("When a bad cluster is read in Passthrough ATA or Direct modes, the ATA return data can include the LBA of the first reported bad sector for the read. When this option is selected, data up to that LBA will be read, the problem sector marked as Bad, and the rest of the chunk marked as Non-Trimmed."));
  gtk_widget_set_tooltip_text(GTK_WIDGET(enable_read_twice_checkbutton_ccc), _("When enabled and the soft reset timeout is exceeded when reading, after the reset a second read attempt will be performed. The second read attempt will use the Retry reset time as the soft reset time for the this second read attempt."));
  gtk_widget_set_tooltip_text(GTK_WIDGET(enable_retry_connecting_checkbutton_ccc), _("If the device fails to identify after a relay power cycle, enabling this option will cause a second attempt to disconnect and reconnect the device."));
  gtk_widget_set_tooltip_text(GTK_WIDGET(enable_scsi_write_checkbutton_ccc), _("If the destination drive sector size is not a multiple of 512, you must enable this to be able to write to the drive."));
  gtk_widget_set_tooltip_text(GTK_WIDGET(enable_phase_log_checkbutton_ccc), _("When enabled a log with a time and phase stamp in the name will be created at the end of every phase."));
  gtk_widget_set_tooltip_text(GTK_WIDGET(enable_output_sector_size_checkbutton_ccc), _("Enable output sector size adjustment. You must close and reopen the Advanced Settings to take effect."));
  gtk_widget_set_tooltip_text(GTK_WIDGET(output_sector_size_button_label_ccc), _("Output sector adjustment in bytes (relative to the source sector size)"));
  gtk_widget_set_tooltip_text(GTK_WIDGET(driver_io_scsi_only_checkbutton_ccc), _("Only use IO SCSI for reading the virtual disk. Normal block reads will return all zeros when this is enabled."));
  gtk_widget_set_tooltip_text(GTK_WIDGET(use_physical_sector_size_for_virtual_checkbutton_ccc), _("This will use the physical sector size as the logical sector size when presenting the virtual disk to the operating system. The physical size is determined by the Block size setting in the Clone Settings."));
  gtk_widget_set_tooltip_text(GTK_WIDGET(driver_return_error_radio_button_ccc), _("Read errors will return with a standard I/O error."));
  gtk_widget_set_tooltip_text(GTK_WIDGET(driver_return_zeros_radio_button_ccc), _("Read errors will return zero filled data."));
  gtk_widget_set_tooltip_text(GTK_WIDGET(driver_return_marked_radio_button_ccc), _("Read errors will return with mark filled data."));
  gtk_widget_set_tooltip_text(GTK_WIDGET(driver_error_options_label_ccc), _("Options for how the virtual disk handles read errors as presented by the recovery."));
  gtk_widget_set_tooltip_text(GTK_WIDGET(driver_minimum_cluster_size_button_label_ccc), _("When in virtual mode 3, this will be the minimum cluster read size. When cloning using a domain, this will also be the minimum cluster read size, so that small domain reads can be prevented if desired."));
  gtk_widget_set_tooltip_text(GTK_WIDGET(virtual_disk_device_name_label_ccc), _("The name of the virtual disk in /dev. The default name is sdv (/dev/sdv). Note that if you choose a name that is already in use, it will cause a system crash. Also, you should choose a conventional name with the SDx format, or some software will not find or list it."));

  if (!advanced_settings_ccc.enable_output_offset)
  {
    gtk_widget_set_sensitive(GTK_WIDGET(output_offset_spinbutton_ccc), FALSE);
  }
  if (!advanced_settings_ccc.enable_current_position)
  {
    gtk_widget_set_sensitive(GTK_WIDGET(current_position_spinbutton_ccc), FALSE);
  }
  if (!advanced_settings_ccc.enable_output_sector_size)
  {
    gtk_widget_set_sensitive(GTK_WIDGET(output_sector_size_spinbutton_ccc), FALSE);
  }

  gtk_entry_set_max_length(GTK_ENTRY(command_to_call_text_ccc), MAX_CALL_LENGTH);
  gtk_entry_set_max_length(GTK_ENTRY(virtual_disk_device_name_text_ccc), MAX_DEVICE_NAME_LENGTH);

  g_signal_connect(G_OBJECT(no_log_backup_checkbutton_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_NOLOGBACK));
  g_signal_connect(G_OBJECT(force_mounted_checkbutton_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_FORCE));
  g_signal_connect(G_OBJECT(force_dangerous_checkbutton_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_FORCEDANGEROUS));
  g_signal_connect(G_OBJECT(enable_output_offset_checkbutton_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_OFFSET));
  g_signal_connect(G_OBJECT(enable_current_position_checkbutton_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_POSITION));
  g_signal_connect(G_OBJECT(disable_identify_checkbutton_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_DONTIDENTIFY));
  g_signal_connect(G_OBJECT(pio_mode_checkbutton_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_PIOMODE));
  g_signal_connect(G_OBJECT(enable_rebuild_assist_checkbutton_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_ENABLEREBUILDASSIST));
  g_signal_connect(G_OBJECT(enable_process_chunk_checkbutton_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_ENABLEPROCESSCHUNK));
  g_signal_connect(G_OBJECT(enable_read_twice_checkbutton_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_ENABLEREADTWICE));
  g_signal_connect(G_OBJECT(enable_retry_connecting_checkbutton_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_ENABLERETRYCONNECTING));
  g_signal_connect(G_OBJECT(enable_scsi_write_checkbutton_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_ENABLESCSIWRITE));
  g_signal_connect(G_OBJECT(enable_phase_log_checkbutton_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_ENABLEPHASELOGS));
  g_signal_connect(G_OBJECT(enable_output_sector_size_checkbutton_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_ENABLEOUTPUTSECTOR));
  g_signal_connect(G_OBJECT(driver_io_scsi_only_checkbutton_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_IOSCSIONLY));
  g_signal_connect(G_OBJECT(use_physical_sector_size_for_virtual_checkbutton_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_USEPHYSICALSECTORSIZE));

  load_advanced_settings_ccc();
  gtk_entry_set_text(GTK_ENTRY(command_to_call_text_ccc), advanced_settings_ccc.command_to_call);
  gtk_entry_set_text(GTK_ENTRY(virtual_disk_device_name_text_ccc), advanced_settings_ccc.virtual_disk_device_name);
  update_advanced_button_settings_ccc();

  if (direct_mode_ccc)
  {
    gtk_widget_set_sensitive(GTK_WIDGET(disable_identify_checkbutton_ccc), TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(pio_mode_checkbutton_ccc), TRUE);
  }
  else
  {
    gtk_widget_set_sensitive(GTK_WIDGET(disable_identify_checkbutton_ccc), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(pio_mode_checkbutton_ccc), FALSE);
  }

  gtk_window_set_title(GTK_WINDOW(dialog), _("Advanced Settings"));

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == 5)
  {
    advanced_settings_ccc.output_offset = gtk_spin_button_get_value(GTK_SPIN_BUTTON(output_offset_spinbutton_ccc));
    advanced_settings_ccc.current_position = gtk_spin_button_get_value(GTK_SPIN_BUTTON(current_position_spinbutton_ccc));
    advanced_settings_ccc.stop_on_error = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(stop_on_error_radio_button_ccc));
    advanced_settings_ccc.call_command = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(call_command_radio_button_ccc));
    advanced_settings_ccc.activate_primary_relay = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(primary_relay_on_error_radio_button_ccc));
    advanced_settings_ccc.write_buffer_disable = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(write_buffer_disable_radio_button_ccc));
    advanced_settings_ccc.write_buffer_enable = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(write_buffer_enable_radio_button_ccc));
    advanced_settings_ccc.driver_return_error = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(driver_return_error_radio_button_ccc));
    advanced_settings_ccc.driver_return_zeros = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(driver_return_zeros_radio_button_ccc));
    advanced_settings_ccc.driver_return_marked = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(driver_return_marked_radio_button_ccc));
    advanced_settings_ccc.output_sector_adjustment = gtk_spin_button_get_value(GTK_SPIN_BUTTON(output_sector_size_spinbutton_ccc));
    advanced_settings_ccc.driver_minimum_cluster_size = gtk_spin_button_get_value(GTK_SPIN_BUTTON(driver_minimum_cluster_size_spinbutton_ccc));
    const gchar *entry_text;
    entry_text = gtk_entry_get_text(GTK_ENTRY(command_to_call_text_ccc));
    strcpy(advanced_settings_ccc.command_to_call, entry_text);
    const gchar *entry_text2;
    entry_text2 = gtk_entry_get_text(GTK_ENTRY(virtual_disk_device_name_text_ccc));
    strcpy(advanced_settings_ccc.virtual_disk_device_name, entry_text2);
    update_advanced_settings_ccc();
    logfile_changed_ccc = true;
  }
  gtk_widget_destroy(dialog);
  update_display_ccc(0);
}

void open_timer_settings_dialog_ccc(void)
{
  GtkBuilder *builder = gtk_builder_new();
  GError *err = NULL; // It is mandatory to initialize to NULL
  if (0 == gtk_builder_add_from_string(builder, (const gchar *)opensuperclone_glade, opensuperclone_glade_len, &err))
  {
    fprintf(stderr, "Error adding build from file. Error: %s\n", err->message);
  }
  gtk_builder_connect_signals(builder, NULL);
  GtkWidget *dialog = GTK_WIDGET(gtk_builder_get_object(builder, "timer_settings_dialog"));
  timer_settings_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "timer_settings_label"));
  timer_settings_info_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "timer_settings_info_label"));
  initial_busy_wait_time_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "initial_busy_wait_time_button_label"));
  initial_busy_wait_time_spinbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "initial_busy_wait_time_spinbutton"));
  busy_wait_time_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "busy_wait_time_button_label"));
  busy_wait_time_spinbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "busy_wait_time_spinbutton"));
  soft_reset_time_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "soft_reset_time_button_label"));
  soft_reset_time_spinbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "soft_reset_time_spinbutton"));
  hard_reset_time_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "hard_reset_time_button_label"));
  hard_reset_time_spinbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "hard_reset_time_spinbutton"));
  power_cycle_time_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "power_cycle_time_button_label"));
  power_cycle_time_spinbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "power_cycle_time_spinbutton"));
  general_timout_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "general_timout_button_label"));
  general_timeout_spinbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "general_timeout_spinbutton"));
  action_power_cycle_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "action_power_cycle_label"));
  stop_on_power_cycle_radio_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "stop_on_power_cycle_radio_button"));
  call_power_command_radio_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "call_power_command_radio_button"));
  power_command_to_call_text_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "power_command_to_call_text"));
  test_power_command_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "test_power_command_button"));
  primary_relay_power_cycle_radio_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "primary_relay_power_cycle_radio_button"));
  phase_timers_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "phase_timers_label"));
  phase_timers_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "phase_timers_checkbutton"));
  p12_soft_reset_time_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "p12_soft_reset_time_button_label"));
  p12_soft_reset_time_spinbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "p12_soft_reset_time_spinbutton"));
  p3_soft_reset_time_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "p3_soft_reset_time_button_label"));
  p3_soft_reset_time_spinbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "p3_soft_reset_time_spinbutton"));
  p4_soft_reset_time_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "p4_soft_reset_time_button_label"));
  p4_soft_reset_time_spinbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "p4_soft_reset_time_spinbutton"));
  td_soft_reset_time_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "td_soft_reset_time_button_label"));
  td_soft_reset_time_spinbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "td_soft_reset_time_spinbutton"));
  d2_soft_reset_time_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "d2_soft_reset_time_button_label"));
  d2_soft_reset_time_spinbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "d2_soft_reset_time_spinbutton"));
  sc_soft_reset_time_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "sc_soft_reset_time_button_label"));
  sc_soft_reset_time_spinbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "sc_soft_reset_time_spinbutton"));
  rt_soft_reset_time_button_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "rt_soft_reset_time_button_label"));
  rt_soft_reset_time_spinbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "rt_soft_reset_time_spinbutton"));
  always_wait_for_reset_timers_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "always_wait_for_reset_timers_checkbutton"));
  usb_resets_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "usb_resets_label"));
  enable_usb_bulk_reset_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "enable_usb_bulk_reset_checkbutton"));
  enable_usb_soft_reset_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "enable_usb_soft_reset_checkbutton"));
  enable_usb_hard_reset_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "enable_usb_hard_reset_checkbutton"));
  enable_usb_port_reset_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "enable_usb_port_reset_checkbutton"));
  g_object_unref(builder);

  gtk_label_set_text(GTK_LABEL(timer_settings_label_ccc), "");
  gtk_label_set_text(GTK_LABEL(timer_settings_info_label_ccc), _("These timer settings are only for the Direct modes.\nThey have no effect with the Passthrough modes.\n"));
  gtk_label_set_text(GTK_LABEL(initial_busy_wait_time_button_label_ccc), _("Initial busy wait time (ms)"));
  gtk_label_set_text(GTK_LABEL(busy_wait_time_button_label_ccc), _("Busy wait time (ms)"));
  gtk_label_set_text(GTK_LABEL(soft_reset_time_button_label_ccc), _("Soft reset time (ms)"));
  gtk_label_set_text(GTK_LABEL(hard_reset_time_button_label_ccc), _("Hard reset time (ms)"));
  gtk_label_set_text(GTK_LABEL(power_cycle_time_button_label_ccc), _("Reset timeout (ms)"));
  gtk_label_set_text(GTK_LABEL(general_timout_button_label_ccc), _("General timeout (ms)"));
  gtk_label_set_text(GTK_LABEL(action_power_cycle_label_ccc), _("Action for reset timeout"));
  gtk_button_set_label(GTK_BUTTON(stop_on_power_cycle_radio_button_ccc), _("Do nothing"));
  gtk_button_set_label(GTK_BUTTON(call_power_command_radio_button_ccc), _("Call command:"));
  gtk_button_set_label(GTK_BUTTON(test_power_command_button_ccc), _("Test command"));
  gtk_button_set_label(GTK_BUTTON(primary_relay_power_cycle_radio_button_ccc), _("Activate primary relay"));
  gtk_label_set_text(GTK_LABEL(phase_timers_label_ccc), _("Phase timers (soft reset timers)"));
  gtk_button_set_label(GTK_BUTTON(phase_timers_checkbutton_ccc), _("Enable phase timers"));
  gtk_label_set_text(GTK_LABEL(p12_soft_reset_time_button_label_ccc), _("Phase 1-2 reset time (ms)"));
  gtk_label_set_text(GTK_LABEL(p3_soft_reset_time_button_label_ccc), _("Phase 3 reset time (ms)"));
  gtk_label_set_text(GTK_LABEL(p4_soft_reset_time_button_label_ccc), _("Phase 4 reset time (ms)"));
  gtk_label_set_text(GTK_LABEL(td_soft_reset_time_button_label_ccc), _("Trim/Divide reset time (ms)"));
  gtk_label_set_text(GTK_LABEL(d2_soft_reset_time_button_label_ccc), _("Divide 2 reset time (ms)"));
  gtk_label_set_text(GTK_LABEL(sc_soft_reset_time_button_label_ccc), _("Scrape reset time (ms)"));
  gtk_label_set_text(GTK_LABEL(rt_soft_reset_time_button_label_ccc), _("Retry reset time (ms)"));
  gtk_button_set_label(GTK_BUTTON(always_wait_for_reset_timers_checkbutton_ccc), _("Always wait for reset timers"));
  gtk_label_set_text(GTK_LABEL(usb_resets_label_ccc), _("USB reset choices"));
  gtk_button_set_label(GTK_BUTTON(enable_usb_bulk_reset_checkbutton_ccc), _("Enable USB bulk reset"));
  gtk_button_set_label(GTK_BUTTON(enable_usb_soft_reset_checkbutton_ccc), _("Enable USB soft reset"));
  gtk_button_set_label(GTK_BUTTON(enable_usb_hard_reset_checkbutton_ccc), _("Enable USB hard reset"));
  gtk_button_set_label(GTK_BUTTON(enable_usb_port_reset_checkbutton_ccc), _("Enable USB port reset"));

  gtk_entry_set_max_length(GTK_ENTRY(power_command_to_call_text_ccc), MAX_CALL_LENGTH);

  g_signal_connect(G_OBJECT(phase_timers_checkbutton_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_PHASETIMERS));

  load_timer_settings_ccc();
  gtk_entry_set_text(GTK_ENTRY(power_command_to_call_text_ccc), timer_settings_ccc.power_command_to_call);
  update_timer_button_settings_ccc();

  if (!direct_mode_ccc && !usb_mode_ccc)
  {
    gtk_widget_set_sensitive(GTK_WIDGET(initial_busy_wait_time_spinbutton_ccc), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(busy_wait_time_spinbutton_ccc), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(soft_reset_time_spinbutton_ccc), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(power_cycle_time_spinbutton_ccc), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(stop_on_power_cycle_radio_button_ccc), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(call_power_command_radio_button_ccc), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(test_power_command_button_ccc), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(power_command_to_call_text_ccc), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(primary_relay_power_cycle_radio_button_ccc), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(phase_timers_checkbutton_ccc), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(p12_soft_reset_time_spinbutton_ccc), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(p3_soft_reset_time_spinbutton_ccc), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(p4_soft_reset_time_spinbutton_ccc), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(td_soft_reset_time_spinbutton_ccc), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(d2_soft_reset_time_spinbutton_ccc), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(sc_soft_reset_time_spinbutton_ccc), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(rt_soft_reset_time_spinbutton_ccc), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(always_wait_for_reset_timers_checkbutton_ccc), FALSE);
  }
  if (!usb_mode_ccc)
  {
    gtk_widget_set_sensitive(GTK_WIDGET(enable_usb_bulk_reset_checkbutton_ccc), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(enable_usb_soft_reset_checkbutton_ccc), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(enable_usb_hard_reset_checkbutton_ccc), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(enable_usb_port_reset_checkbutton_ccc), FALSE);
  }
  if (!ahci_mode_ccc && !usb_mode_ccc)
  {
    gtk_widget_set_sensitive(GTK_WIDGET(hard_reset_time_spinbutton_ccc), FALSE);
  }

  gtk_window_set_title(GTK_WINDOW(dialog), _("Timer Settings"));

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == 5)
  {
    timer_settings_ccc.initial_busy_wait_time = gtk_spin_button_get_value(GTK_SPIN_BUTTON(initial_busy_wait_time_spinbutton_ccc));
    timer_settings_ccc.busy_wait_time = gtk_spin_button_get_value(GTK_SPIN_BUTTON(busy_wait_time_spinbutton_ccc));
    timer_settings_ccc.soft_reset_time = gtk_spin_button_get_value(GTK_SPIN_BUTTON(soft_reset_time_spinbutton_ccc));
    timer_settings_ccc.hard_reset_time = gtk_spin_button_get_value(GTK_SPIN_BUTTON(hard_reset_time_spinbutton_ccc));
    timer_settings_ccc.power_cycle_time = gtk_spin_button_get_value(GTK_SPIN_BUTTON(power_cycle_time_spinbutton_ccc));
    timer_settings_ccc.general_timeout = gtk_spin_button_get_value(GTK_SPIN_BUTTON(general_timeout_spinbutton_ccc));
    timer_settings_ccc.stop_on_power_cycle = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(stop_on_power_cycle_radio_button_ccc));
    timer_settings_ccc.call_power_command = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(call_power_command_radio_button_ccc));
    timer_settings_ccc.power_cycle_primary_relay = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(primary_relay_power_cycle_radio_button_ccc));

    timer_settings_ccc.p12_soft_reset_time = gtk_spin_button_get_value(GTK_SPIN_BUTTON(p12_soft_reset_time_spinbutton_ccc));
    timer_settings_ccc.p3_soft_reset_time = gtk_spin_button_get_value(GTK_SPIN_BUTTON(p3_soft_reset_time_spinbutton_ccc));
    timer_settings_ccc.p4_soft_reset_time = gtk_spin_button_get_value(GTK_SPIN_BUTTON(p4_soft_reset_time_spinbutton_ccc));
    timer_settings_ccc.td_soft_reset_time = gtk_spin_button_get_value(GTK_SPIN_BUTTON(td_soft_reset_time_spinbutton_ccc));
    timer_settings_ccc.d2_soft_reset_time = gtk_spin_button_get_value(GTK_SPIN_BUTTON(d2_soft_reset_time_spinbutton_ccc));
    timer_settings_ccc.sc_soft_reset_time = gtk_spin_button_get_value(GTK_SPIN_BUTTON(sc_soft_reset_time_spinbutton_ccc));
    timer_settings_ccc.rt_soft_reset_time = gtk_spin_button_get_value(GTK_SPIN_BUTTON(rt_soft_reset_time_spinbutton_ccc));

    timer_settings_ccc.always_wait_for_reset_timers = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(always_wait_for_reset_timers_checkbutton_ccc));

    timer_settings_ccc.usb_bulk_reset_enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enable_usb_bulk_reset_checkbutton_ccc));
    timer_settings_ccc.usb_soft_reset_enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enable_usb_soft_reset_checkbutton_ccc));
    timer_settings_ccc.usb_hard_reset_enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enable_usb_hard_reset_checkbutton_ccc));
    timer_settings_ccc.usb_port_reset_enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enable_usb_port_reset_checkbutton_ccc));

    const gchar *entry_text;
    entry_text = gtk_entry_get_text(GTK_ENTRY(power_command_to_call_text_ccc));
    strcpy(timer_settings_ccc.power_command_to_call, entry_text);
    update_timer_settings_ccc();
    logfile_changed_ccc = true;
  }
  gtk_widget_destroy(dialog);
  update_display_ccc(0);
}

int open_ports_dialog_ccc(char *current_ports)
{
  disabled_ports_changed_ccc = 0;
  int confirm = 0;
  GtkBuilder *builder = gtk_builder_new();
  GError *err = NULL; // It is mandatory to initialize to NULL
  if (0 == gtk_builder_add_from_string(builder, (const gchar *)opensuperclone_glade, opensuperclone_glade_len, &err))
  {
    fprintf(stderr, "Error adding build from file. Error: %s\n", err->message);
  }
  gtk_builder_connect_signals(builder, NULL);
  GtkWidget *dialog = GTK_WIDGET(gtk_builder_get_object(builder, "disable_ports_dialog"));
  disable_ports_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "disable_ports_label"));
  disable_ports_info_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "disable_ports_info_label"));
  current_ports_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "current_ports_label"));
  current_ports_info_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "current_ports_info_label"));
  set_disabled_ports_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "set_disabled_ports_label"));
  set_disabled_ports_text_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "set_disabled_ports_text"));
  updated_ports_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "updated_ports_label"));
  updated_ports_info_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "updated_ports_info_label"));
  update_ports_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "update_ports_button"));
  g_object_unref(builder);

  new_ports_ccc[0] = '\0';
  char portsinfo[4096] = "";
  snprintf(portsinfo, sizeof(portsinfo), "%s%s", _("Disable ports for AHCI mode to hide a drive from the operating system.\nWarning! If you disable the port for the operating system drive,\nthe system will not be able to boot!\n"),
           _("Enter the ports you wish to be disabled as integers separated by spaces.\nIt is best that the port numbers are in order from lowest to highest.\nYou can find port information by listing source drives in AHCI mode.\n"));
  gtk_label_set_text(GTK_LABEL(disable_ports_info_label_ccc), portsinfo);
  gtk_label_set_text(GTK_LABEL(disable_ports_label_ccc), _("Disable Ports"));
  gtk_label_set_text(GTK_LABEL(current_ports_label_ccc), _("Current disabled ports (GRUB data)"));
  gtk_label_set_text(GTK_LABEL(current_ports_info_label_ccc), current_ports);
  gtk_label_set_text(GTK_LABEL(set_disabled_ports_label_ccc), _("Set disabled ports"));
  gtk_label_set_text(GTK_LABEL(updated_ports_label_ccc), _("Updated disabled ports (GRUB data)"));
  gtk_label_set_text(GTK_LABEL(updated_ports_info_label_ccc), current_ports);
  gtk_button_set_label(GTK_BUTTON(update_ports_button_ccc), _("Update ports"));

  gtk_window_set_title(GTK_WINDOW(dialog), _("Disable Ports"));

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == 5)
  {
    if (disabled_ports_changed_ccc)
    {
      confirm = 1;
    }
  }
  gtk_widget_destroy(dialog);
  return confirm;
}

int set_lun_dialog_ccc(int max_lun)
{
  GtkBuilder *builder = gtk_builder_new();
  GError *err = NULL; // It is mandatory to initialize to NULL
  if (0 == gtk_builder_add_from_string(builder, (const gchar *)opensuperclone_glade, opensuperclone_glade_len, &err))
  {
    fprintf(stderr, "Error adding build from file. Error: %s\n", err->message);
  }
  gtk_builder_connect_signals(builder, NULL);
  GtkWidget *dialog = GTK_WIDGET(gtk_builder_get_object(builder, "select_lun_dialog"));
  select_lun_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "select_lun_label"));
  select_lun_info_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "select_lun_info_label"));
  lun_values_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "lun_values_label"));
  lun_values_info_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "lun_values_info_label"));
  set_current_lun_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "set_current_lun_label"));
  current_lun_spinbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "current_lun_spinbutton"));
  g_object_unref(builder);

  char luninfo[256] = "";
  snprintf(luninfo, sizeof(luninfo), "0 - %d", max_lun);
  gtk_label_set_text(GTK_LABEL(select_lun_label_ccc), _("Select Logical Unit"));
  gtk_label_set_text(GTK_LABEL(select_lun_info_label_ccc), _("This device has more than 1 logical unit.\nPlease choose which logical unit."));
  gtk_label_set_text(GTK_LABEL(lun_values_label_ccc), _("Possible Logical Units"));
  gtk_label_set_text(GTK_LABEL(lun_values_info_label_ccc), luninfo);
  gtk_label_set_text(GTK_LABEL(set_current_lun_label_ccc), _("Set Logical Unit"));

  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, 0, max_lun, 1, 1, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(current_lun_spinbutton_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(current_lun_spinbutton_ccc), usb_lun_ccc);

  gtk_window_set_title(GTK_WINDOW(dialog), _("Select Logical Unit"));

  int ret = 1;
  if (gtk_dialog_run(GTK_DIALOG(dialog)) == 5)
  {
    usb_lun_ccc = gtk_spin_button_get_value(GTK_SPIN_BUTTON(current_lun_spinbutton_ccc));
    usb_lun_set_ccc = 1;
    ret = 0;
    fprintf(stdout, "lun=%d\n", usb_lun_ccc);
  }
  gtk_widget_destroy(dialog);
  return ret;
}

// Preset status poll rates for the Asclepius poller thread. The millisecond
// value itself is persisted, so reordering this list cannot corrupt a config.
static const unsigned int asclepius_poll_intervals_ccc[] = { 25, 50, 100, 200, 500, 1000, 2000, 5000 };
#define ASCLEPIUS_NUM_POLL_INTERVALS (sizeof(asclepius_poll_intervals_ccc) / sizeof(asclepius_poll_intervals_ccc[0]))

// Preset software OCP trip limits in mA. As with the poll rate, the mA value
// is persisted rather than an index into this table.
static const unsigned int asclepius_ocp_limits_ccc[] = { 500, 1000, 1500, 2000, 2500, 3000, 3500, 4000, 4500 };
#define ASCLEPIUS_NUM_OCP_LIMITS (sizeof(asclepius_ocp_limits_ccc) / sizeof(asclepius_ocp_limits_ccc[0]))

// Index of the first preset at or above value, clamped into the table
static int asclepius_preset_index_ccc(const unsigned int *presets, size_t count, unsigned int value)
{
  size_t i;

  for (i = 0; i < count; i++)
  {
    if (presets[i] >= value)
    {
      return (int)i;
    }
  }

  return (int)(count - 1);
}

// Shows only the settings that apply to the selected board type, so the
// window reads as one relay board at a time rather than two bolted together.
void update_primary_relay_board_type_visibility_ccc(void)
{
  bool asclepius = primary_relay_board_type_ccc == PRIMARY_RELAY_BOARD_ASCLEPIUS;

  for(int i = 0; i < 4; i++)
  {
    if(primary_relay_generic_frames_ccc[i] != NULL)
    {
      gtk_widget_set_visible(primary_relay_generic_frames_ccc[i], !asclepius);
    }
    if(primary_relay_asclepius_frames_ccc[i] != NULL)
    {
      gtk_widget_set_visible(primary_relay_asclepius_frames_ccc[i], asclepius);
    }
  }

  // The relay buttons only work when the selected board is actually reachable.
  bool connected = primary_relay_board_is_connected_ccc(primary_relay_board_type_ccc);
  if(activate_primary_relay_button_ccc != NULL)
  {
    gtk_widget_set_sensitive(activate_primary_relay_button_ccc, connected);
  }
  if(deactivate_primary_relay_button_ccc != NULL)
  {
    gtk_widget_set_sensitive(deactivate_primary_relay_button_ccc, connected);
  }
}

static void update_primary_relay_connection_summary_ccc(void)
{
  char summary[512];

  if(data_current_relay_board_a_ccc == NULL)
  {
    return;
  }

  if(primary_relay_board_type_ccc == PRIMARY_RELAY_BOARD_ASCLEPIUS)
  {
    snprintf(summary, sizeof(summary), _("Asclepius\nConnection: %s\nControl port: %s"),
             asclepius_get_connection_status() ? _("Connected") : _("Disconnected"),
             asclepius_tty_device_ccc[0] != '\0' ? asclepius_tty_device_ccc : _("not configured"));
  }
  else
  {
    snprintf(summary, sizeof(summary), _("USB relay\nConnection: %s"),
             usbr1_chosen_ccc ? _("Selected") : _("Not selected"));
  }

  gtk_label_set_text(GTK_LABEL(data_current_relay_board_a_ccc), summary);
}

void on_primary_relay_board_type_changed_ccc(GtkWidget *button, gpointer data)
{
  (void)data;

  // Both radios fire this handler, so only act on the one that became active.
  if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
  {
    return;
  }

  int board_type = PRIMARY_RELAY_BOARD_GENERIC;

  if(button == primary_relay_board_asclepius_radio_ccc)
  {
    board_type = PRIMARY_RELAY_BOARD_ASCLEPIUS;
  }

  primary_relay_board_type_ccc = board_type;
  primary_relay_settings_ccc.primary_relay_board_type = board_type;

  update_primary_relay_board_type_visibility_ccc();
  update_primary_relay_connection_summary_ccc();

  // Capture the rest of the window before applying the buffer. Applying it on
  // its own pushes every other setting the buffer holds over whatever the user
  // has typed since the dialog opened, so clicking this radio could undo a
  // chosen tty. from_buttons() leaves board_type and the relay name alone, so
  // the choice above still wins.
  update_primary_relay_settings_from_buttons_ccc();
  update_primary_relay_settings_ccc();
}

void open_primary_relay_dialog_ccc(void)
{
  struct primary_relay_settings_t original_settings;
  int original_board_type;

  GtkBuilder *builder = gtk_builder_new();
  GError *err = NULL; // It is mandatory to initialize to NULL
  if (0 == gtk_builder_add_from_string(builder, (const gchar *)opensuperclone_glade, opensuperclone_glade_len, &err))
  {
    fprintf(stderr, "Error adding build from file. Error: %s\n", err->message);
  }
  gtk_builder_connect_signals(builder, NULL);
  GtkWidget *dialog = GTK_WIDGET(gtk_builder_get_object(builder, "primary_relay_settings_dialog"));
  primary_relay_settings_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "primary_relay_settings_label"));
  primary_relay_settings_info_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "primary_relay_settings_info_label"));
  label_current_primary_relay_board_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "label_current_primary_relay_board"));
  data_current_relay_board_a_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "data_current_relay_board_a"));
  asclepius_relay_source_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_relay_source_label"));
  asclepius_relay_source_combobox_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_relay_source_combobox"));
  asclepius_soft_ocp_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_soft_ocp_label"));
  asclepius_poll_interval_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_poll_interval_label"));
  asclepius_poll_interval_combobox_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_poll_interval_combobox"));
  asclepius_ocp_12v_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_ocp_12v_label"));
  asclepius_ocp_5v_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_ocp_5v_label"));
  asclepius_ocp_usb_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_ocp_usb_label"));
  asclepius_ocp_action_12v_combobox_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_ocp_action_12v_combobox"));
  asclepius_ocp_action_5v_combobox_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_ocp_action_5v_combobox"));
  asclepius_ocp_action_usb_combobox_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_ocp_action_usb_combobox"));
  asclepius_ocp_limit_12v_combobox_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_ocp_limit_12v_combobox"));
  asclepius_ocp_limit_5v_combobox_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_ocp_limit_5v_combobox"));
  asclepius_ocp_limit_usb_combobox_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_ocp_limit_usb_combobox"));
  asclepius_tty_device_text_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_tty_device_text"));
  asclepius_connect_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_connect_button"));
  asclepius_disconnect_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_disconnect_button"));
  primary_relay_board_settings_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "primary_relay_board_settings_label"));

  // Board-type selector and the per-type setting frames. The two radios are
  // grouped by the "group" property in the glade: GTK3 does not group radio
  // buttons merely by sharing a container, and an ungrouped radio button
  // starts out active in its own singleton group.
  primary_relay_board_type_frame_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "frame_primary_relay_board_type"));
  primary_relay_board_type_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "primary_relay_board_type_label"));
  primary_relay_board_type_frame_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "primary_relay_board_type_frame_label"));
  primary_relay_board_generic_radio_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "primary_relay_board_generic_radio"));
  primary_relay_board_asclepius_radio_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "primary_relay_board_asclepius_radio"));

  // The generic USB relay's sequencing model: "Activation condition" is
  // frame100 (the Energize relay 1..8 checkbuttons), "Deactivation condition"
  // is frame96, hbox34 holds the two timers and frame99 the test button. None
  // of it applies to Asclepius, whose rails are switched directly.
  primary_relay_generic_frames_ccc[0] = GTK_WIDGET(gtk_builder_get_object(builder, "frame100"));
  primary_relay_generic_frames_ccc[1] = GTK_WIDGET(gtk_builder_get_object(builder, "frame96"));
  primary_relay_generic_frames_ccc[2] = GTK_WIDGET(gtk_builder_get_object(builder, "hbox34"));
  primary_relay_generic_frames_ccc[3] = GTK_WIDGET(gtk_builder_get_object(builder, "frame99"));

  primary_relay_asclepius_frames_ccc[0] = GTK_WIDGET(gtk_builder_get_object(builder, "frame_asclepius_relay_source"));
  primary_relay_asclepius_frames_ccc[1] = GTK_WIDGET(gtk_builder_get_object(builder, "frame_asclepius_tty_device"));
  primary_relay_asclepius_frames_ccc[2] = GTK_WIDGET(gtk_builder_get_object(builder, "frame_asclepius_soft_ocp"));
  primary_relay_asclepius_frames_ccc[3] = GTK_WIDGET(gtk_builder_get_object(builder, "hbox_asclepius_buttons"));

  primary_relay_activation_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "primary_relay_activation_label"));
  activate_primary_relay1_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "activate_primary_relay1_checkbutton"));
  activate_primary_relay2_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "activate_primary_relay2_checkbutton"));
  activate_primary_relay3_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "activate_primary_relay3_checkbutton"));
  activate_primary_relay4_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "activate_primary_relay4_checkbutton"));
  activate_primary_relay5_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "activate_primary_relay5_checkbutton"));
  activate_primary_relay6_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "activate_primary_relay6_checkbutton"));
  activate_primary_relay7_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "activate_primary_relay7_checkbutton"));
  activate_primary_relay8_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "activate_primary_relay8_checkbutton"));
  primary_relay_deactivation_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "primary_relay_deactivation_label"));
  deactivate_primary_relay1_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "deactivate_primary_relay1_checkbutton"));
  deactivate_primary_relay2_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "deactivate_primary_relay2_checkbutton"));
  deactivate_primary_relay3_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "deactivate_primary_relay3_checkbutton"));
  deactivate_primary_relay4_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "deactivate_primary_relay4_checkbutton"));
  deactivate_primary_relay5_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "deactivate_primary_relay5_checkbutton"));
  deactivate_primary_relay6_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "deactivate_primary_relay6_checkbutton"));
  deactivate_primary_relay7_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "deactivate_primary_relay7_checkbutton"));
  deactivate_primary_relay8_checkbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "deactivate_primary_relay8_checkbutton"));
  primary_relay_activate_time_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "primary_relay_activate_time_label"));
  primary_relay_energize_time_spinbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "primary_relay_energize_time_spinbutton"));
  primary_relay_delay_time_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "primary_relay_delay_time_label"));
  primary_relay_delay_time_spinbutton_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "primary_relay_delay_time_spinbutton"));
  test_primary_relay_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "test_primary_relay_button"));
  activate_primary_relay_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "activate_primary_relay_button"));
  deactivate_primary_relay_button_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "deactivate_primary_relay_button"));

  // Port picker, so the device does not have to be typed or guessed after a
  // re-enumeration moves it to a different tty. Wired here rather than further
  // down because the builder is released just below, and the handler needs a
  // live GtkBuilder to reach the button.
  {
    GtkWidget *ports_btn = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_ports_button"));
    if (ports_btn)
    {
      g_object_set_data(G_OBJECT(ports_btn), "asclepius_ports_target_key", (gpointer)"asclepius_tty_device_text");
      g_signal_connect(ports_btn, "clicked", G_CALLBACK(on_asclepius_ports_clicked_ccc), NULL);
    }
  }
  // = GTK_WIDGET (gtk_builder_get_object (builder, ""));
  // = GTK_WIDGET (gtk_builder_get_object (builder, ""));
  // = GTK_WIDGET (gtk_builder_get_object (builder, ""));
  // = GTK_WIDGET (gtk_builder_get_object (builder, ""));
  g_object_unref(builder);

  gtk_label_set_text(GTK_LABEL(primary_relay_settings_label_ccc), "");
  gtk_label_set_text(GTK_LABEL(primary_relay_settings_info_label_ccc), _("Primary relay board settings"));
  gtk_label_set_text(GTK_LABEL(label_current_primary_relay_board_ccc), _("Current relay board"));
  gtk_label_set_text(GTK_LABEL(asclepius_relay_source_label_ccc), _("Rail driven by power-cycling"));
  gtk_label_set_text(GTK_LABEL(asclepius_soft_ocp_label_ccc), _("Asclepius soft OCP"));
  gtk_label_set_text(GTK_LABEL(asclepius_poll_interval_label_ccc), _("Status poll interval"));
  gtk_label_set_text(GTK_LABEL(asclepius_ocp_12v_label_ccc), _("12V software OCP"));
  gtk_label_set_text(GTK_LABEL(asclepius_ocp_5v_label_ccc), _("5V software OCP"));
  gtk_label_set_text(GTK_LABEL(asclepius_ocp_usb_label_ccc), _("USB 5V software OCP"));
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(asclepius_relay_source_combobox_ccc), _("12V"));
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(asclepius_relay_source_combobox_ccc), _("5V"));
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(asclepius_relay_source_combobox_ccc), _("USB"));
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(asclepius_relay_source_combobox_ccc), _("MOLEX"));
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(asclepius_relay_source_combobox_ccc), _("All"));
  gtk_label_set_text(GTK_LABEL(primary_relay_board_settings_label_ccc), _("Relay board settings"));

  // The caption lives in the frame's label child, not the frame itself.
  // Passing the GtkFrame to gtk_label_set_text() only trips an assertion and
  // leaves the untranslated glade string in place.
  if(primary_relay_board_type_frame_label_ccc != NULL)
  {
    gtk_label_set_text(GTK_LABEL(primary_relay_board_type_frame_label_ccc), _("Relay board in use"));
  }
  if(primary_relay_board_type_label_ccc != NULL)
  {
    gtk_label_set_text(GTK_LABEL(primary_relay_board_type_label_ccc), _("Board type"));
  }
  if(primary_relay_board_generic_radio_ccc != NULL)
  {
    gtk_button_set_label(GTK_BUTTON(primary_relay_board_generic_radio_ccc), _("USB relay"));
  }
  if(primary_relay_board_asclepius_radio_ccc != NULL)
  {
    gtk_button_set_label(GTK_BUTTON(primary_relay_board_asclepius_radio_ccc), _("Asclepius"));
  }
  gtk_label_set_text(GTK_LABEL(primary_relay_activation_label_ccc), _("Activation condition"));
  gtk_button_set_label(GTK_BUTTON(activate_primary_relay1_checkbutton_ccc), _("Energize relay 1"));
  gtk_button_set_label(GTK_BUTTON(activate_primary_relay2_checkbutton_ccc), _("Energize relay 2"));
  gtk_button_set_label(GTK_BUTTON(activate_primary_relay3_checkbutton_ccc), _("Energize relay 3"));
  gtk_button_set_label(GTK_BUTTON(activate_primary_relay4_checkbutton_ccc), _("Energize relay 4"));
  gtk_button_set_label(GTK_BUTTON(activate_primary_relay5_checkbutton_ccc), _("Energize relay 5"));
  gtk_button_set_label(GTK_BUTTON(activate_primary_relay6_checkbutton_ccc), _("Energize relay 6"));
  gtk_button_set_label(GTK_BUTTON(activate_primary_relay7_checkbutton_ccc), _("Energize relay 7"));
  gtk_button_set_label(GTK_BUTTON(activate_primary_relay8_checkbutton_ccc), _("Energize relay 8"));
  gtk_label_set_text(GTK_LABEL(primary_relay_deactivation_label_ccc), _("Deactivation condition"));
  gtk_button_set_label(GTK_BUTTON(deactivate_primary_relay1_checkbutton_ccc), _("Active after power-on: relay 1"));
  gtk_button_set_label(GTK_BUTTON(deactivate_primary_relay2_checkbutton_ccc), _("Active after power-on: relay 2"));
  gtk_button_set_label(GTK_BUTTON(deactivate_primary_relay3_checkbutton_ccc), _("Active after power-on: relay 3"));
  gtk_button_set_label(GTK_BUTTON(deactivate_primary_relay4_checkbutton_ccc), _("Active after power-on: relay 4"));
  gtk_button_set_label(GTK_BUTTON(deactivate_primary_relay5_checkbutton_ccc), _("Active after power-on: relay 5"));
  gtk_button_set_label(GTK_BUTTON(deactivate_primary_relay6_checkbutton_ccc), _("Active after power-on: relay 6"));
  gtk_button_set_label(GTK_BUTTON(deactivate_primary_relay7_checkbutton_ccc), _("Active after power-on: relay 7"));
  gtk_button_set_label(GTK_BUTTON(deactivate_primary_relay8_checkbutton_ccc), _("Active after power-on: relay 8"));
  gtk_label_set_text(GTK_LABEL(primary_relay_activate_time_label_ccc), _("Activation time (sec)"));
  gtk_label_set_text(GTK_LABEL(primary_relay_delay_time_label_ccc), _("Post activation delay time (sec)"));
  gtk_button_set_label(GTK_BUTTON(test_primary_relay_button_ccc), _("Test relay operation"));
  gtk_button_set_label(GTK_BUTTON(activate_primary_relay_button_ccc), _("Activate (Power Off)"));
  gtk_button_set_label(GTK_BUTTON(deactivate_primary_relay_button_ccc), _("Deactivate (Power On)"));
  gtk_button_set_label(GTK_BUTTON(asclepius_connect_button_ccc), _("Connect Asclepius"));
  gtk_button_set_label(GTK_BUTTON(asclepius_disconnect_button_ccc), _("Disconnect Asclepius"));

  char rate_text[32];
  size_t i;
  for (i = 0; i < ASCLEPIUS_NUM_POLL_INTERVALS; i++)
  {
    snprintf(rate_text, sizeof(rate_text), "%u ms", asclepius_poll_intervals_ccc[i]);
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(asclepius_poll_interval_combobox_ccc), rate_text);
  }

  char limit_text[32];
  for (i = 0; i < ASCLEPIUS_NUM_OCP_LIMITS; i++)
  {
    snprintf(limit_text, sizeof(limit_text), "%.1f A", (float)asclepius_ocp_limits_ccc[i] / 1000);
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(asclepius_ocp_limit_12v_combobox_ccc), limit_text);
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(asclepius_ocp_limit_5v_combobox_ccc), limit_text);
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(asclepius_ocp_limit_usb_combobox_ccc), limit_text);
  }

  GtkWidget *action_combos[] = { asclepius_ocp_action_12v_combobox_ccc, asclepius_ocp_action_5v_combobox_ccc, asclepius_ocp_action_usb_combobox_ccc };
  for (i = 0; i < sizeof(action_combos) / sizeof(action_combos[0]); i++)
  {
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(action_combos[i]), _("Off"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(action_combos[i]), _("Warn only"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(action_combos[i]), _("Cut power"));
  }

  // gtk_entry_set_max_length (GTK_ENTRY (power_command_to_call_text_ccc), MAX_CALL_LENGTH);
  gtk_entry_set_max_length(GTK_ENTRY(asclepius_tty_device_text_ccc), 255);

  // g_signal_connect(G_OBJECT(phase_timers_checkbutton_ccc), "toggled", G_CALLBACK(set_state_from_button_ccc), GINT_TO_POINTER(BUTTONID_PHASETIMERS));

  load_primary_relay_settings_ccc();
  original_settings = primary_relay_settings_ccc;
  original_board_type = primary_relay_board_type_ccc;
  gtk_entry_set_text(GTK_ENTRY(asclepius_tty_device_text_ccc), primary_relay_settings_ccc.asclepius_tty_device);
  update_primary_relay_button_settings_ccc();

  // Reflect the saved board type in the selector, then show the matching
  // settings. Setting the radio fires the toggled handler, so do this before
  // the visibility update rather than after.
  primary_relay_board_type_ccc = primary_relay_board_type_valid(primary_relay_settings_ccc.primary_relay_board_type);
  {
    // Setting the active radio fires on_primary_relay_board_type_changed_ccc,
    // which stores the choice and shows the matching settings section.
    bool asclepius = primary_relay_board_type_ccc == PRIMARY_RELAY_BOARD_ASCLEPIUS;
    GtkWidget *target = asclepius ? primary_relay_board_asclepius_radio_ccc : primary_relay_board_generic_radio_ccc;
    if(target != NULL)
    {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(target), TRUE);
    }
  }
  update_primary_relay_board_type_visibility_ccc();
  update_primary_relay_connection_summary_ccc();

  gtk_window_set_title(GTK_WINDOW(dialog), _("Primary Relay Settings"));

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == 5)
  {
    update_primary_relay_settings_from_buttons_ccc();

    update_primary_relay_settings_ccc();
    logfile_changed_ccc = true;
  }
  else
  {
    primary_relay_settings_ccc = original_settings;
    primary_relay_board_type_ccc = original_board_type;
    update_primary_relay_settings_ccc();
  }
  gtk_widget_destroy(dialog);
  update_display_ccc(0);
}

void update_primary_relay_settings_from_buttons_ccc(void)
{
  primary_relay_settings_ccc.activate_primary_relay1 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(activate_primary_relay1_checkbutton_ccc));
  primary_relay_settings_ccc.activate_primary_relay2 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(activate_primary_relay2_checkbutton_ccc));
  primary_relay_settings_ccc.activate_primary_relay3 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(activate_primary_relay3_checkbutton_ccc));
  primary_relay_settings_ccc.activate_primary_relay4 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(activate_primary_relay4_checkbutton_ccc));
  primary_relay_settings_ccc.activate_primary_relay5 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(activate_primary_relay5_checkbutton_ccc));
  primary_relay_settings_ccc.activate_primary_relay6 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(activate_primary_relay6_checkbutton_ccc));
  primary_relay_settings_ccc.activate_primary_relay7 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(activate_primary_relay7_checkbutton_ccc));
  primary_relay_settings_ccc.activate_primary_relay8 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(activate_primary_relay8_checkbutton_ccc));
  primary_relay_settings_ccc.deactivate_primary_relay1 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(deactivate_primary_relay1_checkbutton_ccc));
  primary_relay_settings_ccc.deactivate_primary_relay2 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(deactivate_primary_relay2_checkbutton_ccc));
  primary_relay_settings_ccc.deactivate_primary_relay3 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(deactivate_primary_relay3_checkbutton_ccc));
  primary_relay_settings_ccc.deactivate_primary_relay4 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(deactivate_primary_relay4_checkbutton_ccc));
  primary_relay_settings_ccc.deactivate_primary_relay5 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(deactivate_primary_relay5_checkbutton_ccc));
  primary_relay_settings_ccc.deactivate_primary_relay6 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(deactivate_primary_relay6_checkbutton_ccc));
  primary_relay_settings_ccc.deactivate_primary_relay7 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(deactivate_primary_relay7_checkbutton_ccc));
  primary_relay_settings_ccc.deactivate_primary_relay8 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(deactivate_primary_relay8_checkbutton_ccc));
  primary_relay_settings_ccc.primary_relay_activation_time = gtk_spin_button_get_value(GTK_SPIN_BUTTON(primary_relay_energize_time_spinbutton_ccc));
  primary_relay_settings_ccc.primary_relay_delay_time = gtk_spin_button_get_value(GTK_SPIN_BUTTON(primary_relay_delay_time_spinbutton_ccc));
  if (asclepius_relay_source_combobox_ccc)
  {
    int relay_source = gtk_combo_box_get_active(GTK_COMBO_BOX(asclepius_relay_source_combobox_ccc));
    switch (relay_source)
    {
    case 0:
      primary_relay_settings_ccc.asclepius_relay_source = ASCLEPIUS_MAIN12V;
      break;
    case 1:
      primary_relay_settings_ccc.asclepius_relay_source = ASCLEPIUS_MAIN5V;
      break;
    case 2:
      primary_relay_settings_ccc.asclepius_relay_source = ASCLEPIUS_USB5V;
      break;
    case 3:
      primary_relay_settings_ccc.asclepius_relay_source = ASCLEPIUS_MOLEX;
      break;
    case 4:
    default:
      primary_relay_settings_ccc.asclepius_relay_source = ASCLEPIUS_ALL;
      break;
    }
  }

  const gchar *tty_entry_text = gtk_entry_get_text(GTK_ENTRY(asclepius_tty_device_text_ccc));
  strcpy(primary_relay_settings_ccc.asclepius_tty_device, tty_entry_text);

  if (asclepius_poll_interval_combobox_ccc)
  {
    int poll_index = gtk_combo_box_get_active(GTK_COMBO_BOX(asclepius_poll_interval_combobox_ccc));
    if (poll_index >= 0 && (size_t)poll_index < ASCLEPIUS_NUM_POLL_INTERVALS)
    {
      primary_relay_settings_ccc.asclepius_poll_interval_ms = asclepius_poll_intervals_ccc[poll_index];
    }
  }

  GtkWidget *action_combos[] = { asclepius_ocp_action_12v_combobox_ccc, asclepius_ocp_action_5v_combobox_ccc, asclepius_ocp_action_usb_combobox_ccc };
  GtkWidget *limit_combos[] = { asclepius_ocp_limit_12v_combobox_ccc, asclepius_ocp_limit_5v_combobox_ccc, asclepius_ocp_limit_usb_combobox_ccc };
  size_t i;

  for (i = 0; i < sizeof(action_combos) / sizeof(action_combos[0]); i++)
  {
    if (action_combos[i])
    {
      int action_index = gtk_combo_box_get_active(GTK_COMBO_BOX(action_combos[i]));
      if (action_index >= 0 && action_index <= ASCLEPIUS_OCP_CUT)
      {
        if (i == 0)
        {
          primary_relay_settings_ccc.asclepius_ocp_action_12v = action_index;
        }
        else if (i == 1)
        {
          primary_relay_settings_ccc.asclepius_ocp_action_5v = action_index;
        }
        else
        {
          primary_relay_settings_ccc.asclepius_ocp_action_usb = action_index;
        }
      }
    }

    if (limit_combos[i])
    {
      int limit_index = gtk_combo_box_get_active(GTK_COMBO_BOX(limit_combos[i]));
      if (limit_index >= 0 && (size_t)limit_index < ASCLEPIUS_NUM_OCP_LIMITS)
      {
        if (i == 0)
        {
          primary_relay_settings_ccc.asclepius_ocp_limit_12v = asclepius_ocp_limits_ccc[limit_index];
        }
        else if (i == 1)
        {
          primary_relay_settings_ccc.asclepius_ocp_limit_5v = asclepius_ocp_limits_ccc[limit_index];
        }
        else
        {
          primary_relay_settings_ccc.asclepius_ocp_limit_usb = asclepius_ocp_limits_ccc[limit_index];
        }
      }
    }
  }

  // = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON());
  // = gtk_spin_button_get_value(GTK_SPIN_BUTTON());
}

void do_test_primary_relay_ccc(void)
{
  if (!usbr1_chosen_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error: No relay has been chosen"));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
    return;
  }
  int error = 0;
  bool activate_primary_relay1_bak = primary_relay_settings_ccc.activate_primary_relay1;
  bool activate_primary_relay2_bak = primary_relay_settings_ccc.activate_primary_relay2;
  bool activate_primary_relay3_bak = primary_relay_settings_ccc.activate_primary_relay3;
  bool activate_primary_relay4_bak = primary_relay_settings_ccc.activate_primary_relay4;
  bool activate_primary_relay5_bak = primary_relay_settings_ccc.activate_primary_relay5;
  bool activate_primary_relay6_bak = primary_relay_settings_ccc.activate_primary_relay6;
  bool activate_primary_relay7_bak = primary_relay_settings_ccc.activate_primary_relay7;
  bool activate_primary_relay8_bak = primary_relay_settings_ccc.activate_primary_relay8;
  bool deactivate_primary_relay1_bak = primary_relay_settings_ccc.deactivate_primary_relay1;
  bool deactivate_primary_relay2_bak = primary_relay_settings_ccc.deactivate_primary_relay2;
  bool deactivate_primary_relay3_bak = primary_relay_settings_ccc.deactivate_primary_relay3;
  bool deactivate_primary_relay4_bak = primary_relay_settings_ccc.deactivate_primary_relay4;
  bool deactivate_primary_relay5_bak = primary_relay_settings_ccc.deactivate_primary_relay5;
  bool deactivate_primary_relay6_bak = primary_relay_settings_ccc.deactivate_primary_relay6;
  bool deactivate_primary_relay7_bak = primary_relay_settings_ccc.deactivate_primary_relay7;
  bool deactivate_primary_relay8_bak = primary_relay_settings_ccc.deactivate_primary_relay8;
  unsigned long long primary_relay_activation_time_bak = primary_relay_settings_ccc.primary_relay_activation_time;
  unsigned long long primary_relay_delay_time_bak = primary_relay_settings_ccc.primary_relay_delay_time;

  update_primary_relay_settings_from_buttons_ccc();
  update_primary_relay_settings_ccc();

  g_print("activate primary relay\n");
  if (activate_primary_relay_ccc())
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error operating relay, see console for more information"));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
    error = 1;
  }

  // wait to deactivate
  if (!error)
  {
    unsigned int i;
    for (i = primary_relay_settings_ccc.primary_relay_activation_time; i > 0; i--)
    {
      g_print("%d  ", i);
      do_nanosleep_ccc(1000000000);
      update_display_ccc(0);
    }
    g_print("\n");
    g_print("deactivate primary relay\n");
    if (deactivate_primary_relay_ccc())
    {
      snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error operating relay, see console for more information"));
      message_error_ccc(tempmessage_ccc);
      print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
      clear_error_message_ccc();
      error = 1;
    }
  }

  // wait for delay time
  if (!error)
  {
    g_print("wait for delay time\n");
    unsigned int i;
    for (i = primary_relay_settings_ccc.primary_relay_delay_time; i > 0; i--)
    {
      g_print("%d  ", i);
      do_nanosleep_ccc(1000000000);
      update_display_ccc(0);
    }
    g_print("\n");
    g_print("relay test complete\n");
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Operation completed successfully\n"));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Information"), 0);
    clear_error_message_ccc();
  }

  primary_relay_settings_ccc.activate_primary_relay1 = activate_primary_relay1_bak;
  primary_relay_settings_ccc.activate_primary_relay2 = activate_primary_relay2_bak;
  primary_relay_settings_ccc.activate_primary_relay3 = activate_primary_relay3_bak;
  primary_relay_settings_ccc.activate_primary_relay4 = activate_primary_relay4_bak;
  primary_relay_settings_ccc.activate_primary_relay5 = activate_primary_relay5_bak;
  primary_relay_settings_ccc.activate_primary_relay6 = activate_primary_relay6_bak;
  primary_relay_settings_ccc.activate_primary_relay7 = activate_primary_relay7_bak;
  primary_relay_settings_ccc.activate_primary_relay8 = activate_primary_relay8_bak;
  primary_relay_settings_ccc.deactivate_primary_relay1 = deactivate_primary_relay1_bak;
  primary_relay_settings_ccc.deactivate_primary_relay2 = deactivate_primary_relay2_bak;
  primary_relay_settings_ccc.deactivate_primary_relay3 = deactivate_primary_relay3_bak;
  primary_relay_settings_ccc.deactivate_primary_relay4 = deactivate_primary_relay4_bak;
  primary_relay_settings_ccc.deactivate_primary_relay5 = deactivate_primary_relay5_bak;
  primary_relay_settings_ccc.deactivate_primary_relay6 = deactivate_primary_relay6_bak;
  primary_relay_settings_ccc.deactivate_primary_relay7 = deactivate_primary_relay7_bak;
  primary_relay_settings_ccc.deactivate_primary_relay8 = deactivate_primary_relay8_bak;
  primary_relay_settings_ccc.primary_relay_activation_time = primary_relay_activation_time_bak;
  primary_relay_settings_ccc.primary_relay_delay_time = primary_relay_delay_time_bak;
  update_primary_relay_settings_ccc();
}

// A manual power command takes a rail out of any latched software OCP trip
static void clear_asclepius_soft_ocp_ccc(uint8_t channel)
{
  if (channel == ASCLEPIUS_ALL)
  {
    asclepius_ocp_reset();
  }
  else
  {
    asclepius_ocp_clear_channel(channel);
  }
}

int cycle_primary_relay_ccc(void)
{
  int asclepius_relay_source = primary_relay_settings_ccc.asclepius_relay_source;
  if (asclepius_relay_source < ASCLEPIUS_MAIN12V || asclepius_relay_source > ASCLEPIUS_ALL)
  {
    asclepius_relay_source = ASCLEPIUS_ALL;
  }

  if(relay_controls_use_asclepius_ccc())
  {
    if (asclepius_disable_channel(asclepius_relay_source) != 0)
    {
      return 1;
    }

    clear_asclepius_soft_ocp_ccc((uint8_t)asclepius_relay_source);

    // wait to deactivate
    unsigned int i;
    for (i = primary_relay_activation_time_ccc; i > 0; i--)
    {
      g_print("%d ", i);
      do_nanosleep_ccc(1000000000);
    }

    if (asclepius_enable_channel(asclepius_relay_source) != 0)
    {
      return 1;
    }

    clear_asclepius_soft_ocp_ccc((uint8_t)asclepius_relay_source);

    // wait for delay time
    for (i = primary_relay_delay_time_ccc; i > 0; i--)
    {
      g_print("%d ", i);
      do_nanosleep_ccc(1000000000);
    }

    return 0;
  }

  if (!usbr1_chosen_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error: No relay has been chosen"));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
    return -1;
  }

  g_print("activate primary relay ");
  if (activate_primary_relay_ccc())
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error operating relay, see console for more information"));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
    return -1;
  }

  // wait to deactivate
  unsigned int i;
  for (i = primary_relay_activation_time_ccc; i > 0; i--)
  {
    g_print("%d ", i);
    do_nanosleep_ccc(1000000000);
  }
  g_print("\n");
  g_print("deactivate and wait ");
  if (deactivate_primary_relay_ccc())
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error operating relay, see console for more information"));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
    return -1;
  }

  // wait for delay time
  for (i = primary_relay_delay_time_ccc; i > 0; i--)
  {
    g_print("%d ", i);
    do_nanosleep_ccc(1000000000);
  }
  g_print("done\n");

  return 0;
}

void update_ports_ccc(void)
{
  const gchar *entry_text;
  entry_text = gtk_entry_get_text(GTK_ENTRY(set_disabled_ports_text_ccc));
  // fprintf (stdout, "%s\n", entry_text);    //debug
  char line[256];
  char rawport[16][256];
  strncpy(line, entry_text, 255);
  strcpy(new_ports_ccc, "libata.force=");
  int first = 1;
  int count = 0;
  int scanline = sscanf(line, "%s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s", rawport[0], rawport[1], rawport[2], rawport[3], rawport[4], rawport[5], rawport[6], rawport[7], rawport[8], rawport[9], rawport[10], rawport[11], rawport[12], rawport[13], rawport[14], rawport[15]);
  while (scanline > 0)
  {
    unsigned int port = strtoull(rawport[count], NULL, 0);
    char temp[256];
    if (first)
    {
      snprintf(temp, sizeof(temp), "%u:disable", port);
    }
    else
    {
      snprintf(temp, sizeof(temp), ",%u:disable", port);
    }
    size_t port_len = strlen(new_ports_ccc);
    snprintf(new_ports_ccc + port_len, sizeof(new_ports_ccc) - port_len, "%s", temp);
    first = 0;
    count++;
    scanline--;
  }
  gtk_label_set_text(GTK_LABEL(updated_ports_info_label_ccc), new_ports_ccc);
  disabled_ports_changed_ccc = 1;
}

int open_confirmation_dialog_ccc(char *message)
{
  int confirm = 0;
  GtkBuilder *builder = gtk_builder_new();
  GError *err = NULL; // It is mandatory to initialize to NULL
  if (0 == gtk_builder_add_from_string(builder, (const gchar *)opensuperclone_glade, opensuperclone_glade_len, &err))
  {
    fprintf(stderr, "Error adding build from file. Error: %s\n", err->message);
  }
  gtk_builder_connect_signals(builder, NULL);
  GtkWidget *dialog = GTK_WIDGET(gtk_builder_get_object(builder, "confirmation_dialog"));
  confirmation_box_label_ccc = GTK_WIDGET(gtk_builder_get_object(builder, "confirmation_box_label"));
  g_object_unref(builder);

  gtk_label_set_text(GTK_LABEL(confirmation_box_label_ccc), message);

  gtk_window_set_title(GTK_WINDOW(dialog), _("Confirm Operation"));

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == 5)
  {
    confirm = 1;
  }
  gtk_widget_destroy(dialog);
  return confirm;
}

void test_command_ccc(void)
{
  const gchar *entry_text;
  entry_text = gtk_entry_get_text(GTK_ENTRY(command_to_call_text_ccc));
  int ret = system(entry_text);
  if (ret)
  {
    int status = WEXITSTATUS(ret);
    int signal = WTERMSIG(ret);
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("The called command failed with exit signal / status"));
    message_error_ccc(tempmessage_ccc);
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, " (%d / %d)", signal, status);
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
  else
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("The command completed normally"));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Information"), 0);
    clear_error_message_ccc();
  }
}

void test_power_command_ccc(void)
{
  const gchar *entry_text;
  entry_text = gtk_entry_get_text(GTK_ENTRY(power_command_to_call_text_ccc));
  int ret = system(entry_text);
  if (ret)
  {
    int status = WEXITSTATUS(ret);
    int signal = WTERMSIG(ret);
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("The called command failed with exit signal / status"));
    message_error_ccc(tempmessage_ccc);
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, " (%d / %d)", signal, status);
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
  else
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("The command completed normally"));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Information"), 0);
    clear_error_message_ccc();
  }
}

void activate_status_buttons_ccc(void)
{
  // status registers
  gtk_image_set_from_file(GTK_IMAGE(bsy_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(drdy_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(df_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(dsc_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(drq_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(corr_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(idx_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(err_status_icon), status_icon_off_path);

  // error registers
  gtk_image_set_from_file(GTK_IMAGE(bbk_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(unc_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(mc_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(idnf_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(mcr_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(abrt_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(tk0nf_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(amnf_status_icon), status_icon_off_path);
}

void deactivate_status_buttons_ccc(void)
{
  // status registers
  gtk_image_set_from_file(GTK_IMAGE(bsy_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(drdy_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(df_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(dsc_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(drq_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(corr_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(idx_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(err_status_icon), status_icon_off_path);

  // error registers
  gtk_image_set_from_file(GTK_IMAGE(bbk_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(unc_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(mc_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(idnf_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(mcr_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(abrt_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(tk0nf_status_icon), status_icon_off_path);
  gtk_image_set_from_file(GTK_IMAGE(amnf_status_icon), status_icon_off_path);
}

void update_status_buttons_ccc(void)
{
  if (!scsi_passthrough_ccc && !fill_mode_ccc && !generic_mode_ccc)
  {
    activate_status_buttons_ccc();
    if (connected_ccc)
    {
      // status registers
      if (ata_status_ccc & 0x80)
      {
        gtk_image_set_from_file(GTK_IMAGE(bsy_status_icon), status_icon_on_path);
      }

      if (ata_status_ccc & 0x40)
      {
        gtk_image_set_from_file(GTK_IMAGE(drdy_status_icon), status_icon_on_path);
      }

      if (ata_status_ccc & 0x20)
      {
        gtk_image_set_from_file(GTK_IMAGE(df_status_icon), status_icon_on_path);
      }

      if (ata_status_ccc & 0x10)
      {
        gtk_image_set_from_file(GTK_IMAGE(dsc_status_icon), status_icon_on_path);
      }

      if (ata_status_ccc & 0x08)
      {
        gtk_image_set_from_file(GTK_IMAGE(drq_status_icon), status_icon_on_path);
      }

      if (ata_status_ccc & 0x04)
      {
        gtk_image_set_from_file(GTK_IMAGE(corr_status_icon), status_icon_on_path);
      }

      if (ata_status_ccc & 0x02)
      {
        gtk_image_set_from_file(GTK_IMAGE(idx_status_icon), status_icon_on_path);
      }

      if (ata_status_ccc & 0x01)
      {
        gtk_image_set_from_file(GTK_IMAGE(err_status_icon), error_icon_on_path);
      }

      // error registers
      if (ata_error_ccc & 0x80)
      {
        gtk_image_set_from_file(GTK_IMAGE(bbk_status_icon), error_icon_on_path);
      }

      if (ata_error_ccc & 0x40)
      {
        gtk_image_set_from_file(GTK_IMAGE(unc_status_icon), error_icon_on_path);
      }

      if (ata_error_ccc & 0x20)
      {
        gtk_image_set_from_file(GTK_IMAGE(mc_status_icon), error_icon_on_path);
      }

      if (ata_error_ccc & 0x10)
      {
        gtk_image_set_from_file(GTK_IMAGE(idnf_status_icon), error_icon_on_path);
      }

      if (ata_error_ccc & 0x08)
      {
        gtk_image_set_from_file(GTK_IMAGE(mcr_status_icon), error_icon_on_path);
      }

      if (ata_error_ccc & 0x04)
      {
        gtk_image_set_from_file(GTK_IMAGE(abrt_status_icon), error_icon_on_path);
      }

      if (ata_error_ccc & 0x02)
      {
        gtk_image_set_from_file(GTK_IMAGE(tk0nf_status_icon), error_icon_on_path);
      }

      if (ata_error_ccc & 0x01)
      {
        gtk_image_set_from_file(GTK_IMAGE(amnf_status_icon), error_icon_on_path);
      }
    }
  }
}

static void set_asclepius_soft_ocp_tooltip_ccc(GtkWidget *icon, const char *rail_name, const struct asclepius_ocp_rail_t *ocp)
{
  char tooltip[128];

  snprintf(tooltip, sizeof(tooltip), _("%s Channel - Software OCP tripped at %.2fA (limit %.2fA%s)"), rail_name, (float)ocp->trip_ma / 1000, (float)ocp->limit_ma / 1000, ocp->action == ASCLEPIUS_OCP_CUT ? ", channel off" : "");

  gtk_image_set_from_file(GTK_IMAGE(icon), error_icon_on_path);
  gtk_widget_set_tooltip_text(icon, tooltip);
}

void update_asclepius_status(void)
{
  bool connected = asclepius_get_connection_status();

  if (!connected)
  {
    gtk_label_set_text(GTK_LABEL(asclepius_12v_label), _("-- / --"));
    gtk_label_set_text(GTK_LABEL(asclepius_5v_label), _("-- / --"));
    gtk_label_set_text(GTK_LABEL(asclepius_usb_label), _("-- / --"));
    gtk_image_set_from_file(GTK_IMAGE(asclepius_12v_icon), status_icon_off_path);
    gtk_image_set_from_file(GTK_IMAGE(asclepius_5v_icon), status_icon_off_path);
    gtk_image_set_from_file(GTK_IMAGE(asclepius_usb_icon), status_icon_off_path);
    gtk_widget_set_tooltip_text(asclepius_12v_icon, _("Asclepius disconnected"));
    gtk_widget_set_tooltip_text(asclepius_5v_icon, _("Asclepius disconnected"));
    gtk_widget_set_tooltip_text(asclepius_usb_icon, _("Asclepius disconnected"));
    return;
  }

  {
    struct asclepius_status_t status;
    struct asclepius_ocp_rail_t ocp12v;
    struct asclepius_ocp_rail_t ocp5v;
    struct asclepius_ocp_rail_t ocpusb;

    asclepius_lock();
    status = asclepius_status;
    asclepius_unlock();

    asclepius_ocp_get_rail_state(ASCLEPIUS_MAIN12V, &ocp12v);
    asclepius_ocp_get_rail_state(ASCLEPIUS_MAIN5V, &ocp5v);
    asclepius_ocp_get_rail_state(ASCLEPIUS_USB5V, &ocpusb);

    if (status.main12v_enabled)
    {
      gtk_image_set_from_file(GTK_IMAGE(asclepius_12v_icon), status_icon_on_path);
      gtk_widget_set_tooltip_text(asclepius_12v_icon, _("12V Channel - Active"));
    }
    else
    {
      gtk_image_set_from_file(GTK_IMAGE(asclepius_12v_icon), status_icon_off_path);
      gtk_widget_set_tooltip_text(asclepius_12v_icon, _("12V Channel - Inactive"));
    }
    if (status.main12v_enabled)
    {
      if (status.main12v_voltage < ASCLEPIUS_V12_MIN_MV)
      {
        gtk_image_set_from_file(GTK_IMAGE(asclepius_12v_icon), status_warn_icon_path);
        gtk_widget_set_tooltip_text(asclepius_12v_icon, _("12V Channel - Out of Spec (<11.4V)"));
      }
      else if (status.main12v_voltage > ASCLEPIUS_V12_MAX_MV)
      {
        gtk_image_set_from_file(GTK_IMAGE(asclepius_12v_icon), status_warn_icon_path);
        gtk_widget_set_tooltip_text(asclepius_12v_icon, _("12V Channel - Out of Spec (>12.6V)"));
      }
    }
    if (ocp12v.tripped)
    {
      set_asclepius_soft_ocp_tooltip_ccc(asclepius_12v_icon, "12V", &ocp12v);
    }
    if (status.main12v_ocp)
    {
      gtk_image_set_from_file(GTK_IMAGE(asclepius_12v_icon), error_icon_on_path);
      gtk_widget_set_tooltip_text(asclepius_12v_icon, _("12V Channel - Overcurrent Detected!"));
    }

    if(status.main5v_enabled)
    {
      gtk_image_set_from_file(GTK_IMAGE(asclepius_5v_icon), status_icon_on_path);
      gtk_widget_set_tooltip_text(asclepius_5v_icon, _("5V Channel - Active"));
    }
    else
    {
      gtk_image_set_from_file(GTK_IMAGE(asclepius_5v_icon), status_icon_off_path);
      gtk_widget_set_tooltip_text(asclepius_5v_icon, _("5V Channel - Inactive"));
    }
    if (status.main5v_enabled)
    {
      if (status.main5v_voltage < ASCLEPIUS_V5_MIN_MV)
      {
        gtk_image_set_from_file(GTK_IMAGE(asclepius_5v_icon), status_warn_icon_path);
        gtk_widget_set_tooltip_text(asclepius_5v_icon, _("5V Channel - Out of Spec (<4.75V)"));
      }
      else if (status.main5v_voltage > ASCLEPIUS_V5_MAX_MV)
      {
        gtk_image_set_from_file(GTK_IMAGE(asclepius_5v_icon), status_warn_icon_path);
        gtk_widget_set_tooltip_text(asclepius_5v_icon, _("5V Channel - Out of Spec (>5.25V)"));
      }
    }
    if (ocp5v.tripped)
    {
      set_asclepius_soft_ocp_tooltip_ccc(asclepius_5v_icon, "5V", &ocp5v);
    }
    if (status.main5v_ocp)
    {
      gtk_image_set_from_file(GTK_IMAGE(asclepius_5v_icon), error_icon_on_path);
      gtk_widget_set_tooltip_text(asclepius_5v_icon, _("5V Channel - Overcurrent Detected!"));
    }

    if(status.usb5v_enabled)
    {
      gtk_image_set_from_file(GTK_IMAGE(asclepius_usb_icon), status_icon_on_path);
      gtk_widget_set_tooltip_text(asclepius_usb_icon, _("USB 5V Channel - Active"));
    }
    else
    {
      gtk_image_set_from_file(GTK_IMAGE(asclepius_usb_icon), status_icon_off_path);
      gtk_widget_set_tooltip_text(asclepius_usb_icon, _("USB 5V Channel - Inactive"));
    }
    if (status.usb5v_enabled)
    {
      if (status.main5v_voltage < ASCLEPIUS_V5_MIN_MV)
      {
        gtk_image_set_from_file(GTK_IMAGE(asclepius_usb_icon), status_warn_icon_path);
        gtk_widget_set_tooltip_text(asclepius_usb_icon, _("USB 5V Channel - Out of Spec (<4.75V)"));
      }
      else if (status.main5v_voltage > ASCLEPIUS_V5_MAX_MV)
      {
        gtk_image_set_from_file(GTK_IMAGE(asclepius_usb_icon), status_warn_icon_path);
        gtk_widget_set_tooltip_text(asclepius_usb_icon, _("USB 5V Channel - Out of Spec (>5.25V)"));
      }
    }
    if (ocpusb.tripped)
    {
      set_asclepius_soft_ocp_tooltip_ccc(asclepius_usb_icon, "USB 5V", &ocpusb);
    }
    if (status.usb5v_ocp)
    {
      gtk_image_set_from_file(GTK_IMAGE(asclepius_usb_icon), error_icon_on_path);
        gtk_widget_set_tooltip_text(asclepius_usb_icon, _("USB 5V Channel - Overcurrent Detected"));
    }

    char temp[16];
    snprintf(temp, sizeof(temp), _("%.1fV / %dmA"), (float)status.main12v_voltage / 1000, status.main12v_current);
    gtk_label_set_text(GTK_LABEL(asclepius_12v_label), temp);
    snprintf(temp, sizeof(temp), _("%.1fV / %dmA"), (float)status.main5v_voltage / 1000, status.main5v_current);
    gtk_label_set_text(GTK_LABEL(asclepius_5v_label), temp);
    snprintf(temp, sizeof(temp), _("%.1fV / %dmA"), (float)status.main5v_voltage / 1000, status.usb5v_current);
    gtk_label_set_text(GTK_LABEL(asclepius_usb_label), temp);
  }
}

void update_mode_ccc(void)
{
  if (generic_mode_ccc || scsi_passthrough_ccc || nvme_passthrough_ccc || fill_mode_ccc)
  {
    deactivate_status_buttons_ccc();
  }
  else
  {
    activate_status_buttons_ccc();
  }
  if (fill_mode_ccc)
  {
    gtk_widget_set_sensitive(GTK_WIDGET(sourcemi_ccc), FALSE);
  }
  else
  {
    gtk_widget_set_sensitive(GTK_WIDGET(sourcemi_ccc), TRUE);
  }
  update_display_ccc(0);
}

void update_clone_button_settings_ccc(void)
{
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(phase1_check_button_ccc), !clone_settings_ccc.no_phase1);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(phase2_check_button_ccc), !clone_settings_ccc.no_phase2);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(phase3_check_button_ccc), !clone_settings_ccc.no_phase3);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(phase4_check_button_ccc), !clone_settings_ccc.no_phase4);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(divide_check_button_ccc), !clone_settings_ccc.no_divide1);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(divide2_check_button_ccc), clone_settings_ccc.do_divide2);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(trim_check_button_ccc), !clone_settings_ccc.no_trim);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(scrape_check_button_ccc), !clone_settings_ccc.no_scrape);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(markbad_check_button_ccc), clone_settings_ccc.mark_bad_head);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(readbad_check_button_ccc), clone_settings_ccc.read_bad_head);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rebuild_assist_check_button_ccc), clone_settings_ccc.rebuild_assist);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(reverse_check_button_ccc), clone_settings_ccc.reverse);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(skipfast_check_button_ccc), clone_settings_ccc.skip_fast);

  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, 0, MAX_RETRIES, 1, 1, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(retries_spin_button_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(retries_spin_button_ccc), clone_settings_ccc.retries);

  int cluster_limit = MAX_CLUSTERSIZE;
  if (source_chosen_ccc)
  {
    cluster_limit = check_buffer_limit_ccc();
    if (cluster_limit < 0)
    {
      print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    }
    if (clone_settings_ccc.cluster_size > cluster_limit)
    {
      clone_settings_ccc.cluster_size = cluster_limit;
    }
  }
  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, 1, cluster_limit, 8, 64, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(clustersize_spin_button_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(clustersize_spin_button_ccc), clone_settings_ccc.cluster_size);

  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, 0, 0x7fffffffffffffff, 1, 100, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(inputoffset_spin_button_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(inputoffset_spin_button_ccc), clone_settings_ccc.input_offset);

  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, -1, 0x7fffffffffffffff, 1, 100, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(size_spin_button_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(size_spin_button_ccc), clone_settings_ccc.read_size);

  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, 1, MAX_BLOCKSIZE, 1, 8, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(blocksize_spin_button_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(blocksize_spin_button_ccc), clone_settings_ccc.block_size);

  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, 0, 0x7fffffffffffffff, 1, 100, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(skipsize_spin_button_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(skipsize_spin_button_ccc), clone_settings_ccc.min_skip_size);

  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, 0, 0x7fffffffffffffff, 1, 100, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(maxskipsize_spin_button_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(maxskipsize_spin_button_ccc), clone_settings_ccc.max_skip_size);

  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, -1, 300000, 1, 100, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(skipthreshold_spin_button_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(skipthreshold_spin_button_ccc), clone_settings_ccc.skip_timeout / 1000);

  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, -1, 1000000, 10, 1000, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(rateskip_spin_button_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(rateskip_spin_button_ccc), clone_settings_ccc.rate_skip / 1000);

  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, -1, 1000000, 10, 1000, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(exitonslow_spin_button_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(exitonslow_spin_button_ccc), clone_settings_ccc.exit_on_slow / 1000);

  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, -1, 300, 1, 10, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(exitonslowtime_spin_button_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(exitonslowtime_spin_button_ccc), clone_settings_ccc.exit_slow_time / 1000);

  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, MIN_SECTORSIZE, MAX_SECTORSIZE, 8, 64, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(sectorsize_spinbutton_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(sectorsize_spinbutton_ccc), clone_settings_ccc.sector_size);

  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, 0, MAX_BLOCKSIZE, 1, 8, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(alignment_spinbutton_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(alignment_spinbutton_ccc), clone_settings_ccc.block_offset);

  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, 0, 10000000, 100, 1000, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(maxreadrate_spinbutton_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(maxreadrate_spinbutton_ccc), clone_settings_ccc.max_read_rate / 1000);

  // gtk_adjustment_ccc = (GtkAdjustment *) gtk_adjustment_new (0, 1, 1, 1, 1, 0);
  // gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(), gtk_adjustment_ccc);
  // gtk_spin_button_set_value(GTK_SPIN_BUTTON(), clone_settings_ccc.);
}

void update_advanced_button_settings_ccc(void)
{
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(force_mounted_checkbutton_ccc), advanced_settings_ccc.force_mounted);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(force_dangerous_checkbutton_ccc), advanced_settings_ccc.force_dangerous);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(no_log_backup_checkbutton_ccc), advanced_settings_ccc.no_log_backup);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_output_offset_checkbutton_ccc), advanced_settings_ccc.enable_output_offset);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_current_position_checkbutton_ccc), advanced_settings_ccc.enable_current_position);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(stop_on_error_radio_button_ccc), advanced_settings_ccc.stop_on_error);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(call_command_radio_button_ccc), advanced_settings_ccc.call_command);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(primary_relay_on_error_radio_button_ccc), advanced_settings_ccc.activate_primary_relay);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(write_buffer_disable_radio_button_ccc), advanced_settings_ccc.write_buffer_disable);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(write_buffer_enable_radio_button_ccc), advanced_settings_ccc.write_buffer_enable);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(driver_return_error_radio_button_ccc), advanced_settings_ccc.driver_return_error);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(driver_return_zeros_radio_button_ccc), advanced_settings_ccc.driver_return_zeros);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(driver_return_marked_radio_button_ccc), advanced_settings_ccc.driver_return_marked);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(disable_identify_checkbutton_ccc), advanced_settings_ccc.disable_identify);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pio_mode_checkbutton_ccc), advanced_settings_ccc.pio_mode);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_rebuild_assist_checkbutton_ccc), advanced_settings_ccc.enable_rebuild_assist);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_process_chunk_checkbutton_ccc), advanced_settings_ccc.enable_process_chunk);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_read_twice_checkbutton_ccc), advanced_settings_ccc.enable_read_twice);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_retry_connecting_checkbutton_ccc), advanced_settings_ccc.enable_retry_connecting);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_scsi_write_checkbutton_ccc), advanced_settings_ccc.enable_scsi_write);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_phase_log_checkbutton_ccc), advanced_settings_ccc.enable_phase_log);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_output_sector_size_checkbutton_ccc), advanced_settings_ccc.enable_output_sector_size);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(driver_io_scsi_only_checkbutton_ccc), advanced_settings_ccc.driver_io_scsi_only);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(use_physical_sector_size_for_virtual_checkbutton_ccc), advanced_settings_ccc.use_physical_sector_size_for_virtual);

  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, -1, 0x7fffffffffffffff, 1, 100, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(output_offset_spinbutton_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(output_offset_spinbutton_ccc), advanced_settings_ccc.output_offset);

  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, -1, 0x7fffffffffffffff, 1, 100, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(current_position_spinbutton_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(current_position_spinbutton_ccc), advanced_settings_ccc.current_position);

  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, -128, 128, 8, 64, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(output_sector_size_spinbutton_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(output_sector_size_spinbutton_ccc), advanced_settings_ccc.output_sector_adjustment);

  int cluster_limit = MAX_CLUSTERSIZE;
  if (source_chosen_ccc)
  {
    cluster_limit = check_buffer_limit_ccc();
    if (cluster_limit < 0)
    {
      print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    }
  }
  if (clone_settings_ccc.cluster_size > 0 && cluster_limit > clone_settings_ccc.cluster_size)
  {
    cluster_limit = clone_settings_ccc.cluster_size;
  }
  if (advanced_settings_ccc.driver_minimum_cluster_size > cluster_limit)
  {
    advanced_settings_ccc.driver_minimum_cluster_size = cluster_limit;
  }
  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, 1, cluster_limit, 8, 64, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(driver_minimum_cluster_size_spinbutton_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(driver_minimum_cluster_size_spinbutton_ccc), advanced_settings_ccc.driver_minimum_cluster_size);

  // gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON (), advanced_settings_ccc. );

  // gtk_adjustment_ccc = (GtkAdjustment *) gtk_adjustment_new (0, 1, 1, 1, 1, 0);
  // gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(), gtk_adjustment_ccc);
  // gtk_spin_button_set_value(GTK_SPIN_BUTTON(), advanced_settings_ccc.);
}

void update_timer_button_settings_ccc(void)
{
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(stop_on_power_cycle_radio_button_ccc), timer_settings_ccc.stop_on_power_cycle);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(call_power_command_radio_button_ccc), timer_settings_ccc.call_power_command);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(primary_relay_power_cycle_radio_button_ccc), timer_settings_ccc.power_cycle_primary_relay);

  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, 500, 900000, 1, 100, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(initial_busy_wait_time_spinbutton_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(initial_busy_wait_time_spinbutton_ccc), (timer_settings_ccc.initial_busy_wait_time));
  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, 5000, 900000, 1, 100, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(busy_wait_time_spinbutton_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(busy_wait_time_spinbutton_ccc), timer_settings_ccc.busy_wait_time);
  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, 500, 900000, 1, 100, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(soft_reset_time_spinbutton_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(soft_reset_time_spinbutton_ccc), timer_settings_ccc.soft_reset_time);
  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, 0, 900000, 1, 100, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(hard_reset_time_spinbutton_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(hard_reset_time_spinbutton_ccc), timer_settings_ccc.hard_reset_time);
  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, 0, 900000, 1, 100, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(power_cycle_time_spinbutton_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(power_cycle_time_spinbutton_ccc), timer_settings_ccc.power_cycle_time);
  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, 10000, 900000, 1, 100, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(general_timeout_spinbutton_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(general_timeout_spinbutton_ccc), timer_settings_ccc.general_timeout);

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(phase_timers_checkbutton_ccc), timer_settings_ccc.phase_timers);

  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, 50, 900000, 1, 100, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(p12_soft_reset_time_spinbutton_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(p12_soft_reset_time_spinbutton_ccc), timer_settings_ccc.p12_soft_reset_time);

  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, 50, 900000, 1, 100, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(p3_soft_reset_time_spinbutton_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(p3_soft_reset_time_spinbutton_ccc), timer_settings_ccc.p3_soft_reset_time);

  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, 50, 900000, 1, 100, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(p4_soft_reset_time_spinbutton_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(p4_soft_reset_time_spinbutton_ccc), timer_settings_ccc.p4_soft_reset_time);

  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, 50, 900000, 1, 100, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(td_soft_reset_time_spinbutton_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(td_soft_reset_time_spinbutton_ccc), timer_settings_ccc.td_soft_reset_time);

  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, 50, 900000, 1, 100, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(d2_soft_reset_time_spinbutton_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(d2_soft_reset_time_spinbutton_ccc), timer_settings_ccc.d2_soft_reset_time);

  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, 50, 900000, 1, 100, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(sc_soft_reset_time_spinbutton_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(sc_soft_reset_time_spinbutton_ccc), timer_settings_ccc.sc_soft_reset_time);

  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, 50, 900000, 1, 100, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(rt_soft_reset_time_spinbutton_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(rt_soft_reset_time_spinbutton_ccc), timer_settings_ccc.rt_soft_reset_time);

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(always_wait_for_reset_timers_checkbutton_ccc), timer_settings_ccc.always_wait_for_reset_timers);

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_usb_bulk_reset_checkbutton_ccc), timer_settings_ccc.usb_bulk_reset_enabled);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_usb_soft_reset_checkbutton_ccc), timer_settings_ccc.usb_soft_reset_enabled);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_usb_hard_reset_checkbutton_ccc), timer_settings_ccc.usb_hard_reset_enabled);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_usb_port_reset_checkbutton_ccc), timer_settings_ccc.usb_port_reset_enabled);

  // gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON (), timer_settings_ccc. );

  // gtk_adjustment_ccc = (GtkAdjustment *) gtk_adjustment_new (0, 1, 1, 1, 1, 0);
  // gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(), gtk_adjustment_ccc);
  // gtk_spin_button_set_value(GTK_SPIN_BUTTON(), timer_settings_ccc.);
}

void update_primary_relay_button_settings_ccc(void)
{
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(activate_primary_relay1_checkbutton_ccc), primary_relay_settings_ccc.activate_primary_relay1);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(activate_primary_relay2_checkbutton_ccc), primary_relay_settings_ccc.activate_primary_relay2);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(activate_primary_relay3_checkbutton_ccc), primary_relay_settings_ccc.activate_primary_relay3);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(activate_primary_relay4_checkbutton_ccc), primary_relay_settings_ccc.activate_primary_relay4);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(activate_primary_relay5_checkbutton_ccc), primary_relay_settings_ccc.activate_primary_relay5);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(activate_primary_relay6_checkbutton_ccc), primary_relay_settings_ccc.activate_primary_relay6);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(activate_primary_relay7_checkbutton_ccc), primary_relay_settings_ccc.activate_primary_relay7);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(activate_primary_relay8_checkbutton_ccc), primary_relay_settings_ccc.activate_primary_relay8);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(deactivate_primary_relay1_checkbutton_ccc), primary_relay_settings_ccc.deactivate_primary_relay1);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(deactivate_primary_relay2_checkbutton_ccc), primary_relay_settings_ccc.deactivate_primary_relay2);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(deactivate_primary_relay3_checkbutton_ccc), primary_relay_settings_ccc.deactivate_primary_relay3);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(deactivate_primary_relay4_checkbutton_ccc), primary_relay_settings_ccc.deactivate_primary_relay4);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(deactivate_primary_relay5_checkbutton_ccc), primary_relay_settings_ccc.deactivate_primary_relay5);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(deactivate_primary_relay6_checkbutton_ccc), primary_relay_settings_ccc.deactivate_primary_relay6);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(deactivate_primary_relay7_checkbutton_ccc), primary_relay_settings_ccc.deactivate_primary_relay7);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(deactivate_primary_relay8_checkbutton_ccc), primary_relay_settings_ccc.deactivate_primary_relay8);

  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, 1, 900, 1, 1, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(primary_relay_energize_time_spinbutton_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(primary_relay_energize_time_spinbutton_ccc), primary_relay_settings_ccc.primary_relay_activation_time);

  gtk_adjustment_ccc = (GtkAdjustment *)gtk_adjustment_new(0, 1, 900, 1, 1, 0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(primary_relay_delay_time_spinbutton_ccc), gtk_adjustment_ccc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(primary_relay_delay_time_spinbutton_ccc), primary_relay_settings_ccc.primary_relay_delay_time);

  int relay_source_index;
  switch (primary_relay_settings_ccc.asclepius_relay_source)
  {
  case ASCLEPIUS_MAIN12V:
    relay_source_index = 0;
    break;
  case ASCLEPIUS_MAIN5V:
    relay_source_index = 1;
    break;
  case ASCLEPIUS_USB5V:
    relay_source_index = 2;
    break;
  case ASCLEPIUS_MOLEX:
    relay_source_index = 3;
    break;
  case ASCLEPIUS_ALL:
  default:
    relay_source_index = 4;
    break;
  }
  if (asclepius_relay_source_combobox_ccc)
  {
    gtk_combo_box_set_active(GTK_COMBO_BOX(asclepius_relay_source_combobox_ccc), relay_source_index);
  }

  gtk_entry_set_text(GTK_ENTRY(asclepius_tty_device_text_ccc), primary_relay_settings_ccc.asclepius_tty_device);

  if (asclepius_poll_interval_combobox_ccc)
  {
    gtk_combo_box_set_active(GTK_COMBO_BOX(asclepius_poll_interval_combobox_ccc), asclepius_preset_index_ccc(asclepius_poll_intervals_ccc, ASCLEPIUS_NUM_POLL_INTERVALS, primary_relay_settings_ccc.asclepius_poll_interval_ms));
  }

  GtkWidget *action_combos[] = { asclepius_ocp_action_12v_combobox_ccc, asclepius_ocp_action_5v_combobox_ccc, asclepius_ocp_action_usb_combobox_ccc };
  int actions[] = { primary_relay_settings_ccc.asclepius_ocp_action_12v, primary_relay_settings_ccc.asclepius_ocp_action_5v, primary_relay_settings_ccc.asclepius_ocp_action_usb };
  GtkWidget *limit_combos[] = { asclepius_ocp_limit_12v_combobox_ccc, asclepius_ocp_limit_5v_combobox_ccc, asclepius_ocp_limit_usb_combobox_ccc };
  unsigned int limits[] = { primary_relay_settings_ccc.asclepius_ocp_limit_12v, primary_relay_settings_ccc.asclepius_ocp_limit_5v, primary_relay_settings_ccc.asclepius_ocp_limit_usb };
  size_t i;

  for (i = 0; i < sizeof(action_combos) / sizeof(action_combos[0]); i++)
  {
    if (action_combos[i])
    {
      gtk_combo_box_set_active(GTK_COMBO_BOX(action_combos[i]), asclepius_ocp_clamp_action(actions[i]));
    }
    if (limit_combos[i])
    {
      gtk_combo_box_set_active(GTK_COMBO_BOX(limit_combos[i]), asclepius_preset_index_ccc(asclepius_ocp_limits_ccc, ASCLEPIUS_NUM_OCP_LIMITS, limits[i]));
    }
  }

  // gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON (), primary_relay_settings_ccc. );

  // gtk_adjustment_ccc = (GtkAdjustment *) gtk_adjustment_new (0, 1, 1, 1, 1, 0);
  // gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(), gtk_adjustment_ccc);
  // gtk_spin_button_set_value(GTK_SPIN_BUTTON(), primary_relay_settings_ccc.);
}

void set_agressive_driver_status_from_button_ccc(void)
{
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(agressive_driver_checkbutton_ccc)))
  {
    aggressive_driver_ccc = TRUE;
    // fprintf (stdout, "agressive mode on\n");    //debug
  }
  else
  {
    aggressive_driver_ccc = FALSE;
    // fprintf (stdout, "agressive mode off\n");    //debug
  }
}

void set_state_from_button_ccc(GtkWidget *widget, gpointer data)
{
  int data_pointer = GPOINTER_TO_INT(data);
  bool button_status;
  bool previous_status;
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
  {
    button_status = TRUE;
  }
  else
  {
    button_status = FALSE;
  }
  // g_print ("status=%d, pointer=%d\n", button_status, data_pointer );

  switch (data_pointer)
  {
  case BUTTONID_PHASE1:
    // g_print ("phase1=%d\n", button_status);
    clone_settings_ccc.no_phase1 = !button_status;
    break;

  case BUTTONID_PHASE2:
    // g_print ("phase2=%d\n", button_status);
    clone_settings_ccc.no_phase2 = !button_status;
    break;

  case BUTTONID_PHASE3:
    // g_print ("phase3=%d\n", button_status);
    clone_settings_ccc.no_phase3 = !button_status;
    break;

  case BUTTONID_PHASE4:
    // g_print ("phase4=%d\n", button_status);
    clone_settings_ccc.no_phase4 = !button_status;
    break;

  case BUTTONID_DIVIDE:
    // g_print ("divide=%d\n", button_status);
    clone_settings_ccc.no_divide1 = !button_status;
    break;

  case BUTTONID_DIVIDE2:
    // g_print ("divide2=%d\n", button_status);
    clone_settings_ccc.do_divide2 = button_status;
    break;

  case BUTTONID_TRIM:
    // g_print ("trim=%d\n", button_status);
    clone_settings_ccc.no_trim = !button_status;
    break;

  case BUTTONID_SCRAPE:
    // g_print ("scrape=%d\n", button_status);
    clone_settings_ccc.no_scrape = !button_status;
    break;

  case BUTTONID_MARKBAD:
    // g_print ("badhead=%d\n", button_status);
    clone_settings_ccc.mark_bad_head = button_status;
    break;

  case BUTTONID_READBAD:
    // g_print ("goodhead=%d\n", button_status);
    clone_settings_ccc.read_bad_head = button_status;
    break;

  case BUTTONID_REBUILDASSIST:
    clone_settings_ccc.rebuild_assist = button_status;
    break;

  case BUTTONID_REVERSE:
    // g_print ("reverse=%d\n", button_status);
    clone_settings_ccc.reverse = button_status;
    break;

  case BUTTONID_SKIPFAST:
    // g_print ("skipfast=%d\n", button_status);
    clone_settings_ccc.skip_fast = button_status;
    break;

  case BUTTONID_NOLOGBACK:
    // g_print ("nologback=%d\n", button_status);
    advanced_settings_ccc.no_log_backup = button_status;
    break;

  case BUTTONID_FORCE:
    // g_print ("force=%d\n", button_status);
    advanced_settings_ccc.force_mounted = button_status;
    break;

  case BUTTONID_FORCEDANGEROUS:
    advanced_settings_ccc.force_dangerous = button_status;
    break;

  case BUTTONID_DONTIDENTIFY:
    advanced_settings_ccc.disable_identify = button_status;
    break;

  case BUTTONID_PIOMODE:
    advanced_settings_ccc.pio_mode = button_status;
    break;

  case BUTTONID_ENABLEREBUILDASSIST:
    advanced_settings_ccc.enable_rebuild_assist = button_status;
    break;

  case BUTTONID_ENABLEPROCESSCHUNK:
    advanced_settings_ccc.enable_process_chunk = button_status;
    break;

  case BUTTONID_ENABLEREADTWICE:
    advanced_settings_ccc.enable_read_twice = button_status;
    break;

  case BUTTONID_ENABLERETRYCONNECTING:
    advanced_settings_ccc.enable_retry_connecting = button_status;
    break;

  case BUTTONID_ENABLESCSIWRITE:
    advanced_settings_ccc.enable_scsi_write = button_status;
    break;

  case BUTTONID_ENABLEPHASELOGS:
    advanced_settings_ccc.enable_phase_log = button_status;
    break;

  case BUTTONID_PHASETIMERS:
    // g_print ("phasetimers=%d\n", button_status);
    timer_settings_ccc.phase_timers = button_status;
    break;

  case BUTTONID_OFFSET:
    // g_print ("offset=%d\n", button_status);
    previous_status = advanced_settings_ccc.enable_output_offset;
    advanced_settings_ccc.enable_output_offset = button_status;
    if (previous_status != advanced_settings_ccc.enable_output_offset)
    {
      snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "%s", _("You must reopen the advanced setting for this change to take effect"));
      message_error_ccc(tempmessage_ccc);
      print_gui_error_message_ccc(error_message_ccc, _("Information"), 0);
      clear_error_message_ccc();
    }
    break;

  case BUTTONID_POSITION:
    // g_print ("offset=%d\n", button_status);
    previous_status = advanced_settings_ccc.enable_current_position;
    advanced_settings_ccc.enable_current_position = button_status;
    if (previous_status != advanced_settings_ccc.enable_current_position)
    {
      snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "%s", _("You must reopen the advanced setting for this change to take effect"));
      message_error_ccc(tempmessage_ccc);
      print_gui_error_message_ccc(error_message_ccc, _("Information"), 0);
      clear_error_message_ccc();
    }
    break;

  case BUTTONID_ENABLEOUTPUTSECTOR:
    // g_print ("offset=%d\n", button_status);
    previous_status = advanced_settings_ccc.enable_output_sector_size;
    advanced_settings_ccc.enable_output_sector_size = button_status;
    if (previous_status != advanced_settings_ccc.enable_output_sector_size)
    {
      snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "%s", _("You must reopen the advanced setting for this change to take effect"));
      message_error_ccc(tempmessage_ccc);
      print_gui_error_message_ccc(error_message_ccc, _("Information"), 0);
      clear_error_message_ccc();
    }
    break;

  case BUTTONID_IOSCSIONLY:
    advanced_settings_ccc.driver_io_scsi_only = button_status;
    break;

  case BUTTONID_USEPHYSICALSECTORSIZE:
    advanced_settings_ccc.use_physical_sector_size_for_virtual = button_status;
    break;

  case BUTTONID_COLORSTATUSBAR:
    advanced_settings_ccc.color_statusbar = button_status;
    break;

  default:
    g_print("Unknown button %d", data_pointer);
    break;
  }
}

void do_reset_status_ccc(void)
{
  if (!project_chosen_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("You must choose a project first."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
    return;
  }
  GtkWidget *new_dialog_window;
  new_dialog_window = gtk_dialog_new_with_buttons(_("Reset Current Status"), GTK_WINDOW(main_window_ccc), GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);
  // gtk_widget_set_size_request (new_dialog_window, 400, 200);

  GtkWidget *new_label = gtk_label_new(_("This will reset the current log status to Phase1,\n and current position to 0,\n and reset all skip data"));
  // gtk_box_pack_start (GTK_BOX (GTK_DIALOG(new_dialog_window)->vbox), new_label, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(new_dialog_window))), new_label, TRUE, TRUE, 0);

  gtk_widget_show_all(new_dialog_window);
  gint result = gtk_dialog_run(GTK_DIALOG(new_dialog_window));
  gtk_widget_destroy(new_dialog_window);

  if (result == GTK_RESPONSE_ACCEPT)
  {
    g_print("resetting status\n");
    reset_current_status_ccc();
    logfile_changed_ccc = true;
    update_display_ccc(0);
  }
}

void do_reset_log_status_ccc(void)
{
  if (!project_chosen_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("You must choose a project first."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
    return;
  }
  GtkWidget *new_dialog_window;
  new_dialog_window = gtk_dialog_new_with_buttons(_("Reset Log"), GTK_WINDOW(main_window_ccc), GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);
  // gtk_widget_set_size_request (new_dialog_window, 400, 200);

  GtkWidget *new_label = gtk_label_new(_("This will reset the position to 0, \nthe current status to non-tried, \nand will change all unfinished blocks \nin the log to a status of non-tried. \nThis will also reset all skipping data."));
  // gtk_box_pack_start (GTK_BOX (GTK_DIALOG(new_dialog_window)->vbox), new_label, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(new_dialog_window))), new_label, TRUE, TRUE, 0);

  gtk_widget_show_all(new_dialog_window);
  gint result = gtk_dialog_run(GTK_DIALOG(new_dialog_window));
  gtk_widget_destroy(new_dialog_window);

  if (result == GTK_RESPONSE_ACCEPT)
  {
    g_print("resetting log\n");
    reset_log_status_ccc();
    update_display_ccc(0);
    // check log and if same status found then repair it
    if (source_chosen_ccc)
    {
      check_log_size_ccc = 1;
    }
    else
    {
      check_log_size_ccc = 0;
    }
    repair_log_ccc = true;
    int ret = check_log_ccc();
    clear_error_message_ccc();
    if (ret == 0x10)
    {
      check_and_repair_log_ccc();
    }
    repair_log_ccc = false;
    logfile_changed_ccc = true;
    update_display_ccc(0);
  }
}

void do_repair_log_ccc(void)
{
  if (!project_chosen_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("You must choose a project first."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
    return;
  }
  GtkWidget *new_dialog_window;
  new_dialog_window = gtk_dialog_new_with_buttons(_("Repair Log"), GTK_WINDOW(main_window_ccc), GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);
  // gtk_widget_set_size_request (new_dialog_window, 400, 200);

  GtkWidget *new_label = gtk_label_new(_("This will repair the current progress log in memory. \nIt is recommended to make a backup copy of the log before repairing."));
  // gtk_box_pack_start (GTK_BOX (GTK_DIALOG(new_dialog_window)->vbox), new_label, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(new_dialog_window))), new_label, TRUE, TRUE, 0);

  gtk_widget_show_all(new_dialog_window);
  gint result = gtk_dialog_run(GTK_DIALOG(new_dialog_window));
  gtk_widget_destroy(new_dialog_window);

  if (result == GTK_RESPONSE_ACCEPT)
  {
    g_print("repairing log\n");
    if (source_chosen_ccc)
    {
      check_log_size_ccc = 1;
    }
    else
    {
      check_log_size_ccc = 0;
    }
    repair_log_ccc = true;
    int ret = check_and_repair_log_ccc();
    if (check_log_ccc() || ret)
    {
      snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("The log file has errors!\nThe log file will need to be repaired before it can be used.\nPlease reference console messages for more information.\n"));
      message_error_ccc(tempmessage_ccc);
      print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
      clear_error_message_ccc();
    }
    repair_log_ccc = false;
    check_log_size_ccc = 1;
    logfile_changed_ccc = true;
    update_display_ccc(0);
  }
}

void about_ccc(void)
{
  GtkWidget *dialog = gtk_about_dialog_new();
  char temp[1024];

  gtk_window_set_title(GTK_WINDOW(dialog), _("About OpenSuperClone"));

  gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dialog), title_ccc);

  snprintf(temp, sizeof(temp), "%s %s", version_number_ccc, GIT_REVISION);
  gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), temp);

  snprintf(temp, sizeof(temp), _("Copyright (C) %s Scott Dwyer and OpenSuperClone contributors"), copyright_year_ccc);
  gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(dialog), temp);

  snprintf(temp, sizeof(temp), _("License type: GPL2\nThere is NO WARRANTY, to the extent permitted by law."));
  gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog), temp);

  gtk_about_dialog_set_website_label(GTK_ABOUT_DIALOG(dialog), OSC_HOMEPAGE);
  gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(dialog), OSC_HOMEPAGE);

  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}

// open choose usb dialog window
void choose_primary_usb_ccc(void)
{
  find_all_usb_devices_ccc();

  choose_usb_dialog_window_ccc = gtk_dialog_new_with_buttons(_("Choose USB Relay"), GTK_WINDOW(main_window_ccc), GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);
  gtk_container_set_border_width(GTK_CONTAINER(choose_usb_dialog_window_ccc), 0);
  gtk_widget_set_size_request(choose_usb_dialog_window_ccc, 600, 400);

  choose_usb_scrolled_window_ccc = gtk_scrolled_window_new(NULL, NULL);
  gtk_container_set_border_width(GTK_CONTAINER(choose_usb_scrolled_window_ccc), 10);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(choose_usb_scrolled_window_ccc), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  // gtk_box_pack_start (GTK_BOX (GTK_DIALOG(choose_usb_dialog_window_ccc)->vbox), choose_usb_scrolled_window_ccc, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(choose_usb_dialog_window_ccc))), choose_usb_scrolled_window_ccc, TRUE, TRUE, 0);

  choose_usb_vbox_ccc = gtk_vbox_new(FALSE, 0);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(choose_usb_scrolled_window_ccc), choose_usb_vbox_ccc);

  GtkWidget *button[MAX_USB_DEVICES];

  int i;
  for (i = 0; i < usb_device_count_ccc; i++)
  {
    if (usb_known_relay_ccc[i])
    {
      char button_label[MAX_BUTTON_LABEL_SIZE] = "";
      snprintf(button_label, MAX_BUTTON_LABEL_SIZE, "%04x:%04x %s %s %s\n", usb_vendor_id_ccc[i], usb_product_id_ccc[i], usb_vendor_string_ccc[i], usb_product_string_ccc[i], usb_extra_id_string_ccc[i]);
      button[i] = gtk_button_new_with_label(button_label);
      gtk_button_set_alignment(GTK_BUTTON(button[i]), 0, .5);
      g_signal_connect(button[i], "clicked", G_CALLBACK(get_usb_from_button_ccc), GINT_TO_POINTER(i));
      gtk_box_pack_start(GTK_BOX(choose_usb_vbox_ccc), button[i], FALSE, FALSE, 0);
    }
  }

  gtk_widget_show_all(choose_usb_dialog_window_ccc);
  gint result = gtk_dialog_run(GTK_DIALOG(choose_usb_dialog_window_ccc));
  gtk_widget_destroy(choose_usb_dialog_window_ccc);

  if (result == GTK_RESPONSE_ACCEPT)
  {
    // fprintf (stdout, "OK\n");
    int ret = choose_usb_relay_ccc();
    if (ret)
    {
      fprintf(stdout, "error selecting usb, ret=%d\n", ret);
      clear_usbr1_ccc();
      snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "%s", _("Error choosing USB relay"));
      message_error_ccc(tempmessage_ccc);
      print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
      clear_error_message_ccc();
    }
    else
    {
      usbr1_chosen_ccc = 1;
      logfile_changed_ccc = true;
      char name[MAX_RELAY_NAME_LENGTH];
      snprintf(name, MAX_RELAY_NAME_LENGTH, "%04x:%04x %s %s %s\n", usbr1_vendor_id_ccc, usbr1_product_id_ccc, usbr1_vendor_string_ccc, usbr1_product_string_ccc, usbr1_extra_id_string_ccc);
      strcpy(primary_relay_settings_ccc.primary_relay_name, name);
      strcpy(primary_relay_name_ccc, name);
      // Choosing a generic relay makes it the board the relay controls drive,
      // matching the Asclepius connect path.
      primary_relay_board_type_ccc = PRIMARY_RELAY_BOARD_GENERIC;
      primary_relay_settings_ccc.primary_relay_board_type = PRIMARY_RELAY_BOARD_GENERIC;
      update_primary_relay_button_settings_ccc();
      update_primary_relay_settings_ccc();
      update_primary_relay_board_type_visibility_ccc();
      gtk_widget_set_sensitive(GTK_WIDGET(activate_primary_relay_button_main_ccc), TRUE);
      gtk_widget_set_sensitive(GTK_WIDGET(deactivate_primary_relay_button_main_ccc), TRUE);
    }
    update_display_ccc(0);
  }
}

void get_usb_from_button_ccc(GtkWidget *w, gpointer data)
{
  new_usb_ccc = GPOINTER_TO_INT(data);
  g_print("selection=%d  \n", new_usb_ccc);
}

void clear_usbr1_ccc(void)
{
  usbr1_chosen_ccc = 0;
  primary_relay_settings_ccc.primary_relay_name[0] = '\0';
  primary_relay_name_ccc[0] = '\0';
  gtk_label_set_text(GTK_LABEL(data_current_relay_board_a_ccc), "");
  // Dropping the selected relay changes whether the controls have a reachable
  // board, so the dialog's Activate/Deactivate sensitivity has to follow.
  update_primary_relay_board_type_visibility_ccc();
  update_primary_relay_connection_summary_ccc();
  gtk_widget_set_sensitive(GTK_WIDGET(activate_primary_relay_button_main_ccc), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(deactivate_primary_relay_button_main_ccc), FALSE);
}

void do_activate_primary_relay_ccc(void)
{
  if (asclepius_relay_source_combobox_ccc)
  {
    int relay_source = gtk_combo_box_get_active(GTK_COMBO_BOX(asclepius_relay_source_combobox_ccc));
    switch (relay_source)
    {
    case 0:
      primary_relay_settings_ccc.asclepius_relay_source = ASCLEPIUS_MAIN12V;
      break;
    case 1:
      primary_relay_settings_ccc.asclepius_relay_source = ASCLEPIUS_MAIN5V;
      break;
    case 2:
      primary_relay_settings_ccc.asclepius_relay_source = ASCLEPIUS_USB5V;
      break;
    case 3:
      primary_relay_settings_ccc.asclepius_relay_source = ASCLEPIUS_MOLEX;
      break;
    case 4:
    default:
      primary_relay_settings_ccc.asclepius_relay_source = ASCLEPIUS_ALL;
      break;
    }
  }

  int asclepius_relay_source = primary_relay_settings_ccc.asclepius_relay_source;
  if (asclepius_relay_source < ASCLEPIUS_MAIN12V || asclepius_relay_source > ASCLEPIUS_ALL)
  {
    asclepius_relay_source = ASCLEPIUS_ALL;
  }

  if(relay_controls_use_asclepius_ccc())
  {
    if (asclepius_disable_channel(asclepius_relay_source) != 0)
    {
      snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error operating Asclepius relay, see console for more information"));
      message_error_ccc(tempmessage_ccc);
      print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
      clear_error_message_ccc();
    }
    else
    {
      clear_asclepius_soft_ocp_ccc((uint8_t)asclepius_relay_source);
    }

    return;
  }

  if (!usbr1_chosen_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error: No relay has been chosen"));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
    return;
  }
  bool activate_primary_relay1_bak = primary_relay_settings_ccc.activate_primary_relay1;
  bool activate_primary_relay2_bak = primary_relay_settings_ccc.activate_primary_relay2;
  bool activate_primary_relay3_bak = primary_relay_settings_ccc.activate_primary_relay3;
  bool activate_primary_relay4_bak = primary_relay_settings_ccc.activate_primary_relay4;
  bool activate_primary_relay5_bak = primary_relay_settings_ccc.activate_primary_relay5;
  bool activate_primary_relay6_bak = primary_relay_settings_ccc.activate_primary_relay6;
  bool activate_primary_relay7_bak = primary_relay_settings_ccc.activate_primary_relay7;
  bool activate_primary_relay8_bak = primary_relay_settings_ccc.activate_primary_relay8;
  bool deactivate_primary_relay1_bak = primary_relay_settings_ccc.deactivate_primary_relay1;
  bool deactivate_primary_relay2_bak = primary_relay_settings_ccc.deactivate_primary_relay2;
  bool deactivate_primary_relay3_bak = primary_relay_settings_ccc.deactivate_primary_relay3;
  bool deactivate_primary_relay4_bak = primary_relay_settings_ccc.deactivate_primary_relay4;
  bool deactivate_primary_relay5_bak = primary_relay_settings_ccc.deactivate_primary_relay5;
  bool deactivate_primary_relay6_bak = primary_relay_settings_ccc.deactivate_primary_relay6;
  bool deactivate_primary_relay7_bak = primary_relay_settings_ccc.deactivate_primary_relay7;
  bool deactivate_primary_relay8_bak = primary_relay_settings_ccc.deactivate_primary_relay8;
  unsigned long long primary_relay_activation_time_bak = primary_relay_settings_ccc.primary_relay_activation_time;
  unsigned long long primary_relay_delay_time_bak = primary_relay_settings_ccc.primary_relay_delay_time;

  update_primary_relay_settings_from_buttons_ccc();
  update_primary_relay_settings_ccc();

  g_print("activate primary relay\n");
  if (activate_primary_relay_ccc())
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error operating relay, see console for more information"));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }

  primary_relay_settings_ccc.activate_primary_relay1 = activate_primary_relay1_bak;
  primary_relay_settings_ccc.activate_primary_relay2 = activate_primary_relay2_bak;
  primary_relay_settings_ccc.activate_primary_relay3 = activate_primary_relay3_bak;
  primary_relay_settings_ccc.activate_primary_relay4 = activate_primary_relay4_bak;
  primary_relay_settings_ccc.activate_primary_relay5 = activate_primary_relay5_bak;
  primary_relay_settings_ccc.activate_primary_relay6 = activate_primary_relay6_bak;
  primary_relay_settings_ccc.activate_primary_relay7 = activate_primary_relay7_bak;
  primary_relay_settings_ccc.activate_primary_relay8 = activate_primary_relay8_bak;
  primary_relay_settings_ccc.deactivate_primary_relay1 = deactivate_primary_relay1_bak;
  primary_relay_settings_ccc.deactivate_primary_relay2 = deactivate_primary_relay2_bak;
  primary_relay_settings_ccc.deactivate_primary_relay3 = deactivate_primary_relay3_bak;
  primary_relay_settings_ccc.deactivate_primary_relay4 = deactivate_primary_relay4_bak;
  primary_relay_settings_ccc.deactivate_primary_relay5 = deactivate_primary_relay5_bak;
  primary_relay_settings_ccc.deactivate_primary_relay6 = deactivate_primary_relay6_bak;
  primary_relay_settings_ccc.deactivate_primary_relay7 = deactivate_primary_relay7_bak;
  primary_relay_settings_ccc.deactivate_primary_relay8 = deactivate_primary_relay8_bak;
  primary_relay_settings_ccc.primary_relay_activation_time = primary_relay_activation_time_bak;
  primary_relay_settings_ccc.primary_relay_delay_time = primary_relay_delay_time_bak;
  update_primary_relay_settings_ccc();
}

void do_deactivate_primary_relay_ccc(void)
{
  if (asclepius_relay_source_combobox_ccc)
  {
    int relay_source = gtk_combo_box_get_active(GTK_COMBO_BOX(asclepius_relay_source_combobox_ccc));
    switch (relay_source)
    {
    case 0:
      primary_relay_settings_ccc.asclepius_relay_source = ASCLEPIUS_MAIN12V;
      break;
    case 1:
      primary_relay_settings_ccc.asclepius_relay_source = ASCLEPIUS_MAIN5V;
      break;
    case 2:
      primary_relay_settings_ccc.asclepius_relay_source = ASCLEPIUS_USB5V;
      break;
    case 3:
      primary_relay_settings_ccc.asclepius_relay_source = ASCLEPIUS_MOLEX;
      break;
    case 4:
    default:
      primary_relay_settings_ccc.asclepius_relay_source = ASCLEPIUS_ALL;
      break;
    }
  }

  int asclepius_relay_source = primary_relay_settings_ccc.asclepius_relay_source;
  if (asclepius_relay_source < ASCLEPIUS_MAIN12V || asclepius_relay_source > ASCLEPIUS_ALL)
  {
    asclepius_relay_source = ASCLEPIUS_ALL;
  }

  if(relay_controls_use_asclepius_ccc())
  {
    if (asclepius_enable_channel(asclepius_relay_source) != 0)
    {
      snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error operating Asclepius relay, see console for more information"));
      message_error_ccc(tempmessage_ccc);
      print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
      clear_error_message_ccc();
    }
    else
    {
      clear_asclepius_soft_ocp_ccc((uint8_t)asclepius_relay_source);
    }

    return;
  }

  if (!usbr1_chosen_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error: No relay has been chosen"));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
    return;
  }
  bool activate_primary_relay1_bak = primary_relay_settings_ccc.activate_primary_relay1;
  bool activate_primary_relay2_bak = primary_relay_settings_ccc.activate_primary_relay2;
  bool activate_primary_relay3_bak = primary_relay_settings_ccc.activate_primary_relay3;
  bool activate_primary_relay4_bak = primary_relay_settings_ccc.activate_primary_relay4;
  bool activate_primary_relay5_bak = primary_relay_settings_ccc.activate_primary_relay5;
  bool activate_primary_relay6_bak = primary_relay_settings_ccc.activate_primary_relay6;
  bool activate_primary_relay7_bak = primary_relay_settings_ccc.activate_primary_relay7;
  bool activate_primary_relay8_bak = primary_relay_settings_ccc.activate_primary_relay8;
  bool deactivate_primary_relay1_bak = primary_relay_settings_ccc.deactivate_primary_relay1;
  bool deactivate_primary_relay2_bak = primary_relay_settings_ccc.deactivate_primary_relay2;
  bool deactivate_primary_relay3_bak = primary_relay_settings_ccc.deactivate_primary_relay3;
  bool deactivate_primary_relay4_bak = primary_relay_settings_ccc.deactivate_primary_relay4;
  bool deactivate_primary_relay5_bak = primary_relay_settings_ccc.deactivate_primary_relay5;
  bool deactivate_primary_relay6_bak = primary_relay_settings_ccc.deactivate_primary_relay6;
  bool deactivate_primary_relay7_bak = primary_relay_settings_ccc.deactivate_primary_relay7;
  bool deactivate_primary_relay8_bak = primary_relay_settings_ccc.deactivate_primary_relay8;
  unsigned long long primary_relay_activation_time_bak = primary_relay_settings_ccc.primary_relay_activation_time;
  unsigned long long primary_relay_delay_time_bak = primary_relay_settings_ccc.primary_relay_delay_time;

  update_primary_relay_settings_from_buttons_ccc();
  update_primary_relay_settings_ccc();

  g_print("deactivate primary relay\n");
  if (deactivate_primary_relay_ccc())
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error operating relay, see console for more information"));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }

  primary_relay_settings_ccc.activate_primary_relay1 = activate_primary_relay1_bak;
  primary_relay_settings_ccc.activate_primary_relay2 = activate_primary_relay2_bak;
  primary_relay_settings_ccc.activate_primary_relay3 = activate_primary_relay3_bak;
  primary_relay_settings_ccc.activate_primary_relay4 = activate_primary_relay4_bak;
  primary_relay_settings_ccc.activate_primary_relay5 = activate_primary_relay5_bak;
  primary_relay_settings_ccc.activate_primary_relay6 = activate_primary_relay6_bak;
  primary_relay_settings_ccc.activate_primary_relay7 = activate_primary_relay7_bak;
  primary_relay_settings_ccc.activate_primary_relay8 = activate_primary_relay8_bak;
  primary_relay_settings_ccc.deactivate_primary_relay1 = deactivate_primary_relay1_bak;
  primary_relay_settings_ccc.deactivate_primary_relay2 = deactivate_primary_relay2_bak;
  primary_relay_settings_ccc.deactivate_primary_relay3 = deactivate_primary_relay3_bak;
  primary_relay_settings_ccc.deactivate_primary_relay4 = deactivate_primary_relay4_bak;
  primary_relay_settings_ccc.deactivate_primary_relay5 = deactivate_primary_relay5_bak;
  primary_relay_settings_ccc.deactivate_primary_relay6 = deactivate_primary_relay6_bak;
  primary_relay_settings_ccc.deactivate_primary_relay7 = deactivate_primary_relay7_bak;
  primary_relay_settings_ccc.deactivate_primary_relay8 = deactivate_primary_relay8_bak;
  primary_relay_settings_ccc.primary_relay_activation_time = primary_relay_activation_time_bak;
  primary_relay_settings_ccc.primary_relay_delay_time = primary_relay_delay_time_bak;
  update_primary_relay_settings_ccc();
}

void do_asclepius_connect_ccc(void)
{
  const gchar *tty_device = gtk_entry_get_text(GTK_ENTRY(asclepius_tty_device_text_ccc));
  strcpy(asclepius_tty_device_ccc, tty_device);

  // Both refreshes below read from primary_relay_settings_ccc, which still
  // holds whatever the dialog was opened with. Without syncing the selection
  // into that buffer first they write the stale device back over this one and
  // into the entry, so picking a tty here silently reverted it. The board type
  // a few lines down is synced for the same reason.
  snprintf(primary_relay_settings_ccc.asclepius_tty_device, sizeof(primary_relay_settings_ccc.asclepius_tty_device), "%s", tty_device);

  if (asclepius_connect() != 0)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error connecting to Asclepius, see console for more information"));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
    return;
  }

  select_connected_asclepius_ccc();

  // The dialog widgets exist only while Primary Relay Settings is open, so they
  // are refreshed here rather than in the shared bookkeeping above.
  update_primary_relay_button_settings_ccc();

  // Refresh the board-type visibility, which is also what enables the relay
  // dialog's Activate/Deactivate buttons. Connecting from inside that dialog
  // never goes through the radio handler or the dialog opener, so without this
  // the buttons kept the sensitivity they had when the window was opened.
  update_primary_relay_board_type_visibility_ccc();
  update_primary_relay_connection_summary_ccc();
}

void do_asclepius_disconnect_ccc(void)
{
  asclepius_disconnect();
  update_primary_relay_board_type_visibility_ccc();
  update_primary_relay_connection_summary_ccc();
  gtk_widget_set_sensitive(GTK_WIDGET(activate_primary_relay_button_main_ccc), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(deactivate_primary_relay_button_main_ccc), FALSE);
}

void do_activate_primary_relay_main_ccc(void)
{
  int asclepius_relay_source = primary_relay_settings_ccc.asclepius_relay_source;
  if (asclepius_relay_source < ASCLEPIUS_MAIN12V || asclepius_relay_source > ASCLEPIUS_ALL)
  {
    asclepius_relay_source = ASCLEPIUS_ALL;
  }

  if(relay_controls_use_asclepius_ccc())
  {
    if (asclepius_disable_channel(asclepius_relay_source) != 0)
    {
      snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error operating Asclepius relay, see console for more information"));
      message_error_ccc(tempmessage_ccc);
      print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
      clear_error_message_ccc();
    }
    else
    {
      clear_asclepius_soft_ocp_ccc((uint8_t)asclepius_relay_source);
    }

    return;
  }

  if (!usbr1_chosen_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error: No relay has been chosen"));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
    return;
  }
  g_print("activate primary relay\n");
  if (activate_primary_relay_ccc())
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error operating relay, see console for more information"));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
}

void do_deactivate_primary_relay_main_ccc(void)
{
  int asclepius_relay_source = primary_relay_settings_ccc.asclepius_relay_source;
  if (asclepius_relay_source < ASCLEPIUS_MAIN12V || asclepius_relay_source > ASCLEPIUS_ALL)
  {
    asclepius_relay_source = ASCLEPIUS_ALL;
  }

  if(relay_controls_use_asclepius_ccc())
  {
    if (asclepius_enable_channel(asclepius_relay_source) != 0)
    {
      snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error operating Asclepius relay, see console for more information"));
      message_error_ccc(tempmessage_ccc);
      print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
      clear_error_message_ccc();
    }
    else
    {
      clear_asclepius_soft_ocp_ccc((uint8_t)asclepius_relay_source);
    }

    return;
  }

  if (!usbr1_chosen_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error: No relay has been chosen"));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
    return;
  }
  g_print("deactivate primary relay\n");
  if (deactivate_primary_relay_ccc())
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Error operating relay, see console for more information"));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
}

void map_heads_ccc(void)
{
  if (!connected_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "you must be connected to map heads");
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
  }
  else
  {
    if (rebuild_assist_map_heads_ccc())
    {
      snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, "error mapping heads");
      message_error_ccc(tempmessage_ccc);
      print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
      clear_error_message_ccc();
    }
  }
}

void open_in_oscviewer_ccc(void)
{
  if (!project_chosen_ccc)
  {
    snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("You must choose a project first."));
    message_error_ccc(tempmessage_ccc);
    print_gui_error_message_ccc(error_message_ccc, _("Error!"), 1);
    clear_error_message_ccc();
    return;
  }

  char temp[1024];
  if (domain_file_ccc[0] != '\0')
  {
    snprintf(temp, sizeof(temp), "%s/oscviewer -l %s -f %s", OSC_INSTALL_PATH, log_file_ccc, domain_file_ccc);
  }
  else
  {
  snprintf(temp, sizeof(temp), "%s/oscviewer -l %s", OSC_INSTALL_PATH, log_file_ccc);
  }
  g_spawn_command_line_async(temp, NULL);
}

void read_config_file_with_name_ccc(char* filename)
{
  FILE *config_file = fopen(filename, "r");
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

  // load clone settings
  load_clone_settings_ccc();

  // read ui settings from config file
  group = config_setting_get_member(root, "ui");
  if (group != NULL)
  {
    setting = config_setting_get_member(group, "window_width");
    if (setting != NULL)
    {
      default_window_width = config_setting_get_int(setting);
    }

    setting = config_setting_get_member(group, "window_height");
    if (setting != NULL)
    {
      default_window_height = config_setting_get_int(setting);
    }
  }

  // read clone settings from config file
  group = config_setting_get_member(root, "clone");
  if (group != NULL)
  {
  
    setting = config_setting_get_member(group, "no_phase1");
    if (setting != NULL)
    {
      clone_settings_ccc.no_phase1 = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "no_phase2");
    if (setting != NULL)
    {
      clone_settings_ccc.no_phase2 = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "no_phase3");
    if (setting != NULL)
    {
      clone_settings_ccc.no_phase3 = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "no_phase4");
    if (setting != NULL)
    {
      clone_settings_ccc.no_phase4 = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "no_trim");
    if (setting != NULL)
    {
      clone_settings_ccc.no_trim = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "no_scrape");
    if (setting != NULL)
    {
      clone_settings_ccc.no_scrape = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "no_divide1");
    if (setting != NULL)
    {
      clone_settings_ccc.no_divide1 = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "do_divide2");
    if (setting != NULL)
    {
      clone_settings_ccc.do_divide2 = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "read_bad_head");
    if (setting != NULL)
    {
      clone_settings_ccc.read_bad_head = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "mark_bad_head");
    if (setting != NULL)
    {
      clone_settings_ccc.mark_bad_head = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "rebuild_assist");
    if (setting != NULL)
    {
      clone_settings_ccc.rebuild_assist = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "reverse");
    if (setting != NULL)
    {
      clone_settings_ccc.reverse = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "retries");
    if (setting != NULL)
    {
      clone_settings_ccc.retries = config_setting_get_int(setting);
    }

    setting = config_setting_get_member(group, "cluster_size");
    if (setting != NULL)
    {
      clone_settings_ccc.cluster_size = config_setting_get_int(setting);
    }

    setting = config_setting_get_member(group, "block_size");
    if (setting != NULL)
    {
      clone_settings_ccc.block_size = config_setting_get_int(setting);
    }

    setting = config_setting_get_member(group, "input_offset");
    if (setting != NULL)
    {
      clone_settings_ccc.input_offset = config_setting_get_int64(setting);
    }

    setting = config_setting_get_member(group, "read_size");;
    if (setting != NULL)
    {
      clone_settings_ccc.read_size = config_setting_get_int64(setting);
    }

    setting = config_setting_get_member(group, "skip_fast");
    if (setting != NULL)
    {
      clone_settings_ccc.skip_fast = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "rate_skip");
    if (setting != NULL)
    {
      clone_settings_ccc.rate_skip = config_setting_get_int64(setting);
    }

    setting = config_setting_get_member(group, "exit_on_slow");
    if (setting != NULL)
    {
      clone_settings_ccc.exit_on_slow = config_setting_get_int64(setting);
    }

    setting = config_setting_get_member(group, "exit_slow_time");
    if (setting != NULL)
    {
      clone_settings_ccc.exit_slow_time = config_setting_get_int64(setting);
    }

    // setting = config_setting_get_member(group, "min_skip_size");
    // if (setting != NULL)
    // {
    //   clone_settings_ccc.min_skip_size = config_setting_get_int64(setting);
    // }

    // setting = config_setting_get_member(group, "min_skip_size_bak");
    // if (setting != NULL)
    // {
    //   clone_settings_ccc.min_skip_size_bak = config_setting_get_int64(setting);
    // }

    // setting = config_setting_get_member(group, "max_skip_size");
    // if (setting != NULL)
    // {
    //   clone_settings_ccc.max_skip_size = config_setting_get_int64(setting);
    // }

    setting = config_setting_get_member(group, "skip_timeout");
    if (setting != NULL)
    {
      clone_settings_ccc.skip_timeout = config_setting_get_int64(setting);
    }

    // setting = config_setting_get_member(group, "sector_size");
    // if (setting != NULL)
    // {
    //   clone_settings_ccc.sector_size = config_setting_get_int(setting);
    // }

    // setting = config_setting_get_member(group, "block_offset");
    // if (setting != NULL)
    // {
    //   clone_settings_ccc.block_offset = config_setting_get_int(setting);
    // }

    setting = config_setting_get_member(group, "max_read_rate");
    if (setting != NULL)
    {
      clone_settings_ccc.max_read_rate = config_setting_get_int64(setting);
    }
  }

  // update clone settings
  update_clone_settings_ccc();

  // load advanced settings
  load_advanced_settings_ccc();

  // read advanced settings from config file
  group = config_setting_get_member(root, "advanced");
  if (group != NULL)
  {
    setting = config_setting_get_member(group, "force_mounted");
    if (setting != NULL)
    {
      advanced_settings_ccc.force_mounted = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "force_dangerous");
    if (setting != NULL)
    {
      advanced_settings_ccc.force_dangerous = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "no_log_backup");
    if (setting != NULL)
    {
      advanced_settings_ccc.no_log_backup = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "enable_output_offset");;
    if (setting != NULL)
    {
      advanced_settings_ccc.enable_output_offset = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "output_offset");
    if (setting != NULL)
    {
      advanced_settings_ccc.output_offset = config_setting_get_int64(setting);
    }

    setting = config_setting_get_member(group, "enable_current_position");
    if (setting != NULL)
    {
      advanced_settings_ccc.enable_current_position = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "current_position");
    if (setting != NULL)
    {
      advanced_settings_ccc.current_position = config_setting_get_int64(setting);
    }

    setting = config_setting_get_member(group, "command_to_call");
    if (setting != NULL)
    {
      const char *cmd = config_setting_get_string(setting);
      snprintf(advanced_settings_ccc.command_to_call, sizeof(advanced_settings_ccc.command_to_call), "%s", cmd);
    }

    setting = config_setting_get_member(group, "stop_on_error");
    if (setting != NULL)
    {
      advanced_settings_ccc.stop_on_error = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "call_command");
    if (setting != NULL)
    {
      advanced_settings_ccc.call_command = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "activate_primary_relay");;
    if (setting != NULL)
    {
      advanced_settings_ccc.activate_primary_relay = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "write_buffer_auto");
    if (setting != NULL)
    {
      advanced_settings_ccc.write_buffer_auto = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "write_buffer_disable");
    if (setting != NULL)
    {
      advanced_settings_ccc.write_buffer_disable = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "write_buffer_enable");
    if (setting != NULL)
    {
      advanced_settings_ccc.write_buffer_enable = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "driver_return_error");
    if (setting != NULL)
    {
      advanced_settings_ccc.driver_return_error = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "driver_return_zeros");
    if (setting != NULL)
    {
      advanced_settings_ccc.driver_return_zeros = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "driver_return_marked");
    if (setting != NULL)
    {
      advanced_settings_ccc.driver_return_marked = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "virtual_disk_device_name");
    if (setting != NULL)
    {
      const char *vdn = config_setting_get_string(setting);
      snprintf(advanced_settings_ccc.virtual_disk_device_name, sizeof(advanced_settings_ccc.virtual_disk_device_name), "%s", vdn);
    }

    setting = config_setting_get_member(group, "disable_identify");
    if (setting != NULL)
    {
      advanced_settings_ccc.disable_identify = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "pio_mode");
    if (setting != NULL)
    {
      advanced_settings_ccc.pio_mode = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "enable_rebuild_assist");
    if (setting != NULL)
    {
      advanced_settings_ccc.enable_rebuild_assist = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "enable_process_chunk");
    if (setting != NULL)
    {
      advanced_settings_ccc.enable_process_chunk = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "enable_read_twice");
    if (setting != NULL)
    {
      advanced_settings_ccc.enable_read_twice = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "enable_retry_connecting");
    if (setting != NULL)
    {
      advanced_settings_ccc.enable_retry_connecting = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "enable_scsi_write");
    if (setting != NULL)
    {
      advanced_settings_ccc.enable_scsi_write = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "enable_phase_log");
    if (setting != NULL)
    {
      advanced_settings_ccc.enable_phase_log = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "enable_output_sector_size");
    if (setting != NULL)
    {
      advanced_settings_ccc.enable_output_sector_size = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "output_sector_adjustment");
    if (setting != NULL)
    {
      advanced_settings_ccc.output_sector_adjustment = config_setting_get_int64(setting);
    }

    setting = config_setting_get_member(group, "driver_io_scsi_only");
    if (setting != NULL)
    {
      advanced_settings_ccc.driver_io_scsi_only = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "use_physical_sector_size_for_virtual");
    if (setting != NULL)
    {
      advanced_settings_ccc.use_physical_sector_size_for_virtual = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "driver_minimum_cluster_size");
    if (setting != NULL)
    {
      advanced_settings_ccc.driver_minimum_cluster_size = config_setting_get_int64(setting);
    }

    // setting = config_setting_get_member(group, "color_statusbar");
    // if (setting != NULL)
    // {
    //   advanced_settings_ccc.color_statusbar = config_setting_get_bool(setting);
    // }
  }

  // update advanced settings
  update_advanced_settings_ccc();

  // load timer settings
  load_timer_settings_ccc();

  // read timer settings from config file
  group = config_setting_get_member(root, "timers");
  if (group != NULL)
  {
    setting = config_setting_get_member(group, "initial_busy_wait_time");
    if (setting != NULL)
    {
      timer_settings_ccc.initial_busy_wait_time = config_setting_get_int64(setting);
    }

    setting = config_setting_get_member(group, "busy_wait_time");
    if (setting != NULL)
    {
      timer_settings_ccc.busy_wait_time = config_setting_get_int64(setting);
    }

    setting = config_setting_get_member(group, "soft_reset_time");
    if (setting != NULL)
    {
      timer_settings_ccc.soft_reset_time = config_setting_get_int64(setting);
    }

    setting = config_setting_get_member(group, "hard_reset_time");
    if (setting != NULL)
    {
      timer_settings_ccc.hard_reset_time = config_setting_get_int64(setting);
    }

    setting = config_setting_get_member(group, "power_cycle_time");
    if (setting != NULL)
    {
      timer_settings_ccc.power_cycle_time = config_setting_get_int64(setting);
    }

    setting = config_setting_get_member(group, "general_timeout");
    if (setting != NULL)
    {
      timer_settings_ccc.general_timeout = config_setting_get_int64(setting);
    }

    setting = config_setting_get_member(group, "power_command_to_call");
    if (setting != NULL)
    {
      const char *pcmd = config_setting_get_string(setting);
      snprintf(timer_settings_ccc.power_command_to_call, sizeof(timer_settings_ccc.power_command_to_call), "%s", pcmd);
    }

    setting = config_setting_get_member(group, "stop_on_power_cycle");
    if (setting != NULL)
    {
      timer_settings_ccc.stop_on_power_cycle = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "call_power_command");
    if (setting != NULL)
    {
      timer_settings_ccc.call_power_command = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "power_cycle_primary_relay");
    if (setting != NULL)
    {
      timer_settings_ccc.power_cycle_primary_relay = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "phase_timers");
    if (setting != NULL)
    {
      timer_settings_ccc.phase_timers = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "p12_soft_reset_time");
    if (setting != NULL)
    {
      timer_settings_ccc.p12_soft_reset_time = config_setting_get_int64(setting);
    }

    setting = config_setting_get_member(group, "p3_soft_reset_time");
    if (setting != NULL)
    {
      timer_settings_ccc.p3_soft_reset_time = config_setting_get_int64(setting);
    }

    setting = config_setting_get_member(group, "p4_soft_reset_time");
    if (setting != NULL)
    {
      timer_settings_ccc.p4_soft_reset_time = config_setting_get_int64(setting);
    }

    setting = config_setting_get_member(group, "td_soft_reset_time");
    if (setting != NULL)
    {
      timer_settings_ccc.td_soft_reset_time = config_setting_get_int64(setting);
    }

    setting = config_setting_get_member(group, "d2_soft_reset_time");
    if (setting != NULL)
    {
      timer_settings_ccc.d2_soft_reset_time = config_setting_get_int64(setting);
    }

    setting = config_setting_get_member(group, "sc_soft_reset_time");
    if (setting != NULL)
    {
      timer_settings_ccc.sc_soft_reset_time = config_setting_get_int64(setting);
    }

    setting = config_setting_get_member(group, "rt_soft_reset_time");
    if (setting != NULL)
    {
      timer_settings_ccc.rt_soft_reset_time = config_setting_get_int64(setting);
    }

    setting = config_setting_get_member(group, "always_wait_for_reset_timers");
    if (setting != NULL)
    {
      timer_settings_ccc.always_wait_for_reset_timers = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "usb_bulk_reset_enabled");
    if (setting != NULL)
    {
      timer_settings_ccc.usb_bulk_reset_enabled = config_setting_get_int(setting);
    }

    setting = config_setting_get_member(group, "usb_soft_reset_enabled");
    if (setting != NULL)
    {
      timer_settings_ccc.usb_soft_reset_enabled = config_setting_get_int(setting);
    }

    setting = config_setting_get_member(group, "usb_hard_reset_enabled");
    if (setting != NULL)
    {
      timer_settings_ccc.usb_hard_reset_enabled = config_setting_get_int(setting);
    }

    setting = config_setting_get_member(group, "usb_port_reset_enabled");
    if (setting != NULL)
    {
      timer_settings_ccc.usb_port_reset_enabled = config_setting_get_int(setting);
    }
  }

  // update timer settings
  update_timer_settings_ccc();

  // load primary relay settings
  load_primary_relay_settings_ccc();

  // read primary relay settings from config file
  group = config_setting_get_member(root, "relay");
  if (group != NULL)
  {
    setting = config_setting_get_member(group, "activate_primary_relay1");
    if (setting != NULL)
    {
      primary_relay_settings_ccc.activate_primary_relay1 = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "activate_primary_relay2");
    if (setting != NULL)
    {
      primary_relay_settings_ccc.activate_primary_relay2 = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "activate_primary_relay3");
    if (setting != NULL)
    {
      primary_relay_settings_ccc.activate_primary_relay3 = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "activate_primary_relay4");
    if (setting != NULL)
    {
      primary_relay_settings_ccc.activate_primary_relay4 = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "activate_primary_relay5");;
    if (setting != NULL)
    {
      primary_relay_settings_ccc.activate_primary_relay5 = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "activate_primary_relay6");
    if (setting != NULL)
    {
      primary_relay_settings_ccc.activate_primary_relay6 = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "activate_primary_relay7");
    if (setting != NULL)
    {
      primary_relay_settings_ccc.activate_primary_relay7 = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "activate_primary_relay8");
    if (setting != NULL)
    {
      primary_relay_settings_ccc.activate_primary_relay8 = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "deactivate_primary_relay1");
    if (setting != NULL)
    {
      primary_relay_settings_ccc.deactivate_primary_relay1 = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "deactivate_primary_relay2");
    if (setting != NULL)
    {
      primary_relay_settings_ccc.deactivate_primary_relay2 = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "deactivate_primary_relay3");
    if (setting != NULL)
    {
      primary_relay_settings_ccc.deactivate_primary_relay3 = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "deactivate_primary_relay4");
    if (setting != NULL)
    {
      primary_relay_settings_ccc.deactivate_primary_relay4 = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "deactivate_primary_relay5");
    if (setting != NULL)
    {
      primary_relay_settings_ccc.deactivate_primary_relay5 = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "deactivate_primary_relay6");
    if (setting != NULL)
    {
      primary_relay_settings_ccc.deactivate_primary_relay6 = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "deactivate_primary_relay7");
    if (setting != NULL)
    {
      primary_relay_settings_ccc.deactivate_primary_relay7 = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "deactivate_primary_relay8");
    if (setting != NULL)
    {
      primary_relay_settings_ccc.deactivate_primary_relay8 = config_setting_get_bool(setting);
    }

    setting = config_setting_get_member(group, "primary_relay_activation_time");
    if (setting != NULL)
    {
      primary_relay_settings_ccc.primary_relay_activation_time = config_setting_get_int64(setting);
    }

    setting = config_setting_get_member(group, "primary_relay_delay_time");
    if (setting != NULL)
    {
      primary_relay_settings_ccc.primary_relay_delay_time = config_setting_get_int64(setting);
    }

    setting = config_setting_get_member(group, "primary_relay_board_type");
    if (setting != NULL)
    {
      primary_relay_settings_ccc.primary_relay_board_type = primary_relay_board_type_valid(config_setting_get_int(setting));
    }

    setting = config_setting_get_member(group, "asclepius_relay_source");
    if (setting != NULL)
    {
      primary_relay_settings_ccc.asclepius_relay_source = config_setting_get_int(setting);
      if (primary_relay_settings_ccc.asclepius_relay_source < ASCLEPIUS_MAIN12V || primary_relay_settings_ccc.asclepius_relay_source > ASCLEPIUS_ALL)
      {
        primary_relay_settings_ccc.asclepius_relay_source = ASCLEPIUS_ALL;
      }
    }

    setting = config_setting_get_member(group, "asclepius_tty_device");
    if (setting != NULL)
    {
      const char *atd = config_setting_get_string(setting);
      snprintf(primary_relay_settings_ccc.asclepius_tty_device, sizeof(primary_relay_settings_ccc.asclepius_tty_device), "%s", atd);
    }

    setting = config_setting_get_member(group, "asclepius_ext_tty_device");
    if (setting != NULL)
    {
      const char *atd = config_setting_get_string(setting);
      snprintf(primary_relay_settings_ccc.asclepius_ext_tty_device, sizeof(primary_relay_settings_ccc.asclepius_ext_tty_device), "%s", atd);
    }

    setting = config_setting_get_member(group, "asclepius_ext_baud_rate");
    if (setting != NULL)
    {
      primary_relay_settings_ccc.asclepius_ext_baud_rate = config_setting_get_int(setting);
    }

    setting = config_setting_get_member(group, "asclepius_poll_interval_ms");
    if (setting != NULL)
    {
      int poll_ms = config_setting_get_int(setting);
      if (poll_ms < ASCLEPIUS_POLL_INTERVAL_MIN_MS || poll_ms > ASCLEPIUS_POLL_INTERVAL_MAX_MS)
      {
        poll_ms = ASCLEPIUS_POLL_INTERVAL_DEFAULT_MS;
      }
      primary_relay_settings_ccc.asclepius_poll_interval_ms = (unsigned int)poll_ms;
    }

    setting = config_setting_get_member(group, "asclepius_ocp_action_12v");
    if (setting != NULL)
    {
      primary_relay_settings_ccc.asclepius_ocp_action_12v = asclepius_ocp_clamp_action(config_setting_get_int(setting));
    }

    setting = config_setting_get_member(group, "asclepius_ocp_action_5v");
    if (setting != NULL)
    {
      primary_relay_settings_ccc.asclepius_ocp_action_5v = asclepius_ocp_clamp_action(config_setting_get_int(setting));
    }

    setting = config_setting_get_member(group, "asclepius_ocp_action_usb");
    if (setting != NULL)
    {
      primary_relay_settings_ccc.asclepius_ocp_action_usb = asclepius_ocp_clamp_action(config_setting_get_int(setting));
    }

    setting = config_setting_get_member(group, "asclepius_ocp_limit_12v");
    if (setting != NULL)
    {
      primary_relay_settings_ccc.asclepius_ocp_limit_12v = asclepius_ocp_clamp_limit_mA((unsigned int)config_setting_get_int(setting));
    }

    setting = config_setting_get_member(group, "asclepius_ocp_limit_5v");
    if (setting != NULL)
    {
      primary_relay_settings_ccc.asclepius_ocp_limit_5v = asclepius_ocp_clamp_limit_mA((unsigned int)config_setting_get_int(setting));
    }

    setting = config_setting_get_member(group, "asclepius_ocp_limit_usb");
    if (setting != NULL)
    {
      primary_relay_settings_ccc.asclepius_ocp_limit_usb = asclepius_ocp_clamp_limit_mA((unsigned int)config_setting_get_int(setting));
    }

    // setting = config_setting_get_member(group, "primary_relay_name");;
    // if (setting != NULL)
    // {
    //   const char *prn = config_setting_get_string(setting);
    //   snprintf(primary_relay_settings_ccc.primary_relay_name, sizeof(primary_relay_settings_ccc.primary_relay_name), "%s", prn);
    // }
  }

  // update primary relay settings
  update_primary_relay_settings_ccc();

  config_destroy(&config);

  fclose(config_file);
}

void read_config_file_ccc(void)
{
  GtkWidget *dialog;
  dialog = gtk_file_chooser_dialog_new(_("Load Settings File"),
                                        GTK_WINDOW(main_window_ccc),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                        NULL);
  
  gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), template_directory);

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
  {
    char *filename;
    filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    
    read_config_file_with_name_ccc(filename);

    g_free(filename);
  }
  gtk_widget_destroy(dialog);
}

void write_config_file_with_name_ccc(char* filename)
{
  FILE *config_file = fopen(filename, "w");
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

  // load clone settings
  load_clone_settings_ccc();

  // write ui settings to config file
  group = config_setting_add(root, "ui", CONFIG_TYPE_GROUP);

  setting = config_setting_add(group, "window_width", CONFIG_TYPE_INT);
  config_setting_set_int(setting, default_window_width);

  setting = config_setting_add(group, "window_height", CONFIG_TYPE_INT);
  config_setting_set_int(setting, default_window_height);

  // write clone settings to config file
  group = config_setting_add(root, "clone", CONFIG_TYPE_GROUP);

  setting = config_setting_add(group, "no_phase1", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, clone_settings_ccc.no_phase1);

  setting = config_setting_add(group, "no_phase2", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, clone_settings_ccc.no_phase2);

  setting = config_setting_add(group, "no_phase3", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, clone_settings_ccc.no_phase3);

  setting = config_setting_add(group, "no_phase4", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, clone_settings_ccc.no_phase4);

  setting = config_setting_add(group, "no_trim", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, clone_settings_ccc.no_trim);

  setting = config_setting_add(group, "no_scrape", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, clone_settings_ccc.no_scrape);

  setting = config_setting_add(group, "no_divide1", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, clone_settings_ccc.no_divide1);

  setting = config_setting_add(group, "do_divide2", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, clone_settings_ccc.do_divide2);

  setting = config_setting_add(group, "read_bad_head", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, clone_settings_ccc.read_bad_head);

  setting = config_setting_add(group, "mark_bad_head", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, clone_settings_ccc.mark_bad_head);

  setting = config_setting_add(group, "rebuild_assist", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, clone_settings_ccc.rebuild_assist);

  setting = config_setting_add(group, "reverse", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, clone_settings_ccc.reverse);

  setting = config_setting_add(group, "retries", CONFIG_TYPE_INT);
  config_setting_set_bool(setting, clone_settings_ccc.retries);

  setting = config_setting_add(group, "cluster_size", CONFIG_TYPE_INT);
  config_setting_set_int(setting, clone_settings_ccc.cluster_size);

  setting = config_setting_add(group, "block_size", CONFIG_TYPE_INT);
  config_setting_set_int(setting, clone_settings_ccc.block_size);

  setting = config_setting_add(group, "input_offset", CONFIG_TYPE_INT64);
  config_setting_set_int64(setting, clone_settings_ccc.input_offset);

  setting = config_setting_add(group, "read_size", CONFIG_TYPE_INT64);
  config_setting_set_int64(setting, clone_settings_ccc.read_size);

  setting = config_setting_add(group, "skip_fast", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, clone_settings_ccc.skip_fast);

  setting = config_setting_add(group, "rate_skip", CONFIG_TYPE_INT64);
  config_setting_set_int64(setting, clone_settings_ccc.rate_skip);

  setting = config_setting_add(group, "exit_on_slow", CONFIG_TYPE_INT64);
  config_setting_set_int64(setting, clone_settings_ccc.exit_on_slow);

  setting = config_setting_add(group, "exit_slow_time", CONFIG_TYPE_INT64);
  config_setting_set_int64(setting, clone_settings_ccc.exit_slow_time);

  //setting = config_setting_add(group, "min_skip_size", CONFIG_TYPE_INT64);
  //config_setting_set_int64(setting, clone_settings_ccc.min_skip_size);

  //setting = config_setting_add(group, "min_skip_size_bak", CONFIG_TYPE_INT64);
  //config_setting_set_int64(setting, clone_settings_ccc.min_skip_size_bak);

  //setting = config_setting_add(group, "max_skip_size", CONFIG_TYPE_INT64);
  //config_setting_set_int64(setting, clone_settings_ccc.max_skip_size);

  setting = config_setting_add(group, "skip_timeout", CONFIG_TYPE_INT64);
  config_setting_set_int64(setting, clone_settings_ccc.skip_timeout);

  //setting = config_setting_add(group, "sector_size", CONFIG_TYPE_INT);
  //config_setting_set_int(setting, clone_settings_ccc.sector_size);

  //setting = config_setting_add(group, "block_offset", CONFIG_TYPE_INT);
  //config_setting_set_int(setting, clone_settings_ccc.block_offset);

  setting = config_setting_add(group, "max_read_rate", CONFIG_TYPE_INT64);
  config_setting_set_int64(setting, clone_settings_ccc.max_read_rate);

  // load advanced settings
  load_advanced_settings_ccc();

  // write advanced settings to config file
  group = config_setting_add(root, "advanced", CONFIG_TYPE_GROUP);

  setting = config_setting_add(group, "force_mounted", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, advanced_settings_ccc.force_mounted);

  setting = config_setting_add(group, "force_dangerous", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, advanced_settings_ccc.force_dangerous);

  setting = config_setting_add(group, "no_log_backup", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, advanced_settings_ccc.no_log_backup);

  setting = config_setting_add(group, "enable_output_offset", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, advanced_settings_ccc.enable_output_offset);

  setting = config_setting_add(group, "output_offset", CONFIG_TYPE_INT64);
  config_setting_set_int64(setting, advanced_settings_ccc.output_offset);

  setting = config_setting_add(group, "enable_current_position", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, advanced_settings_ccc.enable_current_position);

  setting = config_setting_add(group, "current_position", CONFIG_TYPE_INT64);
  config_setting_set_int64(setting, advanced_settings_ccc.current_position);

  setting = config_setting_add(group, "command_to_call", CONFIG_TYPE_STRING);
  config_setting_set_string(setting, advanced_settings_ccc.command_to_call);

  setting = config_setting_add(group, "stop_on_error", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, advanced_settings_ccc.stop_on_error);

  setting = config_setting_add(group, "call_command", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, advanced_settings_ccc.call_command);

  setting = config_setting_add(group, "activate_primary_relay", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, advanced_settings_ccc.activate_primary_relay);

  setting = config_setting_add(group, "write_buffer_auto", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, advanced_settings_ccc.write_buffer_auto);

  setting = config_setting_add(group, "write_buffer_disable", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, advanced_settings_ccc.write_buffer_disable);

  setting = config_setting_add(group, "write_buffer_enable", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, advanced_settings_ccc.write_buffer_enable);

  setting = config_setting_add(group, "driver_return_error", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, advanced_settings_ccc.driver_return_error);

  setting = config_setting_add(group, "driver_return_zeros", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, advanced_settings_ccc.driver_return_zeros);

  setting = config_setting_add(group, "driver_return_marked", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, advanced_settings_ccc.driver_return_marked);

  setting = config_setting_add(group, "virtual_disk_device_name", CONFIG_TYPE_STRING);
  config_setting_set_string(setting, advanced_settings_ccc.virtual_disk_device_name);

  setting = config_setting_add(group, "disable_identify", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, advanced_settings_ccc.disable_identify);

  setting = config_setting_add(group, "pio_mode", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, advanced_settings_ccc.pio_mode);

  setting = config_setting_add(group, "enable_rebuild_assist", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, advanced_settings_ccc.enable_rebuild_assist);

  setting = config_setting_add(group, "enable_process_chunk", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, advanced_settings_ccc.enable_process_chunk);

  setting = config_setting_add(group, "enable_read_twice", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, advanced_settings_ccc.enable_read_twice);

  setting = config_setting_add(group, "enable_retry_connecting", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, advanced_settings_ccc.enable_retry_connecting);

  setting = config_setting_add(group, "enable_scsi_write", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, advanced_settings_ccc.enable_scsi_write);

  setting = config_setting_add(group, "enable_phase_log", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, advanced_settings_ccc.enable_phase_log);

  setting = config_setting_add(group, "enable_output_sector_size", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, advanced_settings_ccc.enable_output_sector_size);

  setting = config_setting_add(group, "output_sector_adjustment", CONFIG_TYPE_INT64);
  config_setting_set_int64(setting, advanced_settings_ccc.output_sector_adjustment);

  setting = config_setting_add(group, "driver_io_scsi_only", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, advanced_settings_ccc.driver_io_scsi_only);

  setting = config_setting_add(group, "use_physical_sector_size_for_virtual", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, advanced_settings_ccc.use_physical_sector_size_for_virtual);

  setting = config_setting_add(group, "driver_minimum_cluster_size", CONFIG_TYPE_INT64);
  config_setting_set_int64(setting, advanced_settings_ccc.driver_minimum_cluster_size);

  // setting = config_setting_add(group, "color_statusbar", CONFIG_TYPE_BOOL);
  // config_setting_set_bool(setting, advanced_settings_ccc.color_statusbar);

  // load timer settings
  load_timer_settings_ccc();

  // write timer settings to config file
  group = config_setting_add(root, "timers", CONFIG_TYPE_GROUP);

  setting = config_setting_add(group, "initial_busy_wait_time", CONFIG_TYPE_INT64);
  config_setting_set_int64(setting, timer_settings_ccc.initial_busy_wait_time);

  setting = config_setting_add(group, "busy_wait_time", CONFIG_TYPE_INT64);
  config_setting_set_int64(setting, timer_settings_ccc.busy_wait_time);

  setting = config_setting_add(group, "soft_reset_time", CONFIG_TYPE_INT64);
  config_setting_set_int64(setting, timer_settings_ccc.soft_reset_time);

  setting = config_setting_add(group, "hard_reset_time", CONFIG_TYPE_INT64);
  config_setting_set_int64(setting, timer_settings_ccc.hard_reset_time);

  setting = config_setting_add(group, "power_cycle_time", CONFIG_TYPE_INT64);
  config_setting_set_int64(setting, timer_settings_ccc.power_cycle_time);

  setting = config_setting_add(group, "general_timeout", CONFIG_TYPE_INT64);
  config_setting_set_int64(setting, timer_settings_ccc.general_timeout);

  setting = config_setting_add(group, "power_command_to_call", CONFIG_TYPE_STRING);
  config_setting_set_string(setting, timer_settings_ccc.power_command_to_call);

  setting = config_setting_add(group, "stop_on_power_cycle", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, timer_settings_ccc.stop_on_power_cycle);

  setting = config_setting_add(group, "call_power_command", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, timer_settings_ccc.call_power_command);

  setting = config_setting_add(group, "power_cycle_primary_relay", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, timer_settings_ccc.power_cycle_primary_relay);

  setting = config_setting_add(group, "phase_timers", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, timer_settings_ccc.phase_timers);

  setting = config_setting_add(group, "p12_soft_reset_time", CONFIG_TYPE_INT64);
  config_setting_set_int64(setting, timer_settings_ccc.p12_soft_reset_time);

  setting = config_setting_add(group, "p3_soft_reset_time", CONFIG_TYPE_INT64);
  config_setting_set_int64(setting, timer_settings_ccc.p3_soft_reset_time);

  setting = config_setting_add(group, "p4_soft_reset_time", CONFIG_TYPE_INT64);
  config_setting_set_int64(setting, timer_settings_ccc.p4_soft_reset_time);

  setting = config_setting_add(group, "td_soft_reset_time", CONFIG_TYPE_INT64);
  config_setting_set_int64(setting, timer_settings_ccc.td_soft_reset_time);

  setting = config_setting_add(group, "d2_soft_reset_time", CONFIG_TYPE_INT64);
  config_setting_set_int64(setting, timer_settings_ccc.d2_soft_reset_time);

  setting = config_setting_add(group, "sc_soft_reset_time", CONFIG_TYPE_INT64);
  config_setting_set_int64(setting, timer_settings_ccc.sc_soft_reset_time);

  setting = config_setting_add(group, "rt_soft_reset_time", CONFIG_TYPE_INT64);
  config_setting_set_int64(setting, timer_settings_ccc.rt_soft_reset_time);

  setting = config_setting_add(group, "always_wait_for_reset_timers", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, timer_settings_ccc.always_wait_for_reset_timers);

  setting = config_setting_add(group, "usb_bulk_reset_enabled", CONFIG_TYPE_INT);
  config_setting_set_int(setting, timer_settings_ccc.usb_bulk_reset_enabled);

  setting = config_setting_add(group, "usb_soft_reset_enabled", CONFIG_TYPE_INT);
  config_setting_set_int(setting, timer_settings_ccc.usb_soft_reset_enabled);

  setting = config_setting_add(group, "usb_hard_reset_enabled", CONFIG_TYPE_INT);
  config_setting_set_int(setting, timer_settings_ccc.usb_hard_reset_enabled);

  setting = config_setting_add(group, "usb_port_reset_enabled", CONFIG_TYPE_INT);
  config_setting_set_int(setting, timer_settings_ccc.usb_port_reset_enabled);

  // load primary relay settings
  load_primary_relay_settings_ccc();

  // write primary relay settings to config file
  group = config_setting_add(root, "relay", CONFIG_TYPE_GROUP);

  setting = config_setting_add(group, "activate_primary_relay1", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, primary_relay_settings_ccc.activate_primary_relay1);

  setting = config_setting_add(group, "activate_primary_relay2", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, primary_relay_settings_ccc.activate_primary_relay2);

  setting = config_setting_add(group, "activate_primary_relay3", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, primary_relay_settings_ccc.activate_primary_relay3);

  setting = config_setting_add(group, "activate_primary_relay4", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, primary_relay_settings_ccc.activate_primary_relay4);

  setting = config_setting_add(group, "activate_primary_relay5", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, primary_relay_settings_ccc.activate_primary_relay5);

  setting = config_setting_add(group, "activate_primary_relay6", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, primary_relay_settings_ccc.activate_primary_relay6);

  setting = config_setting_add(group, "activate_primary_relay7", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, primary_relay_settings_ccc.activate_primary_relay7);

  setting = config_setting_add(group, "activate_primary_relay8", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, primary_relay_settings_ccc.activate_primary_relay8);

  setting = config_setting_add(group, "deactivate_primary_relay1", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, primary_relay_settings_ccc.deactivate_primary_relay1);

  setting = config_setting_add(group, "deactivate_primary_relay2", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, primary_relay_settings_ccc.deactivate_primary_relay2);

  setting = config_setting_add(group, "deactivate_primary_relay3", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, primary_relay_settings_ccc.deactivate_primary_relay3);

  setting = config_setting_add(group, "deactivate_primary_relay4", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, primary_relay_settings_ccc.deactivate_primary_relay4);

  setting = config_setting_add(group, "deactivate_primary_relay5", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, primary_relay_settings_ccc.deactivate_primary_relay5);

  setting = config_setting_add(group, "deactivate_primary_relay6", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, primary_relay_settings_ccc.deactivate_primary_relay6);

  setting = config_setting_add(group, "deactivate_primary_relay7", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, primary_relay_settings_ccc.deactivate_primary_relay7);

  setting = config_setting_add(group, "deactivate_primary_relay8", CONFIG_TYPE_BOOL);
  config_setting_set_bool(setting, primary_relay_settings_ccc.deactivate_primary_relay8);

  setting = config_setting_add(group, "primary_relay_activation_time", CONFIG_TYPE_INT64);
  config_setting_set_int64(setting, primary_relay_settings_ccc.primary_relay_activation_time);

  setting = config_setting_add(group, "primary_relay_delay_time", CONFIG_TYPE_INT64);
  config_setting_set_int64(setting, primary_relay_settings_ccc.primary_relay_delay_time);

  setting = config_setting_add(group, "primary_relay_board_type", CONFIG_TYPE_INT);
  config_setting_set_int(setting, primary_relay_settings_ccc.primary_relay_board_type);

  setting = config_setting_add(group, "asclepius_relay_source", CONFIG_TYPE_INT);
  config_setting_set_int(setting, primary_relay_settings_ccc.asclepius_relay_source);

  setting = config_setting_add(group, "asclepius_tty_device", CONFIG_TYPE_STRING);
  config_setting_set_string(setting, primary_relay_settings_ccc.asclepius_tty_device);

  setting = config_setting_add(group, "asclepius_ext_tty_device", CONFIG_TYPE_STRING);
  config_setting_set_string(setting, primary_relay_settings_ccc.asclepius_ext_tty_device);

  setting = config_setting_add(group, "asclepius_ext_baud_rate", CONFIG_TYPE_INT);
  config_setting_set_int(setting, primary_relay_settings_ccc.asclepius_ext_baud_rate);

  setting = config_setting_add(group, "asclepius_poll_interval_ms", CONFIG_TYPE_INT);
  config_setting_set_int(setting, (int)primary_relay_settings_ccc.asclepius_poll_interval_ms);

  setting = config_setting_add(group, "asclepius_ocp_action_12v", CONFIG_TYPE_INT);
  config_setting_set_int(setting, primary_relay_settings_ccc.asclepius_ocp_action_12v);

  setting = config_setting_add(group, "asclepius_ocp_action_5v", CONFIG_TYPE_INT);
  config_setting_set_int(setting, primary_relay_settings_ccc.asclepius_ocp_action_5v);

  setting = config_setting_add(group, "asclepius_ocp_action_usb", CONFIG_TYPE_INT);
  config_setting_set_int(setting, primary_relay_settings_ccc.asclepius_ocp_action_usb);

  setting = config_setting_add(group, "asclepius_ocp_limit_12v", CONFIG_TYPE_INT);
  config_setting_set_int(setting, (int)primary_relay_settings_ccc.asclepius_ocp_limit_12v);

  setting = config_setting_add(group, "asclepius_ocp_limit_5v", CONFIG_TYPE_INT);
  config_setting_set_int(setting, (int)primary_relay_settings_ccc.asclepius_ocp_limit_5v);

  setting = config_setting_add(group, "asclepius_ocp_limit_usb", CONFIG_TYPE_INT);
  config_setting_set_int(setting, (int)primary_relay_settings_ccc.asclepius_ocp_limit_usb);

  // setting = config_setting_add(group, "primary_relay_name", CONFIG_TYPE_STRING);
  // config_setting_set_string(setting, primary_relay_settings_ccc.primary_relay_name);

  config_write(&config, config_file);

  config_destroy(&config);

  fclose(config_file);
}

void write_config_file_ccc(void)
{
  GtkWidget *dialog;
  dialog = gtk_file_chooser_dialog_new(_("Save Settings File As"),
                                        GTK_WINDOW(main_window_ccc),
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                        NULL);

  gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), template_directory);

  char defaultfilename[1024];
  snprintf(defaultfilename, sizeof(defaultfilename), "%s.cfg", current_source_model_ccc);
  gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), defaultfilename);
  
  gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
  {
    char *filename;
    filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

    write_config_file_with_name_ccc(filename);

    g_free(filename);

    gtk_widget_destroy(dialog);
  }
}

void check_source_against_templates_ccc(void)
{
  // check against an exact model name first
  char filename[1024];
  snprintf(filename, 1024, "%s%s.cfg", template_directory, current_source_model_ccc);

  if (access(filename, F_OK) != -1)
  {
    GtkWidget *dialog;
    dialog = gtk_message_dialog_new(GTK_WINDOW(main_window_ccc),
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_QUESTION,
                                    GTK_BUTTONS_YES_NO,
                                    _("Source was detected as %s.\nLoad optimized settings for this drive?"), current_source_model_ccc);
    gint result = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    if (result == GTK_RESPONSE_YES)
    {
      read_config_file_with_name_ccc(filename);

      snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Optimized settings loaded for %s\n"), current_source_model_ccc);
      message_now_ccc(tempmessage_ccc);
    }
  }
  // check if the drive is an SSD
  else if (identify_device_data_ccc.solid_state_drive)
  {
    GtkWidget *dialog;
    dialog = gtk_message_dialog_new(GTK_WINDOW(main_window_ccc),
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_QUESTION,
                                    GTK_BUTTONS_YES_NO,
                                    _("Source was detected as a solid state drive.\nLoad optimized settings for SSDs?"));
    gint result = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    if (result == GTK_RESPONSE_YES)
    {
      char filename[1024];
      snprintf(filename, 1024, "%s%s", template_directory, default_ssd_config_filename);

      read_config_file_with_name_ccc(filename);

      snprintf(tempmessage_ccc, TEMP_MESSAGE_SIZE, _("Optimized settings loaded for SSDs\n"));
      message_now_ccc(tempmessage_ccc);
    }
  }
}

// -----------------------------------------------------------------------
// Ext Terminal (UART) dialog
// -----------------------------------------------------------------------

static const int ext_terminal_baud_rates[] = {9600, 19200, 38400, 57600, 115200, 230400, 921600};
#define EXT_TERMINAL_NUM_BAUDS (sizeof(ext_terminal_baud_rates) / sizeof(ext_terminal_baud_rates[0]))

// How received bytes are shown. Auto picks per chunk, which is what the
// terminal always did; pinning Text or Hex is what stops it appearing to
// change representation underneath you.
typedef enum
{
  EXT_VIEW_TEXT = 0,
  EXT_VIEW_HEX = 1,
  EXT_VIEW_AUTO = 2
} ext_view_t;

// How the send entry is interpreted. Hex mode takes whitespace separated byte
// pairs; Text mode takes the escape sequences.
typedef enum
{
  EXT_SEND_TEXT = 0,
  EXT_SEND_HEX = 1
} ext_send_mode_t;

typedef enum
{
  EXT_EOL_NONE = 0,
  EXT_EOL_LF = 1,
  EXT_EOL_CR = 2,
  EXT_EOL_CRLF = 3
} ext_eol_t;

#define EXT_TAG_TS    "ext_tag_ts"
#define EXT_TAG_INF   "ext_tag_inf"
#define EXT_TAG_SND   "ext_tag_snd"
#define EXT_TAG_RCV   "ext_tag_rcv"
#define EXT_TAG_ERR   "ext_tag_err"
#define EXT_TAG_SCRIPT "ext_tag_script"
#define EXT_TERMINAL_MAX_LOG_CHARS (1024 * 1024)
#define EXT_TERMINAL_HISTORY_MAX 32

static const char *ext_prefix_tag(const char *prefix)
{
  if (strncmp(prefix, "INF:", 4) == 0) return EXT_TAG_INF;
  if (strncmp(prefix, "SND:", 4) == 0) return EXT_TAG_SND;
  if (strncmp(prefix, "RCV:", 4) == 0) return EXT_TAG_RCV;
  if (strncmp(prefix, "ERR:", 4) == 0) return EXT_TAG_ERR;
  return NULL;
}

static void ext_maybe_scroll_to_mark(GtkWidget *textview, GtkTextMark *mark)
{
  GtkWidget *dialog = gtk_widget_get_toplevel(textview);
  if (GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(dialog), "ext_autoscroll")))
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(textview), mark, 0.0, FALSE, 0.0, 0.0);
}

static void ext_trim_terminal_log(GtkTextBuffer *buf)
{
  int count = gtk_text_buffer_get_char_count(buf);

  if(count <= EXT_TERMINAL_MAX_LOG_CHARS)
  {
    return;
  }

  GtkTextIter start;
  GtkTextIter cut;
  gtk_text_buffer_get_start_iter(buf, &start);
  cut = start;
  gtk_text_iter_forward_chars(&cut, count - EXT_TERMINAL_MAX_LOG_CHARS);
  gtk_text_buffer_delete(buf, &start, &cut);
}

static void append_ext_terminal_text(GtkWidget *textview, const char *text, const char *tag_name)
{
  GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
  GtkTextIter iter;
  gtk_text_buffer_get_iter_at_mark(buf, &iter, gtk_text_buffer_get_insert(buf));
  gtk_text_buffer_insert_with_tags_by_name(buf, &iter, text, -1, tag_name, NULL);
  GtkTextMark *mark = gtk_text_buffer_get_insert(buf);
  ext_trim_terminal_log(buf);
  ext_maybe_scroll_to_mark(textview, mark);
}

static void append_ext_terminal_line(GtkWidget *textview, const char *prefix, const char *data)
{
  char timestamp[32];
  time_t now = time(NULL);
  strftime(timestamp, sizeof(timestamp), "%H:%M:%S", localtime(&now));

  GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
  GtkTextIter iter;
  gtk_text_buffer_get_iter_at_mark(buf, &iter, gtk_text_buffer_get_insert(buf));

  gtk_text_buffer_insert_with_tags_by_name(buf, &iter, timestamp, -1, EXT_TAG_TS, NULL);
  gtk_text_buffer_insert(buf, &iter, " ", 1);

  const char *tag = ext_prefix_tag(prefix);
  if (tag)
    gtk_text_buffer_insert_with_tags_by_name(buf, &iter, prefix, -1, tag, NULL);
  else
    gtk_text_buffer_insert(buf, &iter, prefix, -1);

  gtk_text_buffer_insert(buf, &iter, " ", 1);
  gtk_text_buffer_insert(buf, &iter, data, -1);
  gtk_text_buffer_insert(buf, &iter, "\n", 1);

  GtkTextMark *mark = gtk_text_buffer_get_insert(buf);
  ext_trim_terminal_log(buf);
  ext_maybe_scroll_to_mark(textview, mark);
}

static void update_ext_status(GtkWidget *dialog)
{
  GtkWidget *label = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "ext_status_label"));
  if (!label) return;
  unsigned int rx = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(dialog), "ext_rx_count"));
  unsigned int tx = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(dialog), "ext_tx_count"));
  char text[320];
  if (asclepius_ext_connected)
  {
    snprintf(text, sizeof(text), _("%s @ %d  |  RX: %u  TX: %u"),
             asclepius_ext_tty_device_ccc, asclepius_ext_baud_rate_ccc, rx, tx);
  }
  else
  {
    snprintf(text, sizeof(text), _("Disconnected  |  RX: %u  TX: %u"), rx, tx);
  }
  gtk_label_set_text(GTK_LABEL(label), text);
}

static gboolean on_ext_terminal_entry_key_press(GtkWidget *entry, GdkEventKey *event, gpointer data)
{
  GtkWidget *dialog = GTK_WIDGET(data);
  GPtrArray *history = g_object_get_data(G_OBJECT(dialog), "ext_command_history");
  guint history_index;

  if(history == NULL || history->len == 0)
  {
    return FALSE;
  }

  history_index = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(dialog), "ext_history_index"));

  if(event->keyval == GDK_KEY_Up)
  {
    if(history_index > 0)
    {
      history_index--;
    }
    gtk_entry_set_text(GTK_ENTRY(entry), g_ptr_array_index(history, history_index));
    gtk_editable_set_position(GTK_EDITABLE(entry), -1);
    g_object_set_data(G_OBJECT(dialog), "ext_history_index", GUINT_TO_POINTER(history_index));
    return TRUE;
  }

  if(event->keyval == GDK_KEY_Down)
  {
    if(history_index + 1 < history->len)
    {
      history_index++;
      gtk_entry_set_text(GTK_ENTRY(entry), g_ptr_array_index(history, history_index));
    }
    else
    {
      history_index = history->len;
      gtk_entry_set_text(GTK_ENTRY(entry), "");
    }
    gtk_editable_set_position(GTK_EDITABLE(entry), -1);
    g_object_set_data(G_OBJECT(dialog), "ext_history_index", GUINT_TO_POINTER(history_index));
    return TRUE;
  }

  return FALSE;
}

static void ext_terminal_history_add(GtkWidget *dialog, const char *text)
{
  GPtrArray *history = g_object_get_data(G_OBJECT(dialog), "ext_command_history");

  if(history == NULL || text == NULL || text[0] == '\0')
  {
    return;
  }

  if(history->len > 0 && strcmp(g_ptr_array_index(history, history->len - 1), text) == 0)
  {
    g_object_set_data(G_OBJECT(dialog), "ext_history_index", GUINT_TO_POINTER(history->len));
    return;
  }

  if(history->len >= EXT_TERMINAL_HISTORY_MAX)
  {
    g_ptr_array_remove_index(history, 0);
  }

  g_ptr_array_add(history, g_strdup(text));
  g_object_set_data(G_OBJECT(dialog), "ext_history_index", GUINT_TO_POINTER(history->len));
}

void on_ext_terminal_send_byte_ccc(GtkWidget *button)
{
  GtkWidget *dialog = GTK_WIDGET(gtk_widget_get_toplevel(button));
  GtkWidget *textview = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "ext_terminal_textview"));
  if (!asclepius_ext_connected)
  {
    append_ext_terminal_line(textview, "ERR:", "Not connected");
    return;
  }

  unsigned int byte = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "ext_byte_value"));
  const char *display = (const char *)g_object_get_data(G_OBJECT(button), "ext_display_text");

  unsigned char b = (unsigned char)byte;
  if (asclepius_ext_send((const char *)&b, 1) != 0)
  {
    append_ext_terminal_line(textview, "ERR:", "Send failed");
    return;
  }

  append_ext_terminal_line(textview, "SND:", display);

  unsigned int tx = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(dialog), "ext_tx_count"));
  tx++;
  g_object_set_data(G_OBJECT(dialog), "ext_tx_count", GUINT_TO_POINTER(tx));
  update_ext_status(dialog);
}

void on_ext_terminal_clear_ccc(GtkWidget *button)
{
  GtkWidget *dialog = GTK_WIDGET(gtk_widget_get_toplevel(button));
  GtkWidget *textview = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "ext_terminal_textview"));
  GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
  gtk_text_buffer_set_text(buf, "", -1);
  g_object_set_data(G_OBJECT(dialog), "ext_rx_count", GUINT_TO_POINTER(0));
  g_object_set_data(G_OBJECT(dialog), "ext_tx_count", GUINT_TO_POINTER(0));
  update_ext_status(dialog);
}

void on_ext_terminal_save_ccc(GtkWidget *button)
{
  GtkWidget *dialog = GTK_WIDGET(gtk_widget_get_toplevel(button));
  GtkWidget *textview = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "ext_terminal_textview"));
  GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));

  GtkWidget *fc = gtk_file_chooser_dialog_new(
      _("Save Log"),
      GTK_WINDOW(dialog),
      GTK_FILE_CHOOSER_ACTION_SAVE,
      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
      GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
      NULL);

  {
    char default_name[64];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    strftime(default_name, sizeof(default_name), "serial_log_%Y%m%d_%H%M%S.txt", tm);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(fc), default_name);
  }

  if (gtk_dialog_run(GTK_DIALOG(fc)) == GTK_RESPONSE_ACCEPT)
  {
    char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc));
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buf, &start, &end);
    char *content = gtk_text_buffer_get_text(buf, &start, &end, FALSE);

    FILE *f = fopen(filename, "w");
    if (f)
    {
      fputs(content, f);
      fclose(f);
    }
    else
    {
      GtkWidget *err = gtk_message_dialog_new(
          GTK_WINDOW(dialog),
          GTK_DIALOG_DESTROY_WITH_PARENT,
          GTK_MESSAGE_ERROR,
          GTK_BUTTONS_OK,
          _("Failed to save log:\n%s"), strerror(errno));
      gtk_dialog_run(GTK_DIALOG(err));
      gtk_widget_destroy(err);
    }

    g_free(content);
    g_free(filename);
  }
  gtk_widget_destroy(fc);
}

void on_ext_terminal_autoscroll_toggled_ccc(GtkToggleButton *button)
{
  GtkWidget *dialog = GTK_WIDGET(gtk_widget_get_toplevel(GTK_WIDGET(button)));
  int active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
  g_object_set_data(G_OBJECT(dialog), "ext_autoscroll", GUINT_TO_POINTER(active ? 1 : 0));
}

void on_ext_terminal_pause_toggled_ccc(GtkToggleButton *button)
{
  GtkWidget *dialog = GTK_WIDGET(gtk_widget_get_toplevel(GTK_WIDGET(button)));
  gboolean paused = gtk_toggle_button_get_active(button);

  g_object_set_data(G_OBJECT(dialog), "ext_display_paused", GUINT_TO_POINTER(paused ? 1 : 0));
  gtk_button_set_label(GTK_BUTTON(button), paused ? _("Resume Display") : _("Pause Display"));
}

// Describe the bytes that were actually sent. Echoing what was typed is
// misleading once Ctrl or hex mode is involved: typing "c" with Ctrl on sends
// 0x03, not a letter.
static void ext_terminal_format_payload(const char *data, size_t len, char *out, size_t out_size)
{
  size_t at = 0;
  bool printable = len > 0;

  for (size_t i = 0; i < len; i++)
  {
    unsigned char c = (unsigned char)data[i];

    if (c < 0x20 || c >= 0x7F)
    {
      printable = false;
      break;
    }
  }

  if (len == 0)
  {
    snprintf(out, out_size, "(nothing)");
    return;
  }

  if (printable)
  {
    size_t show = len < out_size - 1 ? len : out_size - 1;
    memcpy(out, data, show);
    out[show] = '\0';
    return;
  }

  out[0] = '\0';
  for (size_t i = 0; i < len && at + 4 < out_size; i++)
  {
    at += (size_t)snprintf(out + at, out_size - at, "%02x ", (unsigned char)data[i]);
  }
}

void on_ext_terminal_send_ccc(GtkWidget *sender)
{
  GtkWidget *dialog = GTK_WIDGET(gtk_widget_get_toplevel(sender));
  GtkWidget *textview = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "ext_terminal_textview"));
  GtkWidget *entry = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "ext_terminal_entry"));
  GtkWidget *mode_combo = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "ext_terminal_send_mode_combo"));
  GtkWidget *eol_combo = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "ext_terminal_eol_combo"));
  GtkWidget *ctrl_check = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "ext_terminal_ctrl_check"));

  const char *text = gtk_entry_get_text(GTK_ENTRY(entry));

  if (!asclepius_ext_connected)
  {
    append_ext_terminal_line(textview, "ERR:", _("Not connected"));
    return;
  }

  int mode = mode_combo ? gtk_combo_box_get_active(GTK_COMBO_BOX(mode_combo)) : EXT_SEND_TEXT;
  bool ctrl = ctrl_check && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctrl_check));

  char parsed[8192];
  int len;

  if (mode == EXT_SEND_HEX)
  {
    len = asclepius_parse_hex(text, parsed, sizeof(parsed));
    if (len < 0)
    {
      append_ext_terminal_line(textview, "ERR:", _("Not valid hex: expected byte pairs, for example 48 65 6c 6c 6f"));
      return;
    }
  }
  else
  {
    len = asclepius_parse_escapes(text, parsed, sizeof(parsed));

    if (ctrl)
    {
      // Ctrl-A..Ctrl-Z become 0x01..0x1A, which is how a real terminal
      // delivers them and what devices expect for ^C, ^Z and friends.
      for (int i = 0; i < len; i++)
      {
        parsed[i] = (char)asclepius_ctrl_byte((unsigned char)parsed[i]);
      }
    }
  }

  int eol = eol_combo ? gtk_combo_box_get_active(GTK_COMBO_BOX(eol_combo)) : EXT_EOL_NONE;
  if(eol < EXT_EOL_NONE || eol > EXT_EOL_CRLF)
  {
    eol = EXT_EOL_NONE;
  }

  if(eol == EXT_EOL_LF || eol == EXT_EOL_CR || eol == EXT_EOL_CRLF)
  {
    if(len >= (int)sizeof(parsed) - (eol == EXT_EOL_CRLF ? 2 : 1))
    {
      append_ext_terminal_line(textview, "ERR:", _("Command is too long with the selected line ending"));
      return;
    }

    if(eol == EXT_EOL_LF)
    {
      parsed[len++] = '\n';
    }
    else if(eol == EXT_EOL_CR)
    {
      parsed[len++] = '\r';
    }
    else
    {
      parsed[len++] = '\r';
      parsed[len++] = '\n';
    }
  }

  if (len == 0)
  {
    append_ext_terminal_line(textview, "ERR:", _("Nothing to send"));
    return;
  }

  if (asclepius_ext_send(parsed, len) != 0)
  {
    append_ext_terminal_line(textview, "ERR:", _("Send failed"));
    return;
  }

  ext_terminal_history_add(dialog, text);
  char shown[512];
  ext_terminal_format_payload(parsed, (size_t)len, shown, sizeof(shown));
  append_ext_terminal_line(textview, "SND:", shown);

  unsigned int tx = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(dialog), "ext_tx_count"));
  tx += (unsigned int)len;
  g_object_set_data(G_OBJECT(dialog), "ext_tx_count", GUINT_TO_POINTER(tx));
  update_ext_status(dialog);
  gtk_entry_set_text(GTK_ENTRY(entry), "");
}

// One assembled line from the receive buffer, ready to display.
static void ext_terminal_on_line(const char *line, size_t line_len, void *user)
{
  (void)line_len;
  GtkWidget *dialog = GTK_WIDGET(user);
  GtkWidget *textview = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "ext_terminal_textview"));

  if (textview && line_len > 0)
  {
    append_ext_terminal_line(textview, "RCV:", line);
  }
}

static int ext_terminal_view_mode(GtkWidget *dialog)
{
  GtkWidget *combo = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "ext_terminal_view_combo"));
  int active = combo ? gtk_combo_box_get_active(GTK_COMBO_BOX(combo)) : EXT_VIEW_AUTO;

  return (active < 0) ? EXT_VIEW_AUTO : active;
}

static bool ext_terminal_chunk_is_text(const char *data, size_t len)
{
  size_t i;

  for (i = 0; i < len; i++)
  {
    unsigned char c = (unsigned char)data[i];

    if (c == '\n' || c == '\r' || c == '\t')
    {
      continue;
    }

    if (c < 0x20 || c >= 0x7F)
    {
      return false;
    }
  }

  return true;
}

static void ext_terminal_show_hex(GtkWidget *dialog, const char *data, size_t len)
{
  GtkWidget *textview = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "ext_terminal_textview"));
  unsigned long offset = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(dialog), "ext_hex_offset"));
  size_t at = 0;

  if (!textview || len == 0)
  {
    return;
  }

  while (at < len)
  {
    char line[256];
    size_t take = len - at;

    if (take > ASCLEPIUS_HEX_PER_LINE)
    {
      take = ASCLEPIUS_HEX_PER_LINE;
    }

    asclepius_format_hex_line(line, sizeof(line), (const unsigned char *)data + at, take, offset);
    append_ext_terminal_line(textview, "RCV:", line);

    at += take;
    offset += (unsigned long)take;
  }

  g_object_set_data(G_OBJECT(dialog), "ext_hex_offset", GUINT_TO_POINTER((guint)offset));
}

static void ext_terminal_render(GtkWidget *dialog, const char *data, size_t len)
{
  asclepius_linebuf_t *lb = g_object_get_data(G_OBJECT(dialog), "ext_linebuf");
  int mode = ext_terminal_view_mode(dialog);
  bool want_hex;
  bool was_hex = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(dialog), "ext_was_hex")) ? true : false;

  if (mode == EXT_VIEW_HEX)
  {
    want_hex = true;
  }
  else if (mode == EXT_VIEW_TEXT)
  {
    want_hex = false;
  }
  else
  {
    want_hex = !ext_terminal_chunk_is_text(data, len);
  }

  // A mode switch mid-message would show a half-line: settle whatever was held
  // before changing representation.
  if (want_hex != was_hex && lb != NULL)
  {
    asclepius_linebuf_flush(lb, ext_terminal_on_line, dialog);
  }

  if (want_hex)
  {
    ext_terminal_show_hex(dialog, data, len);
  }
  else if (lb != NULL)
  {
    asclepius_linebuf_feed(lb, data, len, ext_terminal_on_line, dialog);
  }

  g_object_set_data(G_OBJECT(dialog), "ext_was_hex", GUINT_TO_POINTER(want_hex ? 1 : 0));
}

gboolean on_ext_terminal_poll_ccc(gpointer data)
{
  GtkWidget *dialog = GTK_WIDGET(data);
  if (!asclepius_ext_connected)
  {
    return G_SOURCE_CONTINUE;
  }

  char buf[4096];
  int n = asclepius_ext_read(buf, sizeof(buf));
  if (n < 0)
  {
    GtkWidget *textview = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "ext_terminal_textview"));
    GtkWidget *connect_button = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "ext_terminal_connect_button"));

    append_ext_terminal_line(textview, "ERR:", _("Connection lost while reading"));
    asclepius_ext_close();

    if (connect_button != NULL)
    {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(connect_button), FALSE);
    }
    else
    {
      update_ext_status(dialog);
    }

    return G_SOURCE_CONTINUE;
  }

  if (n > 0)
  {
    if (!GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(dialog), "ext_display_paused")))
    {
      ext_terminal_render(dialog, buf, (size_t)n);
    }

    unsigned int rx = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(dialog), "ext_rx_count"));
    rx += (unsigned int)n;
    g_object_set_data(G_OBJECT(dialog), "ext_rx_count", GUINT_TO_POINTER(rx));
    update_ext_status(dialog);
  }

  return G_SOURCE_CONTINUE;
}

void on_ext_terminal_connect_toggled_ccc(GtkWidget *button)
{
  GtkWidget *dialog = GTK_WIDGET(gtk_widget_get_toplevel(button));

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
  {
    GtkWidget *tty_text = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "ext_terminal_tty_text"));
    GtkWidget *baud_combo = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "ext_terminal_baud_combo"));

    if (tty_text)
    {
      const char *path = gtk_entry_get_text(GTK_ENTRY(tty_text));
      snprintf(asclepius_ext_tty_device_ccc, sizeof(asclepius_ext_tty_device_ccc), "%s", path);
    }

    if (baud_combo)
    {
      int idx = gtk_combo_box_get_active(GTK_COMBO_BOX(baud_combo));
      if (idx >= 0 && idx < (int)EXT_TERMINAL_NUM_BAUDS)
      {
        asclepius_ext_baud_rate_ccc = ext_terminal_baud_rates[idx];
      }
    }

    // Sync struct for config persistence
    primary_relay_settings_ccc.asclepius_ext_baud_rate = asclepius_ext_baud_rate_ccc;
    snprintf(primary_relay_settings_ccc.asclepius_ext_tty_device, sizeof(primary_relay_settings_ccc.asclepius_ext_tty_device), "%s", asclepius_ext_tty_device_ccc);

    if (asclepius_ext_open() == 0)
    {
      gtk_button_set_label(GTK_BUTTON(button), _("Disconnect"));
      GtkWidget *textview = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "ext_terminal_textview"));
      GtkWidget *script_btn = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "ext_terminal_script_button"));
      if (script_btn) gtk_widget_set_sensitive(script_btn, TRUE);
      guint old_tid = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(dialog), "ext_terminal_timer"));
      if (!old_tid)
      {
        guint timer_id = gdk_threads_add_timeout(100, on_ext_terminal_poll_ccc, dialog);
        g_object_set_data(G_OBJECT(dialog), "ext_terminal_timer", GUINT_TO_POINTER(timer_id));
      }
      append_ext_terminal_line(textview, "INF:", "Connected");
      update_ext_status(dialog);
    }
    else
    {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
      GtkWidget *textview = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "ext_terminal_textview"));
      append_ext_terminal_line(textview, "ERR:", "Failed to connect");
      update_ext_status(dialog);
    }
  }
  else
  {
    guint tid = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(dialog), "ext_terminal_timer"));
    if (tid)
    {
      g_source_remove(tid);
      g_object_set_data(G_OBJECT(dialog), "ext_terminal_timer", GUINT_TO_POINTER(0));
    }
    asclepius_ext_close();
    // The stream stopped, so any half-received message will never be
    // completed: show it rather than dropping it silently.
    {
      asclepius_linebuf_t *lb = g_object_get_data(G_OBJECT(dialog), "ext_linebuf");
      if (lb)
      {
        asclepius_linebuf_flush(lb, ext_terminal_on_line, dialog);
      }
    }
    gtk_button_set_label(GTK_BUTTON(button), _("Connect"));
    GtkWidget *textview = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "ext_terminal_textview"));
    GtkWidget *script_btn = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "ext_terminal_script_button"));
    if (script_btn) gtk_widget_set_sensitive(script_btn, FALSE);
    append_ext_terminal_line(textview, "INF:", "Disconnected");
    update_ext_status(dialog);
  }
}

void on_ext_terminal_script_clicked_ccc(GtkWidget *button)
{
  GtkWidget *dialog = GTK_WIDGET(gtk_widget_get_toplevel(button));
  GtkWidget *textview = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "ext_terminal_textview"));

  GtkWidget *filechooser = gtk_file_chooser_dialog_new(
      _("Select Script"),
      GTK_WINDOW(dialog),
      GTK_FILE_CHOOSER_ACTION_OPEN,
      _("_Cancel"), GTK_RESPONSE_CANCEL,
      _("_Open"), GTK_RESPONSE_ACCEPT,
      NULL);
  gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(filechooser), OSC_SCRIPT_PATH);

  if (gtk_dialog_run(GTK_DIALOG(filechooser)) == GTK_RESPONSE_ACCEPT)
  {
    char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(filechooser));

    char line[1024];
    snprintf(line, sizeof(line), "Running script: %s\n", filename);
    append_ext_terminal_text(textview, line, EXT_TAG_SCRIPT);

    // Execute script using the tool.c execution loop pattern
    int script_status = 1;
    char file_copy[1024];
    snprintf(file_copy, sizeof(file_copy), "%s", filename);

    while (script_status == 1)
    {
      return_value_ccc = read_script_file_ccc(file_copy);
      script_status = process_lines_ccc();
    }

    if (return_value_ccc == -100 || return_value_ccc == -101 || return_value_ccc == 0)
    {
      snprintf(line, sizeof(line), "Script finished successfully\n");
    }
    else
    {
      snprintf(line, sizeof(line), "Script finished (error: %d)\n", return_value_ccc);
    }
    append_ext_terminal_text(textview, line, EXT_TAG_SCRIPT);

    g_free(filename);
  }
  gtk_widget_destroy(filechooser);
}

// Forward output from script serialsend/serialread to the terminal textview
static GtkWidget *ext_terminal_output_textview = NULL;

static void ext_terminal_output_callback(const char *text, int byte_count)
{
  if (!ext_terminal_output_textview) return;

  GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(ext_terminal_output_textview));
  GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ext_terminal_output_textview));
  GtkTextIter iter;
  gtk_text_buffer_get_iter_at_mark(buf, &iter, gtk_text_buffer_get_insert(buf));

  char timestamp[32];
  time_t now = time(NULL);
  strftime(timestamp, sizeof(timestamp), "%H:%M:%S", localtime(&now));
  gtk_text_buffer_insert_with_tags_by_name(buf, &iter, timestamp, -1, EXT_TAG_TS, NULL);
  gtk_text_buffer_insert(buf, &iter, " ", 1);

  const char *colon = strchr(text, ':');
  if (colon && colon > text && colon - text <= 5)
  {
    int plen = colon - text + 1;
    char prefix[8];
    snprintf(prefix, sizeof(prefix), "%.*s", plen, text);
    const char *tag = ext_prefix_tag(prefix);
    if (tag)
      gtk_text_buffer_insert_with_tags_by_name(buf, &iter, prefix, -1, tag, NULL);
    else
      gtk_text_buffer_insert(buf, &iter, prefix, plen);
    gtk_text_buffer_insert(buf, &iter, " ", 1);
    gtk_text_buffer_insert(buf, &iter, text + plen, -1);

    // Update RX/TX counters using tag from ext_prefix_tag
    if (tag && byte_count > 0)
    {
      if (strcmp(tag, EXT_TAG_SND) == 0)
      {
        unsigned int tx = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(dialog), "ext_tx_count"));
        tx += byte_count;
        g_object_set_data(G_OBJECT(dialog), "ext_tx_count", GUINT_TO_POINTER(tx));
        update_ext_status(dialog);
      }
      else if (strcmp(tag, EXT_TAG_RCV) == 0)
      {
        unsigned int rx = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(dialog), "ext_rx_count"));
        rx += byte_count;
        g_object_set_data(G_OBJECT(dialog), "ext_rx_count", GUINT_TO_POINTER(rx));
        update_ext_status(dialog);
      }
    }
  }
  else
  {
    gtk_text_buffer_insert(buf, &iter, text, -1);
  }

  gtk_text_buffer_insert(buf, &iter, "\n", 1);

  if (GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(dialog), "ext_autoscroll")))
  {
    GtkTextMark *mark = gtk_text_buffer_get_insert(buf);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(ext_terminal_output_textview), mark, 0.0, FALSE, 0.0, 0.0);
  }
}

// The dialog is kept alive when it is closed and only hidden, so the log text,
// command history, RX/TX counters, selector choices and an open serial session
// all survive a close/reopen. This mirrors the monitor window, whose samples
// live outside the dialog and keep accumulating while it is closed.
static GtkWidget *ext_terminal_dialog_ccc = NULL;

void on_ext_terminal_dialog_response_ccc(GtkDialog *dialog, gint response_id, gpointer user_data)
{
  (void)response_id;
  (void)user_data;

  gtk_widget_hide(GTK_WIDGET(dialog));
}

// Window-manager close. Returning TRUE keeps the dialog alive for the next
// open instead of letting GtkDialog destroy it.
static gboolean on_ext_terminal_delete_ccc(GtkWidget *widget, GdkEvent *event, gpointer data)
{
  (void)event;
  (void)data;

  gtk_widget_hide(widget);
  return TRUE;
}

// Only reached when the dialog is really destroyed (application teardown).
static void on_ext_terminal_destroy_ccc(GtkWidget *widget, gpointer data)
{
  (void)data;

  guint tid = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(widget), "ext_terminal_timer"));
  if (tid) g_source_remove(tid);

  ext_terminal_dialog_ccc = NULL;
  asclepius_ext_output_ccc = NULL;
  ext_terminal_output_textview = NULL;
  asclepius_ext_close();
}

// The Asclepius Monitor window lives in its own translation unit; GTK finds
// this handler through the prototype in clone_gui.h.
void open_asclepius_monitor_dialog_ccc(void);

// Port picker, shared by the control-port and passthrough entries. Lists the
// enumerated candidates with a Refresh button; picking one writes it into the
// entry the button belongs to. The entry itself stays editable, so a path that
// enumeration does not match can still be typed.
static void asclepius_refresh_port_list(GtkWidget *list, GtkWidget *status)
{
  char names[ASCLEPIUS_PORT_LIST_MAX][ASCLEPIUS_PORT_NAME_MAX];

  // Emptied by hand rather than with gtk_list_box_clear(), which is newer than
  // the GTK this project builds against.
  GList *kids = gtk_container_get_children(GTK_CONTAINER(list));
  for(GList *k = kids; k != NULL; k = k->next)
  {
    gtk_widget_destroy(GTK_WIDGET(k->data));
  }
  g_list_free(kids);

  int found = asclepius_list_serial_ports(names, ASCLEPIUS_PORT_LIST_MAX);
  int shown = found < ASCLEPIUS_PORT_LIST_MAX ? found : ASCLEPIUS_PORT_LIST_MAX;

  for(int i = 0; i < shown; i++)
  {
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *label = gtk_label_new(names[i]);

    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_container_add(GTK_CONTAINER(row), label);
    gtk_list_box_insert(GTK_LIST_BOX(list), GTK_WIDGET(row), -1);
  }

  g_object_set_data(G_OBJECT(list), "ext_port_count", GUINT_TO_POINTER(shown));

  if(status != NULL)
  {
    char text[160];
    if(found == 0)
    {
      snprintf(text, sizeof(text), "%s", _("No serial ports found"));
    }
    else if(found > shown)
    {
      snprintf(text, sizeof(text), _("Showing %d of %d ports"), shown, found);
    }
    else
    {
      snprintf(text, sizeof(text), _("%d port(s) found"), found);
    }
    gtk_label_set_text(GTK_LABEL(status), text);
  }
}

static void on_ports_refresh_clicked_ccc(GtkWidget *button)
{
  GtkWidget *dialog = GTK_WIDGET(gtk_widget_get_toplevel(button));
  GtkWidget *list = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "asclepius_ports_list"));
  GtkWidget *status = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "asclepius_ports_status"));

  asclepius_refresh_port_list(list, status);
}

static void on_ports_row_activated_ccc(GtkWidget *row, gpointer data)
{
  (void)data;

  GtkWidget *dialog = GTK_WIDGET(gtk_widget_get_toplevel(row));
  GtkWidget *entry = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "asclepius_ports_target"));
  GtkWidget *child = gtk_bin_get_child(GTK_BIN(row));
  const char *text = child ? gtk_label_get_text(GTK_LABEL(child)) : NULL;

  if(entry != NULL && text != NULL)
  {
    gtk_entry_set_text(GTK_ENTRY(entry), text);
  }

  gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
}

void on_asclepius_ports_clicked_ccc(GtkWidget *button)
{
  const char *target_key = g_object_get_data(G_OBJECT(button), "asclepius_ports_target_key");

  GtkWidget *parent = GTK_WIDGET(gtk_widget_get_toplevel(button));
  GtkWidget *entry = GTK_WIDGET(g_object_get_data(G_OBJECT(parent), target_key ? target_key : "asclepius_tty_device_text"));

  GtkWidget *dialog = gtk_dialog_new_with_buttons(_("Select Serial Port"), GTK_WINDOW(gtk_widget_get_toplevel(parent)),
                                                  GTK_DIALOG_MODAL,
                                                  _("_Cancel"), GTK_RESPONSE_CANCEL,
                                                  _("_Select"), GTK_RESPONSE_OK,
                                                  NULL);
  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
  GtkWidget *list = gtk_list_box_new();
  GtkWidget *status = gtk_label_new("");

  gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_SINGLE);
  gtk_widget_set_size_request(scroll, 380, 240);
  gtk_label_set_xalign(GTK_LABEL(status), 0.0);

  g_object_set_data(G_OBJECT(dialog), "asclepius_ports_list", list);
  g_object_set_data(G_OBJECT(dialog), "asclepius_ports_status", status);
  g_object_set_data(G_OBJECT(dialog), "asclepius_ports_target", entry);

  gtk_container_add(GTK_CONTAINER(scroll), list);
  gtk_box_pack_start(GTK_BOX(content), scroll, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(content), status, FALSE, FALSE, 4);

  g_signal_connect(list, "row-activated", G_CALLBACK(on_ports_row_activated_ccc), NULL);

  GtkWidget *refresh = gtk_dialog_add_button(GTK_DIALOG(dialog), _("Refresh"), GTK_RESPONSE_APPLY);
  g_signal_connect(refresh, "clicked", G_CALLBACK(on_ports_refresh_clicked_ccc), NULL);

  asclepius_refresh_port_list(list, status);

  while(TRUE)
  {
    int response = gtk_dialog_run(GTK_DIALOG(dialog));

    if(response == GTK_RESPONSE_APPLY)
    {
      asclepius_refresh_port_list(list, status);
      continue;
    }

    if(response == GTK_RESPONSE_OK)
    {
      // Accept whatever row is selected, so the button works as well as
      // double-clicking a row.
      GtkWidget *row = GTK_WIDGET(gtk_list_box_get_selected_row(GTK_LIST_BOX(list)));

      if(row != NULL && entry != NULL)
      {
        GtkWidget *label = gtk_bin_get_child(GTK_BIN(row));

        if(label != NULL)
        {
          gtk_entry_set_text(GTK_ENTRY(entry), gtk_label_get_text(GTK_LABEL(label)));
        }
      }
    }

    break;
  }

  gtk_widget_destroy(dialog);
}

void open_asclepius_ext_terminal_ccc(void)
{
  // A hidden dialog still owns its log, history and serial session: just bring
  // it back instead of rebuilding it from the glade.
  if (ext_terminal_dialog_ccc != NULL)
  {
    gtk_widget_show_all(ext_terminal_dialog_ccc);
    gtk_window_present(GTK_WINDOW(ext_terminal_dialog_ccc));
    return;
  }

  GtkBuilder *builder = gtk_builder_new();
  gtk_builder_add_from_string(builder, (const gchar *)opensuperclone_glade, opensuperclone_glade_len, NULL);

  gtk_builder_connect_signals(builder, NULL);
  GtkWidget *dialog = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_ext_terminal_dialog"));
  if (!dialog) { g_object_unref(builder); return; }

  GtkWidget *textview = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_ext_terminal_textview"));
  GtkWidget *entry = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_ext_entry"));
  GtkWidget *tty_text = GTK_WIDGET(gtk_builder_get_object(builder, "ext_terminal_tty_device_text"));
  GtkWidget *baud_combo = GTK_WIDGET(gtk_builder_get_object(builder, "ext_terminal_baud_rate_combobox"));
  GtkWidget *view_combo = GTK_WIDGET(gtk_builder_get_object(builder, "ext_terminal_view_combobox"));
  GtkWidget *send_mode_combo = GTK_WIDGET(gtk_builder_get_object(builder, "ext_terminal_send_mode_combobox"));
  GtkWidget *eol_combo = GTK_WIDGET(gtk_builder_get_object(builder, "ext_terminal_eol_combobox"));
  GtkWidget *ctrl_check = GTK_WIDGET(gtk_builder_get_object(builder, "ext_terminal_ctrl_checkbutton"));
  GtkWidget *script_btn = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_ext_script_button"));
  GtkWidget *connect_button = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_ext_connect_button"));

  // Store references in dialog
  if (textview) g_object_set_data(G_OBJECT(dialog), "ext_terminal_textview", textview);
  if (entry) g_object_set_data(G_OBJECT(dialog), "ext_terminal_entry", entry);
  if (tty_text) g_object_set_data(G_OBJECT(dialog), "ext_terminal_tty_text", tty_text);
  if (baud_combo) g_object_set_data(G_OBJECT(dialog), "ext_terminal_baud_combo", baud_combo);
  if (view_combo) g_object_set_data(G_OBJECT(dialog), "ext_terminal_view_combo", view_combo);
  if (send_mode_combo) g_object_set_data(G_OBJECT(dialog), "ext_terminal_send_mode_combo", send_mode_combo);
  if (eol_combo) g_object_set_data(G_OBJECT(dialog), "ext_terminal_eol_combo", eol_combo);
  if (ctrl_check) g_object_set_data(G_OBJECT(dialog), "ext_terminal_ctrl_check", ctrl_check);
  if (script_btn) g_object_set_data(G_OBJECT(dialog), "ext_terminal_script_button", script_btn);
  if (connect_button) g_object_set_data(G_OBJECT(dialog), "ext_terminal_connect_button", connect_button);

  if (entry)
  {
    GPtrArray *history = g_ptr_array_new_with_free_func(g_free);
    g_object_set_data_full(G_OBJECT(dialog), "ext_command_history", history, (GDestroyNotify)g_ptr_array_unref);
    g_object_set_data(G_OBJECT(dialog), "ext_history_index", GUINT_TO_POINTER(0));
    g_signal_connect(entry, "key-press-event", G_CALLBACK(on_ext_terminal_entry_key_press), dialog);
  }

  // Held partial lines are owned by the dialog so they are freed with it.
  {
    asclepius_linebuf_t *lb = g_new0(asclepius_linebuf_t, 1);
    asclepius_linebuf_init(lb);
    g_object_set_data_full(G_OBJECT(dialog), "ext_linebuf", lb, g_free);
  }

  if (!textview || !entry) { g_object_unref(builder); return; }

  // Populate baud combo
  if (baud_combo)
  {
    int i;
    for (i = 0; i < (int)EXT_TERMINAL_NUM_BAUDS; i++)
    {
      char buf[16];
      snprintf(buf, sizeof(buf), "%d", ext_terminal_baud_rates[i]);
      gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(baud_combo), buf);
    }
  }

  // View and send mode. Auto is the default for the view, which is how the
  // terminal behaved before these selectors existed; the point is that the
  // choice is now visible and can be pinned instead of the terminal deciding
  // to switch representation mid-stream.
  if (view_combo)
  {
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(view_combo), _("Text"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(view_combo), _("Hex"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(view_combo), _("Auto"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(view_combo), EXT_VIEW_AUTO);
  }
  if (send_mode_combo)
  {
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(send_mode_combo), _("Text"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(send_mode_combo), _("Hex"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(send_mode_combo), EXT_SEND_TEXT);
  }
  if (eol_combo)
  {
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(eol_combo), _("None"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(eol_combo), _("LF"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(eol_combo), _("CR"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(eol_combo), _("CRLF"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(eol_combo), EXT_EOL_NONE);
  }

  // Set current values from globals
  if (tty_text) gtk_entry_set_text(GTK_ENTRY(tty_text), asclepius_ext_tty_device_ccc);
  if (baud_combo)
  {
    int idx = 2;
    int i;
    for (i = 0; i < (int)EXT_TERMINAL_NUM_BAUDS; i++)
    {
      if (ext_terminal_baud_rates[i] == asclepius_ext_baud_rate_ccc)
      {
        idx = i;
        break;
      }
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(baud_combo), idx);
  }

  if (tty_text) gtk_entry_set_max_length(GTK_ENTRY(tty_text), 255);

  // Port picker for the passthrough, same as the control port in the relay
  // settings. Stored under the dialog key the handler looks up.
  {
    GtkWidget *ports_btn = GTK_WIDGET(gtk_builder_get_object(builder, "ext_terminal_ports_button"));
    if (ports_btn)
    {
      g_object_set_data(G_OBJECT(ports_btn), "asclepius_ports_target_key", (gpointer)"ext_terminal_tty_text");
      g_signal_connect(ports_btn, "clicked", G_CALLBACK(on_asclepius_ports_clicked_ccc), NULL);
    }
  }

  // Set monospace font via CSS
  {
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, "textview { font-family: monospace; }", -1, NULL);
    gtk_style_context_add_provider(gtk_widget_get_style_context(textview), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
  }

  // Connect escape sequence button signals (custom data needed per-button)
  {
    static const struct { const char *id; const char *label; unsigned int byte; } esc[] = {
      {"ext_terminal_esc_z", "^Z", 0x1A},
      {"ext_terminal_esc_c", "^C", 0x03},
      {"ext_terminal_esc_l", "^L", 0x0C},
      {"ext_terminal_esc_x", "^X", 0x18},
    };
    int i;
    for (i = 0; i < (int)(sizeof(esc) / sizeof(esc[0])); i++)
    {
      GtkWidget *btn = GTK_WIDGET(gtk_builder_get_object(builder, esc[i].id));
      if (btn)
      {
        g_object_set_data(G_OBJECT(btn), "ext_byte_value", GUINT_TO_POINTER(esc[i].byte));
        g_object_set_data(G_OBJECT(btn), "ext_display_text", (gpointer)esc[i].label);
        g_signal_connect(btn, "clicked", G_CALLBACK(on_ext_terminal_send_byte_ccc), NULL);
      }
    }
  }

  // Store status label reference for RX/TX counter
  {
    GtkWidget *status_label = GTK_WIDGET(gtk_builder_get_object(builder, "ext_terminal_status_label"));
    if (status_label) g_object_set_data(G_OBJECT(dialog), "ext_status_label", status_label);
  }

  g_object_set_data(G_OBJECT(dialog), "ext_autoscroll", GUINT_TO_POINTER(1));
  g_object_set_data(G_OBJECT(dialog), "ext_display_paused", GUINT_TO_POINTER(0));
  g_object_set_data(G_OBJECT(dialog), "ext_rx_count", GUINT_TO_POINTER(0));
  g_object_set_data(G_OBJECT(dialog), "ext_tx_count", GUINT_TO_POINTER(0));

  gtk_window_set_modal(GTK_WINDOW(dialog), FALSE);
  update_ext_status(dialog);

  // Create color tags for terminal output
  {
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
    gtk_text_buffer_create_tag(buf, EXT_TAG_TS, "foreground", "#888888", NULL);
    gtk_text_buffer_create_tag(buf, EXT_TAG_INF, "foreground", "#00BFFF", NULL);
    gtk_text_buffer_create_tag(buf, EXT_TAG_SND, "foreground", "#FFD700", NULL);
    gtk_text_buffer_create_tag(buf, EXT_TAG_RCV, "foreground", "#32CD32", NULL);
    gtk_text_buffer_create_tag(buf, EXT_TAG_ERR, "foreground", "#FF4444", "weight", PANGO_WEIGHT_BOLD, NULL);
    gtk_text_buffer_create_tag(buf, EXT_TAG_SCRIPT, "foreground", "#AAAAAA", "style", PANGO_STYLE_ITALIC, NULL);
  }

  // Route script serialsend/serialread output to this textview
  asclepius_ext_output_ccc = ext_terminal_output_callback;
  ext_terminal_output_textview = textview;

  // Start poll timer
  guint timer_id = gdk_threads_add_timeout(100, on_ext_terminal_poll_ccc, dialog);
  g_object_set_data(G_OBJECT(dialog), "ext_terminal_timer", GUINT_TO_POINTER(timer_id));

  gtk_window_set_title(GTK_WINDOW(dialog), _("Terminal (UART)"));

  ext_terminal_dialog_ccc = dialog;
  g_signal_connect(dialog, "response", G_CALLBACK(on_ext_terminal_dialog_response_ccc), NULL);
  g_signal_connect(dialog, "delete-event", G_CALLBACK(on_ext_terminal_delete_ccc), NULL);
  g_signal_connect(dialog, "destroy", G_CALLBACK(on_ext_terminal_destroy_ccc), NULL);
  gtk_widget_show_all(dialog);
  g_object_unref(builder);
}

// ---------------------------------------------------------------------------
// Western Digital ROYL vendor specific commands
// ---------------------------------------------------------------------------

static GtkWidget *wd_royl_progress_dialog_ccc = NULL;
static GtkWidget *wd_royl_progress_label_ccc = NULL;
static GtkWidget *wd_royl_progress_bar_ccc = NULL;
// Operations started from the SA module editor must parent their progress and
// message dialogs to the editor instead of the main window.
static GtkWindow *wd_royl_dialog_parent_override_ccc = NULL;

static void wd_royl_progress_update_ccc(const char *stage,
                                        unsigned long long done,
                                        unsigned long long total)
{
  double fraction = 0.0;

  if (wd_royl_progress_dialog_ccc == NULL)
  {
    return;
  }
  if (stage != NULL && wd_royl_progress_label_ccc != NULL)
  {
    gtk_label_set_text(GTK_LABEL(wd_royl_progress_label_ccc), stage);
  }
  if (total > 0)
  {
    fraction = (double)done / (double)total;
    if (fraction < 0.0)
    {
      fraction = 0.0;
    }
    if (fraction > 1.0)
    {
      fraction = 1.0;
    }
  }
  if (wd_royl_progress_bar_ccc != NULL)
  {
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(wd_royl_progress_bar_ccc),
                                  fraction);
  }
  while (gtk_events_pending())
  {
    gtk_main_iteration();
  }
}

static void wd_royl_progress_begin_ccc(const char *title, const char *stage)
{
  GtkWidget *content;

  wd_royl_progress_dialog_ccc = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(wd_royl_progress_dialog_ccc), title);
  gtk_window_set_transient_for(
      GTK_WINDOW(wd_royl_progress_dialog_ccc),
      wd_royl_dialog_parent_override_ccc != NULL
          ? wd_royl_dialog_parent_override_ccc
          : GTK_WINDOW(main_window_ccc));
  gtk_window_set_modal(GTK_WINDOW(wd_royl_progress_dialog_ccc), TRUE);
  gtk_window_set_destroy_with_parent(
      GTK_WINDOW(wd_royl_progress_dialog_ccc), TRUE);
  gtk_window_set_resizable(GTK_WINDOW(wd_royl_progress_dialog_ccc), FALSE);
  gtk_window_set_deletable(GTK_WINDOW(wd_royl_progress_dialog_ccc), FALSE);

  content = gtk_dialog_get_content_area(
      GTK_DIALOG(wd_royl_progress_dialog_ccc));
  gtk_container_set_border_width(GTK_CONTAINER(content), 12);
  gtk_box_set_spacing(GTK_BOX(content), 8);

  wd_royl_progress_label_ccc = gtk_label_new(stage != NULL ? stage : "");
  gtk_label_set_line_wrap(GTK_LABEL(wd_royl_progress_label_ccc), TRUE);
  gtk_label_set_xalign(GTK_LABEL(wd_royl_progress_label_ccc), 0.0);
  gtk_box_pack_start(GTK_BOX(content), wd_royl_progress_label_ccc, FALSE,
                     FALSE, 0);

  wd_royl_progress_bar_ccc = gtk_progress_bar_new();
  gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(wd_royl_progress_bar_ccc),
                                 TRUE);
  gtk_box_pack_start(GTK_BOX(content), wd_royl_progress_bar_ccc, FALSE, FALSE,
                     0);

  gtk_widget_show_all(wd_royl_progress_dialog_ccc);
  while (gtk_events_pending())
  {
    gtk_main_iteration();
  }
}

static void wd_royl_progress_end_ccc(void)
{
  if (wd_royl_progress_dialog_ccc != NULL)
  {
    gtk_widget_destroy(wd_royl_progress_dialog_ccc);
    wd_royl_progress_dialog_ccc = NULL;
    wd_royl_progress_label_ccc = NULL;
    wd_royl_progress_bar_ccc = NULL;
    while (gtk_events_pending())
    {
      gtk_main_iteration();
    }
  }
}

static void wd_royl_show_message_ccc(GtkMessageType type, const char *title,
                                     const char *format, ...)
{
  va_list args;
  char *text;
  GtkWidget *dialog;
  GtkWindow *parent = wd_royl_dialog_parent_override_ccc != NULL
                          ? wd_royl_dialog_parent_override_ccc
                          : GTK_WINDOW(main_window_ccc);

  va_start(args, format);
  text = g_strdup_vprintf(format, args);
  va_end(args);

  dialog = gtk_message_dialog_new(parent, GTK_DIALOG_DESTROY_WITH_PARENT,
                                  type, GTK_BUTTONS_OK, "%s", text);
  gtk_window_set_title(GTK_WINDOW(dialog), title);
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
  g_free(text);
}

static bool wd_royl_confirm_ccc(const char *title, const char *accept_label,
                                const char *format, ...)
{
  va_list args;
  char *text;
  GtkWidget *dialog;
  int response;

  va_start(args, format);
  text = g_strdup_vprintf(format, args);
  va_end(args);

  dialog = gtk_message_dialog_new(
      wd_royl_dialog_parent_override_ccc != NULL
          ? wd_royl_dialog_parent_override_ccc
          : GTK_WINDOW(main_window_ccc),
      GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE,
      "%s", text);
  gtk_window_set_title(GTK_WINDOW(dialog), title);
  gtk_dialog_add_buttons(GTK_DIALOG(dialog), GTK_STOCK_CANCEL,
                         GTK_RESPONSE_CANCEL, accept_label,
                         GTK_RESPONSE_ACCEPT, NULL);
  gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
  response = gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
  g_free(text);
  return response == GTK_RESPONSE_ACCEPT;
}

static char *wd_royl_choose_file_ccc(bool save, const char *title,
                                     const char *default_name)
{
  GtkWidget *dialog;
  char *filename = NULL;

  dialog = gtk_file_chooser_dialog_new(
      title, GTK_WINDOW(main_window_ccc),
      save ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN,
      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
      save ? GTK_STOCK_SAVE : GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
  if (save)
  {
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog),
                                                   TRUE);
    if (default_name != NULL)
    {
      gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog),
                                        default_name);
    }
  }
  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
  {
    filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
  }
  gtk_widget_destroy(dialog);
  return filename;
}

static void wd_royl_sanitize_name_ccc(const char *input, char *output,
                                      size_t output_size)
{
  size_t in;
  size_t out = 0;

  if (input == NULL || input[0] == '\0')
  {
    input = "drive";
  }
  for (in = 0; input[in] != '\0' && out + 1 < output_size; in++)
  {
    unsigned char c = (unsigned char)input[in];
    if (isalnum(c) || c == '-' || c == '_')
    {
      output[out++] = (char)c;
    }
    else if (c == ' ' || c == '/' || c == '\\' || c == ':')
    {
      output[out++] = '_';
    }
  }
  output[out] = '\0';
  if (out == 0)
  {
    snprintf(output, output_size, "drive");
  }
}

static void wd_royl_make_backup_name_ccc(char *buffer, size_t buffer_size,
                                         const char *kind)
{
  char serial[64];
  char timestamp[32];
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);

  wd_royl_sanitize_name_ccc(current_source_serial_ccc, serial, sizeof(serial));
  if (tm != NULL)
  {
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm);
  }
  else
  {
    snprintf(timestamp, sizeof(timestamp), "unknown_time");
  }
  snprintf(buffer, buffer_size, "wd_royl_%s_%s_%s.bin", serial, kind,
           timestamp);
}

static void wd_royl_make_backup_path_ccc(char *buffer, size_t buffer_size,
                                         const char *kind)
{
  char name[512];
  const char *home = g_get_home_dir();

  wd_royl_make_backup_name_ccc(name, sizeof(name), kind);
  if (home != NULL && home[0] != '\0')
  {
    snprintf(buffer, buffer_size, "%s/%s", home, name);
  }
  else
  {
    snprintf(buffer, buffer_size, "%s", name);
  }
}

static bool wd_royl_operation_ready_ccc(void)
{
  if (running_clone_ccc)
  {
    wd_royl_show_message_ccc(
        GTK_MESSAGE_WARNING, _("WD ROYL"),
        _("A copy operation is running. Stop it before using vendor specific "
          "commands."));
    return false;
  }
  if (running_analyze_ccc)
  {
    wd_royl_show_message_ccc(
        GTK_MESSAGE_WARNING, _("WD ROYL"),
        _("A drive analysis is running. Stop it before using vendor specific "
          "commands."));
    return false;
  }
  if (!source_chosen_ccc)
  {
    wd_royl_show_message_ccc(
        GTK_MESSAGE_WARNING, _("WD ROYL"),
        _("Choose and connect a source drive before using vendor specific "
          "commands."));
    return false;
  }
  if (usb_mode_ccc && !usb_allow_ata_ccc)
  {
    wd_royl_show_message_ccc(
        GTK_MESSAGE_WARNING, _("WD ROYL"),
        _("Vendor specific commands require USB ATA mode. Select USB ATA mode "
          "and choose the source drive again."));
    return false;
  }
  return true;
}

static void wd_royl_report_error_ccc(const char *title, const char *prefix)
{
  wd_royl_show_message_ccc(GTK_MESSAGE_ERROR, title, "%s\n\n%s", prefix,
                           wd_royl_last_error_ccc());
}

// Vendor commands need direct access to the selected source drive. Choosing a
// source only identifies it and then releases the hardware again, so open the
// source here when the main window is not already connected. The caller must
// pass the returned flag to wd_royl_end_source_access_ccc() when finished.
static bool wd_royl_begin_source_access_ccc(bool *temporary)
{
  *temporary = false;
  if (connected_ccc)
  {
    return true;
  }
  if (connect_source_disk_ccc() != 0)
  {
    release_devices_ccc();
    wd_royl_show_message_ccc(
        GTK_MESSAGE_ERROR, _("WD ROYL"),
        _("Unable to open the selected source drive.\n\nChoose the source "
          "drive again and make sure it is still connected, then retry the "
          "operation."));
    return false;
  }
  *temporary = true;
  return true;
}

static void wd_royl_end_source_access_ccc(bool temporary)
{
  if (temporary)
  {
    release_devices_ccc();
  }
}

// ---------------------------------------------------------------------------
// Service area backup and restore
//
// A backup is a directory containing one module_XXXX.bin file per module,
// manifest.ini (the machine-readable restore source), and summary.txt. The
// directory module 0x0001 is included and is restored last because it points
// at the other modules. Before every restore write, the current module is
// saved under pre_restore_<timestamp>/ and read back to verify that recovery
// copy. The service area write itself is verified by reading the module again.
// ---------------------------------------------------------------------------

#define WD_ROYL_SA_MANIFEST_FORMAT "opensuperclone-wd-royl-sa-backup"
#define WD_ROYL_SA_MANIFEST_VERSION 1
#define WD_ROYL_SA_MANIFEST_NAME "manifest.ini"
#define WD_ROYL_SA_SUMMARY_NAME "summary.txt"
#define WD_ROYL_SA_RESTORE_LOG_NAME "restore.txt"

typedef struct
{
  unsigned int index;
  unsigned int count;
  uint16_t module;
  unsigned long long completed;
  unsigned long long total;
} wd_royl_sa_backup_progress_ccc_t;

static wd_royl_sa_backup_progress_ccc_t *wd_royl_sa_backup_progress_state_ccc =
    NULL;

typedef struct
{
  unsigned int index;
  size_t count;
  uint16_t module;
} wd_royl_sa_restore_progress_ccc_t;

static wd_royl_sa_restore_progress_ccc_t *wd_royl_sa_restore_progress_state_ccc =
    NULL;

typedef struct
{
  uint16_t module;
  uint16_t directory_sectors;
  uint8_t directory_copies;
  uint32_t directory_location1;
  uint32_t directory_location2;
  unsigned long long file_size;
  bool manifest_checksum_ok;
  bool available;
  bool selected;
  char path[PATH_MAX];
  char sha256[65];
  char status[64];
  char error[WD_ROYL_ERROR_SIZE];
} wd_royl_sa_restore_entry_ccc;

typedef struct
{
  char path[PATH_MAX];
  char directory[PATH_MAX];
  char model[64];
  char serial[64];
  char transport[64];
  char created[64];
  char directory_sha256[65];
  unsigned long long directory_size;
  bool directory_checksum_ok;
  wd_royl_sa_restore_entry_ccc *entries;
  size_t entry_count;
} wd_royl_sa_manifest_ccc;

typedef struct
{
  GtkWidget *dialog;
  GtkListStore *store;
  wd_royl_sa_manifest_ccc *manifest;
} wd_royl_sa_restore_picker_ccc_t;

static void wd_royl_format_size_ccc(unsigned long long bytes, char *buffer,
                                    size_t buffer_size);
static int wd_royl_compare_module_entries_ccc(const void *a, const void *b);

static void wd_royl_join_path_ccc(char *destination, size_t destination_size,
                                  const char *directory, const char *name)
{
  char *path = g_build_filename(directory, name, NULL);

  g_strlcpy(destination, path, destination_size);
  g_free(path);
}

static char *wd_royl_key_string_ccc(GKeyFile *key, const char *group,
                                    const char *name)
{
  GError *error = NULL;
  char *value = g_key_file_get_string(key, group, name, &error);

  if (error != NULL)
  {
    g_error_free(error);
  }
  return value;
}

static gint wd_royl_key_integer_ccc(GKeyFile *key, const char *group,
                                    const char *name, gint default_value)
{
  GError *error = NULL;
  gint value = g_key_file_get_integer(key, group, name, &error);

  if (error != NULL)
  {
    g_error_free(error);
    return default_value;
  }
  return value;
}

static gboolean wd_royl_key_boolean_ccc(GKeyFile *key, const char *group,
                                        const char *name,
                                        gboolean default_value)
{
  GError *error = NULL;
  gboolean value = g_key_file_get_boolean(key, group, name, &error);

  if (error != NULL)
  {
    g_error_free(error);
    return default_value;
  }
  return value;
}

static void wd_royl_set_hex_ccc(GKeyFile *key, const char *group,
                                const char *name, uint32_t value)
{
  char text[16];

  snprintf(text, sizeof(text), "0x%08x", value);
  g_key_file_set_string(key, group, name, text);
}

static void wd_royl_sha256_ccc(const uint8_t *data, size_t size, char *output,
                               size_t output_size)
{
  char *checksum =
      g_compute_checksum_for_data(G_CHECKSUM_SHA256, data, size);

  g_strlcpy(output, checksum != NULL ? checksum : "", output_size);
  g_free(checksum);
}

static bool wd_royl_module_checksum_ok_ccc(const uint8_t *data, size_t size)
{
  return wd_royl_get_module_checksum_ccc(data) ==
         wd_royl_calculate_checksum_ccc(data, size);
}

static const char *wd_royl_transport_name_ccc(void)
{
  if (usb_mode_ccc)
  {
    return usb_allow_ata_ccc ? "Direct USB ATA" : "Direct USB";
  }
  if (ahci_mode_ccc)
  {
    return "Direct AHCI";
  }
  if (ata_passthrough_ccc)
  {
    return "ATA Passthrough";
  }
  if (scsi_passthrough_ccc)
  {
    return "SCSI Passthrough";
  }
  if (direct_mode_ccc)
  {
    return "Direct";
  }
  return "Unknown";
}

static char *wd_royl_choose_folder_ccc(const char *title,
                                       const char *initial_folder)
{
  GtkWidget *dialog;
  char *folder = NULL;

  dialog = gtk_file_chooser_dialog_new(
      title, GTK_WINDOW(main_window_ccc), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, _("Select Folder"),
      GTK_RESPONSE_ACCEPT, NULL);
  if (initial_folder != NULL && initial_folder[0] != '\0')
  {
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
                                        initial_folder);
  }
  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
  {
    folder = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
  }
  gtk_widget_destroy(dialog);
  return folder;
}

static void wd_royl_make_backup_folder_path_ccc(const char *parent,
                                                char *path, size_t path_size,
                                                unsigned int attempt)
{
  char serial[64];
  char timestamp[32];
  char name[512];
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);

  wd_royl_sanitize_name_ccc(current_source_serial_ccc, serial, sizeof(serial));
  if (tm != NULL)
  {
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm);
  }
  else
  {
    g_strlcpy(timestamp, "unknown_time", sizeof(timestamp));
  }
  if (attempt == 0)
  {
    snprintf(name, sizeof(name), "wd_royl_%s_sa_%s", serial, timestamp);
  }
  else
  {
    snprintf(name, sizeof(name), "wd_royl_%s_sa_%s_%u", serial, timestamp,
             attempt + 1);
  }
  wd_royl_join_path_ccc(path, path_size, parent, name);
}

static int wd_royl_create_backup_folder_ccc(const char *parent, char *path,
                                            size_t path_size, char *error,
                                            size_t error_size)
{
  unsigned int attempt;

  for (attempt = 0; attempt < 1000; attempt++)
  {
    wd_royl_make_backup_folder_path_ccc(parent, path, path_size, attempt);
    if (g_mkdir(path, 0755) == 0)
    {
      return 0;
    }
    if (errno != EEXIST)
    {
      snprintf(error, error_size,
               _("Unable to create the backup folder:\n%s\n\n%s"), path,
               g_strerror(errno));
      return -1;
    }
  }
  snprintf(error, error_size,
           _("Unable to find an unused name for the backup folder."));
  return -1;
}

static void wd_royl_make_pre_restore_path_ccc(const char *backup_directory,
                                              char *path, size_t path_size)
{
  char timestamp[32];
  char name[128];
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  unsigned int attempt;

  if (tm != NULL)
  {
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm);
  }
  else
  {
    g_strlcpy(timestamp, "unknown_time", sizeof(timestamp));
  }
  for (attempt = 0; attempt < 1000; attempt++)
  {
    if (attempt == 0)
    {
      snprintf(name, sizeof(name), "pre_restore_%s", timestamp);
    }
    else
    {
      snprintf(name, sizeof(name), "pre_restore_%s_%u", timestamp,
               attempt + 1);
    }
    wd_royl_join_path_ccc(path, path_size, backup_directory, name);
    if (!g_file_test(path, G_FILE_TEST_EXISTS))
    {
      return;
    }
  }
}

static void wd_royl_trim_identity_ccc(const char *input, char *output,
                                      size_t output_size)
{
  size_t start = 0;
  size_t end;

  if (input == NULL)
  {
    input = "";
  }
  while (input[start] != '\0' && g_ascii_isspace(input[start]))
  {
    start++;
  }
  end = strlen(input);
  while (end > start && g_ascii_isspace(input[end - 1]))
  {
    end--;
  }
  if ((end - start) >= output_size)
  {
    end = start + output_size - 1;
  }
  memcpy(output, input + start, end - start);
  output[end - start] = '\0';
}

static bool wd_royl_identity_value_matches_ccc(const char *backup_value,
                                               const char *current_value)
{
  char backup[128];
  char current[128];

  wd_royl_trim_identity_ccc(backup_value, backup, sizeof(backup));
  wd_royl_trim_identity_ccc(current_value, current, sizeof(current));
  if (backup[0] == '\0')
  {
    return true;
  }
  return g_ascii_strcasecmp(backup, current) == 0;
}

static bool wd_royl_restore_identity_ok_ccc(const char *backup_model,
                                            const char *backup_serial)
{
  if (wd_royl_identity_value_matches_ccc(backup_model,
                                         current_source_model_ccc) &&
      wd_royl_identity_value_matches_ccc(backup_serial,
                                         current_source_serial_ccc))
  {
    return true;
  }

  return wd_royl_confirm_ccc(
      _("WD ROYL drive identity mismatch"), _("Restore Anyway"),
      _("The backup was created from a different drive.\n\n"
        "Backup: %s / %s\nCurrent source: %s / %s\n\n"
        "Writing these service area modules to a different drive can damage "
        "it. Continue only if you are sure this is the correct drive."),
      backup_model != NULL && backup_model[0] != '\0' ? backup_model
                                                       : _("Unknown"),
      backup_serial != NULL && backup_serial[0] != '\0' ? backup_serial
                                                         : _("Unknown"),
      current_source_model_ccc[0] != '\0' ? current_source_model_ccc
                                          : _("Unknown"),
      current_source_serial_ccc[0] != '\0' ? current_source_serial_ccc
                                           : _("Unknown"));
}

static const char *wd_royl_restore_status_text_ccc(const char *status)
{
  if (status == NULL || status[0] == '\0')
  {
    return _("Unknown");
  }
  if (strcmp(status, "ok") == 0)
  {
    return _("Ready");
  }
  if (strcmp(status, "read_failed") == 0)
  {
    return _("Backup read failed");
  }
  if (strcmp(status, "header_invalid") == 0)
  {
    return _("Invalid module header");
  }
  if (strcmp(status, "file_write_failed") == 0)
  {
    return _("Backup file write failed");
  }
  if (strcmp(status, "file_missing") == 0)
  {
    return _("File missing");
  }
  if (strcmp(status, "file_invalid") == 0)
  {
    return _("Invalid file");
  }
  if (strcmp(status, "size_mismatch") == 0)
  {
    return _("Size mismatch");
  }
  if (strcmp(status, "checksum_mismatch") == 0)
  {
    return _("Checksum mismatch");
  }
  return status;
}

static int wd_royl_compare_restore_entries_ccc(const void *a, const void *b)
{
  const wd_royl_sa_restore_entry_ccc *entry_a =
      (const wd_royl_sa_restore_entry_ccc *)a;
  const wd_royl_sa_restore_entry_ccc *entry_b =
      (const wd_royl_sa_restore_entry_ccc *)b;

  // The directory module points at the other modules, so restore it last.
  if (entry_a->module == WD_ROYL_MODULE_01 &&
      entry_b->module != WD_ROYL_MODULE_01)
  {
    return 1;
  }
  if (entry_b->module == WD_ROYL_MODULE_01 &&
      entry_a->module != WD_ROYL_MODULE_01)
  {
    return -1;
  }
  if (entry_a->module < entry_b->module)
  {
    return -1;
  }
  if (entry_a->module > entry_b->module)
  {
    return 1;
  }
  return 0;
}

static void wd_royl_sa_backup_progress_callback_ccc(
    const char *stage, unsigned long long done, unsigned long long total)
{
  wd_royl_sa_backup_progress_ccc_t *state =
      wd_royl_sa_backup_progress_state_ccc;
  char text[256];
  unsigned long long current;
  unsigned long long overall;

  if (state == NULL)
  {
    wd_royl_progress_update_ccc(stage, done, total);
    return;
  }
  current = done;
  if (total > 0 && current > total)
  {
    current = total;
  }
  overall = state->completed + current;
  if (state->total > 0 && overall > state->total)
  {
    overall = state->total;
  }
  snprintf(text, sizeof(text), _("Module 0x%04x (%u of %u): %s"),
           state->module, state->index, state->count,
           stage != NULL ? stage : "");
  wd_royl_progress_update_ccc(text, overall, state->total);
}

static void wd_royl_sa_restore_progress_callback_ccc(
    const char *stage, unsigned long long done, unsigned long long total)
{
  wd_royl_sa_restore_progress_ccc_t *state =
      wd_royl_sa_restore_progress_state_ccc;
  char text[256];

  if (state == NULL)
  {
    wd_royl_progress_update_ccc(stage, done, total);
    return;
  }
  snprintf(text, sizeof(text), _("Module 0x%04x (%zu of %zu): %s"),
           state->module, (size_t)state->index, state->count,
           stage != NULL ? stage : "");
  wd_royl_progress_update_ccc(text, done, total);
}

static void wd_royl_sa_manifest_clear_ccc(wd_royl_sa_manifest_ccc *manifest)
{
  if (manifest == NULL)
  {
    return;
  }
  free(manifest->entries);
  memset(manifest, 0, sizeof(*manifest));
}

static int wd_royl_load_sa_manifest_ccc(const char *path,
                                        wd_royl_sa_manifest_ccc *manifest,
                                        char *error, size_t error_size)
{
  GKeyFile *key = NULL;
  GError *gerror = NULL;
  char **groups = NULL;
  gsize group_count = 0;
  gsize group_index;
  char *format = NULL;
  char *value = NULL;
  gint version;
  int result = -1;

  memset(manifest, 0, sizeof(*manifest));
  g_strlcpy(manifest->path, path, sizeof(manifest->path));
  {
    char *directory = g_path_get_dirname(path);
    g_strlcpy(manifest->directory, directory, sizeof(manifest->directory));
    g_free(directory);
  }

  key = g_key_file_new();
  if (!g_key_file_load_from_file(key, path, G_KEY_FILE_NONE, &gerror))
  {
    snprintf(error, error_size,
             _("Unable to read the backup manifest:\n%s\n\n%s"), path,
             gerror != NULL ? gerror->message : _("Unknown error"));
    if (gerror != NULL)
    {
      g_error_free(gerror);
    }
    goto finish;
  }

  format = wd_royl_key_string_ccc(key, "backup", "format");
  if (format == NULL || strcmp(format, WD_ROYL_SA_MANIFEST_FORMAT) != 0)
  {
    snprintf(error, error_size,
             _("The selected file is not a WD ROYL SA backup manifest."));
    goto finish;
  }
  version = wd_royl_key_integer_ccc(key, "backup", "version", 0);
  if (version != WD_ROYL_SA_MANIFEST_VERSION)
  {
    snprintf(error, error_size,
             _("The backup manifest uses unsupported version %d."), version);
    goto finish;
  }

  value = wd_royl_key_string_ccc(key, "backup", "model");
  if (value != NULL)
  {
    g_strlcpy(manifest->model, value, sizeof(manifest->model));
    g_free(value);
  }
  value = wd_royl_key_string_ccc(key, "backup", "serial");
  if (value != NULL)
  {
    g_strlcpy(manifest->serial, value, sizeof(manifest->serial));
    g_free(value);
  }
  value = wd_royl_key_string_ccc(key, "backup", "transport");
  if (value != NULL)
  {
    g_strlcpy(manifest->transport, value, sizeof(manifest->transport));
    g_free(value);
  }
  value = wd_royl_key_string_ccc(key, "backup", "created");
  if (value != NULL)
  {
    g_strlcpy(manifest->created, value, sizeof(manifest->created));
    g_free(value);
  }
  value = wd_royl_key_string_ccc(key, "backup", "directory_sha256");
  if (value != NULL)
  {
    g_strlcpy(manifest->directory_sha256, value,
              sizeof(manifest->directory_sha256));
    g_free(value);
  }
  manifest->directory_size = (unsigned long long)wd_royl_key_integer_ccc(
      key, "backup", "directory_size", 0);
  manifest->directory_checksum_ok = wd_royl_key_boolean_ccc(
      key, "backup", "directory_checksum_ok", FALSE);

  groups = g_key_file_get_groups(key, &group_count);
  for (group_index = 0; group_index < group_count; group_index++)
  {
    wd_royl_sa_restore_entry_ccc *new_entries;
    wd_royl_sa_restore_entry_ccc *entry;
    gint module_id;
    char *file;
    char *status;
    char *sha256;
    char *module_error;
    bool valid_file_name;

    if (!g_str_has_prefix(groups[group_index], "module."))
    {
      continue;
    }
    module_id =
        wd_royl_key_integer_ccc(key, groups[group_index], "id", 0);
    if (module_id <= 0 || module_id > 0xffff)
    {
      continue;
    }
    if (manifest->entry_count >= 4096)
    {
      snprintf(error, error_size,
               _("The backup manifest lists too many modules."));
      goto finish;
    }

    new_entries = (wd_royl_sa_restore_entry_ccc *)realloc(
        manifest->entries,
        (manifest->entry_count + 1) * sizeof(*manifest->entries));
    if (new_entries == NULL)
    {
      snprintf(error, error_size,
               _("Unable to allocate memory for the backup manifest."));
      goto finish;
    }
    manifest->entries = new_entries;
    entry = &manifest->entries[manifest->entry_count];
    memset(entry, 0, sizeof(*entry));
    entry->module = (uint16_t)module_id;
    entry->directory_sectors = (uint16_t)wd_royl_key_integer_ccc(
        key, groups[group_index], "directory_sectors", 0);
    entry->directory_copies = (uint8_t)wd_royl_key_integer_ccc(
        key, groups[group_index], "directory_copies", 0);
    value = wd_royl_key_string_ccc(key, groups[group_index],
                                   "directory_location1");
    if (value != NULL)
    {
      entry->directory_location1 =
          (uint32_t)g_ascii_strtoull(value, NULL, 0);
      g_free(value);
    }
    value = wd_royl_key_string_ccc(key, groups[group_index],
                                   "directory_location2");
    if (value != NULL)
    {
      entry->directory_location2 =
          (uint32_t)g_ascii_strtoull(value, NULL, 0);
      g_free(value);
    }
    entry->file_size = (unsigned long long)wd_royl_key_integer_ccc(
        key, groups[group_index], "read_size", 0);
    entry->manifest_checksum_ok = wd_royl_key_boolean_ccc(
        key, groups[group_index], "checksum_ok", FALSE);

    file = wd_royl_key_string_ccc(key, groups[group_index], "file");
    status = wd_royl_key_string_ccc(key, groups[group_index], "status");
    sha256 = wd_royl_key_string_ccc(key, groups[group_index], "sha256");
    module_error =
        wd_royl_key_string_ccc(key, groups[group_index], "error");
    if (status != NULL && status[0] != '\0')
    {
      g_strlcpy(entry->status, status, sizeof(entry->status));
    }
    else
    {
      g_strlcpy(entry->status, "unknown", sizeof(entry->status));
    }
    if (sha256 != NULL)
    {
      g_strlcpy(entry->sha256, sha256, sizeof(entry->sha256));
    }
    if (module_error != NULL)
    {
      g_strlcpy(entry->error, module_error, sizeof(entry->error));
    }

    valid_file_name =
        file != NULL && file[0] != '\0' && strchr(file, '/') == NULL &&
        strchr(file, '\\') == NULL && strcmp(file, ".") != 0 &&
        strcmp(file, "..") != 0;
    if (valid_file_name)
    {
      GStatBuf stat_buffer;

      wd_royl_join_path_ccc(entry->path, sizeof(entry->path),
                            manifest->directory, file);
      entry->available = strcmp(entry->status, "ok") == 0;
      if (entry->available)
      {
        if (!g_file_test(entry->path, G_FILE_TEST_IS_REGULAR))
        {
          entry->available = false;
          g_strlcpy(entry->status, "file_missing", sizeof(entry->status));
        }
        else if (g_stat(entry->path, &stat_buffer) != 0 ||
                 stat_buffer.st_size <= 0 ||
                 (unsigned long long)stat_buffer.st_size >
                     WD_ROYL_MAX_FILE_SIZE)
        {
          entry->available = false;
          g_strlcpy(entry->status, "file_invalid", sizeof(entry->status));
        }
        else if (entry->file_size != 0 &&
                 (unsigned long long)stat_buffer.st_size != entry->file_size)
        {
          entry->available = false;
          g_strlcpy(entry->status, "size_mismatch", sizeof(entry->status));
        }
        else if (!entry->manifest_checksum_ok)
        {
          entry->available = false;
          g_strlcpy(entry->status, "checksum_mismatch",
                    sizeof(entry->status));
        }
      }
    }
    else if (entry->status[0] == '\0' || strcmp(entry->status, "ok") == 0)
    {
      entry->available = false;
      g_strlcpy(entry->status, "file_missing", sizeof(entry->status));
    }
    entry->selected = entry->available;

    g_free(file);
    g_free(status);
    g_free(sha256);
    g_free(module_error);
    manifest->entry_count++;
  }

  if (manifest->entry_count == 0)
  {
    snprintf(error, error_size,
             _("The backup manifest does not list any modules."));
    goto finish;
  }

  qsort(manifest->entries, manifest->entry_count,
        sizeof(*manifest->entries), wd_royl_compare_restore_entries_ccc);
  if (manifest->entry_count > 1)
  {
    size_t input_index;
    size_t output_index = 1;

    for (input_index = 1; input_index < manifest->entry_count; input_index++)
    {
      if (manifest->entries[input_index].module !=
          manifest->entries[output_index - 1].module)
      {
        if (output_index != input_index)
        {
          manifest->entries[output_index] = manifest->entries[input_index];
        }
        output_index++;
      }
    }
    manifest->entry_count = output_index;
  }

  result = 0;

finish:
  if (groups != NULL)
  {
    g_strfreev(groups);
  }
  g_free(format);
  if (key != NULL)
  {
    g_key_file_free(key);
  }
  if (result != 0)
  {
    wd_royl_sa_manifest_clear_ccc(manifest);
  }
  return result;
}

static void wd_royl_restore_add_text_column_ccc(GtkWidget *tree_view,
                                                const char *title,
                                                int model_column, int width,
                                                float xalign)
{
  GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
  GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
      title, renderer, "text", model_column, NULL);

  gtk_tree_view_column_set_resizable(column, TRUE);
  gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_fixed_width(column, width);
  g_object_set(renderer, "xalign", xalign, NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
}

static void wd_royl_restore_toggle_ccc(GtkCellRendererToggle *renderer,
                                       gchar *path_string, gpointer user_data)
{
  wd_royl_sa_restore_picker_ccc_t *picker =
      (wd_royl_sa_restore_picker_ccc_t *)user_data;
  GtkTreeIter iter;
  gboolean selected;
  gboolean available;
  guint index;

  if (!gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(picker->store), &iter,
                                           path_string))
  {
    return;
  }
  gtk_tree_model_get(GTK_TREE_MODEL(picker->store), &iter, 0, &selected, 1,
                     &available, 6, &index, -1);
  if (!available || index >= picker->manifest->entry_count)
  {
    return;
  }
  selected = !selected;
  gtk_list_store_set(picker->store, &iter, 0, selected, -1);
  picker->manifest->entries[index].selected = selected;
}

static void wd_royl_restore_set_all_ccc(
    wd_royl_sa_restore_picker_ccc_t *picker, bool selected)
{
  GtkTreeIter iter;
  GtkTreeModel *model = GTK_TREE_MODEL(picker->store);

  if (!gtk_tree_model_get_iter_first(model, &iter))
  {
    return;
  }
  do
  {
    gboolean available;
    guint index;

    gtk_tree_model_get(model, &iter, 1, &available, 6, &index, -1);
    if (available && index < picker->manifest->entry_count)
    {
      gtk_list_store_set(picker->store, &iter, 0, selected, -1);
      picker->manifest->entries[index].selected = selected;
    }
  } while (gtk_tree_model_iter_next(model, &iter));
}

static void wd_royl_restore_select_all_clicked_ccc(GtkWidget *widget,
                                                   gpointer user_data)
{
  wd_royl_restore_set_all_ccc(
      (wd_royl_sa_restore_picker_ccc_t *)user_data, true);
}

static void wd_royl_restore_select_none_clicked_ccc(GtkWidget *widget,
                                                    gpointer user_data)
{
  wd_royl_restore_set_all_ccc(
      (wd_royl_sa_restore_picker_ccc_t *)user_data, false);
}

static size_t wd_royl_restore_selected_count_ccc(
    wd_royl_sa_restore_picker_ccc_t *picker)
{
  size_t count = 0;
  size_t i;

  for (i = 0; i < picker->manifest->entry_count; i++)
  {
    if (picker->manifest->entries[i].selected)
    {
      count++;
    }
  }
  return count;
}

static void wd_royl_restore_selection_response_ccc(GtkDialog *dialog,
                                                   gint response,
                                                   gpointer user_data)
{
  wd_royl_sa_restore_picker_ccc_t *picker =
      (wd_royl_sa_restore_picker_ccc_t *)user_data;
  GtkWidget *message;

  if (response != GTK_RESPONSE_ACCEPT ||
      wd_royl_restore_selected_count_ccc(picker) > 0)
  {
    return;
  }
  message = gtk_message_dialog_new(
      GTK_WINDOW(dialog), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING,
      GTK_BUTTONS_OK, "%s",
      _("Select at least one available module to restore."));
  gtk_window_set_title(GTK_WINDOW(message), _("WD ROYL SA Restore"));
  gtk_dialog_run(GTK_DIALOG(message));
  gtk_widget_destroy(message);
  g_signal_stop_emission_by_name(G_OBJECT(dialog), "response");
}

static size_t wd_royl_restore_selection_ccc(wd_royl_sa_manifest_ccc *manifest)
{
  wd_royl_sa_restore_picker_ccc_t picker;
  GtkWidget *content;
  GtkWidget *vbox;
  GtkWidget *info_label;
  GtkWidget *scrolled;
  GtkWidget *button_row;
  GtkWidget *select_all_button;
  GtkWidget *select_none_button;
  GtkWidget *tree_view;
  GtkCellRenderer *toggle_renderer;
  GtkTreeViewColumn *toggle_column;
  GtkTreeIter iter;
  char info_text[2048];
  size_t ready_count = 0;
  size_t i;
  int response;

  memset(&picker, 0, sizeof(picker));
  picker.manifest = manifest;
  for (i = 0; i < manifest->entry_count; i++)
  {
    if (manifest->entries[i].available)
    {
      ready_count++;
    }
  }

  picker.dialog = gtk_dialog_new_with_buttons(
      _("Restore WD ROYL SA Modules"), GTK_WINDOW(main_window_ccc),
      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CANCEL,
      GTK_RESPONSE_CANCEL, _("Restore Selected"), GTK_RESPONSE_ACCEPT, NULL);
  gtk_window_set_default_size(GTK_WINDOW(picker.dialog), 900, 560);
  gtk_dialog_set_default_response(GTK_DIALOG(picker.dialog),
                                  GTK_RESPONSE_ACCEPT);

  content = gtk_dialog_get_content_area(GTK_DIALOG(picker.dialog));
  gtk_container_set_border_width(GTK_CONTAINER(content), 10);
  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_box_pack_start(GTK_BOX(content), vbox, TRUE, TRUE, 0);

  snprintf(info_text, sizeof(info_text),
           _("Backup created: %s\nBackup drive: %s / %s\nTransport: %s\n"
             "Modules in manifest: %zu    Ready to restore: %zu\n\n"
             "The directory module 0x0001 is restored last. Each current "
             "module is backed up before it is written."),
           manifest->created[0] != '\0' ? manifest->created : _("Unknown"),
           manifest->model[0] != '\0' ? manifest->model : _("Unknown"),
           manifest->serial[0] != '\0' ? manifest->serial : _("Unknown"),
           manifest->transport[0] != '\0' ? manifest->transport
                                          : _("Unknown"),
           manifest->entry_count, ready_count);
  info_label = gtk_label_new(info_text);
  gtk_label_set_line_wrap(GTK_LABEL(info_label), TRUE);
  gtk_label_set_xalign(GTK_LABEL(info_label), 0.0);
  gtk_box_pack_start(GTK_BOX(vbox), info_label, FALSE, FALSE, 0);

  {
    GType columns[8] = {G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_STRING,
                        G_TYPE_STRING,  G_TYPE_STRING,  G_TYPE_STRING,
                        G_TYPE_UINT,    G_TYPE_STRING};
    picker.store = gtk_list_store_newv(8, columns);
  }
  tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(picker.store));
  gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(tree_view), FALSE);
  gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(tree_view),
                               GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);
  gtk_tree_view_set_tooltip_column(GTK_TREE_VIEW(tree_view), 7);

  toggle_renderer = gtk_cell_renderer_toggle_new();
  toggle_column = gtk_tree_view_column_new_with_attributes(
      _("Restore"), toggle_renderer, "active", 0, "activatable", 1, NULL);
  gtk_tree_view_column_set_resizable(toggle_column, FALSE);
  gtk_tree_view_column_set_sizing(toggle_column, GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_fixed_width(toggle_column, 72);
  gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), toggle_column);
  g_signal_connect(toggle_renderer, "toggled",
                   G_CALLBACK(wd_royl_restore_toggle_ccc), &picker);

  wd_royl_restore_add_text_column_ccc(tree_view, _("Module"), 2, 90, 0.0);
  wd_royl_restore_add_text_column_ccc(tree_view, _("Size"), 3, 110, 0.0);
  wd_royl_restore_add_text_column_ccc(tree_view, _("Status"), 4, 190, 0.0);
  wd_royl_restore_add_text_column_ccc(tree_view, _("SHA-256"), 5, 230, 0.0);

  for (i = 0; i < manifest->entry_count; i++)
  {
    wd_royl_sa_restore_entry_ccc *entry = &manifest->entries[i];
    char module_text[16];
    char size_text[64];
    char sha_text[24];
    const char *status_text = wd_royl_restore_status_text_ccc(entry->status);

    snprintf(module_text, sizeof(module_text), "0x%04x", entry->module);
    if (entry->file_size != 0)
    {
      wd_royl_format_size_ccc(entry->file_size, size_text, sizeof(size_text));
    }
    else
    {
      g_strlcpy(size_text, "-", sizeof(size_text));
    }
    if (entry->sha256[0] != '\0')
    {
      snprintf(sha_text, sizeof(sha_text), "%.16s...", entry->sha256);
    }
    else
    {
      g_strlcpy(sha_text, "-", sizeof(sha_text));
    }

    gtk_list_store_append(picker.store, &iter);
    gtk_list_store_set(picker.store, &iter, 0, entry->selected, 1,
                       entry->available, 2, module_text, 3, size_text, 4,
                       status_text, 5, sha_text, 6, (guint)i, 7,
                       entry->error, -1);
  }

  scrolled = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_widget_set_size_request(scrolled, -1, 330);
  gtk_container_add(GTK_CONTAINER(scrolled), tree_view);
  gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

  button_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  select_all_button = gtk_button_new_with_label(_("Select All"));
  select_none_button = gtk_button_new_with_label(_("Select None"));
  gtk_box_pack_start(GTK_BOX(button_row), select_all_button, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(button_row), select_none_button, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), button_row, FALSE, FALSE, 0);
  g_signal_connect(select_all_button, "clicked",
                   G_CALLBACK(wd_royl_restore_select_all_clicked_ccc),
                   &picker);
  g_signal_connect(select_none_button, "clicked",
                   G_CALLBACK(wd_royl_restore_select_none_clicked_ccc),
                   &picker);
  g_signal_connect(picker.dialog, "response",
                   G_CALLBACK(wd_royl_restore_selection_response_ccc), &picker);

  gtk_widget_show_all(picker.dialog);
  response = gtk_dialog_run(GTK_DIALOG(picker.dialog));
  g_signal_handlers_disconnect_by_data(picker.dialog, &picker);
  gtk_widget_destroy(picker.dialog);
  g_object_unref(picker.store);

  if (response != GTK_RESPONSE_ACCEPT)
  {
    return 0;
  }
  return wd_royl_restore_selected_count_ccc(&picker);
}

static int wd_royl_restore_one_module_ccc(
    const wd_royl_sa_restore_entry_ccc *entry, const char *pre_restore_directory,
    char *pre_restore_file, size_t pre_restore_file_size, bool *fatal,
    char *error, size_t error_size, wd_royl_progress_callback_ccc progress)
{
  uint8_t *backup_data = NULL;
  uint8_t *current_data = NULL;
  uint8_t *saved_data = NULL;
  size_t backup_size = 0;
  size_t current_size = 0;
  size_t saved_size = 0;
  char calculated_sha256[65];
  char module_file[32];
  int result = -1;

  *fatal = false;
  if (error != NULL && error_size > 0)
  {
    error[0] = '\0';
  }
  if (pre_restore_file != NULL && pre_restore_file_size > 0)
  {
    pre_restore_file[0] = '\0';
  }

  if (wd_royl_read_file_ccc(entry->path, &backup_data, &backup_size) != 0)
  {
    snprintf(error, error_size, "%s", wd_royl_last_error_ccc());
    goto finish;
  }
  if (entry->file_size != 0 &&
      (unsigned long long)backup_size != entry->file_size)
  {
    snprintf(error, error_size,
             _("The backup file is %zu bytes, but the manifest reports %llu "
               "bytes."),
             backup_size, entry->file_size);
    goto finish;
  }
  if (entry->sha256[0] != '\0')
  {
    wd_royl_sha256_ccc(backup_data, backup_size, calculated_sha256,
                       sizeof(calculated_sha256));
    if (g_ascii_strcasecmp(calculated_sha256, entry->sha256) != 0)
    {
      snprintf(error, error_size,
               _("The backup file SHA-256 does not match the manifest."));
      goto finish;
    }
  }
  if (wd_royl_validate_module_ccc(backup_data, backup_size, entry->module) != 0)
  {
    snprintf(error, error_size, "%s", wd_royl_last_error_ccc());
    goto finish;
  }

  if (wd_royl_read_module_ccc(entry->module, &current_data, &current_size,
                              progress) != 0)
  {
    snprintf(error, error_size, "%s", wd_royl_last_error_ccc());
    goto finish;
  }
  if (wd_royl_validate_module_header_ccc(current_data, current_size,
                                         entry->module) != 0)
  {
    snprintf(error, error_size, _("The current module could not be validated. "
                                  "%s"),
             wd_royl_last_error_ccc());
    goto finish;
  }
  if (current_size != backup_size)
  {
    snprintf(error, error_size,
             _("The module on the drive is %zu bytes, but the backup is %zu "
               "bytes. Modules cannot be resized by restore."),
             current_size, backup_size);
    goto finish;
  }

  snprintf(module_file, sizeof(module_file), "module_%04x.bin",
           entry->module);
  wd_royl_join_path_ccc(pre_restore_file, pre_restore_file_size,
                        pre_restore_directory, module_file);
  if (wd_royl_write_file_ccc(pre_restore_file, current_data, current_size) != 0)
  {
    snprintf(error, error_size, "%s", wd_royl_last_error_ccc());
    goto finish;
  }
  if (wd_royl_read_file_ccc(pre_restore_file, &saved_data, &saved_size) != 0)
  {
    snprintf(error, error_size,
             _("The previous module backup could not be read back. The write "
               "was not attempted. %s"),
             wd_royl_last_error_ccc());
    goto finish;
  }
  if (saved_size != current_size ||
      memcmp(saved_data, current_data, current_size) != 0)
  {
    snprintf(error, error_size,
             _("The previous module backup could not be verified. The write "
               "was not attempted."));
    goto finish;
  }
  free(saved_data);
  saved_data = NULL;

  if (wd_royl_write_module_ccc(entry->module, backup_data, backup_size,
                               progress) != 0)
  {
    *fatal = true;
    snprintf(error, error_size, "%s", wd_royl_last_error_ccc());
    goto finish;
  }
  if (wd_royl_verify_module_ccc(entry->module, backup_data, backup_size,
                                progress) != 0)
  {
    *fatal = true;
    snprintf(error, error_size, "%s", wd_royl_last_error_ccc());
    goto finish;
  }

  result = 0;

finish:
  free(backup_data);
  free(current_data);
  free(saved_data);
  return result;
}

void run_wd_royl_backup_sa_ccc(void)
{
  char *parent_directory = NULL;
  char backup_directory[PATH_MAX] = "";
  char manifest_path[PATH_MAX] = "";
  char summary_path[PATH_MAX] = "";
  char folder_error[PATH_MAX + 256] = "";
  char failure_message[4096] = "";
  uint8_t *directory_data = NULL;
  size_t directory_size = 0;
  wd_royl_module_entry_ccc *entries = NULL;
  size_t entry_count = 0;
  size_t i;
  bool temporary_source_access = false;
  bool source_open = false;
  bool progress_active = false;
  GKeyFile *manifest = NULL;
  GString *summary = NULL;
  unsigned int ok_count = 0;
  unsigned int failed_count = 0;
  unsigned int read_failed_count = 0;
  unsigned int file_write_failed_count = 0;
  unsigned int checksum_mismatch_count = 0;
  unsigned long long total_bytes = 0;
  char directory_sha256[65] = "";
  bool directory_checksum_ok = false;
  char created[64] = "";
  bool summary_written = false;
  wd_royl_sa_backup_progress_ccc_t progress_state;

  if (!wd_royl_operation_ready_ccc())
  {
    return;
  }
  parent_directory = wd_royl_choose_folder_ccc(
      _("Choose a folder for the WD ROYL SA backup"), g_get_home_dir());
  if (parent_directory == NULL)
  {
    goto cleanup;
  }
  wd_royl_make_backup_folder_path_ccc(parent_directory, backup_directory,
                                      sizeof(backup_directory), 0);
  if (!wd_royl_confirm_ccc(
          _("Back up WD ROYL SA"), _("Back Up"),
          _("Back up every module listed in SA directory 0x0001?\n\n"
            "Drive: %s\nSerial: %s\nDestination:\n%s\n\n"
            "The backup continues if an individual module cannot be read.\n\n"
            "Continue?"),
          current_source_model_ccc[0] != '\0' ? current_source_model_ccc
                                              : _("Unknown"),
          current_source_serial_ccc[0] != '\0' ? current_source_serial_ccc
                                                : _("Unknown"),
          backup_directory))
  {
    goto cleanup;
  }
  wd_royl_dialog_parent_override_ccc = NULL;
  if (!wd_royl_begin_source_access_ccc(&temporary_source_access))
  {
    goto cleanup;
  }
  source_open = true;
  if (wd_royl_create_backup_folder_ccc(
          parent_directory, backup_directory, sizeof(backup_directory),
          folder_error, sizeof(folder_error)) != 0)
  {
    wd_royl_show_message_ccc(GTK_MESSAGE_ERROR, _("WD ROYL SA Backup"), "%s",
                             folder_error);
    goto cleanup;
  }
  wd_royl_progress_begin_ccc(_("WD ROYL SA Backup"),
                             _("Reading module directory 0x0001"));
  progress_active = true;
  if (wd_royl_read_module_ccc(WD_ROYL_MODULE_01, &directory_data,
                              &directory_size,
                              wd_royl_progress_update_ccc) != 0)
  {
    snprintf(failure_message, sizeof(failure_message),
             _("Unable to read the SA module directory. The backup was not "
               "created.\n\n%s"),
             wd_royl_last_error_ccc());
    goto cleanup;
  }
  if (wd_royl_parse_module_directory_ccc(directory_data, directory_size,
                                         &entries, &entry_count) != 0)
  {
    snprintf(failure_message, sizeof(failure_message),
             _("Unable to parse the SA module directory. The backup was not "
               "created.\n\n%s"),
             wd_royl_last_error_ccc());
    goto cleanup;
  }

  {
    bool have_directory_entry = false;

    for (i = 0; i < entry_count; i++)
    {
      if (entries[i].module_id == WD_ROYL_MODULE_01)
      {
        have_directory_entry = true;
        break;
      }
    }
    if (!have_directory_entry &&
        directory_size >= (size_t)WD_ROYL_SECTOR_SIZE)
    {
      wd_royl_module_entry_ccc *new_entries =
          (wd_royl_module_entry_ccc *)realloc(
              entries, (entry_count + 1) * sizeof(*entries));

      if (new_entries == NULL)
      {
        snprintf(failure_message, sizeof(failure_message),
                 _("Unable to allocate memory for the module directory."));
        goto cleanup;
      }
      entries = new_entries;
      memset(&entries[entry_count], 0, sizeof(entries[entry_count]));
      entries[entry_count].module_id = WD_ROYL_MODULE_01;
      entries[entry_count].sectors =
          (uint16_t)(directory_size / WD_ROYL_SECTOR_SIZE);
      entries[entry_count].copies = 1;
      entry_count++;
    }
  }

  if (entry_count > 1)
  {
    size_t output_index = 1;

    qsort(entries, entry_count, sizeof(*entries),
          wd_royl_compare_module_entries_ccc);
    for (i = 1; i < entry_count; i++)
    {
      if (entries[i].module_id != entries[output_index - 1].module_id)
      {
        if (output_index != i)
        {
          entries[output_index] = entries[i];
        }
        output_index++;
      }
    }
    entry_count = output_index;
  }

  directory_checksum_ok =
      wd_royl_module_checksum_ok_ccc(directory_data, directory_size);
  wd_royl_sha256_ccc(directory_data, directory_size, directory_sha256,
                     sizeof(directory_sha256));
  for (i = 0; i < entry_count; i++)
  {
    if (entries[i].module_id == WD_ROYL_MODULE_01)
    {
      total_bytes += directory_size;
    }
    else if (entries[i].sectors > 0)
    {
      total_bytes += (unsigned long long)entries[i].sectors *
                     WD_ROYL_SECTOR_SIZE;
    }
    else
    {
      total_bytes += WD_ROYL_SECTOR_SIZE;
    }
  }
  if (total_bytes == 0)
  {
    total_bytes = 1;
  }

  {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    if (tm != NULL)
    {
      strftime(created, sizeof(created), "%Y-%m-%d %H:%M:%S", tm);
    }
    else
    {
      g_strlcpy(created, "unknown", sizeof(created));
    }
  }

  manifest = g_key_file_new();
  g_key_file_set_string(manifest, "backup", "format",
                        WD_ROYL_SA_MANIFEST_FORMAT);
  g_key_file_set_integer(manifest, "backup", "version",
                         WD_ROYL_SA_MANIFEST_VERSION);
  g_key_file_set_string(manifest, "backup", "created", created);
  g_key_file_set_string(manifest, "backup", "model",
                        current_source_model_ccc);
  g_key_file_set_string(manifest, "backup", "serial",
                        current_source_serial_ccc);
  g_key_file_set_string(manifest, "backup", "firmware", "");
  g_key_file_set_string(manifest, "backup", "transport",
                        wd_royl_transport_name_ccc());
  g_key_file_set_integer(manifest, "backup", "directory_module",
                         WD_ROYL_MODULE_01);
  g_key_file_set_integer(manifest, "backup", "directory_size",
                         (gint)directory_size);
  g_key_file_set_string(manifest, "backup", "directory_sha256",
                        directory_sha256);
  g_key_file_set_boolean(manifest, "backup", "directory_checksum_ok",
                         directory_checksum_ok);
  g_key_file_set_integer(manifest, "backup", "module_count",
                         (gint)entry_count);

  summary = g_string_new(NULL);
  g_string_append(summary, "WD ROYL SA Backup\n");
  g_string_append_printf(summary, "Created: %s\n", created);
  g_string_append_printf(summary, "Model: %s\n", current_source_model_ccc);
  g_string_append_printf(summary, "Serial: %s\n", current_source_serial_ccc);
  g_string_append_printf(summary, "Transport: %s\n",
                         wd_royl_transport_name_ccc());
  g_string_append_printf(summary,
                         "Directory module 0x0001: %zu bytes, checksum %s\n",
                         directory_size,
                         directory_checksum_ok ? "ok" : "MISMATCH");
  g_string_append_printf(summary, "Modules listed: %zu\n\n", entry_count);

  memset(&progress_state, 0, sizeof(progress_state));
  progress_state.count = (unsigned int)entry_count;
  progress_state.total = total_bytes;
  wd_royl_sa_backup_progress_state_ccc = &progress_state;
  wd_royl_progress_update_ccc(_("Backing up service area modules"), 0,
                              total_bytes);

  for (i = 0; i < entry_count; i++)
  {
    uint8_t *module_data = NULL;
    size_t module_size = 0;
    bool module_from_directory = false;
    bool module_read_ok = false;
    char group_name[32];
    char file_name[32] = "";
    char module_sha256[65] = "";
    char error_text[WD_ROYL_ERROR_SIZE] = "";
    char human_size[64];
    char size_text[64];
    const char *status = "ok";
    uint32_t stored_checksum = 0;
    uint32_t calculated_checksum = 0;
    bool checksum_ok = false;
    unsigned long long span;
    uint16_t module_id = entries[i].module_id;

    progress_state.index = (unsigned int)i + 1;
    progress_state.module = module_id;

    if (module_id == WD_ROYL_MODULE_01)
    {
      module_data = directory_data;
      module_size = directory_size;
      module_from_directory = true;
      module_read_ok = true;
      wd_royl_sa_backup_progress_callback_ccc(
          _("Saving module directory"), 0, directory_size);
    }
    else if (wd_royl_read_module_ccc(
                 module_id, &module_data, &module_size,
                 wd_royl_sa_backup_progress_callback_ccc) == 0)
    {
      module_read_ok = true;
    }
    else
    {
      snprintf(error_text, sizeof(error_text), "%s",
               wd_royl_last_error_ccc());
      status = "read_failed";
      read_failed_count++;
    }

    if (module_read_ok)
    {
      if (wd_royl_validate_module_header_ccc(module_data, module_size,
                                             module_id) != 0)
      {
        snprintf(error_text, sizeof(error_text), "%s",
                 wd_royl_last_error_ccc());
        status = "header_invalid";
        read_failed_count++;
      }
      else
      {
        stored_checksum = wd_royl_get_module_checksum_ccc(module_data);
        calculated_checksum =
            wd_royl_calculate_checksum_ccc(module_data, module_size);
        checksum_ok = stored_checksum == calculated_checksum;
        if (!checksum_ok)
        {
          checksum_mismatch_count++;
        }
        wd_royl_sha256_ccc(module_data, module_size, module_sha256,
                           sizeof(module_sha256));
        snprintf(file_name, sizeof(file_name), "module_%04x.bin", module_id);
        {
          char file_path[PATH_MAX];

          wd_royl_join_path_ccc(file_path, sizeof(file_path),
                                backup_directory, file_name);
          if (wd_royl_write_file_ccc(file_path, module_data, module_size) != 0)
          {
            snprintf(error_text, sizeof(error_text), "%s",
                     wd_royl_last_error_ccc());
            status = "file_write_failed";
            file_write_failed_count++;
            file_name[0] = '\0';
          }
        }
      }
    }

    snprintf(group_name, sizeof(group_name), "module.%04x", module_id);
    g_key_file_set_integer(manifest, group_name, "id", module_id);
    g_key_file_set_string(manifest, group_name, "file", file_name);
    g_key_file_set_string(manifest, group_name, "status", status);
    g_key_file_set_integer(manifest, group_name, "directory_sectors",
                           entries[i].sectors);
    g_key_file_set_integer(manifest, group_name, "directory_copies",
                           entries[i].copies);
    wd_royl_set_hex_ccc(manifest, group_name, "directory_location1",
                        entries[i].location1);
    wd_royl_set_hex_ccc(manifest, group_name, "directory_location2",
                        entries[i].location2);
    g_key_file_set_integer(manifest, group_name, "read_size",
                           (gint)module_size);
    g_key_file_set_string(manifest, group_name, "sha256", module_sha256);
    wd_royl_set_hex_ccc(manifest, group_name, "stored_checksum",
                        stored_checksum);
    wd_royl_set_hex_ccc(manifest, group_name, "calculated_checksum",
                        calculated_checksum);
    g_key_file_set_boolean(manifest, group_name, "checksum_ok", checksum_ok);
    {
      char *clean_error = g_strdup(error_text);

      g_strdelimit(clean_error, "\r\n", ' ');
      g_key_file_set_string(manifest, group_name, "error", clean_error);
      g_free(clean_error);
    }

    if (strcmp(status, "ok") == 0)
    {
      ok_count++;
    }
    else
    {
      failed_count++;
    }

    if (module_size > 0)
    {
      wd_royl_format_size_ccc(module_size, human_size, sizeof(human_size));
      g_strlcpy(size_text, human_size, sizeof(size_text));
    }
    else
    {
      g_strlcpy(size_text, "-", sizeof(size_text));
    }
    g_string_append_printf(
        summary, "0x%04x  %-10s  %-16s  checksum=%s  sha256=%s\n", module_id,
        size_text, status,
        module_size == 0 ? "-" : (checksum_ok ? "ok" : "mismatch"),
        module_sha256[0] != '\0' ? module_sha256 : "-");
    if (error_text[0] != '\0')
    {
      char *clean_error = g_strdup(error_text);

      g_strdelimit(clean_error, "\r\n", ' ');
      g_string_append_printf(summary, "         %s\n", clean_error);
      g_free(clean_error);
    }

    if (module_id == WD_ROYL_MODULE_01)
    {
      span = directory_size;
    }
    else if (entries[i].sectors > 0)
    {
      span = (unsigned long long)entries[i].sectors * WD_ROYL_SECTOR_SIZE;
    }
    else
    {
      span = WD_ROYL_SECTOR_SIZE;
    }
    if (module_size > 0 && strcmp(status, "ok") == 0)
    {
      span = module_size;
    }
    progress_state.completed += span;
    if (progress_state.completed > progress_state.total)
    {
      progress_state.completed = progress_state.total;
    }
    wd_royl_progress_update_ccc(NULL, progress_state.completed,
                                progress_state.total);

    if (!module_from_directory)
    {
      free(module_data);
    }
  }

  wd_royl_sa_backup_progress_state_ccc = NULL;
  g_string_append_printf(
      summary,
      "\nBacked up: %u\nFailed: %u\nRead failures: %u\nFile write failures: "
      "%u\nChecksum mismatches: %u\n",
      ok_count, failed_count, read_failed_count, file_write_failed_count,
      checksum_mismatch_count);
  g_key_file_set_integer(manifest, "backup", "backup_ok_count",
                         (gint)ok_count);
  g_key_file_set_integer(manifest, "backup", "backup_failed_count",
                         (gint)failed_count);
  g_key_file_set_integer(manifest, "backup", "read_failed_count",
                         (gint)read_failed_count);
  g_key_file_set_integer(manifest, "backup", "file_write_failed_count",
                         (gint)file_write_failed_count);
  g_key_file_set_integer(manifest, "backup", "checksum_mismatch_count",
                         (gint)checksum_mismatch_count);

  wd_royl_join_path_ccc(manifest_path, sizeof(manifest_path),
                        backup_directory, WD_ROYL_SA_MANIFEST_NAME);
  {
    GError *gerror = NULL;
    gsize manifest_length = 0;
    char *manifest_data = g_key_file_to_data(manifest, &manifest_length, NULL);

    if (manifest_data == NULL ||
        !g_file_set_contents(manifest_path, manifest_data,
                             (gssize)manifest_length, &gerror))
    {
      snprintf(failure_message, sizeof(failure_message),
               _("The module files were written to:\n%s\n\nbut the backup "
                 "manifest could not be saved:\n%s\n\n%s"),
               backup_directory, manifest_path,
               gerror != NULL ? gerror->message : _("Unknown error"));
      if (gerror != NULL)
      {
        g_error_free(gerror);
      }
      g_free(manifest_data);
      goto cleanup;
    }
    g_free(manifest_data);
  }

  wd_royl_join_path_ccc(summary_path, sizeof(summary_path), backup_directory,
                        WD_ROYL_SA_SUMMARY_NAME);
  {
    GError *gerror = NULL;

    summary_written =
        g_file_set_contents(summary_path, summary->str, -1, &gerror);
    if (gerror != NULL)
    {
      g_error_free(gerror);
    }
  }

  progress_active = false;
  wd_royl_progress_end_ccc();
  source_open = false;
  wd_royl_end_source_access_ccc(temporary_source_access);

  if (failed_count == 0 && checksum_mismatch_count == 0)
  {
    wd_royl_show_message_ccc(
        summary_written ? GTK_MESSAGE_INFO : GTK_MESSAGE_WARNING,
        _("WD ROYL SA Backup"),
        _("SA backup completed.\n\nDrive: %s\nSerial: %s\nModules backed up: "
          "%u of %zu\nFolder:\n%s\n\nManifest:\n%s%s"),
        current_source_model_ccc[0] != '\0' ? current_source_model_ccc
                                            : _("Unknown"),
        current_source_serial_ccc[0] != '\0' ? current_source_serial_ccc
                                             : _("Unknown"),
        ok_count, entry_count, backup_directory, manifest_path,
        summary_written ? "" : _("\n\nWarning: summary.txt was not written."));
  }
  else if (failed_count == 0)
  {
    wd_royl_show_message_ccc(
        GTK_MESSAGE_WARNING, _("WD ROYL SA Backup"),
        _("SA backup completed, but some modules have checksum mismatches.\n\n"
          "Drive: %s\nSerial: %s\nModules backed up: %u of %zu\nChecksum "
          "mismatches: %u\nFolder:\n%s\n\nManifest:\n%s\n\nSee summary.txt "
          "for the affected modules.%s"),
        current_source_model_ccc[0] != '\0' ? current_source_model_ccc
                                            : _("Unknown"),
        current_source_serial_ccc[0] != '\0' ? current_source_serial_ccc
                                             : _("Unknown"),
        ok_count, entry_count, checksum_mismatch_count, backup_directory,
        manifest_path,
        summary_written ? "" : _("\n\nWarning: summary.txt was not written."));
  }
  else
  {
    wd_royl_show_message_ccc(
        GTK_MESSAGE_WARNING, _("WD ROYL SA Backup"),
        _("SA backup completed with failures.\n\nDrive: %s\nSerial: %s\n"
          "Modules backed up: %u of %zu\nFailed: %u\nChecksum mismatches: "
          "%u\nFolder:\n%s\n\nManifest:\n%s\n\nSee summary.txt for the "
          "failed module details.%s"),
        current_source_model_ccc[0] != '\0' ? current_source_model_ccc
                                            : _("Unknown"),
        current_source_serial_ccc[0] != '\0' ? current_source_serial_ccc
                                             : _("Unknown"),
        ok_count, entry_count, failed_count, checksum_mismatch_count,
        backup_directory, manifest_path,
        summary_written ? "" : _("\n\nWarning: summary.txt was not written."));
  }

cleanup:
  wd_royl_sa_backup_progress_state_ccc = NULL;
  if (progress_active)
  {
    wd_royl_progress_end_ccc();
  }
  if (source_open)
  {
    wd_royl_end_source_access_ccc(temporary_source_access);
  }
  if (failure_message[0] != '\0')
  {
    wd_royl_show_message_ccc(GTK_MESSAGE_ERROR, _("WD ROYL SA Backup"), "%s",
                             failure_message);
  }
  if (manifest != NULL)
  {
    g_key_file_free(manifest);
  }
  if (summary != NULL)
  {
    g_string_free(summary, TRUE);
  }
  free(entries);
  free(directory_data);
  g_free(parent_directory);
}

void run_wd_royl_restore_sa_ccc(void)
{
  char *manifest_path = NULL;
  wd_royl_sa_manifest_ccc manifest;
  char load_error[1024] = "";
  char pre_restore_directory[PATH_MAX] = "";
  char restore_log_path[PATH_MAX] = "";
  char restore_log_error[512] = "";
  char failure_message[2048] = "";
  char started[64] = "";
  size_t selected_count;
  size_t module_index = 0;
  unsigned int restored_count = 0;
  unsigned int failed_count = 0;
  bool temporary_source_access = false;
  bool source_open = false;
  bool progress_active = false;
  bool stopped = false;
  GString *restore_log = NULL;
  GString *failure_summary = NULL;
  wd_royl_sa_restore_progress_ccc_t progress_state;
  size_t i;

  memset(&manifest, 0, sizeof(manifest));
  if (!wd_royl_operation_ready_ccc())
  {
    return;
  }
  manifest_path = wd_royl_choose_file_ccc(
      _("Select a WD ROYL SA backup manifest"), false, NULL);
  if (manifest_path == NULL)
  {
    goto cleanup;
  }
  if (wd_royl_load_sa_manifest_ccc(manifest_path, &manifest, load_error,
                                   sizeof(load_error)) != 0)
  {
    wd_royl_show_message_ccc(GTK_MESSAGE_ERROR, _("WD ROYL SA Restore"), "%s",
                             load_error);
    goto cleanup;
  }

  selected_count = wd_royl_restore_selection_ccc(&manifest);
  if (selected_count == 0)
  {
    goto cleanup;
  }
  if (!wd_royl_restore_identity_ok_ccc(manifest.model, manifest.serial))
  {
    goto cleanup;
  }

  wd_royl_make_pre_restore_path_ccc(manifest.directory, pre_restore_directory,
                                    sizeof(pre_restore_directory));
  if (!wd_royl_confirm_ccc(
          _("Restore WD ROYL SA"), _("Restore"),
          _("Restore %zu selected module(s) to the current source drive?\n\n"
            "Backup drive: %s / %s\nCurrent source: %s / %s\n\n"
            "Each current module will be backed up to:\n%s\n\n"
            "Module 0x0001 is restored last. Every write will be verified.\n\n"
            "Continue?"),
          selected_count,
          manifest.model[0] != '\0' ? manifest.model : _("Unknown"),
          manifest.serial[0] != '\0' ? manifest.serial : _("Unknown"),
          current_source_model_ccc[0] != '\0' ? current_source_model_ccc
                                              : _("Unknown"),
          current_source_serial_ccc[0] != '\0' ? current_source_serial_ccc
                                               : _("Unknown"),
          pre_restore_directory))
  {
    goto cleanup;
  }

  wd_royl_dialog_parent_override_ccc = NULL;
  if (!wd_royl_begin_source_access_ccc(&temporary_source_access))
  {
    goto cleanup;
  }
  source_open = true;
  if (g_mkdir_with_parents(pre_restore_directory, 0755) != 0)
  {
    snprintf(failure_message, sizeof(failure_message),
             _("Unable to create the pre-restore backup folder:\n%s\n\n%s"),
             pre_restore_directory, g_strerror(errno));
    goto cleanup;
  }

  wd_royl_progress_begin_ccc(_("WD ROYL SA Restore"), _("Preparing restore"));
  progress_active = true;
  {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    if (tm != NULL)
    {
      strftime(started, sizeof(started), "%Y-%m-%d %H:%M:%S", tm);
    }
    else
    {
      g_strlcpy(started, "unknown", sizeof(started));
    }
  }
  restore_log = g_string_new(NULL);
  failure_summary = g_string_new(NULL);
  g_string_append(restore_log, "WD ROYL SA Restore\n");
  g_string_append_printf(restore_log, "Started: %s\n", started);
  g_string_append_printf(restore_log, "Manifest: %s\n", manifest.path);
  g_string_append_printf(restore_log, "Backup drive: %s / %s\n",
                         manifest.model, manifest.serial);
  g_string_append_printf(restore_log, "Selected modules: %zu\n",
                         selected_count);
  g_string_append_printf(restore_log,
                         "Pre-restore module backups: %s\n\n",
                         pre_restore_directory);

  memset(&progress_state, 0, sizeof(progress_state));
  progress_state.count = selected_count;
  wd_royl_sa_restore_progress_state_ccc = &progress_state;

  for (i = 0; i < manifest.entry_count; i++)
  {
    wd_royl_sa_restore_entry_ccc *entry = &manifest.entries[i];
    char stage[256];
    char error_text[WD_ROYL_ERROR_SIZE] = "";
    char pre_restore_file[PATH_MAX] = "";
    bool fatal = false;
    int result;

    if (!entry->selected)
    {
      continue;
    }
    module_index++;
    // The directory module points at the other modules. If any other selected
    // module failed, leave the current directory in place instead of writing a
    // directory that may point at modules that were not restored.
    if (entry->module == WD_ROYL_MODULE_01 && failed_count > 0)
    {
      char *clean_error = g_strdup(
          _("Directory module not restored because another selected module "
            "failed."));

      g_strdelimit(clean_error, "\r\n", ' ');
      failed_count++;
      g_string_append_printf(restore_log, "0x0001 SKIPPED: %s\n",
                             clean_error);
      g_string_append_printf(failure_summary, "0x0001: %s\n", clean_error);
      g_free(clean_error);
      continue;
    }
    if (!entry->available)
    {
      char *clean_error = g_strdup(
          entry->error[0] != '\0' ? entry->error
                                  : wd_royl_restore_status_text_ccc(
                                        entry->status));

      g_strdelimit(clean_error, "\r\n", ' ');
      failed_count++;
      g_string_append_printf(restore_log, "0x%04x SKIPPED: %s\n",
                             entry->module, clean_error);
      g_string_append_printf(failure_summary, "0x%04x: %s\n", entry->module,
                             clean_error);
      g_free(clean_error);
      continue;
    }

    progress_state.index = (unsigned int)module_index;
    progress_state.module = entry->module;
    snprintf(stage, sizeof(stage),
             _("Module 0x%04x (%zu of %zu): checking backup file"),
             entry->module, module_index, selected_count);
    wd_royl_progress_update_ccc(stage, 0, 1);

    result = wd_royl_restore_one_module_ccc(
        entry, pre_restore_directory, pre_restore_file,
        sizeof(pre_restore_file), &fatal, error_text, sizeof(error_text),
        wd_royl_sa_restore_progress_callback_ccc);
    if (result == 0)
    {
      restored_count++;
      g_string_append_printf(
          restore_log, "0x%04x OK: previous module saved to %s\n",
          entry->module, pre_restore_file);
      continue;
    }

    {
      char *clean_error =
          g_strdup(error_text[0] != '\0' ? error_text : _("Unknown error"));

      g_strdelimit(clean_error, "\r\n", ' ');
      failed_count++;
      g_string_append_printf(restore_log, "0x%04x FAILED: %s\n",
                             entry->module, clean_error);
      if (pre_restore_file[0] != '\0')
      {
        g_string_append_printf(restore_log, "  previous module: %s\n",
                               pre_restore_file);
      }
      g_string_append_printf(failure_summary, "0x%04x: %s\n", entry->module,
                             clean_error);
      g_free(clean_error);
    }
    if (fatal)
    {
      stopped = true;
      g_string_append(
          restore_log,
          "Restore stopped after a write or verification failure.\n");
      break;
    }
  }

  wd_royl_sa_restore_progress_state_ccc = NULL;
  progress_active = false;
  wd_royl_progress_end_ccc();

  wd_royl_join_path_ccc(restore_log_path, sizeof(restore_log_path),
                        pre_restore_directory, WD_ROYL_SA_RESTORE_LOG_NAME);
  {
    GError *gerror = NULL;

    if (!g_file_set_contents(restore_log_path, restore_log->str, -1,
                             &gerror))
    {
      g_strlcpy(restore_log_error,
                gerror != NULL ? gerror->message : _("Unknown error"),
                sizeof(restore_log_error));
      if (gerror != NULL)
      {
        g_error_free(gerror);
      }
    }
  }

  source_open = false;
  wd_royl_end_source_access_ccc(temporary_source_access);
  {
    char *failures = NULL;
    char *message;
    size_t completed_selected =
        (size_t)restored_count + (size_t)failed_count;
    size_t remaining = selected_count > completed_selected
                           ? selected_count - completed_selected
                           : 0;

    if (failure_summary->len > 0)
    {
      failures = g_strndup(failure_summary->str, 3000);
      if (failure_summary->len > 3000)
      {
        char *truncated = g_strconcat(
            failures, _("\n... see restore.txt for the full list."), NULL);

        g_free(failures);
        failures = truncated;
      }
    }
    if (stopped && remaining > 0)
    {
      char *with_remaining = g_strdup_printf(
          _("%s\n\nThe restore stopped before %zu selected module(s) were "
            "attempted."),
          failures != NULL ? failures : "", remaining);

      g_free(failures);
      failures = with_remaining;
    }
    if (restore_log_error[0] != '\0')
    {
      char *with_log_error = g_strdup_printf(
          _("%s\n\nWarning: restore.txt could not be written: %s"),
          failures != NULL ? failures : "", restore_log_error);

      g_free(failures);
      failures = with_log_error;
    }

    message = g_strdup_printf(
        _("Restore finished.\n\nRestored and verified: %u\nFailed or "
          "skipped: %u\nPrevious module backups:\n%s%s"),
        restored_count, failed_count, pre_restore_directory,
        failures != NULL && failures[0] != '\0' ? "\n\n" : "");
    if (failures != NULL && failures[0] != '\0')
    {
      char *with_failures = g_strconcat(message, failures, NULL);

      g_free(message);
      message = with_failures;
    }
    if (failed_count > 0 || stopped)
    {
      char *with_recovery = g_strconcat(
          message,
          _("\n\nIf a module was not written correctly, use the SA Module "
            "Editor to write the matching module_XXXX.bin file from the "
            "previous module backups folder."),
          NULL);

      g_free(message);
      message = with_recovery;
    }
    wd_royl_show_message_ccc(
        failed_count == 0 && !stopped && restore_log_error[0] == '\0'
            ? GTK_MESSAGE_INFO
            : GTK_MESSAGE_WARNING,
        _("WD ROYL SA Restore"), "%s", message);
    g_free(message);
    g_free(failures);
  }

cleanup:
  wd_royl_sa_restore_progress_state_ccc = NULL;
  if (progress_active)
  {
    wd_royl_progress_end_ccc();
  }
  if (source_open)
  {
    wd_royl_end_source_access_ccc(temporary_source_access);
  }
  if (restore_log != NULL)
  {
    g_string_free(restore_log, TRUE);
  }
  if (failure_summary != NULL)
  {
    g_string_free(failure_summary, TRUE);
  }
  if (failure_message[0] != '\0')
  {
    wd_royl_show_message_ccc(GTK_MESSAGE_ERROR, _("WD ROYL SA Restore"), "%s",
                             failure_message);
  }
  wd_royl_sa_manifest_clear_ccc(&manifest);
  g_free(manifest_path);
}

// ---------------------------------------------------------------------------
// Service area module editor
// ---------------------------------------------------------------------------

#define WD_ROYL_EDITOR_PAGE_SIZE 4096
#define WD_ROYL_EDITOR_BYTES_PER_ROW 16
#define WD_ROYL_EDITOR_MODEL_COLUMNS 18

typedef struct
{
  GtkWidget *dialog;
  GtkWidget *module_entry;
  GtkWidget *info_label;
  GtkWidget *status_label;
  GtkWidget *page_label;
  GtkWidget *offset_entry;
  GtkWidget *tree_view;
  GtkListStore *store;
  GtkWidget *prev_button;
  GtkWidget *next_button;
  GtkWidget *save_button;
  GtkWidget *write_button;
  uint8_t *data;
  size_t size;
  size_t page_start;
  uint16_t module;
  bool loaded;
  bool dirty;
} wd_royl_editor_ccc_t;

static void wd_royl_editor_message_ccc(wd_royl_editor_ccc_t *editor,
                                       GtkMessageType type,
                                       const char *title,
                                       const char *format, ...)
{
  va_list args;
  char *text;
  GtkWidget *dialog;
  GtkWindow *parent = GTK_WINDOW(main_window_ccc);

  if (editor != NULL && editor->dialog != NULL)
  {
    parent = GTK_WINDOW(editor->dialog);
  }

  va_start(args, format);
  text = g_strdup_vprintf(format, args);
  va_end(args);

  dialog = gtk_message_dialog_new(parent, GTK_DIALOG_DESTROY_WITH_PARENT, type,
                                  GTK_BUTTONS_OK, "%s", text);
  gtk_window_set_title(GTK_WINDOW(dialog), title);
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
  g_free(text);
}

static bool wd_royl_editor_confirm_ccc(wd_royl_editor_ccc_t *editor,
                                       const char *title,
                                       const char *accept_label,
                                       const char *format, ...)
{
  va_list args;
  char *text;
  GtkWidget *dialog;
  int response;
  GtkWindow *parent = GTK_WINDOW(main_window_ccc);

  if (editor != NULL && editor->dialog != NULL)
  {
    parent = GTK_WINDOW(editor->dialog);
  }

  va_start(args, format);
  text = g_strdup_vprintf(format, args);
  va_end(args);

  dialog = gtk_message_dialog_new(parent, GTK_DIALOG_DESTROY_WITH_PARENT,
                                  GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE,
                                  "%s", text);
  gtk_window_set_title(GTK_WINDOW(dialog), title);
  gtk_dialog_add_buttons(GTK_DIALOG(dialog), GTK_STOCK_CANCEL,
                         GTK_RESPONSE_CANCEL, accept_label,
                         GTK_RESPONSE_ACCEPT, NULL);
  gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
  response = gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
  g_free(text);
  return response == GTK_RESPONSE_ACCEPT;
}

static void wd_royl_editor_report_error_ccc(wd_royl_editor_ccc_t *editor,
                                            const char *title,
                                            const char *prefix)
{
  wd_royl_editor_message_ccc(editor, GTK_MESSAGE_ERROR, title, "%s\n\n%s",
                             prefix, wd_royl_last_error_ccc());
}

static bool wd_royl_parse_number_ccc(const char *text,
                                     unsigned long long maximum,
                                     unsigned long long *value)
{
  const char *start;
  const char *scan;
  char *end;
  unsigned long long parsed;
  int base = 10;

  if (text == NULL || value == NULL)
  {
    return false;
  }
  start = text;
  while (isspace((unsigned char)*start))
  {
    start++;
  }
  if (start[0] == '\0')
  {
    return false;
  }
  if (start[0] == '0' && (start[1] == 'x' || start[1] == 'X'))
  {
    base = 16;
  }
  else
  {
    for (scan = start; *scan != '\0'; scan++)
    {
      if (isxdigit((unsigned char)*scan) && !isdigit((unsigned char)*scan))
      {
        base = 16;
        break;
      }
    }
  }

  parsed = strtoull(start, &end, base);
  if (end == start)
  {
    return false;
  }
  while (isspace((unsigned char)*end))
  {
    end++;
  }
  if (*end != '\0' || parsed > maximum)
  {
    return false;
  }
  *value = parsed;
  return true;
}

static char *wd_royl_editor_choose_file_ccc(wd_royl_editor_ccc_t *editor,
                                            bool save, const char *title,
                                            const char *default_name)
{
  GtkWidget *dialog;
  char *filename = NULL;

  dialog = gtk_file_chooser_dialog_new(
      title, editor != NULL && editor->dialog != NULL
                 ? GTK_WINDOW(editor->dialog)
                 : GTK_WINDOW(main_window_ccc),
      save ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN,
      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
      save ? GTK_STOCK_SAVE : GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
  if (save)
  {
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog),
                                                   TRUE);
    if (default_name != NULL)
    {
      gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog),
                                        default_name);
    }
  }
  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
  {
    filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
  }
  gtk_widget_destroy(dialog);
  return filename;
}

static void wd_royl_editor_update_info_ccc(wd_royl_editor_ccc_t *editor)
{
  char text[512];

  if (!editor->loaded)
  {
    gtk_label_set_text(
        GTK_LABEL(editor->info_label),
        _("No module is loaded. Enter a module number and select Read from "
          "Drive, or load a module file."));
    return;
  }

  uint32_t stored = wd_royl_get_module_checksum_ccc(editor->data);
  uint32_t calculated =
      wd_royl_calculate_checksum_ccc(editor->data, editor->size);
  uint16_t records = wd_royl_get_module_record_count_ccc(editor->data);

  snprintf(text, sizeof(text),
           _("Module 0x%04x  |  %zu bytes  |  %u records  |  checksum "
             "0x%08x (%s)%s"),
           editor->module, editor->size, records, stored,
           stored == calculated ? _("valid") : _("will be updated"),
           editor->dirty ? _("  |  modified") : "");
  gtk_label_set_text(GTK_LABEL(editor->info_label), text);
}

static void wd_royl_editor_update_navigation_ccc(
    wd_royl_editor_ccc_t *editor)
{
  size_t pages;
  size_t page;
  size_t end;
  char text[128];

  if (!editor->loaded)
  {
    gtk_label_set_text(GTK_LABEL(editor->page_label), _("No module loaded"));
    gtk_widget_set_sensitive(editor->prev_button, FALSE);
    gtk_widget_set_sensitive(editor->next_button, FALSE);
    gtk_widget_set_sensitive(editor->offset_entry, FALSE);
    gtk_widget_set_sensitive(editor->save_button, FALSE);
    gtk_widget_set_sensitive(editor->write_button, FALSE);
    return;
  }

  end = editor->page_start + WD_ROYL_EDITOR_PAGE_SIZE;
  if (end > editor->size)
  {
    end = editor->size;
  }
  pages = (editor->size + WD_ROYL_EDITOR_PAGE_SIZE - 1) /
          WD_ROYL_EDITOR_PAGE_SIZE;
  page = editor->page_start / WD_ROYL_EDITOR_PAGE_SIZE + 1;
  snprintf(text, sizeof(text), _("Offset 0x%08zx - 0x%08zx   Page %zu/%zu"),
           editor->page_start, end > 0 ? end - 1 : 0, page, pages);
  gtk_label_set_text(GTK_LABEL(editor->page_label), text);

  gtk_widget_set_sensitive(editor->prev_button, editor->page_start > 0);
  gtk_widget_set_sensitive(editor->next_button, end < editor->size);
  gtk_widget_set_sensitive(editor->offset_entry, TRUE);
  gtk_widget_set_sensitive(editor->save_button, TRUE);
  gtk_widget_set_sensitive(editor->write_button, TRUE);
}

static void wd_royl_editor_update_row_ccc(wd_royl_editor_ccc_t *editor,
                                          GtkTreeIter *iter,
                                          size_t row_start)
{
  char ascii[WD_ROYL_EDITOR_BYTES_PER_ROW + 1];
  int i;

  for (i = 0; i < WD_ROYL_EDITOR_BYTES_PER_ROW; i++)
  {
    size_t offset = row_start + (size_t)i;
    if (offset < editor->size)
    {
      char byte_text[4];
      uint8_t value = editor->data[offset];
      snprintf(byte_text, sizeof(byte_text), "%02X", value);
      gtk_list_store_set(editor->store, iter, 1 + i, byte_text, -1);
      ascii[i] = (value >= 0x20 && value <= 0x7e) ? (char)value : '.';
    }
    else
    {
      gtk_list_store_set(editor->store, iter, 1 + i, "", -1);
      ascii[i] = ' ';
    }
  }
  ascii[WD_ROYL_EDITOR_BYTES_PER_ROW] = '\0';
  gtk_list_store_set(editor->store, iter, 17, ascii, -1);
}

static void wd_royl_editor_load_page_ccc(wd_royl_editor_ccc_t *editor)
{
  GtkTreeIter iter;
  size_t end;
  size_t row_start;

  gtk_list_store_clear(editor->store);
  if (!editor->loaded)
  {
    wd_royl_editor_update_info_ccc(editor);
    wd_royl_editor_update_navigation_ccc(editor);
    return;
  }

  if (editor->page_start >= editor->size)
  {
    editor->page_start =
        ((editor->size - 1) / WD_ROYL_EDITOR_PAGE_SIZE) *
        WD_ROYL_EDITOR_PAGE_SIZE;
  }
  end = editor->page_start + WD_ROYL_EDITOR_PAGE_SIZE;
  if (end > editor->size)
  {
    end = editor->size;
  }

  for (row_start = editor->page_start; row_start < end;
       row_start += WD_ROYL_EDITOR_BYTES_PER_ROW)
  {
    char offset_text[16];
    snprintf(offset_text, sizeof(offset_text), "%08zx", row_start);
    gtk_list_store_append(editor->store, &iter);
    gtk_list_store_set(editor->store, &iter, 0, offset_text, -1);
    wd_royl_editor_update_row_ccc(editor, &iter, row_start);
  }

  wd_royl_editor_update_info_ccc(editor);
  wd_royl_editor_update_navigation_ccc(editor);
}

static void wd_royl_editor_set_data_ccc(wd_royl_editor_ccc_t *editor,
                                        uint8_t *data, size_t size,
                                        uint16_t module, bool dirty)
{
  free(editor->data);
  editor->data = data;
  editor->size = size;
  editor->module = module;
  editor->page_start = 0;
  editor->loaded = data != NULL && size > 0;
  editor->dirty = dirty;

  if (editor->loaded)
  {
    char module_text[16];
    snprintf(module_text, sizeof(module_text), "0x%04x", module);
    gtk_entry_set_text(GTK_ENTRY(editor->module_entry), module_text);
  }
  wd_royl_editor_load_page_ccc(editor);
}

static bool wd_royl_editor_confirm_discard_ccc(
    wd_royl_editor_ccc_t *editor)
{
  if (!editor->dirty)
  {
    return true;
  }
  return wd_royl_editor_confirm_ccc(
      editor, _("Discard changes?"), _("Discard"),
      _("The current module has unsaved changes.\n\nDiscard them?"));
}

static void wd_royl_editor_update_checksum_ccc(
    wd_royl_editor_ccc_t *editor)
{
  uint32_t previous;

  if (!editor->loaded)
  {
    return;
  }
  previous = wd_royl_get_module_checksum_ccc(editor->data);
  wd_royl_update_checksum_ccc(editor->data, editor->size);
  if (wd_royl_get_module_checksum_ccc(editor->data) != previous)
  {
    editor->dirty = true;
  }
  wd_royl_editor_load_page_ccc(editor);
}

static void wd_royl_editor_byte_edited_ccc(GtkCellRendererText *renderer,
                                           char *path, char *new_text,
                                           gpointer user_data)
{
  wd_royl_editor_ccc_t *editor = (wd_royl_editor_ccc_t *)user_data;
  GtkTreePath *tree_path;
  GtkTreeIter iter;
  int *indices;
  int row;
  int byte_index;
  int digits = 0;
  unsigned int value = 0;
  const char *scan = new_text;
  size_t offset;

  if (!editor->loaded)
  {
    return;
  }
  byte_index =
      GPOINTER_TO_INT(g_object_get_data(G_OBJECT(renderer), "byte_index"));
  tree_path = gtk_tree_path_new_from_string(path);
  if (tree_path == NULL ||
      !gtk_tree_model_get_iter(GTK_TREE_MODEL(editor->store), &iter,
                               tree_path))
  {
    if (tree_path != NULL)
    {
      gtk_tree_path_free(tree_path);
    }
    return;
  }
  indices = gtk_tree_path_get_indices(tree_path);
  row = indices != NULL ? indices[0] : -1;
  gtk_tree_path_free(tree_path);

  while (isspace((unsigned char)*scan))
  {
    scan++;
  }
  while (isxdigit((unsigned char)*scan))
  {
    char c = *scan;
    unsigned int digit;
    if (c >= '0' && c <= '9')
    {
      digit = (unsigned int)(c - '0');
    }
    else if (c >= 'a' && c <= 'f')
    {
      digit = (unsigned int)(c - 'a' + 10);
    }
    else
    {
      digit = (unsigned int)(c - 'A' + 10);
    }
    value = value * 16 + digit;
    digits++;
    scan++;
  }
  while (isspace((unsigned char)*scan))
  {
    scan++;
  }

  offset = editor->page_start +
           (size_t)row * WD_ROYL_EDITOR_BYTES_PER_ROW + (size_t)byte_index;
  if (row < 0 || offset >= editor->size || digits == 0 || digits > 2 ||
      *scan != '\0' || value > 0xff)
  {
    gdk_display_beep(gdk_display_get_default());
    gtk_label_set_text(GTK_LABEL(editor->status_label),
                       _("Invalid byte value. Use one or two hexadecimal "
                         "digits."));
    wd_royl_editor_update_row_ccc(editor, &iter,
                                  editor->page_start +
                                      (size_t)row *
                                          WD_ROYL_EDITOR_BYTES_PER_ROW);
    return;
  }

  if (editor->data[offset] != (uint8_t)value)
  {
    editor->data[offset] = (uint8_t)value;
    editor->dirty = true;
    wd_royl_editor_update_row_ccc(editor, &iter,
                                  editor->page_start +
                                      (size_t)row *
                                          WD_ROYL_EDITOR_BYTES_PER_ROW);
    wd_royl_editor_update_info_ccc(editor);
    {
      char status[128];
      snprintf(status, sizeof(status), _("Modified byte at offset 0x%08zx."),
               offset);
      gtk_label_set_text(GTK_LABEL(editor->status_label), status);
    }
  }
  else
  {
    wd_royl_editor_update_row_ccc(editor, &iter,
                                  editor->page_start +
                                      (size_t)row *
                                          WD_ROYL_EDITOR_BYTES_PER_ROW);
  }
}

static void wd_royl_editor_add_column_ccc(wd_royl_editor_ccc_t *editor,
                                          const char *title, int model_column,
                                          int width, bool editable,
                                          float xalign)
{
  GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
  GtkTreeViewColumn *column;

  g_object_set(renderer, "font", "Monospace 10", "xalign", xalign, NULL);
  if (editable)
  {
    g_object_set(renderer, "editable", TRUE, NULL);
    g_object_set_data(G_OBJECT(renderer), "byte_index",
                      GINT_TO_POINTER(model_column - 1));
    // Run after the tree view has stored the entered text in the model so
    // invalid values can be replaced with the original byte.
    g_signal_connect_after(renderer, "edited",
                           G_CALLBACK(wd_royl_editor_byte_edited_ccc),
                           editor);
  }
  column = gtk_tree_view_column_new_with_attributes(title, renderer, "text",
                                                    model_column, NULL);
  gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_fixed_width(column, width);
  gtk_tree_view_column_set_expand(column, FALSE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(editor->tree_view), column);
}

enum
{
  WD_ROYL_PICKER_MODULE_COLUMN = 0,
  WD_ROYL_PICKER_NAME_COLUMN,
  WD_ROYL_PICKER_SIZE_COLUMN,
  WD_ROYL_PICKER_COPIES_COLUMN,
  WD_ROYL_PICKER_LOCATION1_COLUMN,
  WD_ROYL_PICKER_LOCATION2_COLUMN,
  WD_ROYL_PICKER_HEALTH_COLUMN,
  WD_ROYL_PICKER_HEALTH_WEIGHT_COLUMN,
  WD_ROYL_PICKER_MODULE_ID_COLUMN,
  WD_ROYL_PICKER_SIZE_SECTORS_COLUMN,
  WD_ROYL_PICKER_COPIES_VALUE_COLUMN,
  WD_ROYL_PICKER_LOCATION1_VALUE_COLUMN,
  WD_ROYL_PICKER_LOCATION2_VALUE_COLUMN,
  WD_ROYL_PICKER_WARNING_COLUMN,
  WD_ROYL_PICKER_TOOLTIP_COLUMN,
  WD_ROYL_PICKER_COLUMN_COUNT
};

typedef struct
{
  GtkWidget *dialog;
  GtkWidget *tree_view;
  GtkListStore *store;
  GtkTreeModel *filter;
  GtkTreeModel *sort;
  GtkWidget *manual_entry;
  GtkWidget *warnings_only;
  GtkWidget *details_label;
  uint16_t selected_module;
} wd_royl_module_picker_ccc_t;

static void wd_royl_format_size_ccc(unsigned long long bytes, char *buffer,
                                    size_t buffer_size)
{
  if (bytes >= 1024ULL * 1024ULL)
  {
    snprintf(buffer, buffer_size, "%.1f MiB",
             (double)bytes / (1024.0 * 1024.0));
  }
  else if (bytes >= 1024ULL)
  {
    snprintf(buffer, buffer_size, "%.1f KiB", (double)bytes / 1024.0);
  }
  else
  {
    snprintf(buffer, buffer_size, "%llu B", bytes);
  }
}

static int wd_royl_compare_module_entries_ccc(const void *a, const void *b)
{
  const wd_royl_module_entry_ccc *entry_a =
      (const wd_royl_module_entry_ccc *)a;
  const wd_royl_module_entry_ccc *entry_b =
      (const wd_royl_module_entry_ccc *)b;

  if (entry_a->module_id < entry_b->module_id)
  {
    return -1;
  }
  if (entry_a->module_id > entry_b->module_id)
  {
    return 1;
  }
  return 0;
}

static void wd_royl_module_entry_health_ccc(
    const wd_royl_module_entry_ccc *entries, size_t entry_count,
    size_t entry_index, char *health, size_t health_size, bool *warning)
{
  const wd_royl_module_entry_ccc *entry = &entries[entry_index];
  bool duplicate_id = false;
  bool shared_location = false;
  size_t i;

  *warning = false;
  if (entry->module_id == WD_ROYL_MODULE_01)
  {
    g_strlcpy(health, _("Directory"), health_size);
    return;
  }

  for (i = 0; i < entry_count; i++)
  {
    if (i == entry_index)
    {
      continue;
    }
    if (entries[i].module_id == entry->module_id)
    {
      duplicate_id = true;
    }
    if (entry->location1 != 0 &&
        (entries[i].location1 == entry->location1 ||
         entries[i].location2 == entry->location1))
    {
      shared_location = true;
    }
    if (entry->location2 != 0 &&
        (entries[i].location1 == entry->location2 ||
         entries[i].location2 == entry->location2))
    {
      shared_location = true;
    }
  }

  if (duplicate_id)
  {
    g_strlcpy(health, _("Duplicate module ID"), health_size);
    *warning = true;
    return;
  }
  if (entry->sectors == 0)
  {
    g_strlcpy(health, _("Missing size"), health_size);
    *warning = true;
    return;
  }
  if (entry->copies == 0)
  {
    g_strlcpy(health, _("No copies"), health_size);
    *warning = true;
    return;
  }
  if (entry->copies > 2)
  {
    g_strlcpy(health, _("More than 2 copies"), health_size);
    *warning = true;
    return;
  }
  if (entry->copies >= 2)
  {
    if (entry->location1 == 0 && entry->location2 == 0)
    {
      g_strlcpy(health, _("Copy locations not reported"), health_size);
      *warning = true;
      return;
    }
    if (entry->location1 == 0)
    {
      g_strlcpy(health, _("Copy 1 location missing"), health_size);
      *warning = true;
      return;
    }
    if (entry->location2 == 0)
    {
      g_strlcpy(health, _("Copy 2 location missing"), health_size);
      *warning = true;
      return;
    }
    if (entry->location1 == entry->location2)
    {
      g_strlcpy(health, _("Copy 1/2 same address"), health_size);
      *warning = true;
      return;
    }
  }
  if (shared_location)
  {
    g_strlcpy(health, _("Copy location shared"), health_size);
    *warning = true;
    return;
  }
  if (entry->copies == 1)
  {
    g_strlcpy(health, _("Single copy"), health_size);
    return;
  }
  g_strlcpy(health, _("OK"), health_size);
}

// Only module IDs with a confirmed public name are listed here. Unknown IDs
// are deliberately left unlabeled. Sources: HDDSuperTool
// wd_royl_process_sa (0x0001), the Rossmann WD module glossary
// (0x0002, 0x000a, 0x0032, 0x0033, 0x0047, 0x0102, 0x0103), and the
// DFL-WDII manual (0x0031).
static const char *wd_royl_known_module_name_ccc(uint16_t module_id)
{
  static const struct
  {
    uint16_t module_id;
    const char *name;
  } known_modules[] = {
      {WD_ROYL_MODULE_01, N_("Directory")},
      {WD_ROYL_MODULE_02, N_("Configuration")},
      {0x000a, N_("Head map")},
      {0x0031, N_("P-list translator")},
      {0x0032, N_("G-list")},
      {0x0033, N_("P-list")},
      {0x0047, N_("Read adaptives")},
      {0x0102, N_("Head map (SA)")},
      {0x0103, N_("Read adaptives (SA)")},
  };
  size_t i;

  for (i = 0; i < G_N_ELEMENTS(known_modules); i++)
  {
    if (known_modules[i].module_id == module_id)
    {
      return _(known_modules[i].name);
    }
  }
  return NULL;
}

static gboolean wd_royl_picker_filter_visible_ccc(GtkTreeModel *model,
                                                  GtkTreeIter *iter,
                                                  gpointer user_data)
{
  wd_royl_module_picker_ccc_t *picker =
      (wd_royl_module_picker_ccc_t *)user_data;
  gboolean warning = FALSE;

  if (picker->warnings_only == NULL ||
      !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(picker->warnings_only)))
  {
    return TRUE;
  }
  gtk_tree_model_get(model, iter, WD_ROYL_PICKER_WARNING_COLUMN, &warning, -1);
  return warning;
}

static void wd_royl_picker_warnings_only_toggled_ccc(GtkToggleButton *button,
                                                     gpointer user_data)
{
  wd_royl_module_picker_ccc_t *picker =
      (wd_royl_module_picker_ccc_t *)user_data;

  (void)button;
  if (picker->filter != NULL)
  {
    gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(picker->filter));
  }
}

static void wd_royl_picker_format_location_ccc(uint32_t location, char *buffer,
                                               size_t buffer_size)
{
  if (location == 0)
  {
    g_strlcpy(buffer, "-", buffer_size);
  }
  else
  {
    snprintf(buffer, buffer_size, _("sector 0x%08x (byte 0x%08llx)"),
             location,
             (unsigned long long)location * WD_ROYL_SECTOR_SIZE);
  }
}

static void wd_royl_picker_update_details_ccc(GtkTreeSelection *selection,
                                              gpointer user_data)
{
  wd_royl_module_picker_ccc_t *picker =
      (wd_royl_module_picker_ccc_t *)user_data;
  GtkTreeModel *model;
  GtkTreeIter iter;

  if (picker->details_label == NULL)
  {
    return;
  }
  if (!gtk_tree_selection_get_selected(selection, &model, &iter))
  {
    gtk_label_set_text(GTK_LABEL(picker->details_label), "");
    return;
  }

  {
    guint module_id = 0;
    gchar *name = NULL;
    gchar *size_text = NULL;
    gchar *copies_text = NULL;
    gchar *health = NULL;
    guint64 location1 = 0;
    guint64 location2 = 0;
    char location1_text[96];
    char location2_text[96];
    char details[640];

    gtk_tree_model_get(
        model, &iter, WD_ROYL_PICKER_MODULE_ID_COLUMN, &module_id,
        WD_ROYL_PICKER_NAME_COLUMN, &name, WD_ROYL_PICKER_SIZE_COLUMN,
        &size_text, WD_ROYL_PICKER_COPIES_COLUMN, &copies_text,
        WD_ROYL_PICKER_HEALTH_COLUMN, &health,
        WD_ROYL_PICKER_LOCATION1_VALUE_COLUMN, &location1,
        WD_ROYL_PICKER_LOCATION2_VALUE_COLUMN, &location2, -1);

    wd_royl_picker_format_location_ccc((uint32_t)location1, location1_text,
                                       sizeof(location1_text));
    wd_royl_picker_format_location_ccc((uint32_t)location2, location2_text,
                                       sizeof(location2_text));
    if (name != NULL && name[0] != '\0')
    {
      snprintf(details, sizeof(details),
               _("Module 0x%04x - %s\nSize: %s   Copies: %s   Health: %s\n"
                 "Copy 1: %s   Copy 2: %s"),
               module_id, name, size_text != NULL ? size_text : "-",
               copies_text != NULL ? copies_text : "-",
               health != NULL ? health : "-", location1_text, location2_text);
    }
    else
    {
      snprintf(details, sizeof(details),
               _("Module 0x%04x (name not confirmed)\nSize: %s   Copies: %s   "
                 "Health: %s\nCopy 1: %s   Copy 2: %s"),
               module_id, size_text != NULL ? size_text : "-",
               copies_text != NULL ? copies_text : "-",
               health != NULL ? health : "-", location1_text, location2_text);
    }
    gtk_label_set_text(GTK_LABEL(picker->details_label), details);
    g_free(name);
    g_free(size_text);
    g_free(copies_text);
    g_free(health);
  }
}

static GtkTreeViewColumn *wd_royl_picker_add_column_ccc(
    GtkWidget *tree_view, const char *title, int model_column, int width,
    bool monospace, float xalign, int weight_column)
{
  GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
  GtkTreeViewColumn *column;

  g_object_set(renderer, "xalign", xalign, NULL);
  if (monospace)
  {
    g_object_set(renderer, "font", "Monospace 10", NULL);
  }
  if (weight_column >= 0)
  {
    column = gtk_tree_view_column_new_with_attributes(
        title, renderer, "text", model_column, "weight", weight_column, NULL);
  }
  else
  {
    column = gtk_tree_view_column_new_with_attributes(title, renderer, "text",
                                                      model_column, NULL);
  }
  gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_fixed_width(column, width);
  gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
  return column;
}

static void wd_royl_picker_selection_changed_ccc(GtkTreeSelection *selection,
                                                 gpointer user_data)
{
  wd_royl_module_picker_ccc_t *picker =
      (wd_royl_module_picker_ccc_t *)user_data;
  GtkTreeModel *model;
  GtkTreeIter iter;
  guint module_id;

  if (gtk_tree_selection_get_selected(selection, &model, &iter))
  {
    char text[16];
    gtk_tree_model_get(model, &iter, WD_ROYL_PICKER_MODULE_ID_COLUMN,
                       &module_id, -1);
    snprintf(text, sizeof(text), "0x%04x", module_id);
    gtk_entry_set_text(GTK_ENTRY(picker->manual_entry), text);
  }
  wd_royl_picker_update_details_ccc(selection, user_data);
}

static void wd_royl_picker_row_activated_ccc(GtkTreeView *tree_view,
                                             GtkTreePath *path,
                                             GtkTreeViewColumn *column,
                                             gpointer user_data)
{
  wd_royl_module_picker_ccc_t *picker =
      (wd_royl_module_picker_ccc_t *)user_data;
  GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
  GtkTreeIter iter;
  guint module_id;

  if (gtk_tree_model_get_iter(model, &iter, path))
  {
    char text[16];
    gtk_tree_model_get(model, &iter, WD_ROYL_PICKER_MODULE_ID_COLUMN,
                       &module_id, -1);
    snprintf(text, sizeof(text), "0x%04x", module_id);
    gtk_entry_set_text(GTK_ENTRY(picker->manual_entry), text);
  }
  gtk_dialog_response(GTK_DIALOG(picker->dialog), GTK_RESPONSE_ACCEPT);
}

static void wd_royl_picker_entry_activate_ccc(GtkEntry *entry,
                                              gpointer user_data)
{
  wd_royl_module_picker_ccc_t *picker =
      (wd_royl_module_picker_ccc_t *)user_data;
  gtk_dialog_response(GTK_DIALOG(picker->dialog), GTK_RESPONSE_ACCEPT);
}

static void wd_royl_picker_response_ccc(GtkDialog *dialog, gint response,
                                        gpointer user_data)
{
  wd_royl_module_picker_ccc_t *picker =
      (wd_royl_module_picker_ccc_t *)user_data;
  unsigned long long value = 0;
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkWidget *message;

  if (response != GTK_RESPONSE_ACCEPT)
  {
    return;
  }
  if (wd_royl_parse_number_ccc(
          gtk_entry_get_text(GTK_ENTRY(picker->manual_entry)), 0xffff,
          &value) &&
      value != 0)
  {
    picker->selected_module = (uint16_t)value;
    return;
  }

  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(picker->tree_view));
  if (gtk_tree_selection_get_selected(selection, &model, &iter))
  {
    guint module_id;
    gtk_tree_model_get(model, &iter, WD_ROYL_PICKER_MODULE_ID_COLUMN,
                       &module_id, -1);
    if (module_id != 0)
    {
      picker->selected_module = (uint16_t)module_id;
      return;
    }
  }

  message = gtk_message_dialog_new(
      GTK_WINDOW(dialog), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING,
      GTK_BUTTONS_OK, "%s",
      _("Select a module from the list or enter a module number from 1 to "
        "0xffff."));
  gtk_window_set_title(GTK_WINDOW(message), _("WD ROYL SA Module Editor"));
  gtk_dialog_run(GTK_DIALOG(message));
  gtk_widget_destroy(message);
  g_signal_stop_emission_by_name(G_OBJECT(dialog), "response");
}

static uint16_t wd_royl_choose_module_ccc(wd_royl_editor_ccc_t *editor)
{
  wd_royl_module_picker_ccc_t picker;
  GtkWidget *content;
  GtkWidget *vbox;
  GtkWidget *info_label;
  GtkWidget *scrolled;
  GtkWidget *separator;
  GtkWidget *manual_row;
  GtkWidget *manual_label;
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreeIter first_iter;
  GtkTreeIter first_module_iter;
  GtkTreeIter default_iter;
  bool have_first = false;
  bool have_first_module = false;
  bool have_default = false;
  bool temporary_source_access = false;
  bool directory_ok = false;
  uint8_t *directory_data = NULL;
  size_t directory_size = 0;
  size_t entry_count = 0;
  size_t listed_count = 0;
  size_t warning_count = 0;
  size_t known_count = 0;
  wd_royl_module_entry_ccc *entries = NULL;
  char directory_error[WD_ROYL_ERROR_SIZE] = "";
  char info_text[1024];
  GtkWindow *parent;
  size_t i;
  int response;

  memset(&picker, 0, sizeof(picker));
  parent = editor != NULL && editor->dialog != NULL
               ? GTK_WINDOW(editor->dialog)
               : GTK_WINDOW(main_window_ccc);

  wd_royl_dialog_parent_override_ccc = parent;
  if (!wd_royl_begin_source_access_ccc(&temporary_source_access))
  {
    wd_royl_dialog_parent_override_ccc = NULL;
    return 0;
  }

  wd_royl_progress_begin_ccc(_("WD ROYL SA Module Editor"),
                             _("Reading module directory"));
  if (wd_royl_read_module_ccc(WD_ROYL_MODULE_01, &directory_data,
                              &directory_size,
                              wd_royl_progress_update_ccc) == 0)
  {
    if (wd_royl_parse_module_directory_ccc(
            directory_data, directory_size, &entries, &entry_count) == 0)
    {
      directory_ok = true;
    }
    else
    {
      g_strlcpy(directory_error, wd_royl_last_error_ccc(),
                sizeof(directory_error));
    }
  }
  else
  {
    g_strlcpy(directory_error, wd_royl_last_error_ccc(),
              sizeof(directory_error));
  }
  wd_royl_progress_end_ccc();
  wd_royl_end_source_access_ccc(temporary_source_access);
  wd_royl_dialog_parent_override_ccc = NULL;
  free(directory_data);

  listed_count = entry_count;
  if (directory_ok)
  {
    bool have_directory_entry = false;
    for (i = 0; i < entry_count; i++)
    {
      if (entries[i].module_id == WD_ROYL_MODULE_01)
      {
        have_directory_entry = true;
        break;
      }
    }
    if (!have_directory_entry &&
        directory_size >= (size_t)WD_ROYL_SECTOR_SIZE)
    {
      wd_royl_module_entry_ccc *new_entries = (wd_royl_module_entry_ccc *)
          realloc(entries, (entry_count + 1) * sizeof(*entries));
      if (new_entries != NULL)
      {
        entries = new_entries;
        memset(&entries[entry_count], 0, sizeof(entries[entry_count]));
        entries[entry_count].module_id = WD_ROYL_MODULE_01;
        entries[entry_count].sectors =
            (uint16_t)(directory_size / WD_ROYL_SECTOR_SIZE);
        entries[entry_count].copies = 1;
        entry_count++;
      }
    }
    if (entry_count > 1)
    {
      qsort(entries, entry_count, sizeof(*entries),
            wd_royl_compare_module_entries_ccc);
    }
  }

  picker.dialog = gtk_dialog_new_with_buttons(
      _("Select WD ROYL SA Module"), parent,
      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CANCEL,
      GTK_RESPONSE_CANCEL, _("Open Module"), GTK_RESPONSE_ACCEPT, NULL);
  gtk_window_set_default_size(GTK_WINDOW(picker.dialog), 970, 600);
  gtk_dialog_set_default_response(GTK_DIALOG(picker.dialog),
                                  GTK_RESPONSE_ACCEPT);

  content = gtk_dialog_get_content_area(GTK_DIALOG(picker.dialog));
  gtk_container_set_border_width(GTK_CONTAINER(content), 10);
  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_box_pack_start(GTK_BOX(content), vbox, TRUE, TRUE, 0);

  info_label = gtk_label_new("");
  gtk_label_set_line_wrap(GTK_LABEL(info_label), TRUE);
  gtk_label_set_xalign(GTK_LABEL(info_label), 0.0);
  gtk_widget_set_tooltip_text(
      info_label,
      _("Copy health is based on the copy count and locations reported in "
        "directory module 0x0001. Copy 1 and Copy 2 are SA sector numbers; "
        "the byte address is the value multiplied by 512. Known names are "
        "shown only for confirmed module IDs."));
  gtk_box_pack_start(GTK_BOX(vbox), info_label, FALSE, FALSE, 0);

  {
    GType columns[WD_ROYL_PICKER_COLUMN_COUNT] = {
        G_TYPE_STRING,  /* Module */
        G_TYPE_STRING,  /* Known name */
        G_TYPE_STRING,  /* Size */
        G_TYPE_STRING,  /* Copies */
        G_TYPE_STRING,  /* Copy 1 */
        G_TYPE_STRING,  /* Copy 2 */
        G_TYPE_STRING,  /* Copy health */
        G_TYPE_INT,     /* Health font weight */
        G_TYPE_UINT,    /* Module ID, hidden sort key */
        G_TYPE_UINT,    /* Size in sectors, hidden sort key */
        G_TYPE_UINT,    /* Copies, hidden sort key */
        G_TYPE_UINT64,  /* Copy 1 location, hidden sort key */
        G_TYPE_UINT64,  /* Copy 2 location, hidden sort key */
        G_TYPE_BOOLEAN, /* Warning flag, hidden filter key */
        G_TYPE_STRING   /* Row tooltip */
    };
    picker.store = gtk_list_store_newv(WD_ROYL_PICKER_COLUMN_COUNT, columns);
  }
  picker.filter = gtk_tree_model_filter_new(GTK_TREE_MODEL(picker.store), NULL);
  gtk_tree_model_filter_set_visible_func(
      GTK_TREE_MODEL_FILTER(picker.filter), wd_royl_picker_filter_visible_ccc,
      &picker, NULL);
  picker.sort = gtk_tree_model_sort_new_with_model(picker.filter);
  picker.tree_view = gtk_tree_view_new_with_model(picker.sort);
  gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(picker.tree_view), TRUE);
  gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(picker.tree_view),
                               GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);
  {
    GtkTreeViewColumn *module_column;
    GtkTreeViewColumn *name_column;
    GtkTreeViewColumn *size_column;
    GtkTreeViewColumn *copies_column;
    GtkTreeViewColumn *location1_column;
    GtkTreeViewColumn *location2_column;
    GtkTreeViewColumn *health_column;

    module_column = wd_royl_picker_add_column_ccc(
        picker.tree_view, _("Module"), WD_ROYL_PICKER_MODULE_COLUMN, 85, true,
        0.0, -1);
    name_column = wd_royl_picker_add_column_ccc(
        picker.tree_view, _("Known Name"), WD_ROYL_PICKER_NAME_COLUMN, 170,
        false, 0.0, -1);
    size_column = wd_royl_picker_add_column_ccc(
        picker.tree_view, _("Size"), WD_ROYL_PICKER_SIZE_COLUMN, 180, false,
        0.0, -1);
    copies_column = wd_royl_picker_add_column_ccc(
        picker.tree_view, _("Copies"), WD_ROYL_PICKER_COPIES_COLUMN, 65, false,
        0.5, -1);
    location1_column = wd_royl_picker_add_column_ccc(
        picker.tree_view, _("Copy 1"), WD_ROYL_PICKER_LOCATION1_COLUMN, 105,
        true, 0.0, -1);
    location2_column = wd_royl_picker_add_column_ccc(
        picker.tree_view, _("Copy 2"), WD_ROYL_PICKER_LOCATION2_COLUMN, 105,
        true, 0.0, -1);
    health_column = wd_royl_picker_add_column_ccc(
        picker.tree_view, _("Copy Health"), WD_ROYL_PICKER_HEALTH_COLUMN, 190,
        false, 0.0, WD_ROYL_PICKER_HEALTH_WEIGHT_COLUMN);

    gtk_tree_view_column_set_sort_column_id(
        name_column, WD_ROYL_PICKER_NAME_COLUMN);
    gtk_tree_view_column_set_sort_column_id(
        size_column, WD_ROYL_PICKER_SIZE_SECTORS_COLUMN);
    gtk_tree_view_column_set_sort_column_id(
        copies_column, WD_ROYL_PICKER_COPIES_VALUE_COLUMN);
    gtk_tree_view_column_set_sort_column_id(
        location1_column, WD_ROYL_PICKER_LOCATION1_VALUE_COLUMN);
    gtk_tree_view_column_set_sort_column_id(
        location2_column, WD_ROYL_PICKER_LOCATION2_VALUE_COLUMN);
    gtk_tree_view_column_set_sort_column_id(
        health_column, WD_ROYL_PICKER_WARNING_COLUMN);
    // Use the module ID as the initial sort. Calling this last makes it the
    // active sort column while leaving the other headers clickable.
    gtk_tree_view_column_set_sort_column_id(
        module_column, WD_ROYL_PICKER_MODULE_ID_COLUMN);
  }
  gtk_tree_view_set_tooltip_column(GTK_TREE_VIEW(picker.tree_view),
                                   WD_ROYL_PICKER_TOOLTIP_COLUMN);

  for (i = 0; i < entry_count; i++)
  {
    char module_text[16];
    char size_text[96];
    char human_size[32];
    char copies_text[16];
    char location1[16];
    char location2[16];
    char health[64];
    char tooltip[256];
    bool warning = false;
    const char *known_name =
        wd_royl_known_module_name_ccc(entries[i].module_id);
    const char *name_text = known_name != NULL ? known_name : "";

    snprintf(module_text, sizeof(module_text), "0x%04x",
             entries[i].module_id);
    if (entries[i].sectors != 0)
    {
      wd_royl_format_size_ccc((unsigned long long)entries[i].sectors *
                                  WD_ROYL_SECTOR_SIZE,
                              human_size, sizeof(human_size));
      snprintf(size_text, sizeof(size_text), "%u sec (%s)",
               entries[i].sectors, human_size);
    }
    else
    {
      g_strlcpy(size_text, "-", sizeof(size_text));
    }
    if (entries[i].copies != 0)
    {
      snprintf(copies_text, sizeof(copies_text), "%u", entries[i].copies);
    }
    else
    {
      g_strlcpy(copies_text, "-", sizeof(copies_text));
    }
    if (entries[i].location1 != 0)
    {
      snprintf(location1, sizeof(location1), "0x%08x",
               entries[i].location1);
    }
    else
    {
      g_strlcpy(location1, "-", sizeof(location1));
    }
    if (entries[i].location2 != 0)
    {
      snprintf(location2, sizeof(location2), "0x%08x",
               entries[i].location2);
    }
    else
    {
      g_strlcpy(location2, "-", sizeof(location2));
    }
    wd_royl_module_entry_health_ccc(entries, entry_count, i, health,
                                    sizeof(health), &warning);
    if (warning)
    {
      warning_count++;
    }
    if (known_name != NULL)
    {
      known_count++;
    }
    snprintf(tooltip, sizeof(tooltip), _("%s\nCopy 1: %s\nCopy 2: %s"), health,
             location1, location2);

    gtk_list_store_append(picker.store, &iter);
    gtk_list_store_set(
        picker.store, &iter, WD_ROYL_PICKER_MODULE_COLUMN, module_text,
        WD_ROYL_PICKER_NAME_COLUMN, name_text, WD_ROYL_PICKER_SIZE_COLUMN,
        size_text, WD_ROYL_PICKER_COPIES_COLUMN, copies_text,
        WD_ROYL_PICKER_LOCATION1_COLUMN, location1,
        WD_ROYL_PICKER_LOCATION2_COLUMN, location2,
        WD_ROYL_PICKER_HEALTH_COLUMN, health,
        WD_ROYL_PICKER_HEALTH_WEIGHT_COLUMN,
        warning ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
        WD_ROYL_PICKER_MODULE_ID_COLUMN, (guint)entries[i].module_id,
        WD_ROYL_PICKER_SIZE_SECTORS_COLUMN, (guint)entries[i].sectors,
        WD_ROYL_PICKER_COPIES_VALUE_COLUMN, (guint)entries[i].copies,
        WD_ROYL_PICKER_LOCATION1_VALUE_COLUMN, (guint64)entries[i].location1,
        WD_ROYL_PICKER_LOCATION2_VALUE_COLUMN, (guint64)entries[i].location2,
        WD_ROYL_PICKER_WARNING_COLUMN, warning, WD_ROYL_PICKER_TOOLTIP_COLUMN,
        tooltip, -1);
  }

  if (directory_ok && listed_count > 0)
  {
    if (warning_count > 0)
    {
      snprintf(info_text, sizeof(info_text),
               _("Directory module 0x0001 lists %zu modules; %zu have "
                 "copy/metadata warnings and %zu have confirmed names. Select "
                 "a module below or enter a module number."),
               listed_count, warning_count, known_count);
    }
    else
    {
      snprintf(info_text, sizeof(info_text),
               _("Directory module 0x0001 lists %zu modules; copy metadata "
                 "looks consistent and %zu have confirmed names. Select a "
                 "module below or enter a module number."),
               listed_count, known_count);
    }
  }
  else if (directory_ok)
  {
    g_strlcpy(info_text,
              _("Directory module 0x0001 did not list any modules. Enter a "
                "module number manually."),
              sizeof(info_text));
  }
  else
  {
    snprintf(info_text, sizeof(info_text),
             _("The module directory could not be read. Enter a module number "
               "manually.\n\n%s"),
             directory_error);
  }
  gtk_label_set_text(GTK_LABEL(info_label), info_text);

  scrolled = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_widget_set_size_request(scrolled, -1, 300);
  gtk_container_add(GTK_CONTAINER(scrolled), picker.tree_view);
  gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

  separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 0);

  picker.details_label = gtk_label_new("");
  gtk_label_set_line_wrap(GTK_LABEL(picker.details_label), TRUE);
  gtk_label_set_xalign(GTK_LABEL(picker.details_label), 0.0);
  gtk_label_set_selectable(GTK_LABEL(picker.details_label), TRUE);
  gtk_widget_set_tooltip_text(
      picker.details_label,
      _("The selected module's directory record and raw SA copy locations."));
  gtk_box_pack_start(GTK_BOX(vbox), picker.details_label, FALSE, FALSE, 0);

  manual_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start(GTK_BOX(vbox), manual_row, FALSE, FALSE, 0);
  manual_label = gtk_label_new(_("Module number:"));
  gtk_box_pack_start(GTK_BOX(manual_row), manual_label, FALSE, FALSE, 0);
  picker.manual_entry = gtk_entry_new();
  gtk_entry_set_width_chars(GTK_ENTRY(picker.manual_entry), 14);
  gtk_widget_set_tooltip_text(
      picker.manual_entry,
      _("Decimal or 0x-prefixed hexadecimal module number"));
  g_signal_connect(picker.manual_entry, "activate",
                   G_CALLBACK(wd_royl_picker_entry_activate_ccc), &picker);
  gtk_box_pack_start(GTK_BOX(manual_row), picker.manual_entry, FALSE, FALSE, 0);

  picker.warnings_only = gtk_check_button_new_with_label(_("Warnings only"));
  gtk_widget_set_tooltip_text(
      picker.warnings_only,
      _("Show only modules with copy or directory metadata warnings."));
  g_signal_connect(picker.warnings_only, "toggled",
                   G_CALLBACK(wd_royl_picker_warnings_only_toggled_ccc),
                   &picker);
  gtk_box_pack_end(GTK_BOX(manual_row), picker.warnings_only, FALSE, FALSE, 0);

  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(picker.tree_view));
  g_signal_connect(selection, "changed",
                   G_CALLBACK(wd_royl_picker_selection_changed_ccc), &picker);
  g_signal_connect(picker.tree_view, "row-activated",
                   G_CALLBACK(wd_royl_picker_row_activated_ccc), &picker);
  g_signal_connect(picker.dialog, "response",
                   G_CALLBACK(wd_royl_picker_response_ccc), &picker);

  gtk_widget_show_all(picker.dialog);

  model = gtk_tree_view_get_model(GTK_TREE_VIEW(picker.tree_view));
  if (gtk_tree_model_get_iter_first(model, &iter))
  {
    first_iter = iter;
    have_first = true;
    do
    {
      guint module_id;
      gtk_tree_model_get(model, &iter, WD_ROYL_PICKER_MODULE_ID_COLUMN,
                         &module_id, -1);
      if (module_id == WD_ROYL_MODULE_02)
      {
        default_iter = iter;
        have_default = true;
        break;
      }
      if (module_id != WD_ROYL_MODULE_01 && !have_first_module)
      {
        first_module_iter = iter;
        have_first_module = true;
      }
    } while (gtk_tree_model_iter_next(model, &iter));
    if (!have_default)
    {
      if (have_first_module)
      {
        default_iter = first_module_iter;
        have_default = true;
      }
      else
      {
        default_iter = first_iter;
        have_default = have_first;
      }
    }
    if (have_default)
    {
      GtkTreePath *path = gtk_tree_model_get_path(model, &default_iter);
      gtk_tree_selection_select_path(selection, path);
      gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(picker.tree_view), path, NULL,
                                   TRUE, 0.5, 0.0);
      gtk_tree_path_free(path);
    }
  }
  else
  {
    gtk_widget_grab_focus(picker.manual_entry);
  }

  response = gtk_dialog_run(GTK_DIALOG(picker.dialog));
  g_signal_handlers_disconnect_by_data(selection, &picker);
  g_signal_handlers_disconnect_by_data(picker.tree_view, &picker);
  g_signal_handlers_disconnect_by_data(picker.manual_entry, &picker);
  g_signal_handlers_disconnect_by_data(picker.warnings_only, &picker);
  g_signal_handlers_disconnect_by_data(picker.dialog, &picker);
  // The filter may be queried while the dialog is being destroyed. Let it
  // show every row instead of dereferencing the check button being torn down.
  picker.warnings_only = NULL;
  gtk_widget_destroy(picker.dialog);
  g_object_unref(picker.sort);
  g_object_unref(picker.filter);
  g_object_unref(picker.store);
  free(entries);
  if (response != GTK_RESPONSE_ACCEPT)
  {
    return 0;
  }
  return picker.selected_module;
}

static void wd_royl_editor_read_clicked_ccc(GtkWidget *widget,
                                            gpointer user_data)
{
  wd_royl_editor_ccc_t *editor = (wd_royl_editor_ccc_t *)user_data;
  unsigned long long module_value = 0;
  uint8_t *data = NULL;
  size_t size = 0;
  bool temporary_source_access = false;
  int result;

  if (!wd_royl_parse_number_ccc(
          gtk_entry_get_text(GTK_ENTRY(editor->module_entry)), 0xffff,
          &module_value) ||
      module_value == 0)
  {
    wd_royl_editor_message_ccc(
        editor, GTK_MESSAGE_WARNING, _("WD ROYL SA Module Editor"),
        _("Enter a module number from 1 to 0xffff. Decimal and 0x-prefixed "
          "hexadecimal numbers are accepted."));
    return;
  }
  if (!wd_royl_editor_confirm_discard_ccc(editor))
  {
    if (editor->loaded)
    {
      char module_text[16];
      snprintf(module_text, sizeof(module_text), "0x%04x", editor->module);
      gtk_entry_set_text(GTK_ENTRY(editor->module_entry), module_text);
    }
    return;
  }
  wd_royl_dialog_parent_override_ccc = GTK_WINDOW(editor->dialog);
  if (!wd_royl_begin_source_access_ccc(&temporary_source_access))
  {
    wd_royl_dialog_parent_override_ccc = NULL;
    return;
  }

  wd_royl_progress_begin_ccc(_("WD ROYL SA Module Editor"),
                             _("Reading module"));
  result = wd_royl_read_module_ccc((uint16_t)module_value, &data, &size,
                                   wd_royl_progress_update_ccc);
  wd_royl_progress_end_ccc();
  wd_royl_end_source_access_ccc(temporary_source_access);
  wd_royl_dialog_parent_override_ccc = NULL;

  if (result != 0)
  {
    wd_royl_editor_report_error_ccc(
        editor, _("WD ROYL SA Module Editor"),
        _("Unable to read the module from the drive."));
    free(data);
    return;
  }

  wd_royl_editor_set_data_ccc(editor, data, size,
                              (uint16_t)module_value, false);
  {
    char status[128];
    snprintf(status, sizeof(status), _("Read module 0x%04x (%zu bytes)."),
             (unsigned int)module_value, size);
    gtk_label_set_text(GTK_LABEL(editor->status_label), status);
  }
}

static void wd_royl_editor_choose_clicked_ccc(GtkWidget *widget,
                                              gpointer user_data)
{
  wd_royl_editor_ccc_t *editor = (wd_royl_editor_ccc_t *)user_data;
  uint16_t module = wd_royl_choose_module_ccc(editor);
  char module_text[16];

  if (module == 0)
  {
    return;
  }
  snprintf(module_text, sizeof(module_text), "0x%04x", module);
  gtk_entry_set_text(GTK_ENTRY(editor->module_entry), module_text);
  wd_royl_editor_read_clicked_ccc(NULL, editor);
}

static void wd_royl_editor_load_clicked_ccc(GtkWidget *widget,
                                            gpointer user_data)
{
  wd_royl_editor_ccc_t *editor = (wd_royl_editor_ccc_t *)user_data;
  char *filename;
  uint8_t *data = NULL;
  size_t size = 0;
  uint16_t module;

  filename = wd_royl_editor_choose_file_ccc(
      editor, false, _("Load WD ROYL SA module"), NULL);
  if (filename == NULL)
  {
    return;
  }
  if (wd_royl_read_file_ccc(filename, &data, &size) != 0)
  {
    wd_royl_editor_report_error_ccc(
        editor, _("WD ROYL SA Module Editor"),
        _("Unable to read the selected module file."));
    g_free(filename);
    return;
  }
  if (size == 0 || size > WD_ROYL_MAX_FILE_SIZE ||
      wd_royl_validate_module_header_ccc(data, size, 0) != 0)
  {
    wd_royl_editor_message_ccc(
        editor, GTK_MESSAGE_ERROR, _("WD ROYL SA Module Editor"),
        _("The selected file is not a valid ROYL service area module.\n\n%s"),
        wd_royl_last_error_ccc());
    free(data);
    g_free(filename);
    return;
  }
  module = wd_royl_get_module_id_ccc(data);
  if (module == 0)
  {
    wd_royl_editor_message_ccc(
        editor, GTK_MESSAGE_ERROR, _("WD ROYL SA Module Editor"),
        _("The selected file does not contain a valid module number."));
    free(data);
    g_free(filename);
    return;
  }
  if (!wd_royl_editor_confirm_discard_ccc(editor))
  {
    free(data);
    g_free(filename);
    return;
  }

  wd_royl_editor_set_data_ccc(editor, data, size, module, false);
  {
    char status[512];
    snprintf(status, sizeof(status), _("Loaded module 0x%04x from %s."),
             module, filename);
    gtk_label_set_text(GTK_LABEL(editor->status_label), status);
  }
  g_free(filename);
}

static void wd_royl_editor_save_clicked_ccc(GtkWidget *widget,
                                            gpointer user_data)
{
  wd_royl_editor_ccc_t *editor = (wd_royl_editor_ccc_t *)user_data;
  char default_name[256];
  char kind[64];
  char *filename;

  if (!editor->loaded)
  {
    wd_royl_editor_message_ccc(
        editor, GTK_MESSAGE_WARNING, _("WD ROYL SA Module Editor"),
        _("Read or load a module before saving it."));
    return;
  }

  snprintf(kind, sizeof(kind), "module_%04x", editor->module);
  wd_royl_make_backup_name_ccc(default_name, sizeof(default_name), kind);
  filename = wd_royl_editor_choose_file_ccc(
      editor, true, _("Save WD ROYL SA module"), default_name);
  if (filename == NULL)
  {
    return;
  }

  wd_royl_editor_update_checksum_ccc(editor);
  if (wd_royl_write_file_ccc(filename, editor->data, editor->size) != 0)
  {
    wd_royl_editor_report_error_ccc(
        editor, _("WD ROYL SA Module Editor"),
        _("Unable to save the module file."));
    g_free(filename);
    return;
  }

  editor->dirty = false;
  wd_royl_editor_update_info_ccc(editor);
  {
    char status[512];
    snprintf(status, sizeof(status), _("Saved module 0x%04x to %s."),
             editor->module, filename);
    gtk_label_set_text(GTK_LABEL(editor->status_label), status);
  }
  g_free(filename);
}

static void wd_royl_editor_write_clicked_ccc(GtkWidget *widget,
                                             gpointer user_data)
{
  wd_royl_editor_ccc_t *editor = (wd_royl_editor_ccc_t *)user_data;
  uint8_t *current = NULL;
  size_t current_size = 0;
  char backup_name[512];
  char kind[64];
  bool temporary_source_access = false;
  int result = -1;

  if (!editor->loaded)
  {
    wd_royl_editor_message_ccc(
        editor, GTK_MESSAGE_WARNING, _("WD ROYL SA Module Editor"),
        _("Read or load a module before writing it."));
    return;
  }

  wd_royl_editor_update_checksum_ccc(editor);
  if (wd_royl_validate_module_ccc(editor->data, editor->size,
                                  editor->module) != 0)
  {
    wd_royl_editor_report_error_ccc(
        editor, _("WD ROYL SA Module Editor"),
        _("The module data is not valid and was not written."));
    return;
  }
  if (!wd_royl_editor_confirm_ccc(
          editor, _("Confirm module write"), _("Write Module"),
          _("You are about to write module 0x%04x to the service area of the "
            "selected drive.\n\nSize: %zu bytes\n\nThe current module will be "
            "backed up first, and the write will be verified.\n\nContinue?"),
          editor->module, editor->size))
  {
    return;
  }

  wd_royl_dialog_parent_override_ccc = GTK_WINDOW(editor->dialog);
  if (!wd_royl_begin_source_access_ccc(&temporary_source_access))
  {
    wd_royl_dialog_parent_override_ccc = NULL;
    return;
  }

  wd_royl_progress_begin_ccc(_("WD ROYL SA Module Editor"),
                             _("Checking the current module"));
  if (wd_royl_read_module_ccc(editor->module, &current, &current_size,
                              wd_royl_progress_update_ccc) != 0)
  {
    wd_royl_progress_end_ccc();
    wd_royl_editor_report_error_ccc(
        editor, _("WD ROYL SA Module Editor"),
        _("Unable to back up the current module. The module was not written."));
    goto finish;
  }
  wd_royl_progress_end_ccc();

  if (wd_royl_validate_module_header_ccc(current, current_size,
                                         editor->module) != 0)
  {
    wd_royl_editor_report_error_ccc(
        editor, _("WD ROYL SA Module Editor"),
        _("The current module could not be validated. The module was not "
          "written."));
    goto finish;
  }
  if (current_size != editor->size)
  {
    wd_royl_editor_message_ccc(
        editor, GTK_MESSAGE_ERROR, _("WD ROYL SA Module Editor"),
        _("The module on the drive is %zu bytes, but the editor contains %zu "
          "bytes. Modules cannot be resized by the editor."),
        current_size, editor->size);
    goto finish;
  }

  snprintf(kind, sizeof(kind), "module_%04x", editor->module);
  wd_royl_make_backup_path_ccc(backup_name, sizeof(backup_name), kind);
  if (wd_royl_write_file_ccc(backup_name, current, current_size) != 0)
  {
    wd_royl_editor_report_error_ccc(
        editor, _("WD ROYL SA Module Editor"),
        _("Unable to save the module backup. The module was not written."));
    goto finish;
  }

  wd_royl_progress_begin_ccc(_("WD ROYL SA Module Editor"),
                             _("Writing module"));
  result = wd_royl_write_module_ccc(editor->module, editor->data, editor->size,
                                    wd_royl_progress_update_ccc);
  if (result == 0)
  {
    result = wd_royl_verify_module_ccc(editor->module, editor->data,
                                       editor->size,
                                       wd_royl_progress_update_ccc);
  }
  wd_royl_progress_end_ccc();

  if (result != 0)
  {
    wd_royl_editor_message_ccc(
        editor, GTK_MESSAGE_ERROR, _("WD ROYL SA Module Editor"),
        _("The module write or verification failed.\n\n%s\n\nDo not power off "
          "the drive. Try the write again; if it still fails, the backup is "
          "at:\n%s"),
        wd_royl_last_error_ccc(), backup_name);
    goto finish;
  }

  editor->dirty = false;
  wd_royl_editor_update_info_ccc(editor);
  gtk_label_set_text(GTK_LABEL(editor->status_label),
                     _("Module written and verified."));
  wd_royl_editor_message_ccc(
      editor, GTK_MESSAGE_INFO, _("WD ROYL SA Module Editor"),
      _("Module 0x%04x was written and verified successfully.\n\nThe previous "
        "module is backed up at:\n%s"),
      editor->module, backup_name);

finish:
  wd_royl_dialog_parent_override_ccc = NULL;
  free(current);
  wd_royl_end_source_access_ccc(temporary_source_access);
}

static void wd_royl_editor_prev_clicked_ccc(GtkWidget *widget,
                                            gpointer user_data)
{
  wd_royl_editor_ccc_t *editor = (wd_royl_editor_ccc_t *)user_data;

  if (editor->loaded && editor->page_start >= WD_ROYL_EDITOR_PAGE_SIZE)
  {
    editor->page_start -= WD_ROYL_EDITOR_PAGE_SIZE;
    wd_royl_editor_load_page_ccc(editor);
  }
}

static void wd_royl_editor_next_clicked_ccc(GtkWidget *widget,
                                            gpointer user_data)
{
  wd_royl_editor_ccc_t *editor = (wd_royl_editor_ccc_t *)user_data;

  if (editor->loaded &&
      editor->page_start + WD_ROYL_EDITOR_PAGE_SIZE < editor->size)
  {
    editor->page_start += WD_ROYL_EDITOR_PAGE_SIZE;
    wd_royl_editor_load_page_ccc(editor);
  }
}

static void wd_royl_editor_go_clicked_ccc(GtkWidget *widget,
                                          gpointer user_data)
{
  wd_royl_editor_ccc_t *editor = (wd_royl_editor_ccc_t *)user_data;
  unsigned long long offset = 0;

  if (!editor->loaded)
  {
    return;
  }
  if (!wd_royl_parse_number_ccc(
          gtk_entry_get_text(GTK_ENTRY(editor->offset_entry)),
          editor->size - 1, &offset))
  {
    wd_royl_editor_message_ccc(
        editor, GTK_MESSAGE_WARNING, _("WD ROYL SA Module Editor"),
        _("Enter an offset between 0 and 0x%zx. Decimal and 0x-prefixed "
          "hexadecimal numbers are accepted."),
        editor->size - 1);
    return;
  }
  editor->page_start =
      ((size_t)offset / WD_ROYL_EDITOR_PAGE_SIZE) * WD_ROYL_EDITOR_PAGE_SIZE;
  wd_royl_editor_load_page_ccc(editor);
  {
    GtkTreePath *path = gtk_tree_path_new_from_indices(
        (int)(((size_t)offset - editor->page_start) /
              WD_ROYL_EDITOR_BYTES_PER_ROW),
        -1);
    gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(editor->tree_view), path, NULL,
                                 TRUE, 0.5, 0.0);
    gtk_tree_view_set_cursor(GTK_TREE_VIEW(editor->tree_view), path, NULL,
                             FALSE);
    gtk_tree_path_free(path);
  }
}

void open_wd_royl_module_editor_ccc(void)
{
  wd_royl_editor_ccc_t *editor;
  GtkWidget *content;
  GtkWidget *vbox;
  GtkWidget *top_row;
  GtkWidget *module_label;
  GtkWidget *read_button;
  GtkWidget *choose_button;
  GtkWidget *load_button;
  GtkWidget *nav_row;
  GtkWidget *offset_label;
  GtkWidget *go_button;
  GtkWidget *scrolled;
  GtkWidget *hint_label;
  GType columns[WD_ROYL_EDITOR_MODEL_COLUMNS];
  uint16_t startup_module;
  char module_text[16];
  int i;
  int response;
  bool done = false;

  if (!wd_royl_operation_ready_ccc())
  {
    return;
  }
  startup_module = wd_royl_choose_module_ccc(NULL);
  if (startup_module == 0)
  {
    return;
  }

  editor = g_new0(wd_royl_editor_ccc_t, 1);
  editor->dialog = gtk_dialog_new_with_buttons(
      _("WD ROYL SA Module Editor"), GTK_WINDOW(main_window_ccc),
      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CLOSE,
      GTK_RESPONSE_CLOSE, NULL);
  gtk_window_set_default_size(GTK_WINDOW(editor->dialog), 1120, 720);
  gtk_dialog_set_default_response(GTK_DIALOG(editor->dialog),
                                  GTK_RESPONSE_CLOSE);

  content = gtk_dialog_get_content_area(GTK_DIALOG(editor->dialog));
  gtk_container_set_border_width(GTK_CONTAINER(content), 10);
  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_box_pack_start(GTK_BOX(content), vbox, TRUE, TRUE, 0);

  top_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start(GTK_BOX(vbox), top_row, FALSE, FALSE, 0);

  module_label = gtk_label_new(_("Module:"));
  gtk_box_pack_start(GTK_BOX(top_row), module_label, FALSE, FALSE, 0);
  editor->module_entry = gtk_entry_new();
  gtk_entry_set_width_chars(GTK_ENTRY(editor->module_entry), 10);
  gtk_entry_set_text(GTK_ENTRY(editor->module_entry), "0x02");
  gtk_widget_set_tooltip_text(
      editor->module_entry,
      _("Decimal or 0x-prefixed hexadecimal module number"));
  g_signal_connect(editor->module_entry, "activate",
                   G_CALLBACK(wd_royl_editor_read_clicked_ccc), editor);
  gtk_box_pack_start(GTK_BOX(top_row), editor->module_entry, FALSE, FALSE, 0);

  read_button = gtk_button_new_with_label(_("Read from Drive"));
  g_signal_connect(read_button, "clicked",
                   G_CALLBACK(wd_royl_editor_read_clicked_ccc), editor);
  gtk_box_pack_start(GTK_BOX(top_row), read_button, FALSE, FALSE, 0);

  choose_button = gtk_button_new_with_label(_("Choose Module..."));
  gtk_widget_set_tooltip_text(
      choose_button,
      _("Choose a module from the service area directory in module 0x0001"));
  g_signal_connect(choose_button, "clicked",
                   G_CALLBACK(wd_royl_editor_choose_clicked_ccc), editor);
  gtk_box_pack_start(GTK_BOX(top_row), choose_button, FALSE, FALSE, 0);

  load_button = gtk_button_new_with_label(_("Load File..."));
  g_signal_connect(load_button, "clicked",
                   G_CALLBACK(wd_royl_editor_load_clicked_ccc), editor);
  gtk_box_pack_start(GTK_BOX(top_row), load_button, FALSE, FALSE, 0);

  editor->save_button = gtk_button_new_with_label(_("Save File..."));
  g_signal_connect(editor->save_button, "clicked",
                   G_CALLBACK(wd_royl_editor_save_clicked_ccc), editor);
  gtk_box_pack_start(GTK_BOX(top_row), editor->save_button, FALSE, FALSE, 0);

  editor->write_button = gtk_button_new_with_label(_("Write Module..."));
  gtk_widget_set_tooltip_text(
      editor->write_button,
      _("Write the edited module to the service area after making a backup"));
  g_signal_connect(editor->write_button, "clicked",
                   G_CALLBACK(wd_royl_editor_write_clicked_ccc), editor);
  gtk_box_pack_end(GTK_BOX(top_row), editor->write_button, FALSE, FALSE, 0);

  editor->info_label = gtk_label_new("");
  gtk_label_set_line_wrap(GTK_LABEL(editor->info_label), TRUE);
  gtk_label_set_xalign(GTK_LABEL(editor->info_label), 0.0);
  gtk_box_pack_start(GTK_BOX(vbox), editor->info_label, FALSE, FALSE, 0);

  for (i = 0; i < WD_ROYL_EDITOR_MODEL_COLUMNS; i++)
  {
    columns[i] = G_TYPE_STRING;
  }
  editor->store =
      gtk_list_store_newv(WD_ROYL_EDITOR_MODEL_COLUMNS, columns);
  editor->tree_view =
      gtk_tree_view_new_with_model(GTK_TREE_MODEL(editor->store));
  gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(editor->tree_view), FALSE);
  gtk_tree_view_set_enable_search(GTK_TREE_VIEW(editor->tree_view), FALSE);
  gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(editor->tree_view),
                               GTK_TREE_VIEW_GRID_LINES_NONE);
  wd_royl_editor_add_column_ccc(editor, _("Offset"), 0, 92, false, 0.0);
  for (i = 0; i < WD_ROYL_EDITOR_BYTES_PER_ROW; i++)
  {
    char title[3];
    snprintf(title, sizeof(title), "%02X", i);
    wd_royl_editor_add_column_ccc(editor, title, 1 + i, 30, true, 0.5);
  }
  wd_royl_editor_add_column_ccc(editor, _("ASCII"), 17, 150, false, 0.0);

  scrolled = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_widget_set_size_request(scrolled, -1, 420);
  gtk_container_add(GTK_CONTAINER(scrolled), editor->tree_view);
  gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

  nav_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start(GTK_BOX(vbox), nav_row, FALSE, FALSE, 0);

  editor->prev_button = gtk_button_new_with_label(_("Previous Page"));
  g_signal_connect(editor->prev_button, "clicked",
                   G_CALLBACK(wd_royl_editor_prev_clicked_ccc), editor);
  gtk_box_pack_start(GTK_BOX(nav_row), editor->prev_button, FALSE, FALSE, 0);

  editor->next_button = gtk_button_new_with_label(_("Next Page"));
  g_signal_connect(editor->next_button, "clicked",
                   G_CALLBACK(wd_royl_editor_next_clicked_ccc), editor);
  gtk_box_pack_start(GTK_BOX(nav_row), editor->next_button, FALSE, FALSE, 0);

  offset_label = gtk_label_new(_("Go to offset:"));
  gtk_box_pack_start(GTK_BOX(nav_row), offset_label, FALSE, FALSE, 0);
  editor->offset_entry = gtk_entry_new();
  gtk_entry_set_width_chars(GTK_ENTRY(editor->offset_entry), 14);
  gtk_widget_set_tooltip_text(
      editor->offset_entry,
      _("Decimal or 0x-prefixed hexadecimal byte offset"));
  g_signal_connect(editor->offset_entry, "activate",
                   G_CALLBACK(wd_royl_editor_go_clicked_ccc), editor);
  gtk_box_pack_start(GTK_BOX(nav_row), editor->offset_entry, FALSE, FALSE, 0);

  go_button = gtk_button_new_with_label(_("Go"));
  g_signal_connect(go_button, "clicked",
                   G_CALLBACK(wd_royl_editor_go_clicked_ccc), editor);
  gtk_box_pack_start(GTK_BOX(nav_row), go_button, FALSE, FALSE, 0);

  editor->page_label = gtk_label_new("");
  gtk_label_set_xalign(GTK_LABEL(editor->page_label), 1.0);
  gtk_box_pack_end(GTK_BOX(nav_row), editor->page_label, FALSE, FALSE, 0);

  hint_label = gtk_label_new(
      _("Double-click a hexadecimal byte to edit it. The ROYL checksum is "
        "recalculated when the module is saved or written."));
  gtk_label_set_line_wrap(GTK_LABEL(hint_label), TRUE);
  gtk_label_set_xalign(GTK_LABEL(hint_label), 0.0);
  gtk_box_pack_start(GTK_BOX(vbox), hint_label, FALSE, FALSE, 0);

  editor->status_label = gtk_label_new(_("No module loaded."));
  gtk_label_set_line_wrap(GTK_LABEL(editor->status_label), TRUE);
  gtk_label_set_xalign(GTK_LABEL(editor->status_label), 0.0);
  gtk_box_pack_start(GTK_BOX(vbox), editor->status_label, FALSE, FALSE, 0);

  g_object_unref(editor->store);
  wd_royl_editor_update_info_ccc(editor);
  wd_royl_editor_update_navigation_ccc(editor);
  snprintf(module_text, sizeof(module_text), "0x%04x", startup_module);
  gtk_entry_set_text(GTK_ENTRY(editor->module_entry), module_text);
  gtk_widget_show_all(editor->dialog);

  // The startup picker already selected a module, so open it immediately.
  wd_royl_editor_read_clicked_ccc(NULL, editor);

  while (!done)
  {
    response = gtk_dialog_run(GTK_DIALOG(editor->dialog));
    if (response == GTK_RESPONSE_CLOSE ||
        response == GTK_RESPONSE_DELETE_EVENT)
    {
      if (wd_royl_editor_confirm_discard_ccc(editor))
      {
        done = true;
      }
      else
      {
        gtk_widget_show(editor->dialog);
      }
    }
    else
    {
      done = true;
    }
  }

  gtk_widget_destroy(editor->dialog);
  free(editor->data);
  g_free(editor);
}

// Patch one service area module, after saving the unmodified module to a
// backup file. The patch is verified by reading the module back.
static int wd_royl_patch_sa_module_ccc(uint16_t module)
{
  uint8_t *original = NULL;
  uint8_t *patched = NULL;
  size_t size = 0;
  char backup_name[512];
  int result = -1;

  if (wd_royl_read_module_ccc(module, &original, &size,
                              wd_royl_progress_update_ccc) != 0)
  {
    goto finish;
  }
  if (wd_royl_validate_module_ccc(original, size, module) != 0)
  {
    goto finish;
  }

  patched = (uint8_t *)malloc(size);
  if (patched == NULL)
  {
    wd_royl_show_message_ccc(GTK_MESSAGE_ERROR, _("WD ROYL Slow Fix"),
                             _("Unable to allocate %zu bytes for module "
                               "0x%04x."),
                             size, module);
    goto finish;
  }
  memcpy(patched, original, size);
  if (module == WD_ROYL_MODULE_02)
  {
    if (wd_royl_patch_module_02_ccc(patched, size) != 0)
    {
      goto finish;
    }
  }
  else
  {
    if (wd_royl_patch_module_32_ccc(patched, size) != 0)
    {
      goto finish;
    }
  }

  if (memcmp(original, patched, size) == 0)
  {
    wd_royl_show_message_ccc(
        GTK_MESSAGE_INFO, _("WD ROYL Slow Fix"),
        _("Module 0x%04x is already patched. Nothing was written."), module);
    result = 0;
    goto finish;
  }

  if (module == WD_ROYL_MODULE_02)
  {
    wd_royl_make_backup_path_ccc(backup_name, sizeof(backup_name),
                                 "mod02_original");
  }
  else
  {
    wd_royl_make_backup_path_ccc(backup_name, sizeof(backup_name),
                                 "mod32_original");
  }
  if (wd_royl_write_file_ccc(backup_name, original, size) != 0)
  {
    goto finish;
  }
  {
    char message[1024];
    snprintf(message, sizeof(message),
             _("Backup of module 0x%04x saved to:\n%s\n\nWriting the patched "
               "module now."),
             module, backup_name);
    wd_royl_progress_update_ccc(message, 0, 1);
  }

  if (wd_royl_write_module_ccc(module, patched, size,
                               wd_royl_progress_update_ccc) != 0)
  {
    goto finish;
  }
  if (wd_royl_verify_module_ccc(module, patched, size,
                                wd_royl_progress_update_ccc) != 0)
  {
    goto finish;
  }
  result = 0;

finish:
  free(original);
  free(patched);
  return result;
}

void open_wd_royl_slow_fix_dialog_ccc(void)
{
  GtkWidget *dialog;
  GtkWidget *content;
  GtkWidget *vbox;
  GtkWidget *description;
  GtkWidget *location_frame;
  GtkWidget *location_box;
  GtkWidget *ram_radio;
  GtkWidget *sa_radio;
  GtkWidget *modules_frame;
  GtkWidget *modules_box;
  GtkWidget *mod02_check;
  GtkWidget *mod32_check;
  GtkWidget *warning_label;
  bool use_sa;
  bool do_mod02;
  bool do_mod32;
  bool temporary_source_access = false;
  unsigned int module_mask = 0;
  int response;

  if (!wd_royl_operation_ready_ccc())
  {
    return;
  }

  dialog = gtk_dialog_new_with_buttons(
      _("WD ROYL Slow Fix"), GTK_WINDOW(main_window_ccc),
      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CANCEL,
      GTK_RESPONSE_CANCEL, _("Apply Patch"), GTK_RESPONSE_ACCEPT, NULL);
  gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);

  content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  gtk_container_set_border_width(GTK_CONTAINER(content), 12);

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_box_pack_start(GTK_BOX(content), vbox, TRUE, TRUE, 0);

  description = gtk_label_new(
      _("This applies the common Western Digital ROYL slow-responding fix. "
        "Use it only when reads are good but the drive is very slow.\n\n"
        "The RAM patch is temporary and is lost when the drive is power "
        "cycled. The service area patch is permanent and must be used with "
        "care."));
  gtk_label_set_line_wrap(GTK_LABEL(description), TRUE);
  gtk_label_set_xalign(GTK_LABEL(description), 0.0);
  gtk_box_pack_start(GTK_BOX(vbox), description, FALSE, FALSE, 0);

  location_frame = gtk_frame_new(_("Patch location"));
  gtk_box_pack_start(GTK_BOX(vbox), location_frame, FALSE, FALSE, 0);
  location_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_container_set_border_width(GTK_CONTAINER(location_box), 8);
  gtk_container_add(GTK_CONTAINER(location_frame), location_box);

  ram_radio = gtk_radio_button_new_with_label(
      NULL, _("RAM patch (temporary, until the drive is power cycled)"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ram_radio), TRUE);
  gtk_box_pack_start(GTK_BOX(location_box), ram_radio, FALSE, FALSE, 0);
  sa_radio = gtk_radio_button_new_with_label_from_widget(
      GTK_RADIO_BUTTON(ram_radio),
      _("Service area patch (permanent)"));
  gtk_box_pack_start(GTK_BOX(location_box), sa_radio, FALSE, FALSE, 0);

  modules_frame = gtk_frame_new(_("Modules to patch"));
  gtk_box_pack_start(GTK_BOX(vbox), modules_frame, FALSE, FALSE, 0);
  modules_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_container_set_border_width(GTK_CONTAINER(modules_box), 8);
  gtk_container_add(GTK_CONTAINER(modules_frame), modules_box);

  mod02_check = gtk_check_button_new_with_label(
      _("Module 02 (recommended, disables background scanning)"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mod02_check), TRUE);
  gtk_box_pack_start(GTK_BOX(modules_box), mod02_check, FALSE, FALSE, 0);
  mod32_check = gtk_check_button_new_with_label(
      _("Module 32 (clears the RE-LO list; normally not needed)"));
  gtk_box_pack_start(GTK_BOX(modules_box), mod32_check, FALSE, FALSE, 0);

  warning_label = gtk_label_new(
      _("Service area writes modify the drive firmware. A backup is saved "
        "before each write, but do not remove power or disconnect the drive "
        "while the operation is running."));
  gtk_label_set_line_wrap(GTK_LABEL(warning_label), TRUE);
  gtk_label_set_xalign(GTK_LABEL(warning_label), 0.0);
  gtk_box_pack_start(GTK_BOX(vbox), warning_label, FALSE, FALSE, 0);

  gtk_widget_show_all(dialog);
  response = gtk_dialog_run(GTK_DIALOG(dialog));

  use_sa = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(sa_radio));
  do_mod02 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mod02_check));
  do_mod32 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mod32_check));
  gtk_widget_destroy(dialog);

  if (response != GTK_RESPONSE_ACCEPT)
  {
    return;
  }
  if (!do_mod02 && !do_mod32)
  {
    wd_royl_show_message_ccc(GTK_MESSAGE_WARNING, _("WD ROYL Slow Fix"),
                             _("Select at least one module to patch."));
    return;
  }
  if (do_mod02)
  {
    module_mask |= WD_ROYL_MODULE_MASK_02;
  }
  if (do_mod32)
  {
    module_mask |= WD_ROYL_MODULE_MASK_32;
  }

  if (use_sa)
  {
    if (!wd_royl_confirm_ccc(
            _("Confirm service area patch"),
            _("Write Service Area"),
            _("You are about to modify the service area of the selected "
              "drive.\n\nThe original module(s) will be saved to a backup "
              "file before writing, and every write is verified.\n\nIf power "
              "is lost during a write, the drive may be damaged. Continue?")))
    {
      return;
    }

    if (!wd_royl_begin_source_access_ccc(&temporary_source_access))
    {
      return;
    }

    wd_royl_progress_begin_ccc(_("WD ROYL Slow Fix"),
                               _("Starting service area patch"));
    if (do_mod02 && wd_royl_patch_sa_module_ccc(WD_ROYL_MODULE_02) != 0)
    {
      wd_royl_progress_end_ccc();
      wd_royl_end_source_access_ccc(temporary_source_access);
      wd_royl_report_error_ccc(
          _("WD ROYL Slow Fix"),
          _("Failed to patch module 02. The drive was not left in a verified "
            "state."));
      return;
    }
    if (do_mod32 && wd_royl_patch_sa_module_ccc(WD_ROYL_MODULE_32) != 0)
    {
      wd_royl_progress_end_ccc();
      wd_royl_end_source_access_ccc(temporary_source_access);
      if (do_mod02)
      {
        wd_royl_report_error_ccc(
            _("WD ROYL Slow Fix"),
            _("Module 02 was patched and verified, but module 32 failed."));
      }
      else
      {
        wd_royl_report_error_ccc(
            _("WD ROYL Slow Fix"),
            _("Failed to patch module 32. The drive was not left in a "
              "verified state."));
      }
      return;
    }
    wd_royl_progress_end_ccc();
    wd_royl_end_source_access_ccc(temporary_source_access);

    wd_royl_show_message_ccc(
        GTK_MESSAGE_INFO, _("WD ROYL Slow Fix"),
        _("The service area patch completed and was verified.\n\nPower cycle "
          "the drive for the changes to take effect."));
  }
  else
  {
    unsigned int count_02 = 0;
    unsigned int count_32 = 0;

    if (!wd_royl_confirm_ccc(
            _("Confirm RAM patch"), _("Patch RAM"),
            _("This patches module(s) in the selected drive RAM.\n\nThe "
              "change is temporary and will be lost when the drive is power "
              "cycled. Continue?")))
    {
      return;
    }

    if (!wd_royl_begin_source_access_ccc(&temporary_source_access))
    {
      return;
    }

    wd_royl_progress_begin_ccc(_("WD ROYL Slow Fix"),
                               _("Starting RAM patch"));
    if (wd_royl_patch_ram_modules_ccc(module_mask, &count_02, &count_32,
                                      wd_royl_progress_update_ccc) != 0)
    {
      wd_royl_progress_end_ccc();
      wd_royl_end_source_access_ccc(temporary_source_access);
      wd_royl_report_error_ccc(_("WD ROYL Slow Fix"),
                               _("The RAM patch failed."));
      return;
    }
    wd_royl_progress_end_ccc();
    wd_royl_end_source_access_ccc(temporary_source_access);

    wd_royl_show_message_ccc(
        GTK_MESSAGE_INFO, _("WD ROYL Slow Fix"),
        _("The RAM patch completed and was verified.\n\nModule 02 copies "
          "patched: %u\nModule 32 copies patched: %u\n\nPower cycle the drive "
          "to undo the change."),
        count_02, count_32);
  }
}

void run_wd_royl_read_rom_ccc(void)
{
  uint8_t *rom = NULL;
  size_t rom_size = 0;
  char default_name[256];
  char *filename;
  int result;
  bool temporary_source_access = false;

  if (!wd_royl_operation_ready_ccc())
  {
    return;
  }

  wd_royl_make_backup_name_ccc(default_name, sizeof(default_name), "rom");
  filename = wd_royl_choose_file_ccc(true, _("Save WD ROYL ROM dump"),
                                     default_name);
  if (filename == NULL)
  {
    return;
  }

  if (!wd_royl_begin_source_access_ccc(&temporary_source_access))
  {
    g_free(filename);
    return;
  }

  wd_royl_progress_begin_ccc(_("WD ROYL Read ROM"),
                             _("Starting ROM read"));
  result = wd_royl_read_rom_ccc(&rom, &rom_size,
                                wd_royl_progress_update_ccc);
  wd_royl_progress_end_ccc();
  wd_royl_end_source_access_ccc(temporary_source_access);

  if (result != 0)
  {
    wd_royl_report_error_ccc(_("WD ROYL Read ROM"),
                             _("Failed to read the ROM from the drive."));
    g_free(filename);
    return;
  }

  if (wd_royl_write_file_ccc(filename, rom, rom_size) != 0)
  {
    wd_royl_report_error_ccc(_("WD ROYL Read ROM"),
                             _("The ROM was read from the drive, but the file "
                               "could not be saved."));
    g_free(filename);
    free(rom);
    return;
  }

  wd_royl_show_message_ccc(GTK_MESSAGE_INFO, _("WD ROYL Read ROM"),
                           _("ROM read successful.\n\n%zu bytes were saved "
                             "to:\n%s"),
                           rom_size, filename);
  g_free(filename);
  free(rom);
}

void run_wd_royl_write_rom_ccc(void)
{
  uint8_t *new_rom = NULL;
  uint8_t *current_rom = NULL;
  size_t new_size = 0;
  size_t current_size = 0;
  char backup_name[512];
  char *filename;
  int result;
  bool temporary_source_access = false;

  if (!wd_royl_operation_ready_ccc())
  {
    return;
  }

  filename = wd_royl_choose_file_ccc(false, _("Select WD ROYL ROM file"),
                                     NULL);
  if (filename == NULL)
  {
    return;
  }

  if (wd_royl_read_file_ccc(filename, &new_rom, &new_size) != 0)
  {
    wd_royl_report_error_ccc(_("WD ROYL Write ROM"),
                             _("Unable to read the selected ROM file."));
    g_free(filename);
    return;
  }
  if (wd_royl_validate_rom_ccc(new_rom, new_size) != 0)
  {
    wd_royl_show_message_ccc(
        GTK_MESSAGE_ERROR, _("WD ROYL Write ROM"),
        _("The selected file is not a valid ROYL ROM image. It has not been "
          "written to the drive.\n\n%s"),
        wd_royl_last_error_ccc());
    free(new_rom);
    g_free(filename);
    return;
  }

  if (!wd_royl_begin_source_access_ccc(&temporary_source_access))
  {
    free(new_rom);
    g_free(filename);
    return;
  }

  wd_royl_progress_begin_ccc(_("WD ROYL Write ROM"),
                             _("Checking the current ROM"));
  if (wd_royl_get_rom_size_ccc(&current_size) != 0)
  {
    wd_royl_progress_end_ccc();
    wd_royl_end_source_access_ccc(temporary_source_access);
    wd_royl_report_error_ccc(_("WD ROYL Write ROM"),
                             _("Unable to read the ROM size from the drive."));
    free(new_rom);
    g_free(filename);
    return;
  }
  if (current_size != new_size)
  {
    wd_royl_progress_end_ccc();
    wd_royl_end_source_access_ccc(temporary_source_access);
    wd_royl_show_message_ccc(
        GTK_MESSAGE_ERROR, _("WD ROYL Write ROM"),
        _("The selected ROM is %zu bytes, but the drive reports a %zu byte "
          "ROM. The file has not been written."),
        new_size, current_size);
    free(new_rom);
    g_free(filename);
    return;
  }

  // Always keep a copy of the ROM that is currently in the drive. If this
  // fails there is no safe way to continue.
  if (wd_royl_read_rom_ccc(&current_rom, &current_size,
                           wd_royl_progress_update_ccc) != 0)
  {
    wd_royl_progress_end_ccc();
    wd_royl_end_source_access_ccc(temporary_source_access);
    wd_royl_report_error_ccc(
        _("WD ROYL Write ROM"),
        _("Unable to back up the current ROM. The file has not been written."));
    free(new_rom);
    g_free(filename);
    return;
  }
  wd_royl_progress_end_ccc();

  if (current_size != new_size)
  {
    wd_royl_end_source_access_ccc(temporary_source_access);
    wd_royl_show_message_ccc(
        GTK_MESSAGE_ERROR, _("WD ROYL Write ROM"),
        _("The ROM backup size (%zu bytes) does not match the selected file "
          "(%zu bytes). The file has not been written."),
        current_size, new_size);
    free(current_rom);
    free(new_rom);
    g_free(filename);
    return;
  }

  wd_royl_make_backup_path_ccc(backup_name, sizeof(backup_name),
                               "rom_backup");
  if (wd_royl_write_file_ccc(backup_name, current_rom, current_size) != 0)
  {
    wd_royl_end_source_access_ccc(temporary_source_access);
    wd_royl_report_error_ccc(
        _("WD ROYL Write ROM"),
        _("Unable to save the ROM backup file. The selected file has not been "
          "written."));
    free(current_rom);
    free(new_rom);
    g_free(filename);
    return;
  }

  if (!wd_royl_confirm_ccc(
          _("Confirm ROM write"), _("Erase and Write ROM"),
          _("You are about to erase and rewrite the ROM of the selected "
            "drive.\n\nFile: %s\nSize: %zu bytes\nBackup: %s\n\nDo not remove "
            "power or disconnect the drive while this is running. If the "
            "write fails after the erase, do not power off the drive.\n\n"
            "Continue?"),
          filename, new_size, backup_name))
  {
    wd_royl_end_source_access_ccc(temporary_source_access);
    free(current_rom);
    free(new_rom);
    g_free(filename);
    return;
  }

  wd_royl_progress_begin_ccc(_("WD ROYL Write ROM"), _("Erasing ROM"));
  result = wd_royl_write_rom_ccc(new_rom, new_size, false,
                                 wd_royl_progress_update_ccc);
  if (result == 0)
  {
    result = wd_royl_verify_rom_ccc(new_rom, new_size,
                                    wd_royl_progress_update_ccc);
  }
  wd_royl_progress_end_ccc();
  wd_royl_end_source_access_ccc(temporary_source_access);

  if (result != 0)
  {
    wd_royl_show_message_ccc(
        GTK_MESSAGE_ERROR, _("WD ROYL Write ROM"),
        _("The ROM write or verification failed.\n\n%s\n\nDo not power off "
          "the drive. Try the write again; if it still fails, the backup ROM "
          "is at:\n%s"),
        wd_royl_last_error_ccc(), backup_name);
  }
  else
  {
    wd_royl_show_message_ccc(
        GTK_MESSAGE_INFO, _("WD ROYL Write ROM"),
        _("The ROM was written and verified successfully.\n\nThe previous "
          "ROM backup is at:\n%s\n\nPower cycle the drive before using it."),
        backup_name);
  }

  free(current_rom);
  free(new_rom);
  g_free(filename);
}
