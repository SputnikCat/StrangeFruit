#include "music.h"
#include "raylib.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define FOLDER_NAME	"soundtrack"
#define MAX_TRACKS	256
#define FADE_RATE	2.5f	// 1/s, how quickly the volume eases to its target

static char folder[1024];
static bool haveFolder;

static char tracks[MAX_TRACKS][1024];
static int trackCount;
static int current = -1;

static Music music;
static bool playing;
static float volume;

// case-insensitive, so TRACK.MP3 counts
static bool IsTrack(const char *path)
{
	const char *dot = strrchr(path, '.');
	if (!dot) return false;
	char ext[8] = { 0 };
	for (int i = 0; i < 7 && dot[i]; i++) ext[i] = (char)tolower((unsigned char)dot[i]);
	return strcmp(ext, ".wav") == 0 || strcmp(ext, ".mp3") == 0 || strcmp(ext, ".ogg") == 0;
}

// the working folder first (Visual Studio runs from the repo), then beside the exe, then the repo from bin/<config>/
static void FindFolder(void)
{
	const char *app = GetApplicationDirectory();
	const char *candidates[] = {
		FOLDER_NAME,
		TextFormat("%s%s", app, FOLDER_NAME),
		TextFormat("%s../../%s", app, FOLDER_NAME),
	};
	for (int i = 0; i < 3; i++)
	{
		if (DirectoryExists(candidates[i]))
		{
			snprintf(folder, sizeof(folder), "%s", candidates[i]);
			haveFolder = true;
			return;
		}
	}
	haveFolder = false;
}

static void Scan(void)
{
	trackCount = 0;
	if (!haveFolder) return;
	FilePathList files = LoadDirectoryFiles(folder);
	for (unsigned int i = 0; i < files.count && trackCount < MAX_TRACKS; i++)
		if (IsTrack(files.paths[i])) snprintf(tracks[trackCount++], sizeof(tracks[0]), "%s", files.paths[i]);
	UnloadDirectoryFiles(files);
}

static void Stop(void)
{
	if (!playing) return;
	StopMusicStream(music);
	UnloadMusicStream(music);
	playing = false;
	current = -1;
}

// a random track, avoiding the one just played when there's a choice; skips files that won't load
static void PlayRandom(int avoid)
{
	Stop();
	if (trackCount == 0) return;
	int start = GetRandomValue(0, trackCount - 1);
	if (trackCount > 1 && start == avoid) start = (start + 1) % trackCount;
	for (int k = 0; k < trackCount; k++)
	{
		int i = (start + k) % trackCount;
		if (trackCount > 1 && i == avoid) continue;
		Music m = LoadMusicStream(tracks[i]);
		if (!IsMusicValid(m)) continue;
		music = m;
		music.looping = false;	// at the end of a track another random one starts
		SetMusicVolume(music, volume);
		PlayMusicStream(music);
		playing = true;
		current = i;
		return;
	}
}

void MusicInit(void)
{
	if (!IsAudioDeviceReady()) return;
	FindFolder();
	Scan();
	volume = 0.0f;	// fades in
	PlayRandom(-1);
}

void MusicUnload(void)
{
	Stop();
}

void MusicNext(void)
{
	if (!IsAudioDeviceReady()) return;
	int was = current;
	char wasPath[1024] = { 0 };
	if (was >= 0) snprintf(wasPath, sizeof(wasPath), "%s", tracks[was]);

	if (!haveFolder) FindFolder();
	Scan();

	// find the old track in the fresh list so it isn't picked again
	int avoid = -1;
	for (int i = 0; i < trackCount; i++)
		if (strcmp(tracks[i], wasPath) == 0) avoid = i;
	PlayRandom(avoid);
}

void MusicUpdate(float targetVolume, float dt)
{
	if (!playing) return;
	volume += (targetVolume - volume)*(1.0f - expf(-dt*FADE_RATE));
	SetMusicVolume(music, volume);
	UpdateMusicStream(music);
	if (!IsMusicStreamPlaying(music)) MusicNext();
}

const char *MusicTrackName(void)
{
	return playing? GetFileName(tracks[current]) : NULL;
}

int MusicTrackNumber(void)
{
	return playing? current + 1 : 0;
}

int MusicTrackCount(void)
{
	return trackCount;
}

const char *MusicFolder(void)
{
	return haveFolder? folder : NULL;
}
