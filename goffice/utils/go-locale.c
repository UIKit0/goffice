/*
 * go-locale.c :
 *
 * Copyright (C) 1998 Chris Lahey, Miguel de Icaza
 * Copyright (C) 2003-2005 Jody Goldberg (jody@gnome.org)
 * Copyright (C) 2005-2007 Morten Welinder (terra@gnome.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#include <goffice/goffice-config.h>
#include "go-locale.h"
#include <glib/gi18n-lib.h>
#ifdef HAVE_LANGINFO_H
#  include <langinfo.h>
#endif
#ifdef G_OS_WIN32
#  include <windows.h>
#endif

/*
 * Points to the locale information for number display.  All strings are
 * in UTF-8 encoding.
 */
static gboolean locale_info_cached = FALSE;
static GString *lc_decimal = NULL;
static GString *lc_thousand = NULL;
static gboolean lc_precedes;
static gboolean lc_space_sep;
static GString *lc_currency = NULL;

static gboolean date_format_cached = FALSE;
static GString *lc_date_format = NULL;

static gboolean date_order_cached = FALSE;

static gboolean boolean_cached = FALSE;
static char const *lc_TRUE = NULL;
static char const *lc_FALSE = NULL;

char const *
go_setlocale (int category, char const *val)
{
	locale_info_cached = FALSE;
	date_format_cached = FALSE;
	date_order_cached = FALSE;
	boolean_cached = FALSE;
	return setlocale (category, val);
}

static void
convert1 (GString *res, char const *lstr, char const *name, char const *def)
{
	char *tmp;

	if (lstr == NULL || lstr[0] == 0) {
		g_string_assign (res, def);
		return;
	}

	tmp = g_locale_to_utf8 (lstr, -1, NULL, NULL, NULL);
	if (tmp) {
		g_string_assign (res, tmp);
		g_free (tmp);
		return;
	}

	g_warning ("Failed to convert locale's %s \"%s\" to UTF-8.", name, lstr);
	g_string_assign (res, def);
}

static void
update_lc (void)
{
	struct lconv *lc = localeconv ();

	if (!lc_decimal)
		lc_decimal = g_string_new (NULL);

	if (!lc_thousand)
		lc_thousand = g_string_new (NULL);

	if (!lc_currency)
		lc_currency = g_string_new (NULL);

	/*
	 * Extract all information here as lc is not guaranteed to stay
	 * valid after next localeconv call which could be anywhere.
	 */

	convert1 (lc_decimal, lc->decimal_point, "decimal separator", ".");
	if (g_utf8_strlen (lc_decimal->str, -1) != 1)
		g_warning ("Decimal separator is not a single character.");

	convert1 (lc_thousand, lc->mon_thousands_sep, "monetary thousands separator",
		  (lc_decimal->str[0] == ',' ? "." : ","));
	if (g_utf8_strlen (lc_thousand->str, -1) != 1)
		g_warning ("Monetary thousands separator is not a single character.");

	if (g_string_equal (lc_thousand, lc_decimal)) {
		g_string_assign (lc_thousand,
				 (lc_decimal->str[0] == ',') ? "." : ",");
		g_warning ("Monetary thousands separator is the same as the decimal separator; converting '%s' to '%s'",
			   lc_decimal->str, lc_thousand->str);
	}

	/* Use != 0 rather than == 1 so that CHAR_MAX (undefined) is true */
	lc_precedes = (lc->p_cs_precedes != 0);

	/* Use == 1 rather than != 0 so that CHAR_MAX (undefined) is false */
	lc_space_sep = (lc->p_sep_by_space == 1);

	convert1 (lc_currency, lc->currency_symbol, "currency symbol",	"$");

	locale_info_cached = TRUE;
}

GString const *
go_locale_get_decimal (void)
{
	if (!locale_info_cached)
		update_lc ();

	return lc_decimal;
}

GString const *
go_locale_get_thousand (void)
{
	if (!locale_info_cached)
		update_lc ();

	return lc_thousand;
}

/**
 * go_locale_get_currency :
 * @precedes : a pointer to a boolean which is set to TRUE if the currency
 * 		should precede
 * @space_sep: a pointer to a boolean which is set to TRUE if the currency
 * 		should have a space separating it from the the value
 *
 * Play with the default logic so that things come out nicely for the default
 * case.
 */
GString const *
go_locale_get_currency (gboolean *precedes, gboolean *space_sep)
{
	if (!locale_info_cached)
		update_lc ();

	if (precedes)
		*precedes = lc_precedes;

	if (space_sep)
		*space_sep = lc_space_sep;

	return lc_currency;
}

