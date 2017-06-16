#! /usr/bin/env python
#
# This script is compatible with python2 and python3.
#

import argparse
import jinja2
import os
import sys
import yaml

def getopts(argv):
    parser = argparse.ArgumentParser(description='EO Generator')
    parser.add_argument(
        '--output-dir', '-o', type=str, metavar='DIR', default='.',
        help='Generation directory'
    )
    parser.add_argument(
        '--template-dir', '-I', type=str, metavar='DIR', default='.',
        help='Template directory'
    )
    parser.add_argument('api', type=str, help='API file')
    return parser.parse_args(argv[1:])

TYPE_TABLE = {
    'ArrayOf(Window)':      ('Eina_List*', 'pack_list_of_windows'),
    'ArrayOf(Buffer)':      ('Eina_List*', 'pack_list_of_buffers'),
    'ArrayOf(Tabpage)':     ('Eina_List*', 'pack_list_of_tabpages'),
    'ArrayOf(String)':      ('Eina_List*', 'pack_list_of_strings'),
    'ArrayOf(Dictionary)':  ('Eina_List*', 'pack_non_implemented'),
    'ArrayOf(Integer, 2)':  ('s_position', 'pack_position'),
    'Integer':              ('t_int',      'msgpack_pack_int64'),
    'void':                 ('void',       None),
    'String':               ('Eina_Stringshare*', 'pack_stringshare'),
    'Buffer':               ('s_buffer*',  'pack_buffer'),
    'Window':               ('s_window*',  'pack_window'),
    'Tabpage':              ('s_tabpage*', 'pack_tabpage'),
    'Dictionary':           ('Eina_Hash*', 'pack_non_implemented'),
    'Array':                ('Eina_List*', 'pack_non_implemented'),
    'Object':               ('s_object*',  'pack_object'),
    'Boolean':              ('Eina_Bool',  'pack_boolean'),
}

def convert_param(param_type):
    if not param_type in TYPE_TABLE:
        print("warning: {} is not in the conversion DB".format(param_type))
        return param_type
    else:
        return TYPE_TABLE[param_type][0]

def normalize_function(function):
    if function.startswith("nvim"):
        # Do nothing at all
        return function
    elif function.startswith("vim"):
        return function.replace("vim", "nvim")
    else:
        return "nvim_{}".format(function)

def fixup_db(db):
    to_be_removed = []
    for func in db["functions"]:
        # Exclude deprecated items
        if "deprecated_since" in func:
            if func["deprecated_since"] <= 2:
                to_be_removed.append(func)
                continue

        ret_type = convert_param(func["return_type"])
        func["request"] = "REQUEST_{}".format(func["name"].upper())
        func["return_type"] = ret_type
        #func["name"] = normalize_function(func["name"])
        c_parameters = []
        for param in func["parameters"]:
            param_type = convert_param(param[0])
            c_parameters.append((param_type, param[1]))
        func["c_parameters"] = c_parameters
        params_count = len(func["parameters"])
        if func["parameters"] == "void":
            params_count = 0
        func["parameters_count"] = params_count

    # Remove functions that we don't want to generate
    db["functions"] = [x for x in db["functions"] if x not in to_be_removed]

    packers = {}
    for key, val in TYPE_TABLE.items():
        packers[key] = val[1]
    db["packers"] = packers

def main(argv):
    args = getopts(argv)

    # Load the database
    with open(args.api, 'r') as filp:
        db_file = filp.read()
    database = yaml.load(db_file)

    fixup_db(database)

    templates = []
    for file_entry in os.listdir(args.template_dir):
        if os.path.splitext(file_entry)[1] == ".j2":
            templates.append(file_entry)

    env = jinja2.Environment(
        loader=jinja2.FileSystemLoader(args.template_dir),
        lstrip_blocks=True
    )

    if not os.path.exists(args.output_dir):
        os.makedirs(args.output_dir)

    # Template all the files
    for template in templates:
        template_name = os.path.join(
            args.output_dir,
            os.path.splitext(template)[0]
        )
        print("Generating {}".format(template_name))

        template_handle = env.get_template(template)
        generated = template_handle.render(database)

        # Print the output in stdout by default or in a file if specified
        with open(template_name, 'w') as filp:
            filp.write(generated)

    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv))
