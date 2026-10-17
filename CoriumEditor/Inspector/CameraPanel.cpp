#include "stdafx.h"

void CameraPanel::DrawProperties(EntityID, CameraComponent& cam)
{
	PropertyLabel("Main"); ImGui::Checkbox("##main", &cam.isMain);
	PropertyLabel("FOV");  ImGui::SliderFloat("##fov", &cam.fovY, 1.f, 179.f, "%.0f deg");
	PropertyLabel("Near"); ImGui::DragFloat("##near", &cam.nearPlane, 0.01f, 0.001f, cam.farPlane, "%.3f");
	PropertyLabel("Far");  ImGui::DragFloat("##far", &cam.farPlane, 1.f, cam.nearPlane, 100000.f, "%.1f");
}
