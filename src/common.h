/*
 * Public domain
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef _WIN32
# include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
//#include <stdbool.h>
#include <unistd.h>

//#include "w_gtk.h"
#include "gtkcompat.h"

#ifdef ENABLE_NLS
// #include <glib/gi18n.h>
#  include <libintl.h>
#  define _(str) gettext(str)
#  define N_(str) (str)
#else
#  define _(str) (str)
#  define gettext(str) (str)
#  define N_(str) (str)
#  define textdomain(str) (str)
#  define GETTEXT_PACKAGE NULL
#  define dgettext(domain,str) (str)
#  define dcgettext(domain,str,type) (str)
#  define bindtextdomain(domain,directory) (domain)
#  define Q_(str) g_strip_context(str)
#  define ngettext(strS,strP,Number) ((Number==1)?strS:strP)
//#  define setlocale(a,b)
//#  define LC_ALL 
#endif


#ifdef __cplusplus
}
#endif

#endif /* __COMMON_H__ */

