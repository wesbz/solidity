function f() pure {
    assembly "evmasm memory-safe  memory-safe" {}
}
// ----
// SyntaxError 7026: (24-69): Inline assembly dialect marked memory-safe multiple times.
