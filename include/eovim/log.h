/* This file is part of Eovim, which is under the MIT License ****************/

#ifndef __EOVIM_LOG_H__
#define __EOVIM_LOG_H__

extern int _eovim_log_domain;

#define DBG(...) EINA_LOG_DOM_DBG(_eovim_log_domain, __VA_ARGS__)
#define INF(...) EINA_LOG_DOM_INFO(_eovim_log_domain, __VA_ARGS__)
#define WRN(...) EINA_LOG_DOM_WARN(_eovim_log_domain, __VA_ARGS__)
#define ERR(...) EINA_LOG_DOM_ERR(_eovim_log_domain, __VA_ARGS__)
#define CRI(...) EINA_LOG_DOM_CRIT(_eovim_log_domain, __VA_ARGS__)

#endif /* ! __EOVIM_LOG_H__ */
