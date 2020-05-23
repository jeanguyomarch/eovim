/* This file is part of Eovim, which is under the MIT License ****************/

#include <eovim/nvim_completion.h>

s_completion *
nvim_completion_new(const char *word, size_t word_size,
                    const char *kind, size_t kind_size,
                    const char *menu, size_t menu_size,
                    const char *info, size_t info_size)
{
   /*
    * word_ptr + kind_ptr + menu_ptr + info_ptr +
    * [word + \0] + [kind + \0] + [menu + \0] + [info] + \0]
    */
   const size_t buf_size =
     sizeof(s_completion) + word_size + kind_size + menu_size + info_size + 3u;

   s_completion *const compl = malloc(buf_size);
   char *buf = compl->mem;

   compl->word = buf;
   memcpy(buf, word, word_size);
   buf[word_size] = '\0';
   buf += (word_size + 2u);

   compl->kind = buf;
   memcpy(buf, kind, kind_size);
   buf[kind_size] = '\0';
   buf += (kind_size + 2u);

   compl->menu = buf;
   memcpy(buf, menu, menu_size);
   buf[menu_size] = '\0';
   buf += (menu_size + 2u);

   compl->info = buf;
   memcpy(buf, info, info_size);
   buf[info_size] = '\0';

   return compl;
}

void
nvim_completion_free(s_completion *compl)
{
   free(compl);
}
