#pragma once

#include "Types.h"
#include <vector>

class BufferGeometry
{
public:
	explicit BufferGeometry(std::vector<Vertex3> vertexData) : _vertexData(std::move(vertexData)) {}
	virtual ~BufferGeometry() = default;

	const std::vector<Vertex3>& GetVertices() const { return _vertexData; }

protected:
	std::vector<Vertex3> _vertexData;
};

