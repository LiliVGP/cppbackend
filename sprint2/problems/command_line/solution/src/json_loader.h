#pragma once

#include <filesystem>

#include "model.h"

namespace json_loader {

	void LoadGame(const std::filesystem::path& json_path, model::Game& game);

}  // namespace json_loader