/* barconf.c — configuration parser for ~/.config/zovwm/bar_modules.conf.
 *
 * Format: one "key value" per line.
 *   module_order <type:pos,type:pos,...>   (type:position, positions: left/center/right)
 *   workspace_name <index> <name>               (user-defined workspace name)
 *   custom_text <index> <text>                  (custom text for BAR_MODULE_CUSTOM)
 *
 * Module types: workspaces, layout, clienttitle, clock, keyboard, custom
 * Positions: left, center, right
 * Default order: workspaces:left, layout:left, clienttitle:center, clock:right, keyboard:right
 * Default workspace names: "1", "2", ..., "9" */

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "zovwm.h"
#include "barmodule.h"

BarModuleConfig bar_cfg[WSCOUNT][BAR_MODULE_COUNT];
BarModule bar_modules[WSCOUNT][BAR_MODULE_COUNT];
int bar_module_order[BAR_MODULE_COUNT];
int bar_module_count = BAR_MODULE_COUNT;
DragState bar_drag;

static const char *modulename(BarModuleType type) {
	switch (type) {
	case BAR_MODULE_WORKSPACES:   return "workspaces";
	case BAR_MODULE_LAYOUT:       return "layout";
	case BAR_MODULE_CLIENTTITLE:  return "clienttitle";
	case BAR_MODULE_CLOCK:        return "clock";
	case BAR_MODULE_KEYBOARD:     return "keyboard";
	case BAR_MODULE_CUSTOM:       return "custom";
	case BAR_MODULE_COUNT:        return NULL;
	}
	return NULL;
}

static const char *positionname(BarPosition pos) {
	switch (pos) {
	case BAR_POS_LEFT:   return "left";
	case BAR_POS_CENTER: return "center";
	case BAR_POS_RIGHT:  return "right";
	case BAR_POS_COUNT:  return NULL;
	}
	return NULL;
}

static BarPosition
parseposition(const char *s) {
	if (strcmp(s, "left") == 0)   return BAR_POS_LEFT;
	if (strcmp(s, "center") == 0) return BAR_POS_CENTER;
	if (strcmp(s, "right") == 0)  return BAR_POS_RIGHT;
	return BAR_POS_LEFT;
}

static BarModuleType
parsesimplemodule(const char *s) {
	if (strcmp(s, "workspaces") == 0)   return BAR_MODULE_WORKSPACES;
	if (strcmp(s, "layout") == 0)       return BAR_MODULE_LAYOUT;
	if (strcmp(s, "clienttitle") == 0)  return BAR_MODULE_CLIENTTITLE;
	if (strcmp(s, "clock") == 0)        return BAR_MODULE_CLOCK;
	if (strcmp(s, "keyboard") == 0)     return BAR_MODULE_KEYBOARD;
	if (strcmp(s, "custom") == 0)       return BAR_MODULE_CUSTOM;
	return BAR_MODULE_COUNT;
}

static char *
configpath(void) {
	static char path[512];
	const char *home = getenv("HOME");
	snprintf(path, sizeof path, "%s/.config/zovwm/bar_modules.conf", home ? home : "/tmp");
	return path;
}

static void
setdefault_config(void) {
	/* Default module order with positions:
	 * left: workspaces, layout
	 * center: clienttitle
	 * right: clock, keyboard */
	bar_module_order[0] = BAR_MODULE_WORKSPACES;
	bar_module_order[1] = BAR_MODULE_LAYOUT;
	bar_module_order[2] = BAR_MODULE_CLIENTTITLE;
	bar_module_order[3] = BAR_MODULE_CLOCK;
	bar_module_order[4] = BAR_MODULE_KEYBOARD;
	bar_module_count = 5;

	/* Assign positions to each module type */
	bar_cfg[0][BAR_MODULE_WORKSPACES].position = BAR_POS_LEFT;
	bar_cfg[0][BAR_MODULE_LAYOUT].position = BAR_POS_LEFT;
	bar_cfg[0][BAR_MODULE_CLIENTTITLE].position = BAR_POS_CENTER;
	bar_cfg[0][BAR_MODULE_CLOCK].position = BAR_POS_RIGHT;
	bar_cfg[0][BAR_MODULE_KEYBOARD].position = BAR_POS_RIGHT;
	bar_cfg[0][BAR_MODULE_CUSTOM].position = BAR_POS_RIGHT;

	/* Copy positions to all workspaces */
	for (int ws = 1; ws < WSCOUNT; ws++) {
		for (int i = 0; i < BAR_MODULE_COUNT; i++) {
			bar_cfg[ws][i].position = bar_cfg[0][i].position;
		}
	}

	/* Default workspace names */
	for (int i = 0; i < WSCOUNT; i++) {
		char buf[8];
		snprintf(buf, sizeof buf, "%d", i + 1);
		snprintf(bar_cfg[i][BAR_MODULE_WORKSPACES].workspace_name,
		         sizeof bar_cfg[i][BAR_MODULE_WORKSPACES].workspace_name, "%s", buf);
		bar_cfg[i][BAR_MODULE_WORKSPACES].visible = 1;
		bar_cfg[i][BAR_MODULE_CUSTOM].visible = 0;
	}

	/* Default custom text */
	snprintf(bar_cfg[0][BAR_MODULE_CUSTOM].custom_text,
	         sizeof bar_cfg[0][BAR_MODULE_CUSTOM].custom_text, "zovwm");
}

