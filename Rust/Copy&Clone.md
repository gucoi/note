## Copy 和 Clone 的区别：

- Copy 是一个标记 trait，表示类型可以通过简单的位复制（memcpy）来复制。
- Clone 是一个需要实现的 trait，通常用于深复制。
- Copy 类型必须实现 Clone，但 Clone 类型不一定是 Copy。
- Copy 类型的复制是隐式的，而 Clone 需要显式调用 clone() 方法。