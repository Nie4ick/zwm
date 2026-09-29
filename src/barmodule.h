/* barmodule.h — module type definitions and structures for the new
 * modular bar.  Supports drag-and-drop reordering, click-to-edit
 * workspace names, and a pluggable set of modules (clock, keyboard
 * layout, workspace list, custom text, etc.). */

#ifndef BARMODULE_H
#define BARMODULE_H

#include "zovwm.h"

/* Module types */
typedef enum {
	BAR_MODULE_WORKSPACES,   /* clickable workspace list (renameable) */
	BAR_MODULE_LAYOUT,       /* current layout symbol */
	BAR_MODULE_CLIENTTITLE,  /* focused window title */
	BAR_MODULE_CLOCK,        /* HH:MM:SS */
	BAR_MODULE_KEYBOARD,     /* current keyboard layout code */
	BAR_MODULE_CUSTOM,       /* user-defined text */
	BAR_MODULE_COUNT
} BarModuleType;

/* Module configuration entry (from bar_modules.conf) */
typedef struct {
	BarModuleType type;
	char custom_text[128];    /* used only for BAR_MODULE_CUSTOM */
	char workspace_name[64];  /* user-defined name for each workspace (0..WSCOUNT-1) */
	int  visible;             /* 1 = show, 0 = hide */
} BarModuleConfig;

/* Runtime module state */
typedef struct {
	int x;                  /* current x position on the bar */
	int w;                  /* width in pixels */
	BarModuleType type;
	int is_dragging;        /* 1 while user is dragging this module */
	int drag_start_x;       /* x position where drag started (for reordering) */
	int drag_start_index;   /* original index when drag started */
} BarModule;

/* Module render function signature */
typedef void (*BarModuleRenderFunc)(BarModule *mod, int x, int w);

/* Module width calculation */
typedef int (*BarModuleWidthFunc)(void);

/* Module click handler */
typedef void (*BarModuleClickFunc)(int x, int w);

/* Module reorder callback (called when user drops a module) */
typedef void (*BarModuleReorderFunc)(int old_index, int new_index);

/* Drag state */
typedef struct {
	int active;             /* 1 if a drag is in progress */
	int mod_index;          /* index of the module being dragged */
	int start_x;            /* x position when drag started */
	int start_index;        /* original module index */
	int move_threshold;     /* pixels to move before reorder is triggered */
} DragState;

/* Global bar module state */
extern BarModuleConfig bar_cfg[WSCOUNT][BAR_MODULE_COUNT];
extern BarModule bar_modules[WSCOUNT][BAR_MODULE_COUNT];
extern int bar_module_order[BAR_MODULE_COUNT];
extern int bar_module_count;
extern DragState bar_drag;

/* Configuration loading */
int barconf_load(void);
void barconf_save(void);

/* Module registration (called at startup) */
void barmodule_register(void);

/* Module rendering */
void barmodule_render_workspaces(BarModule *mod, int x, int w);
void barmodule_render_layout(BarModule *mod, int x, int w);
void barmodule_render_clienttitle(BarModule *mod, int x, int w);
void barmodule_render_clock(BarModule *mod, int x, int w);
void barmodule_render_keyboard(BarModule *mod, int x, int w);
void barmodule_render_custom(BarModule *mod, int x, int w);

/* Module width calculations */
int barmodule_width_workspaces(void);
int barmodule_width_layout(void);
int barmodule_width_clienttitle(void);
int barmodule_width_clock(void);
int barmodule_width_keyboard(void);
int barmodule_width_custom(void);

/* Module click handlers */
void barmodule_click_workspaces(int x, int w);
void barmodule_click_layout(int x, int w);
void barmodule_click_clienttitle(int x, int w);
void barmodule_click_clock(int x, int w);
void barmodule_click_keyboard(int x, int w);
void barmodule_click_custom(int x, int w);

/* Workspace name management */
void barmodule_set_workspace_name(int ws_idx, const char *name);
const char* barmodule_get_workspace_name(int ws_idx);

#endif /* BARMODULE_H */
