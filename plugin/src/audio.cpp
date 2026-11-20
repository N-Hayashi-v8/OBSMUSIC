// 「ツール → OBSMUSIC 音声の診断」。読むだけで、何も変えない。
// OBS 全体のモニタリングデバイスと、モニタリングしている入力を並べ、音が二重になる設定なら知らせる。
#include <string>

#include <windows.h>

#include <obs-module.h>
#include <obs-frontend-api.h>

#include "nowplaying.h"

static std::wstring wide(const std::string &s)
{
	int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
	std::wstring w(n, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
	return w;
}

struct Report {
	std::string inputs; // 1行ずつ「  入力名: 設定」
	int monitored = 0;
};

static bool add_input(void *data, obs_source_t *src)
{
	auto *r = static_cast<Report *>(data);
	if (!(obs_source_get_output_flags(src) & OBS_SOURCE_AUDIO))
		return true;
	const char *mode = "オフ";
	switch (obs_source_get_monitoring_type(src)) {
	case OBS_MONITORING_TYPE_MONITOR_ONLY:
		mode = "モニターのみ";
		break;
	case OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT:
		mode = "モニターと出力";
		break;
	default:
		break;
	}
	if (obs_source_get_monitoring_type(src) != OBS_MONITORING_TYPE_NONE)
		r->monitored++;
	r->inputs += std::string("  ") + obs_source_get_name(src) + ": " + mode + "\n";
	return true;
}

static void diagnose(void *)
{
	const char *name = nullptr, *id = nullptr;
	obs_get_audio_monitoring_device(&name, &id);
	Report r;
	obs_enum_sources(add_input, &r);

	std::string msg = std::string("モニタリングデバイス: ") + (name ? name : "") + "\n\n入力ごとのモニタリング:\n" +
			  (r.inputs.empty() ? "  （音声のある入力がありません）\n" : r.inputs);
	bool doubled = r.monitored > 0 && id && strcmp(id, "default") == 0;
	if (doubled)
		msg += "\n注意: モニタリングデバイスが既定のデバイスです。モニタリングしている入力の音が、"
		       "元の音と重なって二重に聞こえます。";
	MessageBoxW((HWND)obs_frontend_get_main_window_handle(), wide(msg).c_str(), L"OBSMUSIC 音声の診断",
		    MB_OK | (doubled ? MB_ICONWARNING : MB_ICONINFORMATION));
}

void audio_register()
{
	obs_frontend_add_tools_menu_item("OBSMUSIC 音声の診断", diagnose, nullptr);
}
