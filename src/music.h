/*
Background music: any .wav, .mp3 or .ogg in the soundtrack folder.
A track is picked at random; when it ends the folder is rescanned and
another random track starts. Volume eases toward whatever is asked for.
*/

#pragma once

#include <stdbool.h>

void MusicInit(void);		// after the audio device is up: find the folder, start a random track
void MusicUnload(void);
void MusicUpdate(float targetVolume, float dt);	// every frame
void MusicNext(void);		// rescan the folder and switch to a different random track
const char *MusicTrackName(void);	// file name of the playing track, or NULL
const char *MusicFolder(void);		// folder in use, or NULL if none was found
int MusicTrackNumber(void);		// 1-based position of the playing track in the folder, 0 if none
int MusicTrackCount(void);
