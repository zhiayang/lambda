// eval.cpp
// Copyright (c) 2021, zhiayang
// Licensed under the Apache License Version 2.0.

#include <set>

#include "ast.h"

namespace lc
{
	using namespace ast;

	std::vector<Expr**> find_substitutions(Expr** expr, const std::string& var);
	Lambda* substitute(Lambda* expr, const std::vector<Expr**>& vars, const Expr* value);
	std::set<const Var*> find_free_variables(const Expr* expr);
	std::map<std::string, Lambda*> find_bound_variables(Expr* expr);
	std::string fresh_name(const std::string& name);
	Expr* replace_vars(const Context& ctx, const Expr* expr);

	static Expr* eval(int& step, int print_flags, Expr** whole, Expr** expr);

	bool alpha_equivalent(const Expr* a, const Expr* b);
	Expr* alpha_conversion(Expr* lam, std::string var, const std::string& fresh);
	Expr* beta_reduction(int& step, int print_flags, Expr** whole, Apply* app, Expr** parent);

	template <typename Fn, typename PrinterFn, typename... Args>
	static void do_transform(int print_flags, Fn&& fn, PrinterFn&& printer, Args&&... args)
	{
		if((print_flags & FLAG_FULL_TRACE) && (print_flags & FLAG_TRACE))
		{
			auto [ a, b ] = printer(static_cast<Args&&>(args)...);
			zpr::println("     {}", a);
			zpr::println("     {}", b);
		}

		fn();

		if((print_flags & FLAG_FULL_TRACE) && (print_flags & FLAG_TRACE))
		{
			auto [ a, b ] = printer(static_cast<Args&&>(args)...);
			zpr::println("   > {}", a);
			zpr::println("     {}\n", b);
		}
	}

	template <typename... Args>
	static void print_trace(int print_flags, const char* fmt, Args&&... args)
	{
		if(print_flags & FLAG_TRACE)
			zpr::println(fmt, static_cast<Args&&>(args)...);
	}

	Expr* evaluate(Context& ctx, const Expr* expr, int print_flags)
	{
		// lets are not an expression that we can evaluate, so don't
		// even put it through the loop.
		if(auto let = dynamic_cast<const Let*>(expr))
		{
			bool exists = ctx.vars.find(let->name) != ctx.vars.end();
			ctx.vars[let->name] = let->value->clone();

			print_trace(print_flags, "{}*.{} {}{}defined:{} {}{}{}", BLACK_BOLD, COLOUR_RESET, BLUE_BOLD,
				exists ? "re" : "", COLOUR_RESET, BLACK_BOLD, let->name, COLOUR_RESET);

			// we have to return something non-null, so...
			return let->value;
		}

		// this also makes a clone.
		auto copy = replace_vars(ctx, expr);
		print_trace(print_flags, "{}0.{} {}", BLACK_BOLD, COLOUR_RESET, lc::print(copy, print_flags));

		int step = 1;
		while(copy)
		{
			auto next = eval(step, print_flags, &copy, &copy);
			if(next == nullptr)
				break;

			copy = next;
		}

		print_trace(print_flags, "{}*.{} {}done.{}", BLACK_BOLD, COLOUR_RESET, BLUE_BOLD, COLOUR_RESET);
		return copy;
	}

	static Expr* eval(int& step, int print_flags, Expr** whole, Expr** expr)
	{
		assert(expr);
		if(auto a = dynamic_cast<Apply*>(*expr); a != nullptr)
		{
			return beta_reduction(step, print_flags, whole, a, expr);
		}
		else if(auto l = dynamic_cast<Lambda*>(*expr); l != nullptr)
		{
			auto body = eval(step, print_flags, whole, &l->body);
			if(body != nullptr)
			{
				l->body = body;
				return l;
			}
			else
			{
				return nullptr;
			}
		}
		else
		{
			return nullptr;
		}
	}

