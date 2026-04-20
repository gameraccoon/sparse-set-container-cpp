A container that behaves like a dynamic array, has stable handle-like keys, O(1) element query time with no hashing.

### The premise

* Tightly packed values, with iteration performance comparable to std::vector
* Stable handle-like keys, removing elements from the middle doesn't invalidate the keys
* O(1) time access by key, O(1) access by index
* O(1) time insertion, swapRemove
* No pre-calculation, or building indices, or hidden spikes of computation (except for buffer reallocation on growth, which can be pre-allocated), and no caches that needs to be invalidated
* No hashing
* Additional memory overhead per element (the value of the overhead depends on the implementation)
* More expensive insertion and removal compared to std::vector (see [benchmarks](https://github.com/gameraccoon/sparse-set-container-cpp-tests))

### High-level design

The container is built on top of sparse set data structure utilizing "free list" pattern for managing unoccupied elements. It usually also utilizes "epochs" approach to ensure safe reuse of sparse array keys of the sparse set.

The container is usually composed of:
* Array of tightly packed values (dense array)
* Array of tightly packed keys
* Array of `SparseElement` elements (sparse array)
* Index of the last free sparse value
* (depending on the implementation) sizes of the allocated arrays and numers of stored elements

`SparseElement` can be implemented as untagged `union` or as just two integers that vary the function based on whether the element is occupied or free.
An occupied element stores the index to the corresponding element of dense array and the epoch of the value.
A free element stores the next free index of the "free list" and the next epoch that the value in the current position will have.

The design of the container is based on the design descibed by ["ECS back and forth" articles](https://skypjack.github.io) from [Michele skypjack Caini](https://github.com/skypjack) and ["A free list" pattern](https://gameprogrammingpatterns.com/object-pool.html#a-free-list) from Game Programming Patterns book from Bob Nystrom.

### Implementation considerations

There are many ways to implement the structure based on these considerations:
* Whether manual memory management is acceptable (to reduce the number of the internal buffer allocations)
  * This affects whether the container manages the memory manually, or other data structure implementations (such as std::vector) are used internally.
* Whether the container is intended to "own" the stored values, or the values are onwed by other parts of the application.
  * If the container in not owning the values, it can enforse the users to store and maintain the keys to maintain the lifetime of the elements (when they should be removed from the array), which can affect the design of the keys, but also can simplify the implementation.
* Whether the user should be able to check the validity of keys.
  * The answer here is almost always 'Yes', however if it can be guaranteed that the application would never store keys to removed elements (e.g. if keys are ONLY used for the lifetime management in the point above), then the implementation can avoid using epochs completely.
* Whether only one array of values is needed or one container should store multiple arrays in the Structure of Arrays (SoA) approach.
  * Obviously this affects how the values are stored and the complexity of the implementation.
* Whether the container should manage the case of epoch overflow (if it is possible that the amount of removals can exceed the maximum number of epochs)
  * Managing overflowing may include adding new sparse elements in place of the ones with exhausted epochs, and storing sizes of dense and sparse arrays separately (since the sparse array now can have more elements). If overflow is not handled, addition of new elements should be able to fail, and that failure to be detected and handled by the caller.
* What are the maximum number of elements (and number of element removals if "epoch overflow" is not handled) that the container should support.
  * This affects what integer types should be used in the implementation.

As an example, the Rust [sparse_set_container](github.com/gameraccoon/sparse_set_container) implementation makes these choices:
* Manual memory management is acceptable. All three arrays are stored in the same block of the memory, one after another.
* The array owns the values.
* The validity of keys should be able to be checked.
* Only one array of values is stored per container.
* In case of epoch overflows the sparse array grows which is completely transparent for the rest of the app.
* The implementation is based on the bitness of the system it runs on for 32 bit systems it allows to store up to 2^31-1 elements and produce 2^32 removals per element before new sparse elements are added.

### Implementations available in this repo

#### SparseSetContainer_Trivial - the most trivial implementation of sparse set container

* No manual memory management, each array is implemented as std::vector and allocates separately.
* The array does not own the values, the app should manage their lifetime and key validity.
* The validity of the keys can't be checked, as soon as the value by the key is removed, using that key is prohibited.
* Only one array of values is stored per container.
* Epochs are not used.
* Uses uint32 as indexes, and can store up to 4294967294 elements.

### Benchmarks

Benchmarks and tests can be found here: https://github.com/gameraccoon/sparse-set-container-cpp-tests
