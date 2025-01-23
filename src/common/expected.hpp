#pragma once
#include <variant>
#include <stdexcept>

// Wrapper class for creating error case explicitly.
template <typename E>
class Unexpected
{
public:
    explicit Unexpected(const E& error): error_(error) {}
    const E& error_;
};

// The custom expected type - like Ocaml's Or_error or Rust's Result
template <typename T, typename E>
requires (!std::is_void_v<T>)
class Expected {
public:
    using value_type = T;
    using error_type = E;

    explicit(false) Expected(const T& value): value_or_error(value) {}
    explicit(false) Expected(const E& error): value_or_error(error) {}

    template<class U> requires (std::is_convertible_v<U, E> && !std::is_convertible_v<U, T>)
    explicit(false) Expected(const U& error): value_or_error(error) {}

    template<class U> requires (std::is_convertible_v<U, E>)
    explicit(false) Expected(Unexpected<U> unexpected): value_or_error(E(unexpected.error_)) {}

    Expected& operator=(const T& value) { return *this = Expected(value); }
    Expected& operator=(const E& error) { return *this = Expected(error); }
public:
    // Check if the result is valid (holds a T value)
    [[nodiscard]] bool has_value() const {
        return std::holds_alternative<T>(value_or_error);
    }

    // Get the value or throw an exception if it is an error
    T& value() {
        if (!has_value()) {
            throw std::runtime_error("Called value() on an error result");
        }
        return std::get<T>(value_or_error);
    }

    const T& value() const {
        if (!has_value()) {
            throw std::runtime_error("Called value() on an error result");
        }
        return std::get<T>(value_or_error);
    }

    // Get the error value or throw an exception if it is a valid result
    E& error() {
        if (has_value()) {
            throw std::runtime_error("Called error() on a valid result");
        }
        return std::get<E>(value_or_error);
    }

    const E& error() const {
        if (has_value()) {
            throw std::runtime_error("Called error() on a valid result");
        }
        return std::get<E>(value_or_error);
    }

    // For chaining operations on the happy path and handling the error only once.
    template <typename Func, typename FuncResult = std::invoke_result_t<Func, T>,
              typename FuncResultValue = typename FuncResult::value_type>
    requires std::is_invocable_v<Func, T> && std::is_same_v<FuncResult, Expected<FuncResultValue, E>>
    auto and_then(Func&& f) -> Expected<FuncResultValue, E> {
        if (has_value()) {
            return std::forward<Func>(f)(value());
        }
        return error();
    }

    // Merges value and error into one type.
    template <typename Output>
    auto output(std::function<Output(const T&)> t_conv, std::function<Output(const E&)> e_conv) -> Output {
        if (has_value()) { return t_conv(value()); }
        return e_conv(error());
    }

private:
    // A variant that holds either a valid value of type T or an error of type E
    std::variant<T, E> value_or_error;
};

