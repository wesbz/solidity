function f() pure {
    assembly "evmasm a b c c" {}
}
// ----
// SyntaxError 4430: (24-52): Unexpected inline assembly dialect flag: "a"
// SyntaxError 4430: (24-52): Unexpected inline assembly dialect flag: "b"
// SyntaxError 4430: (24-52): Unexpected inline assembly dialect flag: "c"
// SyntaxError 4430: (24-52): Unexpected inline assembly dialect flag: "c"
