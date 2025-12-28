#!/bin/bash

# List of custom handlers to KEEP (preserve these)
KEEP_HANDLERS=(
    "edit_log_file"
    "edit_target_dir"
    "edit_text_changes"
    "edit_picture_filename"
    "edit_movie_filename"
    "edit_snapshot_filename"
    "edit_timelapse_filename"
    "edit_device_id"
    "edit_pause"
)

# Also keep these essential functions
KEEP_FUNCTIONS=(
    "edit_set_bool"
    "edit_get_bool"
    "edit_generic_bool"
    "edit_generic_int"
    "edit_generic_float"
    "edit_generic_string"
    "edit_generic_list"
    "dispatch_edit"
    "edit_cat"
)

# Extract all edit handler function names from conf.cpp
grep -n "^void cls_config::edit_" /Users/tshuey/Documents/GitHub/motion/src/conf.cpp | \
while IFS=: read -r line_num func_decl; do
    # Extract function name (between edit_ and the first parenthesis)
    func_name=$(echo "$func_decl" | sed 's/^void cls_config::\(edit_[^(]*\).*/\1/')

    # Check if this handler should be kept
    keep=0
    for keep_handler in "${KEEP_HANDLERS[@]}" "${KEEP_FUNCTIONS[@]}"; do
        if [[ "$func_name" == "$keep_handler" ]]; then
            keep=1
            break
        fi
    done

    if [[ $keep -eq 0 ]] && [[ $line_num -ge 356 ]] && [[ $line_num -le 3159 ]]; then
        echo "REMOVE: Line $line_num - $func_name"
    else
        echo "KEEP: Line $line_num - $func_name"
    fi
done
