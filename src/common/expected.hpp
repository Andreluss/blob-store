#pragma once
#include <variant>
#include <stdexcept>

// The custom expected type - like Ocaml's Or_error or Rust's Result
template <typename T, typename E>
class Expected {
public:
    // Constructors for both valid value and error value
    explicit(false) Expected(T value) : value_or_error(value) {}
    explicit(false) Expected(E error) : value_or_error(error) {}

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

private:
    // A variant that holds either a valid value of type T or an error of type E
    std::variant<T, E> value_or_error;
};