GString const *
go_locale_get_date_format (void)
{
	if (!date_format_cached) {
		if (lc_date_format)
			g_string_truncate (lc_date_format, 0);
		else
			lc_date_format = g_string_new (NULL);

#ifdef HAVE_LANGINFO_H
		{
			char const *fmt = nl_langinfo (D_FMT);
			while (*fmt) {
				switch (*fmt) {
				case 'a': g_string_append (lc_date_format, "ddd"); break;
				case 'A': g_string_append (lc_date_format, "dddd"); break;
				case 'b': g_string_append (lc_date_format, "mmm"); break;
				case 'B': g_string_append (lc_date_format, "mmmm"); break;
				case 'd': g_string_append (lc_date_format, "dd"); break;
				case 'D': g_string_append (lc_date_format, "mm/dd/yy"); break;
				case 'e': g_string_append (lc_date_format, "d"); break; /* Approx */
				case 'F': g_string_append (lc_date_format, "yyyy-mm-dd"); break;
				case 'h': g_string_append (lc_date_format, "mmm"); break;
				case 'm': g_string_append (lc_date_format, "mm"); break;
				case 't': g_string_append (lc_date_format, "\t"); break;
				case 'y': g_string_append (lc_date_format, "yy"); break;
				case 'Y': g_string_append (lc_date_format, "yyyy"); break;
				case '%':
					/*
					 * Docs say we get things in strftime format,
					 * but I don't seem to get the '%'s. Hence we
					 * ignore '%'s.
					 */
					break;
				default:
					if (g_ascii_isalpha (*fmt))
						g_warning ("Unhandled locale date code '%c'", *fmt);
					else
						g_string_append_c (lc_date_format, *fmt);
				}
				fmt++;
			}
		}
#endif

		/* Sanity check */
		if (!g_utf8_validate (lc_date_format->str, -1, NULL)) {
			g_warning ("Produced non-UTF-8 date format.  Please report.");
			g_string_truncate (lc_date_format, 0);
		}

		/* Default */
		if (lc_date_format->len == 0) {
			static gboolean warning = TRUE;
			g_string_append (lc_date_format, "dddd, mmmm dd, yyyy");
			if (warning) {
				g_warning ("Using default system date format: %s",
					   lc_date_format->str);
				warning = FALSE;
			}
		}

		date_format_cached = TRUE;
	}
	return lc_date_format;
}

/*
 * go_locale_month_before_day :
 *
 * A quick utility routine to guess whether the default date format
 * uses day/month or month/day
 */
gboolean
go_locale_month_before_day (void)
{
#ifdef HAVE_LANGINFO_H
	static gboolean month_first = TRUE;

	if (!date_order_cached) {
		char const *ptr = nl_langinfo (D_FMT);

		date_order_cached = TRUE;
		month_first = TRUE;
		if (ptr)
			while (*ptr) {
				char c = *ptr++;
				if (c == 'd' || c == 'D') {
					month_first = FALSE;
					break;
				} else if (c == 'm')
					break;
			}
	}

	return month_first;
#elif defined(G_OS_WIN32)
	TCHAR str[2];

	GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_IDATE, str, 2);

	return str[0] != L'1';
#else
	static gboolean warning = TRUE;
	if (warning) {
		g_warning ("Incomplete locale library, dates will be month day year");
		warning = FALSE;
	}
	return TRUE;
#endif
}

/* Use comma as the arg separator unless the decimal point is a
 * comma, in which case use a semi-colon
 */
char
go_locale_get_arg_sep (void)
{
	if (go_locale_get_decimal ()->str[0] == ',')
		return ';';
	return ',';
}

char
go_locale_get_col_sep (void)
{
	if (go_locale_get_decimal ()->str[0] == ',')
		return '\\';
	return ',';
}

char
go_locale_get_row_sep (void)
{
	return ';';
}

char const *
go_locale_boolean_name (gboolean b)
{
	if (!boolean_cached) {
		lc_TRUE = _("TRUE");
		lc_FALSE = _("FALSE");
		boolean_cached = TRUE;
	}
	return b ? lc_TRUE : lc_FALSE;
}

/**
 * go_locale_untranslated_booleans :
 *
 * Short circuit the current locale so that we can import files
 * and still produce error messages in the current LC_MESSAGE
 **/
void
go_locale_untranslated_booleans (void)
{
	lc_TRUE = "TRUE";
	lc_FALSE = "FALSE";
	boolean_cached = TRUE;
}