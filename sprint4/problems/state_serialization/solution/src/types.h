#pragma once

#include <string>
#include <cstdint>
#include "tagged.h"

namespace app {

	// Теги для маркированных типов
	struct PlayerIdTag {};
	struct TokenTag {};

	// Идентификатор игрока
	using PlayerId = util::Tagged<uint32_t, PlayerIdTag>;

	// Токен авторизации (строка)
	using Token = util::Tagged<std::string, TokenTag>;

} // namespace app