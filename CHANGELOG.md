# ChangeLog

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Plug-Ins infrastructure to allow Neovim to manipule the Eovim GUI.
- Sizing plugin to make neovim alter the sizing of the Eovim's Window.
- Plugin to preview images directly from Eovim.
- Externalization of cmdline and wildmenu.
- Support for neovim setting the window title.

### Changed

- Backspace is now always passed as `<BS>` to Eovim.
- Remove --hsplit, --vsplit and --tabs from the CLI, as unknown options are now
  directly forwarded to neovim.

### Fixed

- Dragging the mouse does select characters instead of words.
- Initial sizing issues that may lead to ill-formed windows.


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


[Unreleased]: https://github.com/jeanguyomarch/eovim/compare/v0.1.1...master
[0.1.1]: https://github.com/jeanguyomarch/eovim/compare/v0.1.0...v0.1.1
