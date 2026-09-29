/* Minimal modular dwm-style status bar: workspace indicators, the focused
 * window's title, tray icons, and a clock. Drawn with core Xlib text
 * (XDrawString) — no Xft or Pango, so non-Latin window titles (e.g.
 * Cyrillic) won't render correctly with the default core font; workspace
 * numbers and the clock are ASCII and always fine. Colors/font/height
 * come from `cfg` (src/appconf.c), reloadable at runtime via bar_reload().
 *
 * Modules are configured in ~/.config/zovwm/bar_modules.conf and can be
 * reordered by drag-and-drop. Workspace names are editable via double-click. */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "zovwm.h"
#include "barmodule.h"

static Window barwin;
static GC gc;
static XFontStruct *font;
static unsigned long col_bg, col_fg, col_cur, col_occupied, col_empty;

/* Module render functions table */
static BarModuleRenderFunc render_funcs[] = {
	[BAR_MODULE_WORKSPACES]   = barmodule_render_workspaces,
	[BAR_MODULE_LAYOUT]       = barmodule_render_layout,
	[BAR_MODULE_CLIENTTITLE]  = barmodule_render_clienttitle,
	[BAR_MODULE_CLOCK]        = barmodule_render_clock,
	[BAR_MODULE_KEYBOARD]     = barmodule_render_keyboard,
	[BAR_MODULE_CUSTOM]       = barmodule_render_custom,
};

/* Module width calculation functions table */
static BarModuleWidthFunc width_funcs[] = {
	[BAR_MODULE_WORKSPACES]   = barmodule_width_workspaces,
	[BAR_MODULE_LAYOUT]       = barmodule_width_layout,
	[BAR_MODULE_CLIENTTITLE]  = barmodule_width_clienttitle,
	[BAR_MODULE_CLOCK]        = barmodule_width_clock,
	[BAR_MODULE_KEYBOARD]     = barmodule_width_keyboard,
	[BAR_MODULE_CUSTOM]       = barmodule_width_custom,
};

/* Module click handler functions table */
static BarModuleClickFunc click_funcs[] = {
	[BAR_MODULE_WORKSPACES]   = barmodule_click_workspaces,
	[BAR_MODULE_LAYOUT]       = barmodule_click_layout,
	[BAR_MODULE_CLIENTTITLE]  = barmodule_click_clienttitle,
	[BAR_MODULE_CLOCK]        = barmodule_click_clock,
	[BAR_MODULE_KEYBOARD]     = barmodule_click_keyboard,
	[BAR_MODULE_CUSTOM]       = barmodule_click_custom,
};

/* Double-click tracking */
static struct {
	int x;
	time_t timestamp;
	int mod_index;
} last_click;

static unsigned long
getcolor(const char *name)
{
	XColor color;
	Colormap cmap = DefaultColormap(wm.dpy, wm.screen);
	if (!XAllocNamedColor(wm.dpy, cmap, name, &color, &color))
		return BlackPixel(wm.dpy, wm.screen);
	return color.pixel;
}

static int
workspace_occupied(int idx)
{
	for (Client *c = wm.clients; c; c = c->next)
		if (c->workspace == idx)
			return 1;
	return 0;
}

static void
loadstyle(void)
{
	if (font)
		XFreeFont(wm.dpy, font);
	font = XLoadQueryFont(wm.dpy, cfg.bar_font);
	if (!font)
		font = XLoadQueryFont(wm.dpy, "fixed");
	if (!font)
		die("zovwm: cannot load a core X font for the bar");

	col_bg       = getcolor(cfg.bar_color_bg);
	col_fg       = getcolor(cfg.bar_color_fg);
	col_cur      = getcolor(cfg.bar_color_cur);
	col_occupied = getcolor(cfg.bar_color_occupied);
	col_empty    = getcolor(cfg.bar_color_empty);
}

void
bar_init(void)
{
	XSetWindowAttributes wa;

	loadstyle();
	barconf_load();

	wa.override_redirect = True;
	wa.background_pixel = col_bg;
	wa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
	barwin = XCreateWindow(wm.dpy, wm.root, 0, 0, (unsigned int)wm.sw,
	                         (unsigned int)cfg.bar_height, 0,
	                         DefaultDepth(wm.dpy, wm.screen), CopyFromParent,
	                         DefaultVisual(wm.dpy, wm.screen),
	                         CWOverrideRedirect | CWBackPixel | CWEventMask, &wa);
	gc = XCreateGC(wm.dpy, barwin, 0, NULL);
	XSetFont(wm.dpy, gc, font->fid);
	XMapRaised(wm.dpy, barwin);
	bar_draw();
}

