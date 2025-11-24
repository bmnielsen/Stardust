#pragma once

// Trick from https://stackoverflow.com/a/76075718 to make a non-copyable aggregate in C++20
struct noncopyable {
    noncopyable() = default;
    noncopyable(noncopyable&&) = default;
    noncopyable& operator=(noncopyable&&) = default;
};
