#! /usr/bin/env python3

import argparse
import json
import os
import sys

import jinja2

def getopts(argv):
    parser = argparse.ArgumentParser(description='Eovim Plugin Helper')
    parser.add_argument("model", metavar='MODEL.json')
    parser.add_argument("--template", "-t", metavar='TEMPLATE.j2',
                        required=True, action='append')
    parser.add_argument("--output", "-o", required=True,
                        action='append',
                        help='Output of a --template option (one per -t)')

    args = parser.parse_args(argv[1:])
    if len(args.template) != len(args.output):
        print("*** One -o shall correspond to each -t option")
        sys.exit(1)
    return args

def main(argv):
    args = getopts(argv)

    with open(args.model, 'r') as stream:
        model = json.load(stream)

    plugin = model["plugin"]
    keys = plugin.get("params", [])
    stringshares = model.get("stringshares", [])
    stringshares.extend(keys)
    stringshares = [{"name": x.upper(), "value": x} \
                    for x in stringshares]

    context = {
        "plugin": {
            "name": plugin["name"],
            "params": keys,
        },
        "stringshares": stringshares,
    }

    search_paths = []
    for template in args.template:
        search_paths.append(os.path.dirname(template))

    j2_env = jinja2.Environment(
        loader=jinja2.FileSystemLoader(search_paths),
        undefined=jinja2.StrictUndefined
    )
    for template_file, output in zip(args.template, args.output):
        filename = os.path.basename(template_file)
        template = j2_env.get_template(filename)
        result = template.render(context)
        with open(output, 'w') as stream:
            stream.write(result)

if __name__ == "__main__":
    main(sys.argv)