void
bar_reload(void)
{
	loadstyle();
	XSetFont(wm.dpy, gc, font->fid);
	XSetWindowBackground(wm.dpy, barwin, col_bg);
	XResizeWindow(wm.dpy, barwin, (unsigned int)wm.sw, (unsigned int)cfg.bar_height);
	bar_draw();
}

void
bar_cleanup(void)
{
	XFreeGC(wm.dpy, gc);
	if (font)
		XFreeFont(wm.dpy, font);
	XDestroyWindow(wm.dpy, barwin);
}

Window
bar_window(void)
{
	return barwin;
}

int
bar_right_reserved(void)
{
	return 0; /* No longer reserved — modules take up whatever space they need */
}

/* Module rendering implementations */

void
barmodule_render_workspaces(BarModule *mod, int x, int w)
{
	char label[8];
	int ty = (cfg.bar_height + font->ascent - font->descent) / 2;

	for (int i = 0; i < WSCOUNT; i++) {
		int segw = cfg.bar_height;
		unsigned long bg, fg;

		if (i == wm.curws) {
			bg = col_cur;
			fg = col_bg;
		} else if (workspace_occupied(i)) {
			bg = col_bg;
			fg = col_occupied;
		} else {
			bg = col_bg;
			fg = col_empty;
		}

		XSetForeground(wm.dpy, gc, bg);
		XFillRectangle(wm.dpy, barwin, gc, x, 0, (unsigned int)segw, (unsigned int)cfg.bar_height);

		const char *name = barmodule_get_workspace_name(i);
		XSetForeground(wm.dpy, gc, fg);
		int lw = XTextWidth(font, name, (int)strlen(name));
		XDrawString(wm.dpy, barwin, gc, x + (segw - lw) / 2, ty, name, (int)strlen(name));
		x += segw;
	}
	mod->w = x - mod->x;
}

void
barmodule_render_layout(BarModule *mod, int x, int w)
{
	int ty = (cfg.bar_height + font->ascent - font->descent) / 2;

	static const char *symbols[LAYOUT_COUNT] = {
		[LAYOUT_FULLSCREEN] = "[F]", [LAYOUT_MONOCLE] = "[M]",
		[LAYOUT_BSTACK] = "[B]", [LAYOUT_GRID] = "###",
	};
	const char *sym = symbols[wm.ws[wm.curws].layout];
	XSetForeground(wm.dpy, gc, col_fg);
	XDrawString(wm.dpy, barwin, gc, x, ty, sym, (int)strlen(sym));
	mod->w = XTextWidth(font, sym, (int)strlen(sym)) + 8;
}

void
barmodule_render_clienttitle(BarModule *mod, int x, int w)
{
	int ty = (cfg.bar_height + font->ascent - font->descent) / 2;

	if (wm.focused) {
		char *name = NULL;
		if (XFetchName(wm.dpy, wm.focused->win, &name) && name) {
			XSetForeground(wm.dpy, gc, col_fg);
			XDrawString(wm.dpy, barwin, gc, x, ty, name, (int)strlen(name));
			XFree(name);
			mod->w = XTextWidth(font, name, (int)strlen(name));
		} else {
			mod->w = 0;
		}
	} else {
		mod->w = 0;
	}
}

void
barmodule_render_clock(BarModule *mod, int x, int w)
{
	int ty = (cfg.bar_height + font->ascent - font->descent) / 2;
	char clockbuf[16];
	time_t t = time(NULL);
	struct tm *tmv = localtime(&t);
	strftime(clockbuf, sizeof clockbuf, "%H:%M:%S", tmv);
	int cw = XTextWidth(font, clockbuf, (int)strlen(clockbuf));
	XSetForeground(wm.dpy, gc, col_fg);
	XDrawString(wm.dpy, barwin, gc, x, ty, clockbuf, (int)strlen(clockbuf));
	mod->w = cw;
}

void
barmodule_render_keyboard(BarModule *mod, int x, int w)
{
	int ty = (cfg.bar_height + font->ascent - font->descent) / 2;
	char kblabel[8];
	kblayout_current(kblabel, sizeof kblabel);
	if (kblabel[0]) {
		int kw = XTextWidth(font, kblabel, (int)strlen(kblabel));
		XDrawString(wm.dpy, barwin, gc, x, ty, kblabel, (int)strlen(kblabel));
		mod->w = kw;
	} else {
		mod->w = 0;
	}
}

