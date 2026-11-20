#pragma once

#ifdef __cplusplus
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// 最後に読んだ曲。version は内容が変わるたびに増える。pixels は card_draw で描いた絵。
struct NowPlaying {
	bool playing = false;
	std::string title;
	std::string artist;
	std::vector<uint8_t> thumb; // ジャケット画像のファイルの中身（PNG や JPEG のまま）
	uint64_t version = 0;
	std::shared_ptr<const std::vector<uint8_t>> pixels;
};
NowPlaying nowplaying_get();
std::vector<uint8_t> card_draw(const NowPlaying &n);

extern "C" {
#endif

void nowplaying_start(void);
void nowplaying_stop(void);
void card_register(void);

#ifdef __cplusplus
}
#endif
