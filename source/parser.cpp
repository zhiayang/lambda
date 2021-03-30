// parser.cpp
// Copyright (c) 2021, zhiayang
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "ast.h"

#include <tuple>
#include <optional>

namespace parser
{
	using zst::Ok;
	using zst::Err;

	using TT = TokenType;
	using ResultTy = zst::Result<ast::Expr*, Error>;

	// it's not like i stole this from my own project or anything, b-b-baka
	namespace {
		template <typename> struct is_result : std::false_type { };
		template <typename T, typename E> struct is_result<zst::Result<T, E>> : std::true_type { };

		template <typename T> struct remove_array { using type = T; };
		template <typename T, size_t N> struct remove_array<const T(&)[N]> { using type = const T*; };
		template <typename T, size_t N> struct remove_array<T(&)[N]> { using type = T*; };

		// uwu
		template <typename... Ts>
		using concatenator = decltype(std::tuple_cat(std::declval<Ts>()...));

		[[maybe_unused]] std::tuple<> __unwrap() { return { }; }

		template <typename A, typename = std::enable_if_t<is_result<A>::value>>
		std::tuple<std::optional<typename A::value_type>> __unwrap(const A& a)
		{
			return std::make_tuple(a
				? std::optional<typename A::value_type>(a.unwrap())
				: std::nullopt
			);
		}

		template <typename A, typename = std::enable_if_t<!std::is_array_v<A> && !is_result<A>::value>>
		std::tuple<std::optional<A>> __unwrap(const A& a) { return std::make_tuple(std::optional(a)); }

		template <typename A, typename... As>
		auto __unwrap(const A& a, As&&... as)
		{
			auto x = __unwrap(a);
			auto xs = std::tuple_cat(__unwrap(as)...);

			return std::tuple_cat(x, xs);
		}

		template <typename A, size_t... Is, typename... As>
		std::tuple<As...> __drop_one_impl(std::tuple<A, As...> tup, std::index_sequence<Is...> seq)
		{
			return std::make_tuple(std::get<(1+Is)>(tup)...);
		}

		template <typename A, typename... As>
		std::tuple<As...> __drop_one(std::tuple<A, As...> tup)
		{
			return __drop_one_impl(tup, std::make_index_sequence<sizeof...(As)>());
		}

		[[maybe_unused]] std::optional<std::tuple<>> __transpose()
		{
			return std::make_tuple();
		}

		template <typename A>
		[[maybe_unused]] std::optional<std::tuple<A>> __transpose(std::tuple<std::optional<A>> tup)
		{
			auto elm = std::get<0>(tup);
			if(!elm.has_value())
				return std::nullopt;

			return elm.value();
		}

		template <typename A, typename... As, typename = std::enable_if_t<sizeof...(As) != 0>>
		[[maybe_unused]] std::optional<std::tuple<A, As...>> __transpose(std::tuple<std::optional<A>, std::optional<As>...> tup)
		{
			auto elm = std::get<0>(tup);
			if(!elm.has_value())
				return std::nullopt;

			auto next = __transpose(__drop_one(tup));
			if(!next.has_value())
				return std::nullopt;

			return std::tuple_cat(std::make_tuple(elm.value()), next.value());
		}



		[[maybe_unused]] std::tuple<> __get_error() { return { }; }

		template <typename A, typename = std::enable_if_t<is_result<A>::value>>
		[[maybe_unused]] std::tuple<typename A::error_type> __get_error(const A& a)
		{
			if(a) return typename A::error_type();
			return a.error();
		}

		template <typename A, typename = std::enable_if_t<!is_result<A>::value>>
		[[maybe_unused]] std::tuple<> __get_error(const A& a) { return std::tuple<>(); }


		template <typename A, typename... As>
		auto __get_error(const A& a, As&&... as)
		{
			auto x = __get_error(std::forward<const A&>(a));
			auto xs = std::tuple_cat(__get_error(as)...);

			return std::tuple_cat(x, xs);
		}

		// this is named 'concat', but just take the first one.
		template <typename... Err>
		Error __concat_errors(Err&&... errs)
		{
			Error ret;
			([&ret](auto e) { if(ret.msg.empty()) ret = e; }(errs), ...);

			return ret;
		}
	}


	template <typename Ast, typename... Args>
	static ResultTy makeAST(Args&&... args)
	{
		auto opts = __unwrap(std::forward<Args&&>(args)...);
		auto opt = __transpose(opts);
		if(opt.has_value())
		{
			auto foozle = [](auto... xs) -> Ast* {
				return new Ast(xs...);
			};

			return Ok<Ast*>(std::apply(foozle, opt.value()));
		}

		return Err(std::apply([](auto&&... xs) { return __concat_errors(xs...); }, __get_error(std::forward<Args&&>(args)...)));
	}

	struct State
	{
		State(std::vector<Token> ts) : tokens(std::move(ts)) { }

		bool match(TT t)
		{
			if(tokens.size() == 0 || tokens.front() != t)
				return false;

			this->pop();
			return true;
		}

		const Token& peek(size_t n = 0)
		{
			return tokens.size() <= n ? eof : tokens[n];
		}

