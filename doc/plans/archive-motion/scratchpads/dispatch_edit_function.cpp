// Motion Configuration System - Dispatch Edit Function
// Generated from analysis of src/conf.cpp handlers (lines 356-3159)
// This function consolidates all simple edit handlers into a centralized dispatcher
//
// Custom handlers (preserved as separate functions):
//   - log_file: date/time formatting with strftime
//   - target_dir: special path handling (removes trailing slash)
//   - text_changes: custom bool logic (see existing handler)
//   - picture_filename: path normalization (removes leading slash)
//   - movie_filename: path normalization (removes leading slash)
//   - snapshot_filename: path normalization (removes leading slash)
//   - timelapse_filename: path normalization (removes leading slash)
//   - device_id: duplicate checking with app->cam_list and app->snd_list
//   - pause: special list handling with deprecation warnings

void cls_config::dispatch_edit(const std::string& name, std::string& parm, enum PARM_ACT pact)
{
    // BOOLEANS
    if (name == "daemon") return edit_generic_bool(daemon, parm, pact, false);
    if (name == "native_language") return edit_generic_bool(native_language, parm, pact, true);
    if (name == "emulate_motion") return edit_generic_bool(emulate_motion, parm, pact, false);
    if (name == "threshold_tune") return edit_generic_bool(threshold_tune, parm, pact, false);
    if (name == "noise_tune") return edit_generic_bool(noise_tune, parm, pact, true);
    if (name == "movie_output") return edit_generic_bool(movie_output, parm, pact, true);
    if (name == "movie_output_motion") return edit_generic_bool(movie_output_motion, parm, pact, false);
    if (name == "movie_all_frames") return edit_generic_bool(movie_all_frames, parm, pact, false);
    if (name == "movie_extpipe_use") return edit_generic_bool(movie_extpipe_use, parm, pact, false);
    if (name == "webcontrol_localhost") return edit_generic_bool(webcontrol_localhost, parm, pact, true);
    if (name == "webcontrol_ipv6") return edit_generic_bool(webcontrol_ipv6, parm, pact, false);
    if (name == "webcontrol_tls") return edit_generic_bool(webcontrol_tls, parm, pact, false);
    if (name == "webcontrol_actions") return edit_generic_bool(webcontrol_actions, parm, pact, true);
    if (name == "webcontrol_html") return edit_generic_bool(webcontrol_html, parm, pact, true);
    if (name == "stream_preview_newline") return edit_generic_bool(stream_preview_newline, parm, pact, false);
    if (name == "stream_grey") return edit_generic_bool(stream_grey, parm, pact, false);
    if (name == "stream_motion") return edit_generic_bool(stream_motion, parm, pact, false);
    if (name == "ptz_auto_track") return edit_generic_bool(ptz_auto_track, parm, pact, false);

    // INTEGERS with ranges
    if (name == "log_level") return edit_generic_int(log_level, parm, pact, 6, 1, 9);
    if (name == "log_fflevel") return edit_generic_int(log_fflevel, parm, pact, 3, 1, 9);
    if (name == "device_tmo") return edit_generic_int(device_tmo, parm, pact, 30, 1, INT_MAX);
    if (name == "watchdog_tmo") return edit_generic_int(watchdog_tmo, parm, pact, 90, 1, INT_MAX);
    if (name == "watchdog_kill") return edit_generic_int(watchdog_kill, parm, pact, 0, 0, INT_MAX);
    if (name == "libcam_buffer_count") return edit_generic_int(libcam_buffer_count, parm, pact, 4, 2, 8);
    if (name == "width") return edit_generic_int(width, parm, pact, 640, 64, 9999);
    if (name == "height") return edit_generic_int(height, parm, pact, 480, 64, 9999);
    if (name == "framerate") return edit_generic_int(framerate, parm, pact, 15, 2, 100);
    if (name == "rotate") return edit_generic_int(rotate, parm, pact, 0, 0, 270);  // Special: only 0,90,180,270
    if (name == "text_scale") return edit_generic_int(text_scale, parm, pact, 1, 1, 10);
    if (name == "threshold") return edit_generic_int(threshold, parm, pact, 1500, 1, 2147483647);
    if (name == "threshold_maximum") return edit_generic_int(threshold_maximum, parm, pact, 0, 0, INT_MAX);
    if (name == "threshold_sdevx") return edit_generic_int(threshold_sdevx, parm, pact, 0, 0, INT_MAX);
    if (name == "threshold_sdevy") return edit_generic_int(threshold_sdevy, parm, pact, 0, 0, INT_MAX);
    if (name == "threshold_sdevxy") return edit_generic_int(threshold_sdevxy, parm, pact, 0, 0, INT_MAX);
    if (name == "threshold_ratio") return edit_generic_int(threshold_ratio, parm, pact, 0, 0, 100);
    if (name == "threshold_ratio_change") return edit_generic_int(threshold_ratio_change, parm, pact, 64, 0, 255);
    if (name == "noise_level") return edit_generic_int(noise_level, parm, pact, 32, 1, 255);
    if (name == "smart_mask_speed") return edit_generic_int(smart_mask_speed, parm, pact, 0, 0, 10);
    if (name == "lightswitch_percent") return edit_generic_int(lightswitch_percent, parm, pact, 0, 0, 100);
    if (name == "lightswitch_frames") return edit_generic_int(lightswitch_frames, parm, pact, 5, 1, 1000);
    if (name == "minimum_motion_frames") return edit_generic_int(minimum_motion_frames, parm, pact, 1, 1, 10000);
    if (name == "static_object_time") return edit_generic_int(static_object_time, parm, pact, 10, 1, INT_MAX);
    if (name == "event_gap") return edit_generic_int(event_gap, parm, pact, 60, 0, 2147483647);
    if (name == "pre_capture") return edit_generic_int(pre_capture, parm, pact, 3, 0, 1000);
    if (name == "post_capture") return edit_generic_int(post_capture, parm, pact, 10, 0, 2147483647);
    if (name == "picture_quality") return edit_generic_int(picture_quality, parm, pact, 75, 1, 100);
    if (name == "snapshot_interval") return edit_generic_int(snapshot_interval, parm, pact, 0, 0, 2147483647);
    if (name == "movie_max_time") return edit_generic_int(movie_max_time, parm, pact, 120, 0, 2147483647);
    if (name == "movie_bps") return edit_generic_int(movie_bps, parm, pact, 400000, 0, INT_MAX);
    if (name == "movie_quality") return edit_generic_int(movie_quality, parm, pact, 60, 1, 100);
    if (name == "timelapse_interval") return edit_generic_int(timelapse_interval, parm, pact, 0, 0, 2147483647);
    if (name == "timelapse_fps") return edit_generic_int(timelapse_fps, parm, pact, 30, 1, 100);
    if (name == "webcontrol_port") return edit_generic_int(webcontrol_port, parm, pact, 8080, 0, 65535);
    if (name == "webcontrol_port2") return edit_generic_int(webcontrol_port2, parm, pact, 8081, 0, 65535);
    if (name == "webcontrol_lock_minutes") return edit_generic_int(webcontrol_lock_minutes, parm, pact, 5, 0, INT_MAX);
    if (name == "webcontrol_lock_attempts") return edit_generic_int(webcontrol_lock_attempts, parm, pact, 5, 1, INT_MAX);
    if (name == "stream_preview_scale") return edit_generic_int(stream_preview_scale, parm, pact, 25, 1, 100);
    if (name == "stream_quality") return edit_generic_int(stream_quality, parm, pact, 60, 1, 100);
    if (name == "stream_maxrate") return edit_generic_int(stream_maxrate, parm, pact, 1, 0, 100);
    if (name == "stream_scan_time") return edit_generic_int(stream_scan_time, parm, pact, 5, 0, 3600);
    if (name == "stream_scan_scale") return edit_generic_int(stream_scan_scale, parm, pact, 2, 1, 32);
    if (name == "database_port") return edit_generic_int(database_port, parm, pact, 0, 0, 65535);
    if (name == "database_busy_timeout") return edit_generic_int(database_busy_timeout, parm, pact, 0, 0, INT_MAX);
    if (name == "ptz_wait") return edit_generic_int(ptz_wait, parm, pact, 1, 0, INT_MAX);

    // FLOATS with ranges - libcam parameters
    if (name == "libcam_brightness") return edit_generic_float(parm_cam.libcam_brightness, parm, pact, 0.0f, -1.0f, 1.0f);
    if (name == "libcam_contrast") return edit_generic_float(parm_cam.libcam_contrast, parm, pact, 1.0f, 0.0f, 32.0f);

    // INTEGERS - libcam ISO parameter
    if (name == "libcam_iso") return edit_generic_int(parm_cam.libcam_iso, parm, pact, 100, 100, 6400);

    // STRINGS (simple assignment)
    if (name == "conf_filename") return edit_generic_string(conf_filename, parm, pact, "");
    if (name == "pid_file") return edit_generic_string(pid_file, parm, pact, "");
    if (name == "device_name") return edit_generic_string(device_name, parm, pact, "");
    if (name == "v4l2_device") return edit_generic_string(v4l2_device, parm, pact, "");
    if (name == "v4l2_params") return edit_generic_string(v4l2_params, parm, pact, "");
    if (name == "netcam_url") return edit_generic_string(netcam_url, parm, pact, "");
    if (name == "netcam_params") return edit_generic_string(netcam_params, parm, pact, "");
    if (name == "netcam_high_url") return edit_generic_string(netcam_high_url, parm, pact, "");
    if (name == "netcam_high_params") return edit_generic_string(netcam_high_params, parm, pact, "");
    if (name == "netcam_userpass") return edit_generic_string(netcam_userpass, parm, pact, "");
    if (name == "libcam_device") return edit_generic_string(libcam_device, parm, pact, "");
    if (name == "libcam_params") return edit_generic_string(libcam_params, parm, pact, "");
    if (name == "schedule_params") return edit_generic_string(schedule_params, parm, pact, "");
    if (name == "cleandir_params") return edit_generic_string(cleandir_params, parm, pact, "");
    if (name == "config_dir") return edit_generic_string(config_dir, parm, pact, "");
    if (name == "text_left") return edit_generic_string(text_left, parm, pact, "");
    if (name == "text_right") return edit_generic_string(text_right, parm, pact, "%Y-%m-%d\\n%T");
    if (name == "text_event") return edit_generic_string(text_event, parm, pact, "%Y%m%d%H%M%S");
    if (name == "despeckle_filter") return edit_generic_string(despeckle_filter, parm, pact, "EedDl");
    if (name == "area_detect") return edit_generic_string(area_detect, parm, pact, "");
    if (name == "mask_file") return edit_generic_string(mask_file, parm, pact, "");
    if (name == "mask_privacy") return edit_generic_string(mask_privacy, parm, pact, "");
    if (name == "secondary_params") return edit_generic_string(secondary_params, parm, pact, "");
    if (name == "on_event_start") return edit_generic_string(on_event_start, parm, pact, "");
    if (name == "on_event_end") return edit_generic_string(on_event_end, parm, pact, "");
    if (name == "on_picture_save") return edit_generic_string(on_picture_save, parm, pact, "");
    if (name == "on_area_detected") return edit_generic_string(on_area_detected, parm, pact, "");
    if (name == "on_motion_detected") return edit_generic_string(on_motion_detected, parm, pact, "");
    if (name == "on_movie_start") return edit_generic_string(on_movie_start, parm, pact, "");
    if (name == "on_movie_end") return edit_generic_string(on_movie_end, parm, pact, "");
    if (name == "on_camera_lost") return edit_generic_string(on_camera_lost, parm, pact, "");
    if (name == "on_camera_found") return edit_generic_string(on_camera_found, parm, pact, "");
    if (name == "on_secondary_detect") return edit_generic_string(on_secondary_detect, parm, pact, "");
    if (name == "on_action_user") return edit_generic_string(on_action_user, parm, pact, "");
    if (name == "on_sound_alert") return edit_generic_string(on_sound_alert, parm, pact, "");
    if (name == "picture_exif") return edit_generic_string(picture_exif, parm, pact, "");
    if (name == "movie_extpipe") return edit_generic_string(movie_extpipe, parm, pact, "");
    if (name == "video_pipe") return edit_generic_string(video_pipe, parm, pact, "");
    if (name == "video_pipe_motion") return edit_generic_string(video_pipe_motion, parm, pact, "");
    if (name == "webcontrol_base_path") return edit_generic_string(webcontrol_base_path, parm, pact, "/");
    if (name == "webcontrol_parms") return edit_generic_string(webcontrol_parms, parm, pact, "");
    if (name == "webcontrol_cert") return edit_generic_string(webcontrol_cert, parm, pact, "");
    if (name == "webcontrol_key") return edit_generic_string(webcontrol_key, parm, pact, "");
    if (name == "webcontrol_headers") return edit_generic_string(webcontrol_headers, parm, pact, "");
    if (name == "webcontrol_lock_script") return edit_generic_string(webcontrol_lock_script, parm, pact, "");
    if (name == "stream_preview_params") return edit_generic_string(stream_preview_params, parm, pact, "");
    if (name == "database_dbname") return edit_generic_string(database_dbname, parm, pact, "motion");
    if (name == "database_host") return edit_generic_string(database_host, parm, pact, "");
    if (name == "database_user") return edit_generic_string(database_user, parm, pact, "");
    if (name == "database_password") return edit_generic_string(database_password, parm, pact, "");
    if (name == "sql_event_start") return edit_generic_string(sql_event_start, parm, pact, "");
    if (name == "sql_event_end") return edit_generic_string(sql_event_end, parm, pact, "");
    if (name == "sql_movie_start") return edit_generic_string(sql_movie_start, parm, pact, "");
    if (name == "sql_movie_end") return edit_generic_string(sql_movie_end, parm, pact, "");
    if (name == "sql_pic_save") return edit_generic_string(sql_pic_save, parm, pact, "");
    if (name == "ptz_pan_left") return edit_generic_string(ptz_pan_left, parm, pact, "");
    if (name == "ptz_pan_right") return edit_generic_string(ptz_pan_right, parm, pact, "");
    if (name == "ptz_tilt_up") return edit_generic_string(ptz_tilt_up, parm, pact, "");
    if (name == "ptz_tilt_down") return edit_generic_string(ptz_tilt_down, parm, pact, "");
    if (name == "ptz_zoom_in") return edit_generic_string(ptz_zoom_in, parm, pact, "");
    if (name == "ptz_zoom_out") return edit_generic_string(ptz_zoom_out, parm, pact, "");
    if (name == "ptz_move_track") return edit_generic_string(ptz_move_track, parm, pact, "");
    if (name == "snd_device") return edit_generic_string(snd_device, parm, pact, "");
    if (name == "snd_params") return edit_generic_string(snd_params, parm, pact, "");

    // LISTS (constrained string values)
    static const std::vector<std::string> log_type_values = {"ALL","COR","STR","ENC","NET","DBL","EVT","TRK","VID"};
    if (name == "log_type") return edit_generic_list(log_type_str, parm, pact, "ALL", log_type_values);

    static const std::vector<std::string> flip_axis_values = {"none","vertical","horizontal"};
    if (name == "flip_axis") return edit_generic_list(flip_axis, parm, pact, "none", flip_axis_values);

    static const std::vector<std::string> locate_motion_mode_values = {"off","on","preview"};
    if (name == "locate_motion_mode") return edit_generic_list(locate_motion_mode, parm, pact, "off", locate_motion_mode_values);

    static const std::vector<std::string> locate_motion_style_values = {"box","redbox","cross","redcross"};
    if (name == "locate_motion_style") return edit_generic_list(locate_motion_style, parm, pact, "box", locate_motion_style_values);

    static const std::vector<std::string> secondary_method_values = {"none","haar","hog","dnn"};
    if (name == "secondary_method") return edit_generic_list(secondary_method, parm, pact, "none", secondary_method_values);

    static const std::vector<std::string> picture_output_values = {"on","off","first","best","center"};
    if (name == "picture_output") return edit_generic_list(picture_output, parm, pact, "off", picture_output_values);

    static const std::vector<std::string> picture_output_motion_values = {"on","off","roi"};
    if (name == "picture_output_motion") return edit_generic_list(picture_output_motion, parm, pact, "off", picture_output_motion_values);

    static const std::vector<std::string> picture_type_values = {"jpg","webp","ppm"};
    if (name == "picture_type") return edit_generic_list(picture_type, parm, pact, "jpg", picture_type_values);

    static const std::vector<std::string> movie_encoder_preset_values = {"ultrafast","superfast","veryfast","faster","fast","medium","slow","slower","veryslow"};
    if (name == "movie_encoder_preset") return edit_generic_list(movie_encoder_preset, parm, pact, "medium", movie_encoder_preset_values);

    static const std::vector<std::string> movie_container_values = {"mkv","mp4","3gp"};
    if (name == "movie_container") return edit_generic_list(movie_container, parm, pact, "mkv", movie_container_values);

    static const std::vector<std::string> movie_passthrough_values = {"off","on"};
    if (name == "movie_passthrough") return edit_generic_list(movie_passthrough, parm, pact, "off", movie_passthrough_values);

    static const std::vector<std::string> timelapse_mode_values = {"off","hourly","daily","weekly","monthly"};
    if (name == "timelapse_mode") return edit_generic_list(timelapse_mode, parm, pact, "off", timelapse_mode_values);

    static const std::vector<std::string> timelapse_container_values = {"mkv","mp4","3gp"};
    if (name == "timelapse_container") return edit_generic_list(timelapse_container, parm, pact, "mkv", timelapse_container_values);

    static const std::vector<std::string> webcontrol_interface_values = {"default","auto"};
    if (name == "webcontrol_interface") return edit_generic_list(webcontrol_interface, parm, pact, "default", webcontrol_interface_values);

    static const std::vector<std::string> webcontrol_auth_method_values = {"none","basic","digest"};
    if (name == "webcontrol_auth_method") return edit_generic_list(webcontrol_auth_method, parm, pact, "none", webcontrol_auth_method_values);

    static const std::vector<std::string> webcontrol_authentication_values = {"noauth","user:pass"};
    if (name == "webcontrol_authentication") return edit_generic_list(webcontrol_authentication, parm, pact, "noauth", webcontrol_authentication_values);

    static const std::vector<std::string> stream_preview_method_values = {"mjpeg","snapshot"};
    if (name == "stream_preview_method") return edit_generic_list(stream_preview_method, parm, pact, "mjpeg", stream_preview_method_values);

    static const std::vector<std::string> stream_preview_ptz_values = {"on","off","center"};
    if (name == "stream_preview_ptz") return edit_generic_list(stream_preview_ptz, parm, pact, "off", stream_preview_ptz_values);

    static const std::vector<std::string> database_type_values = {"sqlite3","mariadb","mysql","postgresql"};
    if (name == "database_type") return edit_generic_list(database_type, parm, pact, "sqlite3", database_type_values);

    static const std::vector<std::string> snd_window_values = {"off","on"};
    if (name == "snd_window") return edit_generic_list(snd_window, parm, pact, "off", snd_window_values);

    static const std::vector<std::string> snd_show_values = {"off","on"};
    if (name == "snd_show") return edit_generic_list(snd_show, parm, pact, "off", snd_show_values);

    // CUSTOM HANDLERS (preserved - contain special logic)
    if (name == "log_file") return edit_log_file(parm, pact);
    if (name == "target_dir") return edit_target_dir(parm, pact);
    if (name == "text_changes") return edit_text_changes(parm, pact);
    if (name == "picture_filename") return edit_picture_filename(parm, pact);
    if (name == "movie_filename") return edit_movie_filename(parm, pact);
    if (name == "snapshot_filename") return edit_snapshot_filename(parm, pact);
    if (name == "timelapse_filename") return edit_timelapse_filename(parm, pact);
    if (name == "device_id") return edit_device_id(parm, pact);
    if (name == "pause") return edit_pause(parm, pact);

    // Parameter name not found - could log warning here if needed
    return;
}
