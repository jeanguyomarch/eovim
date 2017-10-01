set number
set mouse=a
set bg=dark

filetype plugin indent on
syntax on
set showmatch

set smarttab
set shiftwidth=3
set expandtab
set backspace=indent,eol,start
set wildmode=longest,list,full
set wildmenu
set smartindent
set autoindent

set nocursorline
highlight LineNr ctermfg=grey ctermbg=none guifg=grey

set hlsearch

hi User1 ctermbg=124 ctermfg=white guibg=red guifg=white
hi User2 ctermbg=215 ctermfg=black guibg=orange guifg=black
set laststatus=2
au InsertEnter * hi StatusLine ctermfg=70 guifg=green
au InsertLeave * hi StatusLine ctermfg=189 guifg=gray
set statusline=%1*%m%*%r%h\ %2*%f%*\ [%{strlen(&fenc)?&fenc:'none'},%{&ff},%{strlen(&ft)?&ft:'none'}]%=%c,%l/%L

hi TabLineFill guifg=red guibg=gray
hi TabLine guifg=black guibg=gray
hi TabLineSel guifg=black guibg=o
