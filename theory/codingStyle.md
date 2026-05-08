Rule (C++ common practice)

Put constructors in the .hpp when they are:

Very small

Trivial initialization

Header-only structs

Likely to be inlined

Put them in .cpp when they:

Contain logic

Are long

May change often

Depend on other compilation units