void
barmodule_render_custom(BarModule *mod, int x, int w)
{
	int ty = (cfg.bar_height + font->ascent - font->descent) / 2;
	const char *text = bar_cfg[wm.curws][BAR_MODULE_CUSTOM].custom_text;
	if (text[0]) {
		XSetForeground(wm.dpy, gc, col_fg);
		XDrawString(wm.dpy, barwin, gc, x, ty, text, (int)strlen(text));
		mod->w = XTextWidth(font, text, (int)strlen(text));
	} else {
		mod->w = 0;
	}
}

/* Module width calculations */

int
barmodule_width_workspaces(void)
{
	int total = 0;
	for (int i = 0; i < WSCOUNT; i++) {
		total += cfg.bar_height;
	}
	return total;
}

int
barmodule_width_layout(void)
{
	static const char *symbols[LAYOUT_COUNT] = {
		[LAYOUT_FULLSCREEN] = "[F]", [LAYOUT_MONOCLE] = "[M]",
		[LAYOUT_BSTACK] = "[B]", [LAYOUT_GRID] = "###",
	};
	const char *sym = symbols[wm.ws[wm.curws].layout];
	return XTextWidth(font, sym, (int)strlen(sym)) + 8;
}

int
barmodule_width_clienttitle(void)
{
	if (wm.focused) {
		char *name = NULL;
		if (XFetchName(wm.dpy, wm.focused->win, &name) && name) {
			int w = XTextWidth(font, name, (int)strlen(name));
			XFree(name);
			return w;
		}
	}
	return 0;
}

int
barmodule_width_clock(void)
{
	char clockbuf[16];
	time_t t = time(NULL);
	struct tm *tmv = localtime(&t);
	strftime(clockbuf, sizeof clockbuf, "%H:%M:%S", tmv);
	return XTextWidth(font, clockbuf, (int)strlen(clockbuf));
}

int
barmodule_width_keyboard(void)
{
	char kblabel[8];
	kblayout_current(kblabel, sizeof kblabel);
	if (kblabel[0]) {
		return XTextWidth(font, kblabel, (int)strlen(kblabel));
	}
	return 0;
}

int
barmodule_width_custom(void)
{
	const char *text = bar_cfg[wm.curws][BAR_MODULE_CUSTOM].custom_text;
	if (text[0]) {
		return XTextWidth(font, text, (int)strlen(text));
	}
	return 0;
}

/* Module click handlers */

void
barmodule_click_workspaces(int x, int w)
{
	(void)x; (void)w;
	/* Nothing to do on click — double-click triggers rename */
}

void
barmodule_click_layout(int x, int w)
{
	(void)x; (void)w;
	/* Cycle layout on click */
	Arg arg = {.i = 1};
	setlayout(&arg);
}

void
barmodule_click_clienttitle(int x, int w)
{
	(void)x; (void)w;
	/* Nothing to do */
}

void
barmodule_click_clock(int x, int w)
{
	(void)x; (void)w;
	/* Nothing to do */
}

void
barmodule_click_keyboard(int x, int w)
{
	(void)x; (void)w;
	Arg arg = {.i = 0};
	kblayout_next(&arg);
}

void
barmodule_click_custom(int x, int w)
{
	(void)x; (void)w;
	/* Nothing to do */
}

/* Draw the entire bar with modules */
void
bar_draw(void)
{
	char label[8], clockbuf[16];
	int x = 0;

	XSetForeground(wm.dpy, gc, col_bg);
	XFillRectangle(wm.dpy, barwin, gc, 0, 0, (unsigned int)wm.sw, (unsigned int)cfg.bar_height);

	/* Render each configured module */
	for (int i = 0; i < bar_module_count; i++) {
		BarModuleType type = (BarModuleType)bar_module_order[i];
		if (type >= BAR_MODULE_COUNT)
			continue;

		BarModule *mod = &bar_modules[0][i];
		mod->x = x;
		mod->type = type;
		mod->is_dragging = 0;

		/* Calculate width */
		if (width_funcs[type]) {
			mod->w = width_funcs[type]();
		} else {
			mod->w = cfg.bar_height;
		}

		/* Clamp width to available space */
		if (x + mod->w > wm.sw) {
			mod->w = wm.sw - x;
		}

		/* Render the module */
		if (render_funcs[type]) {
			render_funcs[type](mod, x, mod->w);
		}

		x += mod->w;

		/* Add spacing between modules (except the last one) */
		if (i < bar_module_count - 1 && x < wm.sw) {
			x += 4;
		}
	}

	XFlush(wm.dpy);
}
