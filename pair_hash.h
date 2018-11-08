#ifndef PAIR_HASH_H
#define PAIR_HASH_H

#include "n3876.h"

// Hash for std::pair. We need it to enbale usage of std::pair as key of
// std::unordered_map or as value in the std::unordered_set.
// Based on this public material: https://stackoverflow.com/a/32685618/1540501
struct pair_hash {
	template <class T1, class T2>
	std::size_t operator () (const std::pair<T1, T2>& p) const
	{
		std::size_t seed = 0;
		n3876::hash_combine(seed, p.first, p.second);
		return seed;
	}
};

#endif // PAIR_HASH_H
