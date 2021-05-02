// highlight.cpp
// Copyright (c) 2021, zhiayang
// Licensed under the Apache License Version 2.0.

#include <set>
#include <iterator>
#include <algorithm>
#include <functional>

#include "ast.h"

namespace lc
{
	using namespace ast;
	using parser::Location;

	// constexpr const char* BLACK         = "\x1b[30m";
	// constexpr const char* RED           = "\x1b[31m";
	// constexpr const char* BLUE          = "\x1b[34m";
	// constexpr const char* MAGENTA       = "\x1b[35m";
	// constexpr const char* CYAN          = "\x1b[36m";
	// constexpr const char* WHITE         = "\x1b[37m";

	// constexpr const char* MAGENTA_BOLD  = "\x1b[1m\x1b[35m";
	// constexpr const char* CYAN_BOLD     = "\x1b[1m\x1b[36m";
	// constexpr const char* WHITE_BOLD    = "\x1b[1m\x1b[37m";

	struct State
	{
		int flags = 0;

		std::function<std::optional<std::string> (const ast::Expr*)> pred;
		std::function<std::optional<std::string> (const ast::Expr*)> arg_pred;
		std::function<std::optional<std::string> (const ast::Expr*)> replacer;

		// internal state
		// int match_depth = 0;
		std::set<std::string> combined_args;

		std::vector<std::string> ulines;
	};

	static void int_highlight(State& st, const Expr* expr, std::string& top, std::string& bot,
		bool combine = false, bool omit_lambda_parens = false)
	{
		bool pop = false;
		bool match = false;

		std::string under;
		if(auto u = st.pred(expr); u.has_value())
			match = true, pop = true, under = *u, st.ulines.push_back(*u);

		else if(st.ulines.size() > 0)
			match = true, under = st.ulines.back();

		else
			under = " ";

		auto add = [&top, &bot](const auto& t, const auto& b) {
			top += t;
			bot += b;
		};

		auto repeat = [](size_t n, const std::string& c) -> std::string {
			std::string ret;
			for(size_t i = 0; i < n; i++)
				ret += c;
			return ret;
		};

		if(st.replacer)
		{
			if(auto rep = st.replacer(expr); rep.has_value())
			{
				add(*rep, repeat(rep->size(), under));
				return;
			}
		}

		if(auto v = dynamic_cast<const Var*>(expr); v)
		{
			add(v->name, repeat(v->name.size(), under));
		}
		else if(auto a = dynamic_cast<const Apply*>(expr); a)
		{
			int_highlight(st, a->fn, top, bot);
			add(" ", under);

			// omit brackets if possible
			bool close = false;
			if(!(st.flags & FLAG_ABBREV_PARENS) || !dynamic_cast<const Var*>(a->arg))
				close = true, add("(", under);

			bool omit_lambda_parens = false;
			if(st.flags & FLAG_ABBREV_PARENS && dynamic_cast<const Lambda*>(a->arg))
				omit_lambda_parens = true;

			int_highlight(st, a->arg, top, bot, /* combine: */ false, /* omit_lambda_parens: */ omit_lambda_parens);

			if(close) add(")", under);
		}
		else if(auto f = dynamic_cast<const Lambda*>(expr); f)
		{
			bool close = false;
			if(!combine)
			{
				if(!omit_lambda_parens)
					close = true, add("(", under);

				if(st.flags & FLAG_HASKELL_STYLE)   add("\\", under);
				else                                add("λ", under);
			}

			if(auto u = st.arg_pred(f); u.has_value())
				add(f->arg, repeat(f->arg.size(), *u));

			else
				add(f->arg, repeat(f->arg.size(), under));

			if(st.flags & FLAG_ABBREV_LAMBDA)
				st.combined_args.insert(f->arg);

			bool omit_next_parens = false;
			if(auto inner = dynamic_cast<Lambda*>(f->body); (st.flags & FLAG_ABBREV_LAMBDA) && inner)
			{
				// if an outer lambda already bound this argument, for disambiguity's sake
				// we must break up the lambda so we don't end up with λx y x y. (...), but
				// rather λx y.λx y.( ... )
				if(st.combined_args.find(inner->arg) != st.combined_args.end())
				{
					// once we start a 'new' lambda, we are free to bind whatever again.
					st.combined_args.clear();
					omit_next_parens = true;
					goto normal;
				}

				// if we're combining, separate args with a space.
				add(" ", under);

				int_highlight(st, inner, top, bot, /* combine: */ true);
			}
			else
			{
			normal:
				if(st.flags & FLAG_HASKELL_STYLE)   add(" -> ", repeat(4, under));
				else                                add(".", under);

				int_highlight(st, f->body, top, bot, /* combine: */ false,
					/* omit_lambda_parens: */ omit_next_parens);
			}

			st.combined_args.erase(f->arg);
			if(close)
				add(")", under);
		}
		else if(auto let = dynamic_cast<const Let*>(expr); let)
		{
			add("let ", repeat(4, " "));
			add(let->name, repeat(let->name.size(), under));
			add(" = ", repeat(3, " "));

			int_highlight(st, let->value, top, bot);
		}
		else
		{
			abort();
		}

		if(pop) st.ulines.pop_back();
	}

