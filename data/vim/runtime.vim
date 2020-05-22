:function! Eovim(request, ...)
:   let l:file = "/dev/stdout"
:   if (a:0 == 0)
:      call writefile(msgpackdump([[2, "eovim", [[a:request]]]]), l:file, "ab")
:   else
:      call writefile(msgpackdump([[2, "eovim", [[a:request, a:000]]]]), l:file, "ab")
:   endif
:endfunction

:augroup Eovim
:   autocmd User EovimReady :
:   autocmd User EovimCapsLockOn :
:   autocmd User EovimCapsLockOff :
:augroup END
