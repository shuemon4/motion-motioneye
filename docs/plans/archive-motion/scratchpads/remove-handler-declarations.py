#!/usr/bin/env python3
"""
Remove individual edit_XXX() handler declarations from conf.hpp while preserving:
1. The 9 custom handlers with special logic
2. The 5 generic type handlers
3. The dispatch_edit() function
4. All category dispatch functions
5. Helper functions
"""

import re

# Handlers to preserve (must match what we kept in conf.cpp)
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
    match = re.search(r'void (edit_\w+)\(', line)
    return match.group(1) if match else None

def should_remove_declaration(line):
    """Determine if declaration should be removed."""
    if 'void edit_' not in line:
        return False

    func_name = extract_function_name(line)
    if not func_name:
        return False

    # Keep preserved handlers
    if func_name in PRESERVE_HANDLERS:
        return False

    # Keep category dispatch functions (edit_cat00 through edit_cat18)
    if re.match(r'edit_cat\d+$', func_name):
        return False

    # Remove all other edit_XXX declarations
    if func_name.startswith('edit_'):
        return True

    return False

def process_file(input_path, output_path):
    """Remove individual handler declarations while preserving essential ones."""
    with open(input_path, 'r') as f:
        lines = f.readlines()

    output_lines = []
    removed_count = 0

    for i, line in enumerate(lines):
        if should_remove_declaration(line):
            func_name = extract_function_name(line)
            removed_count += 1
            print(f"Removed declaration: {func_name} (line {i+1})")
            continue

        output_lines.append(line)

    # Write output
    with open(output_path, 'w') as f:
        f.writelines(output_lines)

    print(f"\nTotal declarations removed: {removed_count}")
    print(f"Output written to: {output_path}")

    return removed_count

if __name__ == '__main__':
    input_file = '/Users/tshuey/Documents/GitHub/motion/src/conf.hpp'
    output_file = '/Users/tshuey/Documents/GitHub/motion/src/conf.hpp.new'

    count = process_file(input_file, output_file)
    print(f"\nTo apply changes: mv {output_file} {input_file}")
