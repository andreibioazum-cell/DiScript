#ifndef HOST_STUB_ANDROID_ASSET_H
#define HOST_STUB_ANDROID_ASSET_H
#include <stddef.h>
#include <sys/types.h>
typedef struct AAssetManager AAssetManager;
typedef struct AAsset AAsset;
typedef struct AAssetDir AAssetDir;
enum { AASSET_MODE_BUFFER = 3 };
AAsset *AAssetManager_open(AAssetManager *mgr, const char *filename, int mode);
AAssetDir *AAssetManager_openDir(AAssetManager *mgr, const char *dirName);
const char *AAssetDir_getNextFileName(AAssetDir *assetDir);
void AAssetDir_close(AAssetDir *assetDir);
off_t AAsset_getLength(AAsset *asset);
int AAsset_read(AAsset *asset, void *buf, size_t count);
int AAsset_close(AAsset *asset);
#endif
