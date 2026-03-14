#include "pch.h"
#include "Types.h"
#include "BufferGeometry.h"
#include "BoxGeometry.h"

BoxGeometry::BoxGeometry(float size)
	: BufferGeometry([](float half)
		{
			return std::vector<Vertex3>
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
