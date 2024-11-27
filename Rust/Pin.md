- 防止自引用结构体在内存中移动，这可能会导致悬垂指针。
- 在异步编程中，确保 Future 不会在执行过程中被移动，因为 Future 可能包含对自身状态的引用。
## `Pin<T>`
`Pin<T>` 用于确保一个值在内存中的位置不会被移动。这对于自引用结构体和异步编程特别重要。
## 创建自引用结构体

### PhantomData：
- 标记类型参数的使用，即使它们没有在结构体字段中直接使用。
- 表示类型具有特定的所有权或生命周期关系。
- 在实现 Drop trait 时，表示类型负责释放一些未直接存储的资源。
- 避免编译器的未使用类型参数警告。

### 错误样例

 ```rust
struct SelfRef { 
	value: String, 
	pointer: *const String, 
	// 指向 value 的指针 
} 
impl SelfRef { 
	fn new(txt: &str) -> Self { 
		let mut s = SelfRef { 
			value: String::from(txt), 
			pointer: std::ptr::null(), 
		}; 
		// 存储指向 value 的指针 
		s.pointer = &s.value; 
		s 
	} 
} 
fn main() { 
	let s1 = SelfRef::new("hello"); 
	let mut s2 = SelfRef::new("world");  // 这里会导致 move，使得原有的指针失效 
	std::mem::swap(&mut s2, &mut s1); 
	// 这会导致未定义行为 
	// 现在 s1 和 s2 中的 pointer 指向错误的位置 
	// 因为数据被移动了，但指针仍指向原来的位置 
}
 ```



### 正确样例

```rust
use std::pin::Pin;
use std::marker::PhantomPinned;

struct SelfReferential {
    data: String,
    ptr: *const String,
    _marker: PhantomPinned,
}

impl SelfReferential {
    fn new(data: String) -> Pin<Box<Self>> {
        let mut boxed = Box::pin(SelfReferential {
            data,
            ptr: std::ptr::null(),
            _marker: PhantomPinned,
        });
        let self_ptr: *const String = &boxed.data;
        // 这是安全的，因为我们使用了 Pin
        unsafe {
            let mut_ref: Pin<&mut Self> = Pin::as_mut(&mut boxed);
            Pin::get_unchecked_mut(mut_ref).ptr = self_ptr;
        }
        boxed
    }
}
```

## Future 中 Pin 的用法

### 错误
```rust
use std::future::Future;
use std::pin::Pin;
use std::task::{Context, Poll};

struct MyFuture {
    name: String,
    // 模拟一个自引用情况
    name_ref: *const String,
}

impl Future for MyFuture {
    type Output = String;

    fn poll(self: Pin<&mut Self>, _cx: &mut Context<'_>) -> Poll<Self::Output> {
        // 错误：这里我们没有正确处理 Pin
        let this = self.get_mut(); // 错误：破坏了 Pin 的保证
        this.name_ref = &this.name; // 危险：可能导致悬垂指针
        Poll::Ready(this.name.clone())
    }
}

async fn wrong_usage() {
    let mut future = MyFuture {
        name: "test".to_string(),
        name_ref: std::ptr::null(),
    };
    
    // 错误：Future 可能被移动，导致自引用失效
    let result = future.await; // 这可能导致未定义行为
}
```

### 正确的样例

```rust
use std::future::Future;
use std::pin::Pin;
use std::task::{Context, Poll};
use std::marker::PhantomPinned;

struct MyPinnedFuture {
    name: String,
    name_ref: *const String,
    _marker: PhantomPinned,
}

impl MyPinnedFuture {
    fn new(name: String) -> Pin<Box<Self>> {
        let mut boxed = Box::pin(Self {
            name,
            name_ref: std::ptr::null(),
            _marker: PhantomPinned,
        });
        
        // 安全地设置自引用
        let self_ptr: *const String = &boxed.name;
        unsafe {
            let mut_ref: Pin<&mut Self> = Pin::as_mut(&mut boxed);
            Pin::get_unchecked_mut(mut_ref).name_ref = self_ptr;
        }
        
        boxed
    }
}

impl Future for MyPinnedFuture {
    type Output = String;

    fn poll(self: Pin<&mut Self>, _cx: &mut Context<'_>) -> Poll<Self::Output> {
        // 安全：我们知道数据已经被固定，不会移动
        let this = unsafe { self.get_unchecked_mut() };
        Poll::Ready(this.name.clone())
    }
}

async fn correct_usage() {
    let future = MyPinnedFuture::new("test".to_string());
    let result = future.await; // 安全：future 已经被固定
    println!("Result: {}", result);
}

// 示例：在异步执行器中使用
#[tokio::main]
async fn main() {
    correct_usage().await;
}
```

流式处理的 future + pin
```rust
use std::future::Future;
use std::pin::Pin;
use std::task::{Context, Poll};
use futures::stream::{Stream, StreamExt};

struct AsyncCounter {
    count: i32,
    max: i32,
}

impl Stream for AsyncCounter {
    type Item = i32;

    fn poll_next(
        mut self: Pin<&mut Self>,
        _cx: &mut Context<'_>,
    ) -> Poll<Option<Self::Item>> {
        // 安全：因为 AsyncCounter 不包含自引用
        let this = &mut *self;
        
        if this.count >= this.max {
            return Poll::Ready(None);
        }
        
        this.count += 1;
        Poll::Ready(Some(this.count))
    }
}

async fn use_stream() {
    let counter = AsyncCounter {
        count: 0,
        max: 5,
    };
    
    let mut pinned = Box::pin(counter);
    
    while let Some(value) = pinned.next().await {
        println!("Count: {}", value);
    }
}
```
