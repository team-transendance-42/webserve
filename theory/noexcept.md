 noexcept tutorial

  noexcept tells the compiler: this function will never throw an
  exception.

  void foo() noexcept;      // guaranteed not to throw
  void bar();               // might throw (default assumption)

    Why it matters for move operations specifically

  The STL (e.g. std::vector) has a rule: when it needs to grow and move
  elements, it will only use your move constructor if it is noexcept. If
   not, it falls back to copying (safe but slow) because a throwing move
   could leave the container half-moved with no way to recover.

=================

 When to use it

  ┌────────────────────────────────────┬────────────────────────────┐
  │                Case                │       Use noexcept?        │
  ├────────────────────────────────────┼────────────────────────────┤
  │ Move constructor/assignment        │ Yes — always if possible   │
  ├────────────────────────────────────┼────────────────────────────┤
  │ Destructor                         │ Already noexcept by        │
  │                                    │ default in C++11           │
  ├────────────────────────────────────┼────────────────────────────┤
  │ Swap functions                     │ Yes                        │
  ├────────────────────────────────────┼────────────────────────────┤
  │ Functions that allocate memory     │ No — new can throw         │
  ├────────────────────────────────────┼────────────────────────────┤
  │ Functions calling other throwing   │ No                         │
  │ functions                          │                            │
  └────────────────────────────────────┴────────────────────────────┘