	Expr* beta_reduction(int& step, int print_flags, Expr** whole, Apply* app, Expr** parent)
	{
		if(auto func = dynamic_cast<Lambda*>(app->fn); func != nullptr)
		{
			// get the free variables of the argument, and the bound variables of the function
			auto free = find_free_variables(app->arg);
			auto bound = find_bound_variables(func);

			// rename (alpha-convert) the target function (or any part of its body)
			// if there is a name conflict
			for(auto v : free)
			{
				auto f = v->name;
				if(auto it = bound.find(f); it != bound.end())
				{
					print_trace(print_flags, "{}{}.{} {}α-con:{} {}{}{} <- {}", BLACK_BOLD, step++, COLOUR_RESET,
						GREEN, COLOUR_RESET, BLACK_BOLD, f, COLOUR_RESET, fresh_name(f));

					do_transform(print_flags, [&]() {
						alpha_conversion(it->second, f, fresh_name(f));
					}, logAlphaConversion, const_cast<const Expr**>(whole), it->second, print_flags);
				}
			}

			// find the substitutions first so we can highlight them
			auto substs = find_substitutions(&func->body, func->arg);

			print_trace(print_flags, "{}{}.{} {}β-red:{} {}{}{} <- {}", BLACK_BOLD, step++, COLOUR_RESET,
				YELLOW, COLOUR_RESET, BLACK_BOLD, func->arg, COLOUR_RESET, lc::print(app->arg, print_flags));

			Lambda* ret = nullptr;
			do_transform(print_flags, [&]() {
				ret = substitute(func, substs, app->arg);
				*parent = ret->body;
			}, logBetaReduction, const_cast<const Expr**>(whole), func, app->arg, substs, print_flags);

			return ret->body;
		}
		else if(auto a = dynamic_cast<Apply*>(app->fn); a != nullptr)
		{
			if(auto red = beta_reduction(step, print_flags, whole, a, &app->fn); red != nullptr)
			{
				app->fn = red;
				return app;
			}
		}
		else if(auto a = dynamic_cast<Apply*>(app->arg); a != nullptr)
		{
			if(auto red = beta_reduction(step, print_flags, whole, a, &app->arg); red != nullptr)
			{
				app->arg = red;
				return app;
			}
		}

		return nullptr;
	}

	Expr* alpha_conversion(Expr* e, std::string name, const std::string& fresh)
	{
		if(auto v = dynamic_cast<Var*>(e); v != nullptr)
		{
			if(v->name == name) return new Var(v->loc, fresh);
			else                return v;
		}
		else if(auto a = dynamic_cast<Apply*>(e); a != nullptr)
		{
			a->fn = alpha_conversion(a->fn, name, fresh);
			a->arg = alpha_conversion(a->arg, name, fresh);
			return a;
		}
		else if(auto l = dynamic_cast<Lambda*>(e); l != nullptr)
		{
			// we need to rename the inner lambda to a different name.
			if(l->arg == fresh)
			{
				auto fresher = fresh_name(fresh);
				l->arg = fresher;
				l->body = alpha_conversion(l->body, fresh, fresher);
			}
			else
			{
				if(l->arg == name)
					l->arg = fresh;

				l->body = alpha_conversion(l->body, name, fresh);
			}

			return l;
		}
		else
		{
			abort();
		}
	}
}




















namespace ast
{
	// expressions own their subs.
	Expr::~Expr()     { }
	Var::~Var()       { }
	Let::~Let()       { delete this->value; }
	Apply::~Apply()   { delete this->fn; delete this->arg; }
	Lambda::~Lambda() { delete this->body; }

	Var* Var::clone() const
	{
		return new Var(this->loc, this->name);
	}

	Apply* Apply::clone() const
	{
		return new Apply(this->loc, this->fn->clone(), this->arg->clone());
	}

	Lambda* Lambda::clone() const
	{
		return new Lambda(this->loc, this->argloc, this->arg, this->body->clone());
	}

	Let* Let::clone() const
	{
		return new Let(this->loc, this->name, this->value->clone());
	}
}
