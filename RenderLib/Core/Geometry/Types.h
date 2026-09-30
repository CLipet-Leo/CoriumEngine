#pragma once

struct Vertex3 {
	float x, y, z;
};

struct Vector3f {
	float x, y, z;
};

struct Object3D {
	Vector3f position;
	Vector3f rotation;
	Vector3f scale;
};
