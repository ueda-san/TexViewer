using System;

// row-major (numpy互換)

public struct Vec3
{
    public float X, Y, Z;

    public Vec3(float x, float y, float z) {
        X = x; Y = y; Z = z;
    }
}

public struct Mat3x3
{
    public float M11, M12, M13;
    public float M21, M22, M23;
    public float M31, M32, M33;

    public Mat3x3(
        float m11, float m12, float m13,
        float m21, float m22, float m23,
        float m31, float m32, float m33)
    {
        M11 = m11; M12 = m12; M13 = m13;
        M21 = m21; M22 = m22; M23 = m23;
        M31 = m31; M32 = m32; M33 = m33;
    }

    public static Mat3x3 Identity => new(1, 0, 0, 0, 1, 0, 0, 0, 1);
    public static Mat3x3 Diag(Vec3 s) => new(s.X, 0, 0, 0, s.Y, 0, 0, 0, s.Z);


    // --- 行列 × ベクトル（NumPy: m @ v）---
    public static Vec3 Multiply(Mat3x3 m, Vec3 v)
    {
        return new Vec3(
            m.M11 * v.X + m.M12 * v.Y + m.M13 * v.Z,
            m.M21 * v.X + m.M22 * v.Y + m.M23 * v.Z,
            m.M31 * v.X + m.M32 * v.Y + m.M33 * v.Z
        );
    }

    // --- 行列 × 行列（NumPy: m1 @ m2）---
    public static Mat3x3 Multiply(Mat3x3 a, Mat3x3 b)
    {
        return new Mat3x3(
            a.M11 * b.M11 + a.M12 * b.M21 + a.M13 * b.M31,
            a.M11 * b.M12 + a.M12 * b.M22 + a.M13 * b.M32,
            a.M11 * b.M13 + a.M12 * b.M23 + a.M13 * b.M33,

            a.M21 * b.M11 + a.M22 * b.M21 + a.M23 * b.M31,
            a.M21 * b.M12 + a.M22 * b.M22 + a.M23 * b.M32,
            a.M21 * b.M13 + a.M22 * b.M23 + a.M23 * b.M33,

            a.M31 * b.M11 + a.M32 * b.M21 + a.M33 * b.M31,
            a.M31 * b.M12 + a.M32 * b.M22 + a.M33 * b.M32,
            a.M31 * b.M13 + a.M32 * b.M23 + a.M33 * b.M33
        );
    }

    // --- 列スケーリング（NumPy: M = M * S）---
    public static Mat3x3 ScaleColumns(Mat3x3 m, Vec3 s)
    {
        return new Mat3x3(
            m.M11 * s.X, m.M12 * s.Y, m.M13 * s.Z,
            m.M21 * s.X, m.M22 * s.Y, m.M23 * s.Z,
            m.M31 * s.X, m.M32 * s.Y, m.M33 * s.Z
        );
    }

    // --- 逆行列（NumPy: np.linalg.inv(m)）---
    public static Mat3x3 Invert(Mat3x3 m)
    {
        float det =
            m.M11 * (m.M22 * m.M33 - m.M23 * m.M32) -
            m.M12 * (m.M21 * m.M33 - m.M23 * m.M31) +
            m.M13 * (m.M21 * m.M32 - m.M22 * m.M31);

        if (Math.Abs(det) < 1e-8f)
            throw new InvalidOperationException("Matrix is singular and cannot be inverted.");

        float invDet = 1.0f / det;

        return new Mat3x3(
            (m.M22 * m.M33 - m.M23 * m.M32) * invDet,
            (m.M13 * m.M32 - m.M12 * m.M33) * invDet,
            (m.M12 * m.M23 - m.M13 * m.M22) * invDet,

            (m.M23 * m.M31 - m.M21 * m.M33) * invDet,
            (m.M11 * m.M33 - m.M13 * m.M31) * invDet,
            (m.M13 * m.M21 - m.M11 * m.M23) * invDet,

            (m.M21 * m.M32 - m.M22 * m.M31) * invDet,
            (m.M12 * m.M31 - m.M11 * m.M32) * invDet,
            (m.M11 * m.M22 - m.M12 * m.M21) * invDet
        );
    }

    public static Vec3 operator *(Mat3x3 m, Vec3 v) => Multiply(m, v);
    public static Mat3x3 operator *(Mat3x3 a, Mat3x3 b) => Multiply(a, b);

    public override string ToString() =>
        $"[{M11}, {M12}, {M13}; {M21}, {M22}, {M23}; {M31}, {M32}, {M33}]";
}
