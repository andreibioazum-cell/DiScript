/* PC shim for Android AAssetManager file access. */
#ifndef PC_SHIM_ANDROID_ASSET_MANAGER_H
#define PC_SHIM_ANDROID_ASSET_MANAGER_H
#include <stddef.h>
#include <sys/types.h>
typedef struct AAssetManager AAssetManager;
typedef struct AAsset AAsset;
enum { AASSET_MODE_BUFFER = 3 };
AAsset *AAssetManager_open(AAssetManager *mgr, const char *filename, int mode);
off_t AAsset_getLength(AAsset *asset);
int AAsset_read(AAsset *asset, void *buf, size_t count);
int AAsset_close(AAsset *asset);
/* PC-only helpers: set/get the asset root and the shared manager instance. */
void pc_assets_set_root(const char *dir);
AAssetManager *pc_asset_manager(void);
#endif
