#!/usr/bin/env python3
"""
Remove individual edit_XXX() handlers from conf.cpp while preserving:
1. The 9 custom handlers with special logic
2. The 5 generic type handlers
3. The dispatch_edit() function
4. All category dispatch functions
5. Helper functions
"""

import re

# Handlers to preserve (custom logic that can't be replaced by generic handlers)
PRESERVE_HANDLERS = {
    'edit_log_file',
    'edit_target_dir',
    'edit_text_changes',
    'edit_picture_filename',
    'edit_movie_filename',
    'edit_snapshot_filename',
    'edit_timelapse_filename',
    'edit_device_id',
    'edit_pause',
    # Generic handlers and utilities
    'edit_set_bool',
    'edit_get_bool',
    'edit_generic_bool',
    'edit_generic_int',
    'edit_generic_float',
    'edit_generic_string',
    'edit_generic_list',
    # Dispatch and category functions
    'dispatch_edit',
    'edit_cat',
    'edit_get',
    'edit_set',
    'edit_list',
}

def extract_function_name(line):
    """Extract function name from declaration line."""
    match = re.match(r'void cls_config::(edit_\w+)\(', line)
    return match.group(1) if match else None

def should_remove_handler(func_name):
    """Determine if handler should be removed."""
    if not func_name:
        return False

    # Keep preserved handlers
    if func_name in PRESERVE_HANDLERS:
        return False

    # Keep category dispatch functions (edit_cat00 through edit_cat18)
    if re.match(r'edit_cat\d+$', func_name):
        return False

    # Remove all other edit_XXX handlers
    if func_name.startswith('edit_') and func_name not in PRESERVE_HANDLERS:
        return True

    return False

def process_file(input_path, output_path):
    """Remove individual handlers while preserving essential functions."""
    with open(input_path, 'r') as f:
        lines = f.readlines()

    output_lines = []
    i = 0
    removed_count = 0

    while i < len(lines):
        line = lines[i]

        # Check if this is a function declaration
        if line.startswith('void cls_config::edit_'):
            func_name = extract_function_name(line)

            if should_remove_handler(func_name):
                # Find the end of this function (next function or end of file)
                func_start = i
                i += 1

                # Scan forward to find the end of the function
                brace_count = 0
                in_function = False

                while i < len(lines):
                    if '{' in lines[i]:
                        brace_count += lines[i].count('{')
                        in_function = True
                    if '}' in lines[i]:
                        brace_count -= lines[i].count('}')

                    if in_function and brace_count == 0:
                        # Function ended, skip to next non-blank line
                        i += 1
                        while i < len(lines) and lines[i].strip() == '':
                            i += 1
                        break

                    i += 1

                removed_count += 1
                print(f"Removed: {func_name} (lines {func_start+1} to {i})")
                continue
            else:
                print(f"Keeping: {func_name}")

        output_lines.append(line)
        i += 1

    # Write output
    with open(output_path, 'w') as f:
        f.writelines(output_lines)

    print(f"\nTotal handlers removed: {removed_count}")
    print(f"Output written to: {output_path}")

    return removed_count

if __name__ == '__main__':
    input_file = '/Users/tshuey/Documents/GitHub/motion/src/conf.cpp'
    output_file = '/Users/tshuey/Documents/GitHub/motion/src/conf.cpp.new'

    count = process_file(input_file, output_file)
    print(f"\nTo apply changes: mv {output_file} {input_file}")
