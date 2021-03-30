## lambda calculus interpreter

### what:
A tiny lambda calculus interpreter written in C++. It supports α-conversion and β-reduction,
as well as precise tracing output of replacements and renames.

### how:
The standard lambda calculus syntax is supported, eg:
```
(λz.(((λx.(λy.x)) z) ((λx.(λy.x)) z)))
```

Note that variable names can be multi-character, so they must be separated with spaces. Syntax sugar
for multi-argument functions is supported:
```
λx y.x == λx.λy.x
```

Haskell-style lambda syntax is also supported, because nobody knows how to type λ:
```
\x y -> x == \x -> \y -> x
```

Let-bindings are also supported:
```
let S = \x y z -> x z (y z)
let K = \x y -> x
let I = \x -> x

S K K == I
```

Right now, the interpreter cannot 'back-substitute' variables, so even when it sees `λx.x`, it doesn't know that
it's equivalent to `I`.


### load
You can load files from the command line simply by passing them as arguments to the interpreter, eg.
```shell
$ build/lc lib/ski.lc
```

You can also use the `:load` directive, either in the REPL or in a file itself (so you can do recursive includes).
Note that include loops are not handled, and you'll crash the interpreter.


### example

Here is some example output:
```
*. parenthesis omission enabled
*. curried abbreviation enabled
*. haskell-style printing enabled
*. loaded 13 lines from 'lib/ski.lc'
λ> S K K
0. (\x y z -> x z (y z)) (\x y -> x) (\x y -> x)
1. β-red: x <- (\x y -> x)
2. β-red: y <- (\x y -> x)
3. β-red: x <- z
4. β-red: y <- (\x y -> x) z
*. done.
(\z -> z)

λ>
```
