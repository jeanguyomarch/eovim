# ChangeLog

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Round0 implementation of the completion popup.
- The eovim logo, inspired from neovim's.
- Options `-O`, `-o` and `-p` are supported.
- Eovim's background can be set directly through neovim.
- Font preview in the settings.
- Handling of the `guicursor` setting.
- Neovim's version detection.

### Changed

- Settings are now in splitted views instead of a single one.
- Blinking in the theme is driven by Eovim, not by the theme itself.

### Fixed

- The _delete_ key is now correctly passed to neovim.
- Invalid scrolling in some cases.

### Removed

- The unified background settings.


## 0.1.0 - 2017-10-02

### Added

- Initial release of eovim.
- Initial implementation of event hooks.
- Initial theme (default).


[Unreleased]: https://github.com/jeanguyomarch/eovim/compare/v0.1.0...master
