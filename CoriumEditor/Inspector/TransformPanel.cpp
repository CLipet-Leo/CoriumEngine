#include "stdafx.h"

void TransformPanel::DrawProperties(EntityID id, TransformComponent& t)
{
	if (Vec3Property("Position", t.position, 0.01f, 0.f)) t.dirty = true;
	if (m_eulerEntity != id || memcmp(&m_eulerSource, &t.rotation, sizeof(Quat)) != 0)
	{
		m_eulerEntity = id;
		m_eulerSource = t.rotation;
		m_euler = ToEulerDeg(t.rotation);
	}
	if (Vec3Property("Rotation", m_euler, 0.5f, 0.f))
	{
		t.SetRotationDeg(m_euler);
		m_eulerSource = t.rotation;
	}
	if (Vec3Property("Scale", t.scale, 0.01f, 1.f)) t.dirty = true;
}
