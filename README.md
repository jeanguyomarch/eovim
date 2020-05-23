[![Neovim](data/images/eovim_banner.png)](https://phab.enlightenment.org/w/projects/eovim/)


Eovim is the Enlightened Neovim. That's just an [EFL][1] GUI client for
[Neovim][2].

# Status

[![Eovim CI](https://github.com/jeanguyomarch/eovim/workflows/Eovim%20CI/badge.svg)](https://github.com/jeanguyomarch/eovim/actions)
[![Coverity Scan Build](https://scan.coverity.com/projects/13836/badge.svg)](https://scan.coverity.com/projects/13836)

Eovim is still in development, but it is stable enough to be used for your daily programming.

# Screenshots

[![Screenshot](https://phab.enlightenment.org/file/data/4dnksyycnc2otmptswgy/PHID-FILE-jcyfful2wqtxzm3ednvb/Screenshot_from_2017-12-31_12-01-28.png)][9]
[![Screenshot](https://phab.enlightenment.org/file/data/mggrhqzd6dl3t3nbmswa/PHID-FILE-mvi65ftut5dbutuowvws/Screenshot_from_2017-12-27_22-59-42.png)][9]

Have a look at [the Enlightenment Wiki][8] for more.


# Why Eovim?

Eovim is written in plain C, with the amazing [EFL][1]. You have great added
value to the text-only neovim with a minimal runtime overhead. No need to spawn
a web browser to use it! If you don't like the externalized UI, it can be
turned off, or changed via themes.

Have a problem/question/suggestion? Feel free to [open an issue][10]. Join the club!

# Installation

Eovim requires the following components to be installed on your system before
you can start hacking around. See [the Wiki][6] for details.

- [EFL][1]: this framework of libraries is packaged in most of the GNU/Linux
  distributions and on macOS. Do not forget to install the efl-devel package
  which provides Eina among others.
- [msgpack-c][3]: this serialization library is not widely packaged, but is
  mandatory to communicate with Neovim.
- [Neovim][2] version 0.2.0 or greater (earlier versions have not been tested),
- [CMake][5].


After making sure you have installed the dependencies aforementioned, run the
following installation procedure:

```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
cmake --build . --target install # Possibly as root (i.e. via sudo)
```

If we want to run `eovim` without installing it, please refer to the
Wiki page [Developing Eovim][11].


# Usage

```bash
eovim [options] [files...]
```

Eovim command-line usage is exactly the same than what Vim or Neovim
provides.  You can run `eovim --help` or `man eovim` to get more help about how
to use its command-line form. It basically adds options on top the ones
provided by Neovim. If a command is not understood by Eovim itself, it will be
passed to Neovim.

The man page will give you greater details, and especially will give
information about the Vim Runtime modifications that are operated by Eovim.

When `eovim` starts, it spawns an instance of Neovim. If it happens that `nvim`
is not in your `PATH` or if you want to use an alterate binary of Neovim, you
can feed it to `eovim` with the option `--nvim`.


# License

Eovim is MIT-licensed. See the [`LICENSE`](License) file for details. Files in
[`data/themes/img`](data/themes/img) have been taken from [terminology][4] or
the [EFL][1] and are not original creations.
Portions of the Eovim logo have been [borrowed][7] from the original Neovim
logo. Eovim's logo should be understood as a tribute to Neovim.

[1]: https://www.enlightenment.org
[2]: https://neovim.io
[3]: https://github.com/msgpack/msgpack-c
[4]: https://www.enlightenment.org/about-terminology
[5]: https://cmake.org/
[6]: https://github.com/jeanguyomarch/eovim/wiki
[7]: https://raw.githubusercontent.com/neovim/neovim.github.io/master/logos/neovim-logo-600x173.png
[8]: https://phab.enlightenment.org/w/projects/eovim/#screenshots
[9]: https://phab.enlightenment.org/w/projects/eovim/
[10]: https://github.com/jeanguyomarch/eovim/issues/new
[11]: https://github.com/jeanguyomarch/eovim/wiki/Developing-Eovim
