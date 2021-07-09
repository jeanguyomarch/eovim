# ChangeLog

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.2.0] - 2020-07-25

### Added

- Support for line space
- Support for ligatures
- Support for undercurl
- Support for underline with different colors than the foreground
- Linegrid and highlight groups support

### Removed

- Plugin architecture
- Eovimrc specific file
- Prefs widget: configuration is now through vimscript
- Config file: configuration is now through vimscript
- Ad-hoc msgpack scripts

### Changed

- Updated suport for wildmenu
- Command-line, popupmenu has been overhauled
- Better integration of eovim through vimscript
- Font can now be specified as a fontconfig format string
- Remove ad-hoc getopts, in favor of `Ecore_Getopt`
- The `--version` now displays a great level of details


### Fixed

- EFL 1.23 support
- Fix EFL detection when building eovim
- Honor non-blinking cursors


## [0.1.3] - 2019-01-04

### Added

- Source a dedicated vimscript file at startup
  (`$XDG_CONFIG_HOME/nvim/eovimrc.vim`).
- Add the `-M` (`--maximized`) command-line option.
- Caps lock auto-detection.

### Changed

- Neovim is not called with `--headless` anymore.
- Unimplemented APIs are notified as warnings, not as errors.
- EFL 1.19 or greater is now required.

### Fixed

- Resizing limits are not necessary anymore.
- Invalid memory accesses with EFL 1.20 and EFL.1.21.
- Spurious crashes because of the EFL backtrace.


## [0.1.2] - 2017-12-31

### Added

- Plug-Ins infrastructure to allow Neovim to manipule the Eovim GUI.
- Sizing plugin to make neovim alter the sizing of the Eovim's Window.
- Plugin to preview images directly from Eovim.
- Externalization of cmdline and wildmenu.
- Support for neovim setting the window title.
- Externalization of the tabline.

### Changed

- Backspace is now always passed as `<BS>` to Eovim.
- Space is now always passed as `<Space>` to Eovim.
- Remove `--hsplit`, `--vsplit` and `--tabs` from the CLI, as unknown options
  are now directly forwarded to neovim.

### Fixed

- Dragging the mouse does select characters instead of words.
- Initial sizing issues that may lead to ill-formed windows.
- Various runtime errors upon invalid msgpack parsing.


## [0.1.1] - 2017-10-26

### Added

- Round0 implementation of the completion popup.
- The eovim logo, inspired from neovim's.
- Options `-O`, `-o` and `-p` are supported.
- Eovim's background can be set directly through neovim.
- Font preview in the settings.
- Handling of the `guicursor` setting.
- Neovim's version detection.
- Configuration option to disable key flickering in the theme.
- Eovim defines `g:eovim_running` to 1, and makes it available to Neovim.
- The `-g` option allows to set the initial size of the eovim window.
- Preference option to enable/disable the completion externalization.
- Preference option to enable/disable the true colors mode.
- Composition with dead keys.

### Changed

- Settings are now in splitted views instead of a single one.
- Blinking in the theme is driven by Eovim, not by the theme itself.

### Fixed

- The _delete_ key is now correctly passed to neovim.
- Invalid scrolling in some cases.
- Resizing issues.

### Removed

- The unified background settings.
- The `--termcolors` (`-T`) option has been removed in favor of a configuration option.


## 0.1.0 - 2017-10-02

### Added

- Initial release of eovim.
- Initial implementation of event hooks.
- Initial theme (default).


[Unreleased]: https://github.com/jeanguyomarch/eovim/compare/v0.2.0...main
[0.1.1]: https://github.com/jeanguyomarch/eovim/compare/v0.1.0...v0.1.1
[0.1.2]: https://github.com/jeanguyomarch/eovim/compare/v0.1.1...v0.1.2
[0.1.3]: https://github.com/jeanguyomarch/eovim/compare/v0.1.2...v0.1.3
[0.2.0]: https://github.com/jeanguyomarch/eovim/compare/v0.1.3...v0.2.0
