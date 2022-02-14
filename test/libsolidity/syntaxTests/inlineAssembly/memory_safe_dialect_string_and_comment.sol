function f() pure {
    /// @solidity memory-safe-assembly
    assembly "evmasm memory-safe" {
    }
}
// ----
// Warning 8544: (63-100): Inline assembly marked as memory safe using a docstring comment and the dialect string. If you are not concerned with backwards compatibility, prefer using the dialect string only.
