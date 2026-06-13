#ifndef _ANDROID_ASSETS_H_
#define _ANDROID_ASSETS_H_

#ifdef __ANDROID__

// Unpack the game data bundled in the APK (under assets/zod/) into the app's
// writable internal storage on first launch, then chdir into it. After this
// the engine's ordinary fopen/ifstream/SDL_LoadBMP calls resolve assets/,
// maps/ and map_list.txt exactly as they do on desktop. Returns true on success.
bool AndroidExtractAssetsAndChdir();

#endif
#endif
