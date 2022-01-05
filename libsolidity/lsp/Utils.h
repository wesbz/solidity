#pragma once

#include <liblangutil/SourceLocation.h>

#include <libsolidity/ast/ASTForward.h>

#include <libsolutil/JSON.h>

#include <optional>
#include <vector>

#if !defined(NDEBUG)
#include <fstream>
#define lspDebug(message) (std::ofstream("/tmp/solc.log", std::ios::app) << (message) << std::endl)
#else
#define lspDebug(message) do {} while (0)
#endif

namespace solidity::langutil
{
class CharStreamProvider;
}

namespace solidity::lsp
{

class FileRepository;

std::optional<langutil::LineColumn> parseLineColumn(Json::Value const& _lineColumn);
Json::Value toJson(langutil::LineColumn _pos);
Json::Value toJsonRange(langutil::LineColumn const& _start, langutil::LineColumn const& _end);

std::vector<frontend::Declaration const*> allAnnotatedDeclarations(frontend::Expression const* _expression);
std::optional<langutil::SourceLocation> declarationLocation(frontend::Declaration const* _declaration);

}