		Token pop()
		{
			if(!tokens.empty())
			{
				auto ret = this->peek();
				tokens.erase(tokens.begin());
				return ret;
			}
			else
			{
				return eof;
			}
		}

		bool empty()
		{
			return tokens.empty();
		}

		std::vector<Token> tokens;
		Token eof { TT::EndOfFile, Location { 0, 0 }, "" };
	};

	static ResultTy make_error(Location loc, const std::string& msg)
	{
		return Err(Error { .msg = msg, .loc = loc });
	}

	static ResultTy parseLet(State& st);
	static ResultTy parseStmt(State& st);
	static ResultTy parseExpr(State& st);
	static ResultTy parseUnary(State& st);
	static ResultTy parseLambda(State& st);
	static ResultTy parseParenthesised(State& st);
	static ResultTy parseApply(State& st, ResultTy lhs);

	ResultTy parse(zbuf::str_view str)
	{
		if(str.empty())
			return make_error({ }, "empty input");

		auto tokens = lex(str);
		if(!tokens) return Err(tokens.error());

		auto st = State(std::move(*tokens));

		auto ret = parseStmt(st);
		if(!ret) return ret;

		if(!st.empty())
			return make_error(st.peek().loc, zpr::sprint("junk at end of expression: '{}'", st.peek().text));

		else
			return Ok(*ret);
	}

	static ResultTy parseStmt(State& st)
	{
		if(st.peek() == TT::Let)
		{
			return parseLet(st);
		}
		else
		{
			return parseExpr(st);
		}
	}

	static ResultTy parseExpr(State& st)
	{
		auto left = parseUnary(st);
		if(!left) return left;

		return parseApply(st, left);
	}

	static ResultTy parseUnary(State& st)
	{
		if(st.peek() == TT::LParen)
		{
			return parseParenthesised(st);
		}
		else if(st.peek() == TT::Identifier)
		{
			return makeAST<ast::Var>(st.peek().loc, st.pop().text.str());
		}
		else if(st.peek() == TT::Lambda)
		{
			return parseLambda(st);
		}

		return make_error({ }, st.empty()
			? zpr::sprint("unexpected end of input")
			: zpr::sprint("unexpected token '{}'", st.peek().text)
		);
	}

	static ResultTy parseParenthesised(State& st)
	{
		Location open;
		if(auto x = st.pop(); x != TT::LParen)
			return make_error(x.loc, zpr::sprint("expected '('"));
		else
			open = x.loc;

		auto expr = parseExpr(st);
		if(!expr) return expr;

		if(st.pop() != TT::RParen)
			return make_error(open, zpr::sprint("expected ')' to match this '('"));

		return expr;
	}

	static ResultTy parseLambda(State& st)
	{
		// note that the backslash (or the λ) is optional -- we do the syntax desugaring
		// here. if we see \x y z . a b c, we desugar to \x.\y.\z.(a b c). i don't want
		// to deal with 'inserting tokens' back into the token stream, so this is the way.

		auto l = st.peek().loc;
		if(st.peek() == TT::Lambda)
			st.pop();

		if(auto x = st.peek(); x != TT::Identifier)
			return make_error(x.loc, zpr::sprint("expected identifier, found '{}'", x.text));

		auto arg = st.pop();

		if(st.peek() == TT::RightArrow || st.peek() == TT::Period)
		{
			st.pop();
			auto body = parseExpr(st);
			if(!body) return body;

			return makeAST<ast::Lambda>(Location {
				.begin = l.begin,
				.length = (*body)->loc.begin + (*body)->loc.length - l.begin
			}, arg.loc, arg.text.str(), body);
		}
		else if(st.peek() == TT::Identifier)
		{
			// we have no \ or λ but that's ok.
			auto sub = parseLambda(st);
			if(!sub) return sub;

			return makeAST<ast::Lambda>(Location {
				.begin = l.begin,
				.length = (*sub)->loc.begin + (*sub)->loc.length - l.begin
			}, arg.loc, arg.text.str(), sub);
		}
		else
		{
			auto x = st.peek();
			return make_error(x.loc, zpr::sprint("expected '.' or '->' or identifier; found '{}'", x.text));
		}
	}

	static ResultTy parseApply(State& st, ResultTy lhs)
	{
		if(!lhs || st.empty())
			return lhs;

		while(true)
		{
			// if it's time, then stop.
			if(st.peek() == TT::RParen || st.peek() == TT::EndOfFile)
				return lhs;

			auto rhs = parseUnary(st);
			if(!rhs) return rhs;

			lhs = makeAST<ast::Apply>((*lhs)->loc, *lhs, *rhs);
		}
	}

	static ResultTy parseLet(State& st)
	{
		assert(st.peek() == TT::Let);
		st.pop();

		auto name = st.pop();
		if(name != TT::Identifier)
			return make_error(name.loc, zpr::sprint("expected identifier for 'let', found '{}'", name.text));

		if(auto x = st.pop(); x != TT::Equal)
			return make_error(x.loc, zpr::sprint("expected '=', found '{}'", x.text));

		auto value = parseExpr(st);
		if(!value) return value;

		return makeAST<ast::Let>(name.loc, name.text.str(), value);
	}
}
