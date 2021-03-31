// repl.cpp
// Copyright (c) 2021, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "defs.h"

#include <iostream>

namespace lc
{
	zbuf::str_view trim(zbuf::str_view s);
	void runReplCommand(Context& ctx, zbuf::str_view cmd);
	void parseError(parser::Error e, zbuf::str_view input);

	// util.cpp
	bool alpha_equivalent(Context& ctx, const ast::Expr* a, const ast::Expr* b);

	static void print_replacing_vars(Context& ctx, const ast::Expr* e);

	void evalLine(Context& ctx, zbuf::str_view sv)
	{
 		auto input = trim(sv);
		if(input.empty())
			return;

		// comment
		if(input[0] == '#')
			return;

		if(input[0] == ':')
		{
			runReplCommand(ctx, input);
			zpr::println("");
			return;
		}

 		auto expr_or_error = parser::parse(input);
 		if(!expr_or_error)
 			return parseError(expr_or_error.error(), input);

		auto expr = lc::evaluate(ctx, expr_or_error.unwrap(), ctx.flags);
		print_replacing_vars(ctx, expr);
	}

	void repl(Context& ctx)
	{
		// by default, trace and try to back-substitute variables.
		ctx.flags |= (FLAG_TRACE | FLAG_VAR_REPLACEMENT);

		while(true)
		{
			zpr::print("λ> ");

			// read one line
			std::string input; std::getline(std::cin, input);
			if(std::cin.eof())
				break;

			if(input == ":q")
				break;

			evalLine(ctx, input);
		}

		zpr::println("");
	}

	static void print_replacing_vars(Context& ctx, const ast::Expr* e)
	{
		auto normal = lc::print(e, ctx.flags);

		std::string replaced;
		if(ctx.flags & FLAG_VAR_REPLACEMENT)
		{
			replaced = lc::print(e, [&](const ast::Expr* expr) -> std::optional<std::string> {
				for(auto& [ name, var ] : ctx.vars)
				{
					// note that this will evaluate the SECOND ARGUMENT only
					if(alpha_equivalent(ctx, expr, var))
						return name;
				}

				return { };
			}, ctx.flags);
		}

		zpr::println("{}", normal);
		if((ctx.flags & FLAG_VAR_REPLACEMENT) && normal != replaced)
			zpr::println("= {}", replaced);

		zpr::println("");
	}





	void runReplCommand(Context& ctx, zbuf::str_view input)
	{
		auto print_thingy = [&ctx](const char* thing, int f) {
			bool en = (ctx.flags & f);
			zpr::println("{}*.{} {} {}{}{}", BLACK_BOLD, COLOUR_RESET, thing, en ? GREEN_BOLD : RED_BOLD,
				en ? "enabled" : "disabled", COLOUR_RESET);
		};

		if(input == ":p")
		{
			ctx.flags ^= FLAG_ABBREV_PARENS;
			print_thingy("parenthesis omission", FLAG_ABBREV_PARENS);
		}
		else if(input == ":h")
		{
			ctx.flags ^= FLAG_HASKELL_STYLE;
			print_thingy("haskell-style printing", FLAG_HASKELL_STYLE);
		}
		else if(input == ":c")
		{
			ctx.flags ^= FLAG_ABBREV_LAMBDA;
			print_thingy("curried abbreviation", FLAG_ABBREV_LAMBDA);
		}
		else if(input == ":t")
		{
			ctx.flags ^= FLAG_TRACE;
			print_thingy("tracing", FLAG_TRACE);
		}
		else if(input == ":v")
		{
			ctx.flags ^= FLAG_VAR_REPLACEMENT;
			print_thingy("reverse variable substitution", FLAG_VAR_REPLACEMENT);
		}
		else if(input == ":ft")
		{
			ctx.flags ^= FLAG_FULL_TRACE;
			print_thingy("full tracing", FLAG_FULL_TRACE);
		}
		else if(input.find(":load ") == 0)
		{
			auto path = trim(input.drop(strlen(":load ")));
			if(path.empty())
				printError(zpr::sprint("expected path for ':load'", input));

			else
				lc::loadFile(ctx, path);
		}
		else
		{
			printError(zpr::sprint("unknown command '{}'", input));
		}
	}

	void printError(zbuf::str_view msg)
	{
		zpr::fprintln(stderr, "{}error:{} {}{}{}", RED_BOLD, COLOUR_RESET, BLACK_BOLD, msg, COLOUR_RESET);
	}

	void parseError(parser::Error e, zbuf::str_view input)
	{
		printError(e.msg);
		zpr::fprintln(stderr, "{}here:{}  {}", GREY_BOLD, COLOUR_RESET, input);

		std::string underline;
		if(e.loc.length > 1)
			for(size_t i = 0; i < e.loc.length; i++) underline += "\u203e";
		else
			underline = "^";

		zpr::fprintln(stderr, "{}{}{}{}\n", zpr::w(7 + e.loc.begin)(""), RED_BOLD, underline, COLOUR_RESET);
	}

	zbuf::str_view trim(zbuf::str_view s)
	{
		while(s.front() == ' ' || s.front() == '\t')
			s.remove_prefix(1);

		while(s.back() == ' ' || s.back() == '\t')
			s.remove_suffix(1);

		return s;
	}
}