static void
applyline(const char *key, const char *val) {
	if (strcmp(key, "module_order") == 0) {
		/* Parse comma-separated module types with optional position */
		bar_module_count = 0;
		char *token = strtok((char *)val, ",");
		while (token && bar_module_count < BAR_MODULE_COUNT) {
			/* Trim whitespace */
			while (isspace((unsigned char)*token)) token++;
			char *end = token + strlen(token) - 1;
			while (end > token && isspace((unsigned char)*end)) *end-- = '\0';
			
			BarModuleType type = BAR_MODULE_COUNT;
			BarPosition pos = BAR_POS_LEFT;
			char typestr[32] = "", posstr[16] = "";
			
			/* Check for type:position format */
			char *colon = strchr(token, ':');
			if (colon) {
				int tlen = (int)(colon - token);
				strncpy(typestr, token, tlen);
				typestr[tlen] = '\0';
				strncpy(posstr, colon + 1, 15);
				posstr[15] = '\0';
				type = parsesimplemodule(typestr);
				pos = parseposition(posstr);
			} else {
				type = parsesimplemodule(token);
			}
			
			if (type != BAR_MODULE_COUNT && bar_module_count < BAR_MODULE_COUNT) {
				bar_module_order[bar_module_count] = type;
				/* Set position for this module type across all workspaces */
				for (int ws = 0; ws < WSCOUNT; ws++) {
					bar_cfg[ws][type].position = pos;
				}
				bar_module_count++;
			}
			token = strtok(NULL, ",");
		}
	} else if (strcmp(key, "workspace_name") == 0) {
		int idx;
		char name[64];
		if (sscanf(val, "%d %63s", &idx, name) == 2 && idx >= 0 && idx < WSCOUNT) {
			snprintf(bar_cfg[idx][BAR_MODULE_WORKSPACES].workspace_name,
			         sizeof bar_cfg[idx][BAR_MODULE_WORKSPACES].workspace_name, "%s", name);
			bar_cfg[idx][BAR_MODULE_WORKSPACES].visible = 1;
		}
	} else if (strcmp(key, "custom_text") == 0) {
		int idx;
		char text[128];
		if (sscanf(val, "%d %127[^\n]", &idx, text) == 2 && idx >= 0 && idx < WSCOUNT) {
			snprintf(bar_cfg[idx][BAR_MODULE_CUSTOM].custom_text,
			         sizeof bar_cfg[idx][BAR_MODULE_CUSTOM].custom_text, "%s", text);
		}
	}
}

int
barconf_load(void) {
	setdefault_config();

	FILE *f = fopen(configpath(), "r");
	if (!f) {
		barconf_save();
		return 0;
	}

	char line[256];
	while (fgets(line, sizeof line, f)) {
		char *p = line;
		while (isspace((unsigned char)*p))
			p++;
		if (*p == '#' || *p == '\0' || *p == '\n')
			continue;

		char key[32] = "", val[256] = "";
		if (sscanf(p, "%31s %255[^\n]", key, val) == 2)
			applyline(key, val);
	}
	fclose(f);
	return 0;
}

void
barconf_save(void) {
	char *path = configpath();
	char dir[512];
	snprintf(dir, sizeof dir, "%s", path);
	char *slash = strrchr(dir, '/');
	if (slash)
		*slash = '\0';
	for (char *s = dir + 1; *s; s++) {
		if (*s == '/') {
			*s = '\0';
			mkdir(dir, 0755);
			*s = '/';
		}
	}
	mkdir(dir, 0755);

	FILE *f = fopen(path, "w");
	if (!f) {
		fprintf(stderr, "zovwm: cannot write %s\n", path);
		return;
	}

	fprintf(f, "# zovwm bar module configuration\n");
	fprintf(f, "# Format: module_order <type:pos,type:pos,...>\n");
	fprintf(f, "# Types: workspaces, layout, clienttitle, clock, keyboard, custom\n");
	fprintf(f, "# Positions: left, center, right\n");
	fprintf(f, "# Default order: workspaces:left,layout:left,clienttitle:center,clock:right,keyboard:right\n");
	fprintf(f, "\n");

	/* Write module order with positions */
	fprintf(f, "module_order ");
	for (int i = 0; i < bar_module_count; i++) {
		BarModuleType type = bar_module_order[i];
		if (i > 0) fprintf(f, ",");
		fprintf(f, "%s:%s", modulename(type), positionname(bar_cfg[0][type].position));
	}
	fprintf(f, "\n\n");

	/* Write workspace names */
	fprintf(f, "# Workspace names (index 0-8)\n");
	for (int i = 0; i < WSCOUNT; i++) {
		fprintf(f, "workspace_name %d %s\n", i,
		        bar_cfg[i][BAR_MODULE_WORKSPACES].workspace_name);
	}

	/* Write custom text */
	fprintf(f, "\n# Custom text for each workspace\n");
	for (int i = 0; i < WSCOUNT; i++) {
		if (bar_cfg[i][BAR_MODULE_CUSTOM].custom_text[0]) {
			fprintf(f, "custom_text %d %s\n", i,
			        bar_cfg[i][BAR_MODULE_CUSTOM].custom_text);
		}
	}

	fclose(f);
}

void
barmodule_set_workspace_name(int ws_idx, const char *name) {
	if (ws_idx < 0 || ws_idx >= WSCOUNT)
		return;
	snprintf(bar_cfg[ws_idx][BAR_MODULE_WORKSPACES].workspace_name,
	         sizeof bar_cfg[ws_idx][BAR_MODULE_WORKSPACES].workspace_name, "%s", name);
	bar_cfg[ws_idx][BAR_MODULE_WORKSPACES].visible = 1;
}

const char*
barmodule_get_workspace_name(int ws_idx) {
	if (ws_idx < 0 || ws_idx >= WSCOUNT)
		return "";
	return bar_cfg[ws_idx][BAR_MODULE_WORKSPACES].workspace_name;
}
