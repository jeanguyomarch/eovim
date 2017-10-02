# Eovim

Eovim is the Enlightened Neovim. That's just an [EFL][1] GUI client for
[Neovim][2].

## Status

[![Build Status](https://travis-ci.org/jeanguyomarch/eovim.svg?branch=master)](https://travis-ci.org/jeanguyomarch/eovim)

Eovim is still at an alpha stage. However, it should be possible to use it for
developping daily, as most of the Neovim interfaces have been implemented.

[![Screenshot](https://phab.enlightenment.org/file/data/jxgenow5sduf4bjlzasd/PHID-FILE-lf7f5hqmdfehkxxize5c/eovim.png)](https://phab.enlightenment.org/w/projects/eovim/)

## Installation

Eovim requires the following components to be installed on your system before
you can start hacking:

- [EFL][1]: this framework of libraries is packaged in most of the GNU/Linux
  distributions and on macOS.
- [msgpack-c][3]: this serialization library is not widely packaged, but is
  mandatory to communicate with Neovim. You are advised to run the script
  `scripts/get-msgpack.sh` to install msgpack. This will retrieve and compile
  a static version of msgpack that `eovim` can work with.
- [Neovim][2] version 0.2.1 or greater (earlier versions have not been tested),
- [CMake][5].

After making sure you have installed the dependencies aforementioned, run the
following installation procedure:

```bash
./scripts/get-msgpack.sh # Optional, but advised.
mkdir -p build && cd build
cmake -G "Unix Makefiles" -DCMAKE_TYPE_BUILD=Release ..
cmake --build .
cmake --build . --target install # Possibly as root (i.e. via sudo)
```

## Usage

```bash
eovim [options] [files...]
```

For now, `eovim` can be run without any argument. This is equivalent to run
Neovim without any file. You can pass files as arguments to `eovim`, they will
be opened at startup the same way Neovim does.

When `eovim` starts, it spawns an instance of Neovim. If it happens that `nvim`
is not in your `PATH` or if you want to use an alterate binary of Neovim, you
can feed it to `eovim` with the option `--nvim`.

By default, `eovim` will connect to Neovim by requesting true colors (24-bits
colordepth), which could be problematic for configurations that rely on
terminal colors (256 colors). To enable the 256-colors mode, run `eovim` with
the `--termcolors` (or `-T`) option.

To get a list of all the available options, run `eovim --help`.


## Hacking

Eovim uses some environment variables that can influence its runtime. Some are
directly inherited from the [EFL][1] framework, others are eovim-specific:
- `EINA_LOG_BACKTRACE` set it to an integer to get run-time backtraces.
- `EINA_LOG_LEVELS` set it to "eovim:INT" where _INT_ is the log level.
- `EOVIM_IN_TREE` set it to non-zero to load files from the build directory
   instead of the installation directory.

To develop/debug, a typical use is to run `eovim` like this (from the build
directory):

```bash
env EOVIM_IN_TREE=1 EINA_LOG_BACKTRACE=0 EINA_LOG_LEVELS="eovim:3" ./eovim
```

## Tests

Tests are disabled by default, but can be enabled by passing `-DWITH_TESTS=ON`.
Beware, tests require additional setup and dependencies:

- run `scripts/get-test-data.sh` from the top source directory,
- make sure you have [Exactness][6] installed,
- after building, you should *install* eovim as well.

Running `make test` will run the test suite.  Details about how the tests work
are explained in `tests/README.md`.


# License

Eovim is MIT-licensed. See the `LICENSE` file for details. Files in
`data/themes/img` have been taken from [terminology][4] or the [EFL][1] and are
not original creations.

[1]: https://www.enlightenment.org
[2]: https://neovim.io
[3]: https://github.com/msgpack/msgpack-c
[4]: https://www.enlightenment.org/about-terminology
[5]: https://cmake.org/
[6]: https://git.enlightenment.org/tools/exactness.git/
