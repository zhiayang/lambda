// ast.h
// Copyright (c) 2021, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once

#include <unordered_map>

#include "defs.h"
#include "result.h"

namespace ast
{
	struct Expr;
}

namespace parser
{
	struct Location
	{
		size_t begin;
		size_t length;
	};

	struct Error
	{
		std::string msg;
		Location loc;
	};

	enum class TokenType
	{
		Invalid,

		LParen,
		RParen,
		RightArrow,
		Period,
		Lambda,
		Equal,
		Dollar,

		Let,
		In,

		Identifier,
		EndOfFile,
	};

	struct Token
	{
		Token() { }
		Token(TokenType t, Location loc, zbuf::str_view s) : loc(loc), text(s), type(t) { }

		Location loc;
		zbuf::str_view text;
		TokenType type = TokenType::Invalid;

		operator TokenType() const { return this->type; }
	};

	zst::Result<ast::Expr*, Error> parse(zbuf::str_view input);
	zst::Result<std::vector<Token>, Error> lex(zbuf::str_view input);
}

namespace ast
{
	constexpr int EXPR_VAR          = 1;
	constexpr int EXPR_APPLY        = 2;
	constexpr int EXPR_LAMBDA       = 3;
	constexpr int EXPR_LET          = 4;

	struct Expr
	{
		Expr(int t, parser::Location l) : type(t), loc(l) { }
		virtual ~Expr();

		virtual Expr* clone() const = 0;
		// virtual Expr* evaluate(const std::unordered_map<std::string, bool>& syms) const = 0;

		const int type;
		parser::Location loc;
	};

	struct Var : Expr
	{
		Var(parser::Location loc, std::string s) : Expr(TYPE, loc), name(std::move(s)) { }
		virtual ~Var() override;
		virtual Var* clone() const override;
		// virtual Expr* evaluate(const std::unordered_map<std::string, bool>& syms) const override;

		static constexpr int TYPE = EXPR_VAR;

		std::string name;
	};

	struct Apply : Expr
	{
		Apply(parser::Location loc, Expr* fn, Expr* arg) : Expr(TYPE, loc), fn(fn), arg(arg) { }
		virtual ~Apply() override;
		virtual Apply* clone() const override;
		// virtual Expr* evaluate(const std::unordered_map<std::string, bool>& syms) const override;

		static constexpr int TYPE = EXPR_APPLY;

		Expr* fn = 0;
		Expr* arg = 0;
	};

	struct Lambda : Expr
	{
		Lambda(parser::Location loc, parser::Location argloc, std::string arg, Expr* body) : Expr(TYPE, loc),
			argloc(argloc), arg(std::move(arg)), body(body) { }

		virtual ~Lambda() override;
		virtual Lambda* clone() const override;

		static constexpr int TYPE = EXPR_LAMBDA;

		parser::Location argloc;
		std::string arg;
		Expr* body = 0;
	};

	// it's not really an expression, but whatever.
	struct Let : Expr
	{
		Let(parser::Location loc, std::string name, Expr* value) : Expr(TYPE, loc),
			name(std::move(name)), value(value) { }

		virtual ~Let() override;
		virtual Let* clone() const override;

		static constexpr int TYPE = EXPR_LET;

		std::string name;
		Expr* value = 0;
	};
}
