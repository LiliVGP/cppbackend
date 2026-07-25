#pragma once

#include <string>
#include <cstdint>
#include "tagged.h"

namespace app {

	// Идентификатор игрока
	using PlayerId = util::Tagged<uint32_t, PlayerId>;

	// Токен авторизации (строка)
	using Token = std::string;

} // namespace app