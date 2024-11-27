## Sized 的含义：

- Sized 表示类型在编译时有已知的固定大小。
- 大多数类型默认都是 Sized 的。
- 非 Sized 类型（如动态大小类型 [T] 或 dyn Trait）必须通过引用或指针使用。

## `{struct A;std::mem::size_of::<A>()} `的值：

- 这个表达式的值是 0。
- 在 Rust 中，没有任何字段的结构体（称为零大小类型或 ZST）的大小为 0 字节