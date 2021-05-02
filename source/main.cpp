// main.cpp
// Copyright (c) 2021, zhiayang
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "ast.h"

namespace lc
{
	void print_error(parser::Error e, zbuf::str_view input);
}

int main(int argc, char** argv)
{
	lc::Context ctx {};

	// TODO: support flags or something
	for(int i = 1; i < argc; i++)
	{
		auto f = argv[i];
		lc::loadFile(ctx, zbuf::str_view(f, strlen(f)));
	}

	lc::repl(ctx);

#if 0
	// SKK = I
	// auto input = "(\x -> \y -> \z -> x z (y z)) (\x y -> x) (\x y -> x)";
	// auto input = "(λz.(((λx.(λy.x)) z) ((λx.(λy.x)) z)))";
	// auto input = "((\x y z -> x z (y z)) ((\x y -> x) (\x y z -> x z (y z)))) (\x y -> x)";
	// auto input = "(λx -> y)((λx -> x x) (λx -> x x))";
#endif
}











/*
B   = \x y z -> x (y z)
B   = S (KS) K
	= ((\x y z -> x z (y z)) ((\x y -> x) (\x y z -> x z (y z)))) (\x y -> x)
	= ((\x y z -> x z (y z)) (\k -> (\x y z -> x z (y z)))) (\x y -> x)
	= ((\a b c -> a c (b c)) (\k -> (\x y z -> x z (y z)))) (\x y -> x)
	= ((\b c -> (\k -> (\x y z -> x z (y z))) c (b c))) (\x y -> x)
	= ((\b c -> (\x y z -> x z (y z)) (b c))) (\x y -> x)
	= ((\b c -> (\p q r -> p r (q r)) (b c))) (\x y -> x)
	= ((\b c -> (\q r -> ((b c) r) (q r)))) (\x y -> x)
	= ((\c -> (\q r -> (((\x y -> x) c) r) (q r))))
	= (\c -> (\q r -> c (q r)))
	= (\c -> (\q -> \r -> c (q r)))
	= (\c -> \q -> \r -> c (q r))
	= (\c q r -> c (q r))
	= (\x y z -> x (y z))
	= B.
Qed.
*/