	std::pair<std::string, std::string> highlight(const Expr* expr,
		std::function<std::optional<std::string> (const ast::Expr*)> pred,
		std::function<std::optional<std::string> (const ast::Expr*)> arg_pred, int flags)
	{
		State st { };
		st.pred = std::move(pred),
		st.flags = flags;
		st.arg_pred = std::move(arg_pred);

		std::string top, bot;
		int_highlight(st, expr, top, bot);

		return { top, bot };
	}

	constexpr const char* UNDERLINE         = "\u203e";
	const static auto ALPHA_HIGHLIGHT       = zpr::sprint("{}{}{}", GREEN_BOLD, UNDERLINE, COLOUR_RESET);
	const static auto BETA_VAR_HIGHLIGHT    = zpr::sprint("{}{}{}", YELLOW_BOLD, '^', COLOUR_RESET);
	const static auto BETA_SUB_HIGHLIGHT    = zpr::sprint("{}{}{}", BLUE_BOLD, UNDERLINE, COLOUR_RESET);
	const static auto BETA_ARG_HIGHLIGHT    = zpr::sprint("{}{}{}", GREEN_BOLD, UNDERLINE, COLOUR_RESET);

	std::pair<std::string, std::string> logAlphaConversion(const Expr** whole, const Expr* sub, int print_flags)
	{
		return highlight(*whole, [&](const Expr* x) -> std::optional<std::string> {
			if(x == sub) return ALPHA_HIGHLIGHT;
			else         return std::nullopt;
		}, [](auto) {
			return std::nullopt;
		}, print_flags);
	}

	std::pair<std::string, std::string> logBetaReduction(const Expr** whole, const Expr* fn, const Expr* arg,
		const std::vector<Expr**>& _substs, int print_flags)
	{
		std::set<const Expr*> subs;
		std::transform(_substs.begin(), _substs.end(), std::inserter(subs, subs.begin()),
			[](auto x) { return *x; });

		return highlight(*whole, [&](const Expr* e) -> std::optional<std::string> {
			if(e == arg)                        return BETA_ARG_HIGHLIGHT;
			else if(subs.find(e) != subs.end()) return BETA_SUB_HIGHLIGHT;
			else                                return std::nullopt;

		}, [&](const Expr* l) -> std::optional<std::string> {
			return (l == fn) ? BETA_VAR_HIGHLIGHT : std::optional<std::string>();
		}, print_flags);
	}

	std::string print(const ast::Expr* expr, int flags)
	{
		return highlight(expr,
			[](auto) { return std::nullopt; },
			[](auto) { return std::nullopt; },
		flags).first;
	}

	std::string print(const ast::Expr* expr, std::function<std::optional<std::string> (const ast::Expr*)> replace,
		int flags)
	{
		State st { };
		st.pred = [](auto) { return std::nullopt; };
		st.arg_pred = [](auto) { return std::nullopt; };
		st.replacer = std::move(replace);
		st.flags = flags;

		std::string top, bot;
		int_highlight(st, expr, top, bot);

		return top;
	}
}
