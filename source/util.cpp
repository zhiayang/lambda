// util.cpp
// Copyright (c) 2021, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "defs.h"

#include <set>
#include <map>
#include <climits>

namespace lc
{
	using namespace ast;

	// eval.cpp
	Expr* alpha_conversion(Expr* e, std::string name, const std::string& fresh);

	// below
	std::set<const Var*> find_free_variables(const Expr* expr);

	// bool is true if we replaced something.
	static std::pair<Expr*, bool> replace_vars_once(const Context& ctx, const std::set<const Var*>& free_vars, const Expr* expr)
	{
		if(auto v = dynamic_cast<const Var*>(expr); v != nullptr)
		{
			if(free_vars.find(v) != free_vars.end())
			{
				if(auto it = ctx.vars.find(v->name); it != ctx.vars.end())
					return { it->second->clone(), true };
			}

			return { v->clone(), false };
		}
		else if(auto a = dynamic_cast<const Apply*>(expr); a != nullptr)
		{
			auto x = replace_vars_once(ctx, free_vars, a->fn);
			auto y = replace_vars_once(ctx, free_vars, a->arg);
			return { new Apply(a->loc, x.first, y.first), x.second || y.second };
		}
		else if(auto l = dynamic_cast<const Lambda*>(expr); l != nullptr)
		{
			auto body = replace_vars_once(ctx, free_vars, l->body);
			return { new Lambda(l->loc, l->argloc, l->arg, body.first), body.second };
		}
		else
		{
			abort();
		}
	}

	Expr* replace_vars(const Context& ctx, const Expr* expr)
	{
		const Expr* ret = expr;
		while(true)
		{
			auto [ next, changed ] = replace_vars_once(ctx, find_free_variables(ret), ret);
			if(!changed)
			{
				return next;
			}
			else
			{
				if(ret != expr)
					delete ret;

				ret = next;
			}
		}
	}

	std::vector<Expr**> find_substitutions(Expr** expr, const std::string& var)
	{
		if(auto v = dynamic_cast<Var*>(*expr); v != nullptr)
		{
			if(v->name == var)  return { expr };
			else                return { };
		}
		else if(auto a = dynamic_cast<Apply*>(*expr); a != nullptr)
		{
			auto ret = find_substitutions(&a->fn, var);
			auto tmp = find_substitutions(&a->arg, var);
			ret.insert(ret.end(), tmp.begin(), tmp.end());
			return ret;
		}
		else if(auto l = dynamic_cast<Lambda*>(*expr); l != nullptr)
		{
			// if the lambda here 're-binds' the name, then stop.
			if(l->arg != var)   return find_substitutions(&l->body, var);
			else                return { };
		}
		else
		{
			abort();
		}
	}

	Lambda* substitute(Lambda* expr, const std::vector<Expr**>& vars, const Expr* value)
	{
		for(auto v : vars)
			*v = value->clone();

		return expr;
	}

	template <bool Bound, int MaxDepth = INT_MAX, typename Retty = std::conditional_t<Bound,
		std::map<std::string, Lambda*>,
		std::set<const Var*>
	>>
	static Retty _find_variables(std::map<std::string, Lambda*> seen, const Expr* expr, int depth = 0)
	{
		if(auto v = dynamic_cast<const Var*>(expr); v != nullptr)
		{
			if constexpr (Bound)
			{
				if(auto it = seen.find(v->name); it != seen.end())
					return { *it };

				return { };
			}
			else
			{
				if(seen.find(v->name) == seen.end())
					return { v };

				return { };
			}
		}
		else if(auto a = dynamic_cast<const Apply*>(expr); a != nullptr)
		{
			Retty ret;
			auto x = _find_variables<Bound>(seen, a->fn);
			auto y = _find_variables<Bound>(seen, a->arg);

			ret.insert(x.begin(), x.end());
			ret.insert(y.begin(), y.end());
			return ret;
		}
		else if(auto l = dynamic_cast<const Lambda*>(expr); l != nullptr)
		{
			if(depth < MaxDepth)
			{
				seen.insert({ l->arg, const_cast<Lambda*>(l) });
				return _find_variables<Bound>(seen, l->body, 1 + depth);
			}
			else
			{
				return { };
			}
		}
		else
		{
			abort();
		}
	}

	std::set<const Var*> find_free_variables(const Expr* expr)
	{
		return _find_variables<false>({ }, const_cast<Expr*>(expr));
	}

	std::map<std::string, Lambda*> find_bound_variables(Expr* expr)
	{
		return _find_variables<true>({ }, expr);
	}

	std::string fresh_name(const std::string& name)
	{
		return name + "'";
	}





	template <typename Container, typename Fn>
	static auto map(Container&& c, Fn&& func) -> std::set<decltype(
		func(std::declval<typename std::decay_t<Container>::value_type>())
	)>
	{
		std::set<decltype(func(std::declval<typename std::decay_t<Container>::value_type>()))> ret;
		std::transform(c.begin(), c.end(), std::inserter(ret, ret.begin()), func);
		return ret;
	}


	struct CheckState
	{
		std::map<std::string, int> var_depths;
		std::map<std::string, Lambda*> bindings;
	};

	static bool alpha_equivalent(const Expr* a, const Expr* b, int cur_depth,
		const CheckState& sta, const CheckState& stb)
	{
		// first just even check the type.
		if(a->type != b->type)
			return false;

		// since we are traversing the entire expression from root to leaf,
		// we only really need to figure out the free variables at the current level.
		auto free_a = _find_variables</* bound: */ false, /* max_depth: */ 1>(sta.bindings, a);
		auto free_b = _find_variables</* bound: */ false, /* max_depth: */ 1>(stb.bindings, b);

		// convert them to names
		auto foo = [](const Var* v) { return v->name; };
		auto free_a_names = map(free_a, foo);
		auto free_b_names = map(free_b, foo);

		// free variables are not the same -- don't.
		if(free_a_names != free_b_names)
			return false;

		if(auto v1 = dynamic_cast<const Var*>(a), v2 = dynamic_cast<const Var*>(b); v1 && v2)
		{
			auto ia = sta.var_depths.find(v1->name);
			auto ib = stb.var_depths.find(v2->name);

			if(ia != sta.var_depths.end() && ib != stb.var_depths.end())
				return ia->second == ib->second;

			return false;
		}
		else if(auto a1 = dynamic_cast<const Apply*>(a), a2 = dynamic_cast<const Apply*>(b); a1 && a2)
		{
			return alpha_equivalent(a1->fn, a2->fn, cur_depth, sta, stb)
				&& alpha_equivalent(a1->arg, a2->arg, cur_depth, sta, stb);
		}
		else if(auto l1 = dynamic_cast<const Lambda*>(a), l2 = dynamic_cast<const Lambda*>(b); l1 && l2)
		{
			auto sta1 = sta;
			auto stb1 = stb;

			sta1.var_depths[l1->arg] = cur_depth;
			sta1.bindings[l1->arg] = const_cast<Lambda*>(l1);

			stb1.var_depths[l2->arg] = cur_depth;
			stb1.bindings[l2->arg] = const_cast<Lambda*>(l2);

			return alpha_equivalent(l1->body, l2->body, cur_depth + 1, sta1, stb1);
		}
		else
		{
			abort();
		}


		return false;
	}

	bool alpha_equivalent(Context& ctx, const Expr* a, const Expr* b)
	{
		auto bb = lc::evaluate(ctx, b, /* flags: */ 0);

		bool ret = alpha_equivalent(a, bb, 0, { }, { });
		delete bb;

		return ret;
	}
}
