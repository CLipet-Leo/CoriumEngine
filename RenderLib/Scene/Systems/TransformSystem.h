#pragma once

class TransformSystem
{
public:
	void Update(Scene& scene);
	void UpdateNode(Scene& scene, EntityID id, FXMMATRIX parentWorld, bool parentChanged);
};
