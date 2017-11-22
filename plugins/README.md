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


## Image Viewer

The image viewer plugin allows Eovim to display images when Neovim requires it.
It accepts a single parameter, which is the absolute path to the image to be
displayed. The supported extensions will vary in function of the EFL installed
on the system, but the most mainstream formats can reliabily be read (jpg, png,
gif).

Once the image is show in Eovim, one must press a key to dismiss it, and grab
back control on Eovim.

The following example will make Eovim display an image when it is open in a
buffer (the buffer will silently be discarded):

```vim
function! EovimImageViewer ()
   call Eovim("imageviewer", expand("%:p"))
   bdelete
endfunction

:autocmd! BufEnter *.png,*.jpg,*gif  call EovimImageViewer()
```
