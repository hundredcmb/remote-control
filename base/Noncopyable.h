#ifndef NONCOPYABLE_H
#define NONCOPYABLE_H

namespace lsy {
class Noncopyable {
public:
    Noncopyable(const Noncopyable &) = delete;

    void operator=(const Noncopyable &) = delete;

protected:
    Noncopyable() = default;

    ~Noncopyable() = default;
};
} // lsy

#endif //NONCOPYABLE_H
