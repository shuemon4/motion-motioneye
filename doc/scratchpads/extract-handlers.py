#!/usr/bin/env python3
"""
Extract metadata from edit_XXX() handlers in conf.cpp to generate dispatch function.
"""

import re
import sys

def extract_handler_metadata(cpp_file):
    """Parse conf.cpp and extract metadata from all edit_XXX() handlers."""

    with open(cpp_file, 'r') as f:
        content = f.read()

    # Find all edit_XXX functions
    pattern = r'void cls_config::edit_(\w+)\(std::string &parm, enum PARM_ACT pact\)\s*\{([^}]+(?:\{[^}]*\}[^}]*)*)\}'
    matches = re.finditer(pattern, content, re.MULTILINE | re.DOTALL)

    handlers = []

    for match in matches:
        name = match.group(1)
        body = match.group(2)

        # Skip generic/helper handlers
        if name in ['set_bool', 'get_bool', 'generic_bool', 'generic_int', 'generic_float', 'generic_string', 'generic_list']:
            continue

        # Extract default value
        default_match = re.search(r'PARM_ACT_DFLT.*?(\w+)\s*=\s*([^;]+);', body, re.DOTALL)
        if not default_match:
            continue

        var_name = default_match.group(1)
        default_val = default_match.group(2).strip()

        # Determine type based on patterns
        handler_type = None
        min_val = None
        max_val = None
        valid_values = []
        is_custom = False

        # Check for bool pattern
        if 'edit_set_bool' in body and 'edit_get_bool' in body:
            handler_type = 'bool'
            default_val = default_val.lower()

        # Check for string pattern (simple assignment)
        elif f'{var_name} = parm;' in body and 'atoi' not in body and 'atof' not in body:
            handler_type = 'string'
            default_val = default_val.replace('"', '\\"')

        # Check for int pattern
        elif 'atoi' in body:
            handler_type = 'int'
            # Extract range
            range_match = re.search(r'\(parm_in <\s*(\d+)\)', body)
            if range_match:
                min_val = range_match.group(1)
            range_match = re.search(r'\(parm_in >\s*(\d+)\)', body)
            if range_match:
                max_val = range_match.group(1)
            if not min_val:
                min_val = '1'
            if not max_val:
                max_val = 'INT_MAX'

        # Check for float pattern
        elif 'atof' in body:
            handler_type = 'float'
            # Extract range
            range_match = re.search(r'\(parm_in <\s*([\d.-]+)\)', body)
            if range_match:
                min_val = range_match.group(1)
            range_match = re.search(r'\(parm_in >\s*([\d.-]+)\)', body)
            if range_match:
                max_val = range_match.group(1)
            if not min_val:
                min_val = '-1.0'
            if not max_val:
                max_val = '1.0'

        # Check for list pattern (has PARM_ACT_LIST and multiple comparisons)
        if 'PARM_ACT_LIST' in body and '||' in body:
            handler_type = 'list'
            # Extract valid values from comparisons
            value_matches = re.findall(r'\(parm == "(\w+)"\)', body)
            valid_values = list(set(value_matches))  # Remove duplicates

        # Check for custom logic
        if 'strftime' in body or 'strstr' in body or 'localtime' in body:
            is_custom = True
        if len(body) > 500:  # Complex handlers are likely custom
            is_custom = True

        handlers.append({
            'name': name,
            'var_name': var_name,
            'type': handler_type,
            'default': default_val,
            'min': min_val,
            'max': max_val,
            'valid_values': valid_values,
            'is_custom': is_custom
        })

    return handlers

def generate_dispatch_function(handlers):
    """Generate the dispatch_edit() function code."""

    # Group handlers by type
    bool_handlers = [h for h in handlers if h['type'] == 'bool' and not h['is_custom']]
    int_handlers = [h for h in handlers if h['type'] == 'int' and not h['is_custom']]
    float_handlers = [h for h in handlers if h['type'] == 'float' and not h['is_custom']]
    string_handlers = [h for h in handlers if h['type'] == 'string' and not h['is_custom']]
    list_handlers = [h for h in handlers if h['type'] == 'list' and not h['is_custom']]
    custom_handlers = [h for h in handlers if h['is_custom']]

    code = []
    code.append('void cls_config::dispatch_edit(const std::string& name, std::string& parm, enum PARM_ACT pact)')
    code.append('{')
    code.append('    /* Boolean parameters */')
    for h in bool_handlers:
        code.append(f'    if (name == "{h["name"]}") return edit_generic_bool({h["var_name"]}, parm, pact, {h["default"]});')

    code.append('')
    code.append('    /* Integer parameters */')
    for h in int_handlers:
        code.append(f'    if (name == "{h["name"]}") return edit_generic_int({h["var_name"]}, parm, pact, {h["default"]}, {h["min"]}, {h["max"]});')

    code.append('')
    code.append('    /* Float parameters */')
    for h in float_handlers:
        code.append(f'    if (name == "{h["name"]}") return edit_generic_float({h["var_name"]}, parm, pact, {h["default"]}f, {h["min"]}f, {h["max"]}f);')

    code.append('')
    code.append('    /* String parameters */')
    for h in string_handlers:
        code.append(f'    if (name == "{h["name"]}") return edit_generic_string({h["var_name"]}, parm, pact, "{h["default"]}");')

    code.append('')
    code.append('    /* List parameters */')
    for h in list_handlers:
        if h['valid_values']:
            values_str = ', '.join(f'"{v}"' for v in h['valid_values'])
            code.append(f'    static const std::vector<std::string> {h["name"]}_values = {{{values_str}}};')
            code.append(f'    if (name == "{h["name"]}") return edit_generic_list({h["var_name"]}, parm, pact, "{h["default"]}", {h["name"]}_values);')

    code.append('')
    code.append('    /* Custom handlers (non-standard logic) */')
    for h in custom_handlers:
        code.append(f'    if (name == "{h["name"]}") return edit_{h["name"]}(parm, pact);')

    code.append('}')

    return '\n'.join(code)

if __name__ == '__main__':
    cpp_file = '/Users/tshuey/Documents/GitHub/motion/src/conf.cpp'

    print("Extracting handler metadata...")
    handlers = extract_handler_metadata(cpp_file)

    print(f"Found {len(handlers)} handlers")
    print(f"  Bool: {len([h for h in handlers if h['type'] == 'bool' and not h['is_custom']])}")
    print(f"  Int: {len([h for h in handlers if h['type'] == 'int' and not h['is_custom']])}")
    print(f"  Float: {len([h for h in handlers if h['type'] == 'float' and not h['is_custom']])}")
    print(f"  String: {len([h for h in handlers if h['type'] == 'string' and not h['is_custom']])}")
    print(f"  List: {len([h for h in handlers if h['type'] == 'list' and not h['is_custom']])}")
    print(f"  Custom: {len([h for h in handlers if h['is_custom']])}")

    print("\nGenerating dispatch function...")
    dispatch_code = generate_dispatch_function(handlers)

    output_file = '/Users/tshuey/Documents/GitHub/motion/doc/scratchpads/dispatch-function-generated.txt'
    with open(output_file, 'w') as f:
        f.write(dispatch_code)

    print(f"\nDispatch function written to: {output_file}")
    print("\nFirst 50 lines:")
    print('\n'.join(dispatch_code.split('\n')[:50]))
