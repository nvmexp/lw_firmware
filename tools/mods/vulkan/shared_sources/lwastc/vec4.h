#ifndef _VEC_H
#define _VEC_H 1

#include <math.h>
#undef min
#undef max
#include <algorithm>
#include <stdio.h>


namespace lwtt_astc
{

	class Vec4 {
	private:
		float d[4];
	public:
		Vec4() { for (int i = 0; i < 4; i++) d[i] = 0.0; }
		explicit Vec4(float x) { for (int i = 0; i < 4; i++) d[i] = x; }
		Vec4(float x, float y, float z, float w) {
			d[0] = x; d[1] = y; d[2] = z; d[3] = w;
		}
		Vec4 operator+(const Vec4 &o) const {
			return Vec4(d[0] + o.d[0], d[1] + o.d[1], d[2] + o.d[2], d[3] + o.d[3]);
		}
		Vec4 operator-(const Vec4 &o) const {
			return Vec4(d[0] - o.d[0], d[1] - o.d[1], d[2] - o.d[2], d[3] - o.d[3]);
		}
		Vec4 operator*(const Vec4 &o) const {
			return Vec4(d[0] * o.d[0], d[1] * o.d[1], d[2] * o.d[2], d[3] * o.d[3]);
		}
		Vec4 operator/(const Vec4 &o) const {
			return Vec4(d[0] / o.d[0], d[1] / o.d[1], d[2] / o.d[2], d[3] / o.d[3]);
		}
		Vec4 operator*=(const float f) {
			for (int i = 0; i < 4; i++) d[i] *= f;
			return *this;
		}
		Vec4 operator+=(const Vec4 &o) {
			for (int i = 0; i < 4; i++) d[i] += o.d[i];
			return *this;
		}

		bool operator!=(const Vec4 &o) const {
			for (int i = 0; i < 4; i++)
				if (d[i] != o.d[i])
					return true;
			return false;
		}

		bool operator==(const Vec4 &o) const {
			return !(*this != o);
		}

		float Min() const {
			float rv = d[0];
			for (int i = 1; i < 4; i++) {
				if (d[i] < rv) rv = d[i];
			}
			return rv;
		}

		float Max() const {
			float rv = d[0];
			for (int i = 1; i < 4; i++) {
				if (d[i] > rv) rv = d[i];
			}
			return rv;
		}

		friend Vec4 Min(const Vec4 &a, const Vec4 &b) {
			return Vec4(std::min(a.d[0], b.d[0]),
				std::min(a.d[1], b.d[1]),
				std::min(a.d[2], b.d[2]),
				std::min(a.d[3], b.d[3]));
		}
		friend Vec4 Max(const Vec4 &a, const Vec4 &b) {
			return Vec4(std::max(a.d[0], b.d[0]),
				std::max(a.d[1], b.d[1]),
				std::max(a.d[2], b.d[2]),
				std::max(a.d[3], b.d[3]));
		}

		friend float dot(const Vec4 &a, const Vec4 &b) {
			float rv = 0.0;
			for (int i = 0; i < 4; i++) rv += a.d[i] * b.d[i];
			return rv;
		}
		float sqLength() const {
			float rv = 0.0;
			for (int i = 0; i < 4; i++) rv += d[i] * d[i];
			return rv;
		}
		float length() const {
			return sqrtf(sqLength());
		}
		const float& x() const { return d[0]; }
		const float& y() const { return d[1]; }
		const float& z() const { return d[2]; }
		const float& w() const { return d[3]; }
		float& x() { return d[0]; }
		float& y() { return d[1]; }
		float& z() { return d[2]; }
		float& w() { return d[3]; }
		const float& elem(int i) const { return d[i]; }
		float& elem(int i) { return d[i]; }
		const float& operator[](int i) const { return d[i]; }
		float& operator[](int i) { return d[i]; }
		Vec4 floor() const {
			return Vec4(floorf(d[0]),
				floorf(d[1]),
				floorf(d[2]),
				floorf(d[3]));
		}
	};

#define POWER_ITERATION_COUNT 8

	class Mat4x4 {
	private:
		Vec4 rows[4];
	public:
		Mat4x4() { ; }
		// Post-multiply by column vector
		Vec4 operator*(const Vec4 &v) const {
			float e[4];
			for (int i = 0; i < 4; i++) {
				e[i] = dot(rows[i], v);
			}
			return Vec4(e[0], e[1], e[2], e[3]);
		}
		const float& elem(int col, int row) const { return rows[row].elem(col); }
		float& elem(int col, int row) { return rows[row].elem(col); }
		const Vec4& row(int row) const { return rows[row]; }
		Vec4& row(int row) { return rows[row]; }

		Vec4 estimatePrincipleComponent() const {
			float bestLen = rows[0].sqLength();
			const Vec4 *bestRow = &rows[0];

			for (int i = 1; i < 4; i++) {
				float sl = rows[i].sqLength();
				if (sl > bestLen) {
					bestRow = &rows[i];
					bestLen = sl;
				}
			}

			return *bestRow;
		}

		Vec4 computePrincipleComponent() const {
			Vec4 v = estimatePrincipleComponent();

			for (int i = 0; i < POWER_ITERATION_COUNT; i++) {
				v = (*this)*v;
				float norm = std::max(std::max(std::max(v.x(), v.y()), v.z()), v.w());
				if (norm == 0.0f) {
					return Vec4(0.0);
				}
				float ilw = 1.0f / norm;

				v *= ilw;
			}

			return v;
		}

		void debugPrint() const {
			for (int r = 0; r < 4; r++) {
				const Vec4 &row = rows[r];
				printf(" |\t");
				for (int i = 0; i < 4; i++) {
					printf("%.3f\t", row[i]);
				}
				printf(" |\n");
			}
			printf("\n");
		}
	};

}

#endif
