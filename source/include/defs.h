// defs.h
// Copyright (c) 2021, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once
#include "zpr.h"
#include "zbuf.h"

#include <map>
#include <optional>
#include <functional>
#include <unordered_map>

namespace ast { struct Expr; struct Lambda; }

namespace lc
{
	constexpr int FLAG_ABBREV_LAMBDA    = 0x1;
	constexpr int FLAG_ABBREV_PARENS    = 0x2;
	constexpr int FLAG_HASKELL_STYLE    = 0x4;
	constexpr int FLAG_NO_PRINT         = 0x8;
	constexpr int FLAG_TRACE            = 0x10;
	constexpr int FLAG_FULL_TRACE       = 0x20;
	constexpr int FLAG_VAR_REPLACEMENT  = 0x40;

	struct Context
	{
		int flags = 0;
		std::map<std::string, const ast::Expr*> vars;
	};

	ast::Expr* evaluate(Context& vc, const ast::Expr* expr, int print_flags);

	std::pair<std::string, std::string> highlight(const ast::Expr* expr,
		std::function<std::optional<std::string> (const ast::Expr*)> pred,
		std::function<std::optional<std::string> (const ast::Expr*)> arg_pred,
		int flags);

	std::string print(const ast::Expr* expr, int flags = 0);
	std::string print(const ast::Expr* expr, std::function<std::optional<std::string> (const ast::Expr*)> replace,
		int flags);

	// the reason that these two take Expr** is... complicated.
	std::pair<std::string, std::string> logBetaReduction(const ast::Expr** whole, const ast::Expr* fn,
		const ast::Expr* arg, const std::vector<ast::Expr**>& substs, int print_flags);

	std::pair<std::string, std::string> logAlphaConversion(const ast::Expr** whole,
		const ast::Expr* sub, int print_flags);

	void printError(zbuf::str_view msg);

	void repl(Context& ctx);
	void loadFile(Context& ctx, zbuf::str_view path);
	void evalLine(Context& ctx, zbuf::str_view sv);

	constexpr const char* COLOUR_RESET  = "\x1b[0m";
	constexpr const char* YELLOW        = "\x1b[33m";
	constexpr const char* GREEN         = "\x1b[32m";

	constexpr const char* GREEN_BOLD    = "\x1b[1m\x1b[32m";
	constexpr const char* YELLOW_BOLD   = "\x1b[1m\x1b[33m";
	constexpr const char* BLUE_BOLD     = "\x1b[1m\x1b[34m";
	constexpr const char* RED_BOLD      = "\x1b[1m\x1b[31m";
	constexpr const char* BLACK_BOLD    = "\x1b[1m";
	constexpr const char* GREY_BOLD     = "\x1b[30;1m";
}

namespace unicode
{
	size_t is_category(zbuf::str_view str, const std::initializer_list<int>& categories);
	size_t get_codepoint_length(zbuf::str_view str);
	size_t is_letter(zbuf::str_view str);
	size_t is_digit(zbuf::str_view str);
}
