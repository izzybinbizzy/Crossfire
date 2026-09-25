// Crossfire - libFuzzer target for the settings parser and form references (tests/run.sh --fuzz).
// GPL-3.0-or-later; see LICENSE.txt.
#include "Core.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* a_data, std::size_t a_size)
{
	const std::string_view    text(reinterpret_cast<const char*>(a_data), a_size);
	Crossfire::Core::Config   config;
	std::vector<std::string>  warnings;
	Crossfire::Core::ParseIni(text, config, warnings);
	Crossfire::Core::FormRef ref;
	(void)Crossfire::Core::ParseFormRef(text, ref);
	// whatever was read must write and read back without a single warning
	Crossfire::Core::Config  back;
	std::vector<std::string> again;
	Crossfire::Core::ParseIni(Crossfire::Core::WriteSettings(config), back, again);
	if (!again.empty() || warnings.size() > 200) {
		__builtin_trap();
	}
	return 0;
}
