#ifndef SCALED_MPZ_HPP
#define SCALED_MPZ_HPP

#include <cstdint>

#include <gmpxx.h>

namespace flopoco
{
	struct ScaledMPZ {
		int64_t lowbit_weight;
		mpz_class value;

		void rescale(int64_t new_scale);
	};
} // namespace flopoco

#endif