js:
const add = (a, b) => a + b;
const greet = (name) => console.log(`Hello ${name}`);

c++
auto add = [](int a, int b) { return a + b; };
auto greet = [](const std::string& name) { std::cout << "Hello " << name << "\n"; };

c++ can campture vars from surrounding scope using []:

// Capture nothing
[](int x) { return x * 2; }

// Capture 'this' pointer (access member variables/functions)
[this](int fd, uint32_t ev) { _epollMod(fd, ev); }

// Capture specific variable by value
int multiplier = 5;
[multiplier](int x) { return x * multiplier; }

// Capture all by value
[=](int x) { return x * multiplier; }

// Capture all by reference
[&](int x) { return x * multiplier; }

[&] = Capture ALL by Reference
The & means reference — the lambda gets direct access to the original variables, not copies.
---------------------
examples:
---------------------
int multiplier = 5;

// By VALUE [=] - makes a COPY of var!
auto lambda_copy = [=](int x) { 
    return x * multiplier;
};

// By REFERENCE [&] - uses the ORIGINAL var!
auto lambda_ref = [&](int x) { 
    return x * multiplier;
};

// Change original:
multiplier = 100;

std::cout << lambda_copy(2) << "\n";  // 10  (still uses old copy)
std::cout << lambda_ref(2) << "\n";   // 200 (sees new value!)
-------------------
danger of & reference
------------------
std::function<void()> create_lambda() {
    int local_var = 42;
    return [&]() {
        std::cout << local_var << "\n";  // DANGER!
    };
}
// local_var destroyed here ↑

auto my_lambda = create_lambda();
my_lambda();  // CRASH! Dangling reference
--------------------
