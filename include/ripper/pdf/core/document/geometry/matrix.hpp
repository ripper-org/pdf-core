#pragma once

namespace ripper::pdf::core::geometry
{

/// A 2D affine transformation matrix, stored in PDF order `[a b c d e f]`
/// (ISO 32000-1 §8.3.3).
///
/// PDF uses row vectors, so a point `(x, y)` is transformed as:
///
///     x' = a·x + c·y + e
///     y' = b·x + d·y + f
///
/// Matrices compose left-to-right in the order they are applied:
/// `translated(tx, ty) * scaled(sx, sy)` first translates, then scales, so the
/// translation is itself scaled.
struct matrix
{
    double a = 1.0;
    double b = 0.0;
    double c = 0.0;
    double d = 1.0;
    double e = 0.0;
    double f = 0.0;

    /// Returns the identity matrix.
    [[nodiscard]] static matrix identity() noexcept
    {
        return matrix{};
    }

    /// Returns a matrix that translates by `(tx, ty)`.
    [[nodiscard]] static matrix translated(double tx, double ty) noexcept
    {
        return matrix{1.0, 0.0, 0.0, 1.0, tx, ty};
    }

    /// Returns a matrix that scales by `(sx, sy)`.
    [[nodiscard]] static matrix scaled(double sx, double sy) noexcept
    {
        return matrix{sx, 0.0, 0.0, sy, 0.0, 0.0};
    }

    /// Returns a matrix that rotates counter-clockwise by `degrees`.
    [[nodiscard]] static matrix rotated(double degrees);

    /// Returns a matrix that skews the x axis by `x_degrees` and the y axis by
    /// `y_degrees`.
    [[nodiscard]] static matrix skewed(double x_degrees, double y_degrees);

    /// Returns the composition `*this` followed by `other`.
    [[nodiscard]] matrix operator*(const matrix& other) const noexcept
    {
        return matrix{
            a * other.a + b * other.c,           a * other.b + b * other.d,
            c * other.a + d * other.c,           c * other.b + d * other.d,
            e * other.a + f * other.c + other.e, e * other.b + f * other.d + other.f,
        };
    }

    bool operator==(const matrix& other) const noexcept = default;
};

} // namespace ripper::pdf::core::geometry
