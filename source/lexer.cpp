// lexer.cpp
// Copyright (c) 2021, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "defs.h"
#include "utf8proc/utf8proc.h"

namespace parser
{
	using zst::Ok;
	using zst::Err;

	static size_t is_valid_identifier(zbuf::str_view str)
	{
		auto k = unicode::is_letter(str);
		if(k > 0) return k;

		k = unicode::is_digit(str);
		if(k > 0) return k;

		k = unicode::is_category(str, {
			UTF8PROC_CATEGORY_MN, UTF8PROC_CATEGORY_MC, UTF8PROC_CATEGORY_ME,
			UTF8PROC_CATEGORY_PC
		});
		if(k > 0) return k;

		return 0;
	}

	using TT = TokenType;

	static zst::Result<Token, Error> lex_one_token(zbuf::str_view& src, size_t& idx, TT prevType)
	{
		// skip all whitespace.
		size_t k = 0;
		while(src.size() > 0 && (k = unicode::is_category(src, {
			UTF8PROC_CATEGORY_ZS, UTF8PROC_CATEGORY_ZL, UTF8PROC_CATEGORY_ZP
		}), k > 0))
		{
			src.remove_prefix(k);
			idx += k;
		}

		if(src.empty())
			return Ok(Token(TT::EndOfFile, Location { idx, 0 }, ""));

		auto take_and_return_taken = [](zbuf::str_view& sv, size_t n) -> zbuf::str_view {
			auto ret = sv.take(n);
			sv.remove_prefix(n);
			return ret;
		};

	#define MATCH_MULTICHAR_TOKEN(match, tok) (src.find(match) == 0) {              \
		auto x = strlen(match);                                                     \
		return Ok<Token>(tok, Location { idx, x }, take_and_return_taken(src, x));  \
	}

		if      MATCH_MULTICHAR_TOKEN("->", TT::RightArrow)
		else if MATCH_MULTICHAR_TOKEN("λ", TT::Lambda)
		else if(is_valid_identifier(src) > 0)
		{
			size_t identLength = is_valid_identifier(src);
			auto tmp = src.drop(identLength);

			size_t k = 0;
			while((k = is_valid_identifier(tmp)), k > 0)
				tmp.remove_prefix(k), identLength += k;

			auto text = src.take(identLength);
			auto type = TT::Identifier;

			if(text == "let")
				type = TT::Let;

			else if(text == "in")
				type = TT::In;

			auto ret = Token(type, Location { idx, text.size() }, text);
			src.remove_prefix(identLength);
			return Ok(ret);
		}
		else
		{
			Token ret;
			auto loc = Location { idx, 1 };

			switch(src[0])
			{
				case '(': ret = Token(TT::LParen, loc, src.take(1));    break;
				case ')': ret = Token(TT::RParen, loc, src.take(1));    break;
				case '.': ret = Token(TT::Period, loc, src.take(1));    break;
				case '$': ret = Token(TT::Dollar, loc, src.take(1));    break;
				case '=': ret = Token(TT::Equal, loc, src.take(1));     break;
				case '\\': ret = Token(TT::Lambda, loc, src.take(1));   break;

				default: {
					auto sz = unicode::get_codepoint_length(src);
					return Err(Error {
						zpr::sprint("invalid token '{}'", src.take(sz)),
						Location { idx, 1 }
					});
				}
			}

			src.remove_prefix(1);
			return Ok(ret);
		}
	}


	zst::Result<std::vector<Token>, Error> lex(zbuf::str_view src)
	{
		Token tok = { };
		std::vector<Token> ret;

		size_t idx = 0;
		while(true)
		{
			auto r = lex_one_token(src, idx, tok.type);
			if(!r) return Err(r.error());

			tok = std::move(r.unwrap());
			if(tok == TT::EndOfFile)
				break;

			ret.push_back(tok);
			idx += tok.text.size();
		}

		return Ok(ret);
	}
}





namespace unicode
{
	size_t is_category(zbuf::str_view str, const std::initializer_list<int>& categories)
	{
		utf8proc_int32_t cp = { };
		auto sz = utf8proc_iterate((const uint8_t*) str.data(), str.size(), &cp);
		if(cp == -1)
			return 0;

		auto cat = utf8proc_category(cp);
		for(auto c : categories)
			if(cat == c)
				return sz;

		return 0;
	}

	size_t get_codepoint_length(zbuf::str_view str)
	{
		utf8proc_int32_t cp = { };
		auto sz = utf8proc_iterate((const uint8_t*) str.data(), str.size(), &cp);
		if(cp == -1)
			return 1;

		return sz;
	}

	size_t is_letter(zbuf::str_view str)
	{
		return is_category(str, {
			UTF8PROC_CATEGORY_LU, UTF8PROC_CATEGORY_LL, UTF8PROC_CATEGORY_LT,
			UTF8PROC_CATEGORY_LM, UTF8PROC_CATEGORY_LO
		});
	}

	size_t is_digit(zbuf::str_view str)
	{
		return is_category(str, {
			UTF8PROC_CATEGORY_ND
		});
	}
}
