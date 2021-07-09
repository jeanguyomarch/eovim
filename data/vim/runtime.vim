" This file is part of Eovim, which is under the MIT License

let g:eovim_running = 1

function! Eovim(request, ...)
   let l:file = '/dev/stdout'
   if (a:0 == 0)
      call writefile(msgpackdump([[2, 'eovim', [[a:request]]]]), l:file, 'ab')
   else
      call writefile(msgpackdump([[2, 'eovim', [[a:request, a:000]]]]), l:file, 'ab')
   endif
endfunction

let g:eovim_theme_bell_enabled = 0
let g:eovim_theme_react_to_key_presses = 1
let g:eovim_theme_react_to_caps_lock = 1
let g:eovim_cursor_cuts_ligatures = 1

let g:eovim_cursor_animated = 1
let g:eovim_cursor_animation_duration = 0.05
let g:eovim_cursor_animation_style = 'accelerate'


let g:eovim_theme_completion_styles = {
	\ 'default': 'font_weight=bold color=#ffffff',
	\ 'm': 'color=#ff00ff',
	\ 'v': 'color=#00ffff',
	\ 'f': 'color=#ffff00',
	\ 't': 'color=#0000ff',
	\ 'd': 'color=#0000ff',
	\}


highlight EovimCmdlineDefault gui=bold guifg=#a7a7ff guibg=#00007f
highlight EovimCmdlineSearch gui=bold guifg=#ffffa7 guibg=#7f7f00
highlight EovimCmdlineReverseSearch gui=bold guifg=#ffcca7 guibg=#7f3500
highlight EovimCmdlineCommand gui=bold guifg=#ffa7d9 guibg=#7f0048

let g:eovim_theme_cmdline_styles = {
	\ 'default': 'EovimCmdlineDefault',
	\ '/': 'EovimCmdlineSearch',
	\ '?': 'EovimCmdlineReverseSearch',
	\ ':': 'EovimCmdlineCommand',
	\}


let g:eovim_ext_tabline = 1
let g:eovim_ext_popupmenu = 1
let g:eovim_ext_cmdline = 1
let g:eovim_ext_multigrid = 0

augroup Eovim
   autocmd User EovimReady :
   autocmd User EovimCapsLockOn :
   autocmd User EovimCapsLockOff :
augroup END
