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

If the `FLAG_VAR_REPLACEMENT` flag (toggle with `:v`) is used, the interpreter will attempt to back-substitute the
end result of an evaluation by using alpha-equivalence; for example, when doing `S K K`, instead of showing `λx.x`,
it will show `I` if an alpha-equivalent definition of `I` is available.


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


### repl commands

| command       | function                                                  |
|---------------|-----------------------------------------------------------|
| `:q`          | quit the repl                                             |
| `:t`          | enable (basic) tracing (shows α and β steps)              |
| `:ft`         | enable full tracing (detailed substitution)               |
| `:v`          | enable back-substitution for the result                   |
| `:p`          | enable omitting unambiguous parentheses when printing     |
| `:c`          | enable shorthand notation when printing curried functions |
| `:h`          | enable haskell-style notation when printing               |
| `:load`       | load a file (eg. `:load foo.lc`) and add it to the context|
