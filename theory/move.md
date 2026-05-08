● Rule of Five in C++

  If your class owns a resource (fd, heap memory, socket, mutex), you
  must explicitly define or delete all 5 special members. If you need to
   define any one of them, you almost certainly need all five.

  The 5 members:
  class Foo {
      Foo();                              // 1. constructor (not part of
   the rule, but context)
      ~Foo();                             // 2. destructor
      Foo(const Foo &);                   // 3. copy constructor
      Foo &operator=(const Foo &);        // 4. copy assignment
      Foo(Foo &&) noexcept;               // 5. move constructor
      Foo &operator=(Foo &&) noexcept;    // 6. move assignment
  };
  ==============================
  Example 1 — raw pointer (what goes wrong without the rule)

  class Buffer {
      int *_data;
  public:
      Buffer() : _data(new int[100]) {}
      ~Buffer() { delete[] _data; }   // you defined destructor → must
  handle the rest
  };

  Buffer a;
  Buffer b = a;   // compiler generates shallow copy: b._data == a._data
                  // when both destruct → double delete → crash
                  =========================

                    Fix — apply Rule of Five:
  class Buffer {
      int *_data;
  public:
      Buffer() : _data(new int[100]) {}
      ~Buffer() { delete[] _data; }

      // copy: deep copy
      Buffer(const Buffer &other) : _data(new int[100]) {
          std::copy(other._data, other._data + 100, _data);
      }
      Buffer &operator=(const Buffer &other) {
          if (this != &other)
              std::copy(other._data, other._data + 100, _data);
          return *this;
      }

      // move: steal the pointer, leave other invalid
      Buffer(Buffer &&other) noexcept : _data(other._data) { other._data
   = nullptr; }
      Buffer &operator=(Buffer &&other) noexcept {
          if (this != &other) {
              delete[] _data;
              _data = other._data;
              other._data = nullptr;
          }
          return *this;
      }
  };
  =================
    // copy is nonsensical → delete
  EpollLoop(const EpollLoop &) = delete;
  EpollLoop &operator=(const EpollLoop &) = delete;

  // move is safe → transfer ownership
  EpollLoop(EpollLoop &&other) noexcept : _fd(other._fd) { other._fd =
  -1; }
  EpollLoop &operator=(EpollLoop &&other) noexcept {
      if (this != &other) {
          if (_fd >= 0) close(_fd);
          _fd = other._fd;
          other._fd = -1;
      }
      return *this;
  }
  ====================
   The rule in one sentence: if you write a destructor that frees
  something, the compiler-generated copy/move will be wrong — you must
  take control of all five.