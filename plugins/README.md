# Eovim Plug-Ins

This directory contains plug-ins that are shipped with Eovim. They must be
manually loaded once from the Eovim preferences panel to be effective. For a
plugin to be detected by Eovim at startup, it must be placed into special
directories that Eovim will scan once:

- the build directory of the plug-ins, only if `EOVIM_IN_TREE` is set (for
  debug);
- the user directory: `$HOME/.local/lib32/eovim` for 32-bits architectures and
  `$HOME/.local/lib64/eovim` for 64-bits architectures;
- the system directory: `<prefix>/lib<arch>/eovim`. Built-in plugins will
  typically be installed there.


Eovim plugins mainly allow to respond to events from Neovim. Eovim makes a
simple API available to Neovim to call its plugins:

```vim
:call Eovim("<plugin>", <parameters>)
```

Where:

- `<plugin>` is the name of the plugin to be queried;
- `<parameters>` are the optional parameters that the plugin accepts.

The following sections describe the effect and use of the built-in plugins.


## Sizing

The sizing plugin allows to modify the sizing of the Eovim window from Neovim.
It accepts a single parameter as a map that associates one or several keys to
values. They are evaluated sequentially, as such, the last keys will have
precedence over the first keys. It is recommanded to use only one key-value
pair.

Currently, the sizing plug-in accepts only one key, which is `"aspect"`. It
accepts the following values:

- `"fullscreen_on"`: to make Eovim a fullscreen window,
- `"fullscreen_off"`: to remove Eovim from a fullscreen state,
- `"fullscreen_toggle"`: to toggle the fullscreen state of the Eovim window.

Examples:

```vim
:call Eovim("sizing", {'aspect': 'fullscreen_on'})      " Set Eovim in fullscreen
:call Eovim("sizing", {'aspect': 'fullscreen_off'})     " Disable fullscreen
:call Eovim("sizing", {'aspect': 'fullscreen_toggle'})  " Toggle fullscreen state

" Make the F11 key toggle fullscreen
:nnoremap <F11> :call Eovim("sizing", {'aspect': 'fullscreen_toggle'})<CR>
```
