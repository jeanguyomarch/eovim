#! /usr/bin/env python3

import argparse
import os
import platform
import subprocess
import sys

THIS_DIR = os.path.dirname(os.path.realpath(__file__))

COLORS = {
    'green': '\033[32m',
    'bold': '\033[1m',
    'red': '\033[31m',
    'reset': '\033[0m',
}

# If the script is not run from a TTY, disable the colors
if not os.isatty(sys.stdout.fileno()):
    for item in COLORS:
        COLORS[item] = ''


def execute_cmd(cmd):
    print("{}Executing: {}{}{}".format(
        COLORS['green'], COLORS['bold'], cmd, COLORS['reset']))
    subprocess.check_call(cmd.split(" "))


class Key:
    ENV = "env"
    CMD = "pkg"

def macos_pkg_config_path_get():
    # Assuming hombrew being installed in /usr/local (default), two paths
    # shall be added to PKG_CONFIG_PATH
    paths = (
        "/usr/local/lib/pkgconfig",
        "/usr/local/opt/openssl/lib/pkgconfig",
        os.environ.get("PKG_CONFIG_PATH", ""),
    )
    return ":".join(paths)

def linux_platform_get():
    # The platform must be linux. Then, use the distro package to find out which
    # GNU/Linux flavour is auto-detected
    assert platform.system() == "Linux"
    try:
        import distro
        return distro.name()
    except ImportError:
        print("*** 'distro' package is not installed.\n"
              "*** Please run 'pip3 install --user distro', or provide your "
              "distribution name with --platform=YOUR_DISTRIBUTION",
              file=sys.stderr)
        sys.exit(2)

def msgpack_cmd_get():
    return os.path.join(THIS_DIR, "get-msgpack.sh")

MACOS_SETUP = {
    Key.ENV: {
        'PKG_CONFIG_PATH': macos_pkg_config_path_get(),
    },
    Key.CMD: [
        "brew update",
        "brew install efl",
        "brew install cmake",
        "brew install wget",
        msgpack_cmd_get(),
    ]
}

UBUNTU_SETUP = {
    Key.CMD: [
        "sudo add-apt-repository ppa:niko2040/e19",
        "sudo apt update",
        "sudo apt install libefl-dev",
        "sudo apt install cmake",
        "sudo apt install wget",
        msgpack_cmd_get(),
    ]
}

# For testing purposes
#DUMMY_SETUP = {
#    Key.CMD: [
#        "ls",
#    ],
#    Key.ENV: {
#        'TEST': "1",
#    }
#}

SETUP = {
    'Darwin': MACOS_SETUP,
    'Ubuntu': UBUNTU_SETUP,
    #'Dummy': DUMMY_SETUP, # To test
}

def platforms_list_get():
    """Return a list containing all the supported platforms, alphabetically
    sorted
    """
    return [p for p in sorted(SETUP)]

class Shell(object):
    """Contains an enumeration of all supported shells, and provide utilities
    to manipulate those shell types
    """
    SH = "sh"
    BASH = "bash"
    ZSH = "zsh"
    FISH = "fish"

    def to_list():
        """Return a list containing all supported shell types"""
        return [Shell.SH, Shell.BASH, Shell.ZSH, Shell.FISH]


def setup_cmd_walk(setup):
    if Key.CMD in setup:
        for cmd in setup[Key.CMD]:
            yield cmd

def posix_shell_export_get(env, value):
    return "export {}='{}'".format(env, value)

def fish_export_get(env, value):
    return "set -x {} '{}'".format(env, value)

def setup_env_walk(setup, shell):
    shell_formats = {
        Shell.SH: posix_shell_export_get,
        Shell.BASH: posix_shell_export_get,
        Shell.ZSH: posix_shell_export_get,
        Shell.FISH: fish_export_get,
    }
    if Key.ENV in setup:
        for env, value in setup[Key.ENV].items():
            yield env, value, shell_formats[shell](env, value)

class Help:
    EXECUTE = "Actually run the commands instead of printing them"
    PLATFORM = """Force the use of a platform instead of relying on
    auto-detection"""
    LIST = "List the available platforms"
    SHELL = """Explicitely provide the users's shell instead of relying
    on auto-detection"""

def getopts(argv):
    parser = argparse.ArgumentParser(description='Setup for Eovim')
    parser.add_argument("--execute", action='store_true', help=Help.EXECUTE)
    parser.add_argument("--platform", choices=platforms_list_get(),
                        help=Help.PLATFORM)
    parser.add_argument("--shell", choices=Shell.to_list(), help=Help.SHELL)
    return parser.parse_args(argv[1:])

def main(argv):
    args = getopts(argv)

    # Auto-detect the platform, if not explicitely provided
    if not args.platform:
        args.platform = platform.system()
        if args.platform == "Linux":
            args.platform = linux_platform_get()

    # Auto-detect the shell, if not explicitely provided. Defaults to SH if not
    # found. It is less crucial.
    if not args.shell:
        shell = os.environ.get("SHELL", Shell.SH)
        if shell:
            args.shell = os.path.basename(shell)

    # Invalid platform? Raise an error.
    if args.platform not in SETUP:
        print("{}{}Platform '{}' is not supported. Please open a ticket:"
              " https://github.com/jeanguyomarch/eovim/issues/new{}"
              .format(COLORS["red"], COLORS["bold"], args.platform,
                      COLORS["reset"]), file=sys.stderr)
        sys.exit(3)

    setup = SETUP[args.platform]

    if args.execute:
        callback = execute_cmd
    else:
        # Tell what commands shall be executed
        print("{}Run the following commands (or re-run this script with "
              "{}--execute{}{}):{}\n"
              .format(COLORS['green'], COLORS['bold'],
                      COLORS['reset'], COLORS['green'], COLORS['reset']))
        callback = print

    # Either run the commands or print them
    for cmd in setup_cmd_walk(setup):
        callback(cmd)

    # If the environment should be altered, tell that!
    if Key.ENV in setup:
        print("\n{}Make sure you have the following environment defined:{}\n"
              .format(COLORS['green'], COLORS['reset']))
        for env, value, cmd in setup_env_walk(setup, args.shell):
            print("'{}{}{}' must be set to: '{}{}{}' ({}{}{}{}) ".format(
                COLORS["green"], env, COLORS['reset'],
                COLORS['green'], value, COLORS['reset'],
                COLORS['green'], COLORS['bold'], cmd, COLORS['reset']))



if __name__ == "__main__":
    main(sys.argv)
