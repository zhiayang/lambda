// file.cpp
// Copyright (c) 2021, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "defs.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <filesystem>

namespace std { namespace fs = filesystem; }

static std::vector<zbuf::str_view> split_string(zbuf::str_view view);
static zst::Result<zbuf::str_view, std::string> read_file_raw(zbuf::str_view path);
static zst::Result<std::vector<zbuf::str_view>, std::string> read_file_lines(zbuf::str_view path);

namespace lc
{
	// repl.cpp
	zbuf::str_view trim(zbuf::str_view s);
	void runReplCommand(Context& ctx, zbuf::str_view cmd);
	void parseError(parser::Error e, zbuf::str_view input);

	void loadFile(Context& ctx, zbuf::str_view path)
	{
		if(!std::fs::exists(path.sv()))
			return printError(zpr::sprint("file '{}' does not exist", path));

		auto lines_or_err = read_file_lines(path);
		if(!lines_or_err)
			return printError(lines_or_err.error());

		auto lines = std::move(lines_or_err.unwrap());
		for(size_t i = 0; i < lines.size(); i++)
		{
			// skip comments and empty lines
			auto line = trim(lines[i]);
			if(line.empty() || line[0] == '#')
				continue;

			if(line[0] == ':')
			{
				runReplCommand(ctx, line);
				continue;
			}

			auto expr_or_err = parser::parse(line);
			if(!expr_or_err)
			{
				auto err = expr_or_err.error();
				err.msg = zpr::sprint("(line {}): {}", i + 1, err.msg);
				parseError(err, line);

				zpr::println("{}*.{} {}warning:{} file '{}' not loaded completely ({} line{})",
					BLACK_BOLD, COLOUR_RESET, YELLOW_BOLD, COLOUR_RESET, path, i, i == 1 ? "" : "s");
				return;
			}

			lc::evaluate(ctx, expr_or_err.unwrap(), ctx.flags);
		}

		zpr::println("{}*.{} loaded {} line{} from '{}'", BLACK_BOLD, COLOUR_RESET, lines.size(),
			lines.size() == 1 ? "" : "s", path);
	}























}


static zst::Result<zbuf::str_view, std::string> read_file_raw(zbuf::str_view path)
{
	FILE* f = fopen(path.str().c_str(), "r");
	if(f == nullptr)
		return zst::Err(zpr::sprint("failed to open file '{}': {}", path, strerror(errno)));

	std::string input;
	{
		fseek(f, 0, SEEK_END);

		long fsize = ftell(f);
		fseek(f, 0, SEEK_SET);  //same as rewind(f);

		char* s = new char[fsize + 1];
		fread(s, fsize, 1, f);
		fclose(f);
		s[fsize] = 0;

		size_t n = fsize - 1;
		while(n > 0 && s[n] == '\n')
			n--;

		return zst::Ok(zbuf::str_view(s, n + 1));
	}
}

static std::vector<zbuf::str_view> split_string(zbuf::str_view view)
{
	std::vector<zbuf::str_view> ret;

	while(!view.empty())
	{
		size_t ln = view.find('\n');

		if(ln != std::string_view::npos)
		{
			ret.emplace_back(view.data(), ln);
			view.remove_prefix(ln + 1);
		}
		else
		{
			break;
		}
	}

	// account for the case when there's no trailing newline, and we still have some stuff stuck in the view.
	if(!view.empty())
		ret.emplace_back(view.data(), view.length());

	return ret;
}

static zst::Result<std::vector<zbuf::str_view>, std::string> read_file_lines(zbuf::str_view path)
{
	auto file = read_file_raw(path);
	if(!file) return zst::Err(file.error());

	return zst::Ok(split_string(file.unwrap()));
}
