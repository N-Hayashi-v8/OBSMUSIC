// 再生中の曲を Windows のメディア情報（SMTC）から読む。
// 今は 2 秒ごとに読み、曲が変わったら OBS のログに出すだけ。
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>

#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>

#include <obs-module.h>
#include <plugin-support.h>

#include "nowplaying.h"

using namespace winrt::Windows::Media::Control;
using namespace winrt::Windows::Storage::Streams;

static std::atomic<bool> running;
static std::thread worker;
static std::mutex mu;
static NowPlaying state;

NowPlaying nowplaying_get()
{
	std::lock_guard<std::mutex> g(mu);
	return state;
}

static std::string utf8(winrt::hstring const &w)
{
	if (w.empty())
		return {};
	int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
	std::string s(n, '\0');
	WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
	return s;
}

static void loop()
{
	winrt::init_apartment();
	std::string last;
	GlobalSystemMediaTransportControlsSessionManager mgr{nullptr};
	for (; running; std::this_thread::sleep_for(std::chrono::seconds(2))) {
		// 失敗はこの回だけ飛ばして、次の回にやり直す（曲を読んでいる最中にブラウザを閉じたときなど）
		try {
			if (!mgr)
				mgr = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
			// ponytail: 「現在のセッション」1つだけ見る。複数アプリが鳴るとき YouTube Music を選ぶ処理は未実装
			NowPlaying n;
			if (auto s = mgr.GetCurrentSession()) {
				auto p = s.TryGetMediaPropertiesAsync().get();
				n.title = utf8(p.Title());
				n.artist = utf8(p.Artist());
				n.playing = s.GetPlaybackInfo().PlaybackStatus() ==
					    GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
			}
			std::string now = n.playing ? n.title + " / " + n.artist : "(再生なし)";
			if (now == last)
				continue;
			// ponytail: ジャケットは曲名が変わったときに1回だけ読む。後から差し替わるアプリでは古いままになる
			if (auto s = mgr.GetCurrentSession(); s && n.playing) {
				if (auto t = s.TryGetMediaPropertiesAsync().get().Thumbnail()) {
					auto st = t.OpenReadAsync().get();
					Buffer buf((uint32_t)st.Size());
					auto r = st.ReadAsync(buf, buf.Capacity(), InputStreamOptions::None).get();
					n.thumb.assign(r.data(), r.data() + r.Length());
				}
			}
			obs_log(LOG_INFO, "now playing: %s", now.c_str());
			last = now;
			// 絵はここ（OBS の描画スレッドの外）で描いておき、ソースは受け取るだけにする
			n.pixels = std::make_shared<const std::vector<uint8_t>>(card_draw(n));
			std::lock_guard<std::mutex> g(mu);
			n.version = state.version + 1;
			state = n;
		} catch (winrt::hresult_error const &e) {
			obs_log(LOG_WARNING, "now playing: 失敗 0x%08x", (unsigned)e.code().value);
		}
	}
}

void nowplaying_start()
{
	running = true;
	worker = std::thread(loop);
}

void nowplaying_stop()
{
	running = false;
	if (worker.joinable())
		worker.join();
}
