// src/ide/ide_project.h
//
// A project is a folder on disk that holds everything a game made in the
// editor needs: a script, the images used as sprites, and a small manifest that
// ties them together. Losing the app must not lose the work, so everything is
// written as ordinary files that can be copied, zipped and read by hand.
//
//   MyGame/
//     project.json     names, sprite list, script list (what this header reads)
//     scripts/main.ds  the code, one file per script entry
//     sprites/ball.png copied straight from the photo library
//
// project.json is deliberately tiny and hand-editable:
//
//   {"version":1,"name":"MyGame","script":"scripts/main.ds",
//    "sprites":[{"name":"ball","file":"sprites/ball.png"},
//               {"name":"tree","file":"sprites/tree.png"}]}
#ifndef IDE_PROJECT_H
#define IDE_PROJECT_H

#include <stddef.h>

#define IDE_PROJECT_MAX_SPRITES 64
#define IDE_PROJECT_MAX_SCRIPTS 16

typedef struct IDEProjectSprite {
    char name[48];   // shown in the editor, used by scripts
    char file[160];  // relative to the project folder
} IDEProjectSprite;

typedef struct IDEProjectScript {
    char name[48];
    char file[160];
} IDEProjectScript;

typedef struct IDEProject {
    char name[64];
    char dir[400];  // absolute folder, no trailing slash
    IDEProjectSprite sprites[IDE_PROJECT_MAX_SPRITES];
    int sprite_count;
    IDEProjectScript scripts[IDE_PROJECT_MAX_SCRIPTS];
    int script_count;
} IDEProject;

/* Start a fresh project rooted at `dir`. Creates the folder and the standard
 * subfolders, writes an empty script and the manifest. Returns 1 on success. */
int ide_project_create(IDEProject *p, const char *dir, const char *name);

/* Load an existing project folder. Returns 1 when project.json parsed. A folder
 * with a script but no manifest still loads (script_count stays 0). */
int ide_project_open(IDEProject *p, const char *dir);

/* Write project.json. Called after any change to the sprite or script list. */
int ide_project_save(IDEProject *p);

/* Path helpers: fill `out` with the absolute path of a project member. */
void ide_project_script_path(const IDEProject *p, char *out, size_t cap);
void ide_project_sprite_path(const IDEProject *p, const char *file, char *out, size_t cap);

/* Turn image bytes (PNG/JPEG as read from storage) into sprites/<name>.img and
 * register it. Renames on a clash. Returns 1 on success. */
int ide_project_add_sprite_file(IDEProject *p, const char *source_path, const char *display_name);
/* Register a sprite whose bytes are already in memory. */
int ide_project_add_sprite_bytes(IDEProject *p, const void *data, size_t len, const char *display_name,
                                 const char *ext);

/* Remove a sprite entry and its file. Returns 1 when it was there. */
int ide_project_remove_sprite(IDEProject *p, const char *name);
int ide_project_find_sprite(const IDEProject *p, const char *name);

/* Add a script entry (writes an empty file if it does not exist). */
int ide_project_add_script(IDEProject *p, const char *name);

/* The project folder the editor starts in when PROJECT_DIR is unset. */
void ide_project_default_root(char *out, size_t cap);

/* ---- small filesystem helpers the editor needs cross-platform ----------- */
// These wrap the two platform branches in engine/network/fs_io.inc so the
// project code has one call to make on Android, Windows and Linux alike.
int ide_fs_mkdir(const char *path);
int ide_fs_exists(const char *path);
int ide_fs_read_all(const char *path, char **out, size_t *len);
int ide_fs_write_all(const char *path, const void *data, size_t len);

#endif
