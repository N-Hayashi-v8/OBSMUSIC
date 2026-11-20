#pragma once

#ifdef __cplusplus
#include <cstdint>
#include <string>

// 最後に読んだ曲。version は内容が変わるたびに増える。
struct NowPlaying {
	bool playing = false;
	std::string title;
	std::string artist;
	uint64_t version = 0;
};
NowPlaying nowplaying_get();

extern "C" {
#endif

void nowplaying_start(void);
void nowplaying_stop(void);
void card_register(void);

#ifdef __cplusplus
}
#endif
