/* This file is part of Eovim, which is under the MIT License ****************/

#ifndef __EOVIM_TYPES_H__
#define __EOVIM_TYPES_H__

#include <Eina.h>
#include <msgpack.h>
#include <stdint.h>

typedef int64_t t_int;
typedef struct version s_version;
typedef struct request s_request;
typedef struct nvim s_nvim;
typedef struct config s_config;
typedef struct mode s_mode;
typedef struct position s_position;
typedef struct gui s_gui;
typedef struct prefs s_prefs;
typedef struct completion s_completion;
typedef struct geometry s_geometry;
typedef Eina_Bool (*f_event_cb)(s_nvim *nvim, const msgpack_object_array *args);
typedef void (*f_nvim_api_cb)(s_nvim *nvim, void *data, const msgpack_object *result);

typedef enum
{
   CURSOR_SHAPE_BLOCK           = 0,
   CURSOR_SHAPE_HORIZONTAL      = 1,
   CURSOR_SHAPE_VERTICAL        = 2,
   __CURSOR_SHAPE_LAST /* Sentinel */
} e_cursor_shape;

struct completion
{
   const char *word;
   const char *kind;
   const char *menu;
   const char *info;
   /* Buffer containing the memory pointed to by word, kind, menu, info */
   char mem[1];
};

struct geometry
{
   unsigned int w;
   unsigned int h;
};

struct version
{
   unsigned int major;
   unsigned int minor;
   unsigned int patch;
};

struct mode
{
   Eina_Stringshare *name; /**< Name of the mode */
   e_cursor_shape cursor_shape; /**< Shape of the cursor */
   unsigned int cell_percentage;
      /**< Percentage of the cell that the cursor occupies */
   unsigned int blinkon; /**< Delay during which the cursor is displayed */
   unsigned int blinkoff; /**< Delay during which the cursor is hidden */
   unsigned int blinkwait; /**< Delay for transitionning ON <-> OFF */
   unsigned int hl_id;
   unsigned int id_lm;
   char short_name[1];
     /**< Short name... I don't remember what this is... but it is a string
      * embedded at the end of this struct (not just a single char) */
};

#endif /* ! __EOVIM_TYPES_H__ */
