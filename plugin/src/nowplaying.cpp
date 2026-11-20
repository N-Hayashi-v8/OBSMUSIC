// 再生中の曲を Windows のメディア情報（SMTC）から読む。
// 今は 2 秒ごとに読み、曲が変わったら OBS のログに出すだけ。
#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>

#include <obs-module.h>
#include <plugin-support.h>

#include "nowplaying.h"

using namespace winrt::Windows::Media::Control;

static std::atomic<bool> running;
static std::thread worker;

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
	try {
		auto mgr = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
		while (running) {
			// ponytail: 「現在のセッション」1つだけ見る。複数アプリが鳴るとき YouTube Music を選ぶ処理は未実装
			std::string now = "(再生なし)";
			if (auto s = mgr.GetCurrentSession()) {
				auto p = s.TryGetMediaPropertiesAsync().get();
				now = utf8(p.Title()) + " / " + utf8(p.Artist());
			}
			if (now != last) {
				obs_log(LOG_INFO, "now playing: %s", now.c_str());
				last = now;
			}
			std::this_thread::sleep_for(std::chrono::seconds(2));
		}
	} catch (winrt::hresult_error const &e) {
		obs_log(LOG_WARNING, "now playing: 失敗 0x%08x", (unsigned)e.code().value);
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
