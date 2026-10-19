#include "pch.h"
#include "BufferGeometry.h"
#include "BoxGeometry.h"

BoxGeometry::BoxGeometry(float size)
	: BufferGeometry([](float half)
		{
			return std::vector<Vec3>
			{
				// Face +Z
				{-half, -half, +half}, {+half, -half, +half}, {+half, +half, +half},
				{-half, -half, +half}, {+half, +half, +half}, {-half, +half, +half},

				// Face -Z
				{+half, -half, -half}, {-half, -half, -half}, {-half, +half, -half},
				{+half, -half, -half}, {-half, +half, -half}, {+half, +half, -half},

				// Face -X
				{-half, -half, -half}, {-half, -half, +half}, {-half, +half, +half},
				{-half, -half, -half}, {-half, +half, +half}, {-half, +half, -half},

				// Face +X
				{+half, -half, +half}, {+half, -half, -half}, {+half, +half, -half},
				{+half, -half, +half}, {+half, +half, -half}, {+half, +half, +half},

				// Face +Y
				{-half, +half, +half}, {+half, +half, +half}, {+half, +half, -half},
				{-half, +half, +half}, {+half, +half, -half}, {-half, +half, -half},

				// Face -Y
				{-half, -half, -half}, {+half, -half, -half}, {+half, -half, +half},
				{-half, -half, -half}, {+half, -half, +half}, {-half, -half, +half}
			};
		}(size * 0.5f))
{
}
