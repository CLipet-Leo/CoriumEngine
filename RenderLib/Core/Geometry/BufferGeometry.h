#pragma once

class BufferGeometry
{
public:
	explicit BufferGeometry(std::vector<Vertex3> vertexData, std::vector<uint32_t> indexData = {})
		: _vertexData(std::move(vertexData)), _indexData(std::move(indexData)) {}
	virtual ~BufferGeometry() = default;

	const std::vector<Vertex3>&  GetVertices() const { return _vertexData; }
	const std::vector<uint32_t>& GetIndices()  const { return _indexData; }
	bool IsIndexed() const { return !_indexData.empty(); }

protected:
	std::vector<Vertex3>  _vertexData;
	std::vector<uint32_t> _indexData;
};

