// Crossfire - SKSE plugin
// Copyright (C) 2026 izzydoingit
// GPL-3.0-or-later; see LICENSE.txt and the notice at the top of main.cpp.
//
// DevBench, when it is in the load order: `inspect kind=crossfire` returns this session's counters and the struggle the
// player is in, and `menu action=invoke name=crossfire set=previewbar` shows the struggle bar's preview, the same as the
// menu's "Preview the bar" button. Nothing here runs unless DevBench asks.

#include "Plugin.h"

#include "DevBenchAPI.h"

namespace Crossfire
{
	namespace
	{
		constexpr const char* kKey = "crossfire";
		constexpr unsigned    kNeedsBuild = 10500;  // DevBench 1.5.0: RegisterToolExtension

		constexpr const char* kInspect =
			R"({"description":"Crossfire - this session's clash and struggle counters, and the struggle the player is locked in now. Read only.","inputSchema":{"type":"object","properties":{}},"readOnly":true})";
		constexpr const char* kMenu =
			R"({"description":"Crossfire - set=previewbar shows the struggle bar's preview for a few seconds (as the menu's button does).","inputSchema":{"type":"object","properties":{"set":{"type":"string"}}}})";

		std::string Text(const char* a_s)
		{
			std::string out;
			for (const char* p = a_s; p && *p; ++p) {
				const auto c = static_cast<unsigned char>(*p);
				if (c == '"' || c == '\\') {
					out += '\\';
					out += *p;
				} else if (c >= 0x20 && c < 0x7F) {
					out += *p;
				}
			}
			return out;
		}

		void Inspect(void*, const char*, void* a_sink, DevBenchAPI::WriteFn a_write) noexcept
		{
			if (!a_write) {
				return;
			}
			try {
				auto&      s = Counters();
				const auto v = StruggleNow();
				const auto out = std::format(
					R"({{"tracked":{},"clashes":{},"destroyed":{},"weakened":{},"bursts":{},"yours":{},"struggles":{},"overwhelms":{},"won":{},"lost":{},"gaveWay":{},"draws":{},"cut":{},"locked":{},"foe":"{}","balance":{:.3f},"seconds":{:.2f},"mySkill":{},"theirSkill":{}}})",
					s.tracked.load(), s.clashes.load(), s.destroyed.load(), s.weakened.load(), s.explosions.load(), s.yours.load(), s.struggles.load(),
					s.overwhelms.load(), s.won.load(), s.lost.load(), s.gaveWay.load(), s.draws.load(), s.cut.load(), v.active ? "true" : "false",
					Text(v.foe), std::isfinite(v.balance) ? v.balance : 0.0f, std::isfinite(v.seconds) ? v.seconds : 0.0f, v.mySkill, v.theirSkill);
				a_write(a_sink, out.c_str());
			} catch (...) {
				a_write(a_sink, R"({"error":"Crossfire could not build its report"})");
			}
		}

		void Menu(void*, const char* a_args, void* a_sink, DevBenchAPI::WriteFn a_write) noexcept
		{
			const std::string_view args = a_args ? a_args : "";
			const bool             ok = args.find("previewbar") != std::string_view::npos;
			if (ok) {
				RequestBarPreview();
			}
			if (a_write) {
				a_write(a_sink, ok ? R"({"queued":true})" : R"({"queued":false,"error":"set is previewbar"})");
			}
		}
	}

	void OfferToDevBench()
	{
		auto* devbench = DevBenchAPI::GetDevBenchInterface001();
		if (!devbench || devbench->GetBuildNumber() < kNeedsBuild) {
			return;  // DevBench is not in this load order, or too old; nothing depends on it
		}
		devbench->RegisterToolExtension("inspect", kKey, kInspect, Inspect, nullptr);
		devbench->RegisterMenuHandler(kKey, kMenu, Menu, nullptr);
		SKSE::log::info("DevBench: inspect kind={} and menu invoke name={} registered", kKey, kKey);
	}
}
