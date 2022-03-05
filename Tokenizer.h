#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "Tokens.h"

namespace MattScript {
namespace Tokenizer {

std::vector<std::shared_ptr<Tokens::Token>> tokenize_string(std::string contents);

}
}
