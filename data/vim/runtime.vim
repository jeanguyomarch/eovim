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

let g:eovim_theme_completion_styles = {
	\ 'default': 'font_weight=bold color=#ffffff',
	\ 'm': 'color=#ff00ff',
	\ 'v': 'color=#00ffff',
	\ 'f': 'color=#ffff00',
	\ 't': 'color=#0000ff',
	\ 'd': 'color=#0000ff',
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
