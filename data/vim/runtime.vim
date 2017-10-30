:function! Eovim(plugin, ...)
:   let l:file = "/dev/stdout"
:   if (a:0 == 0)
:      call writefile(msgpackdump([[2, "eovim", [[a:plugin]]]]), l:file, "ab")
:   else
:      call writefile(msgpackdump([[2, "eovim", [[a:plugin, a:000]]]]), l:file, "ab")
:   endif
:endfunction
