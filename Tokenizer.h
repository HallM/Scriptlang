#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "Tokens.h"

std::vector<std::shared_ptr<Token>> tokenize_string(std::string contents);
