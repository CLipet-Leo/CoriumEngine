#pragma once

// -------------------------------------------------------
// Widgets de propriétés (tableau libellé | widget), cf. Inspector/ComponentBase.cpp
// -------------------------------------------------------
bool BeginProperties(const char* id);
void PropertyLabel(const char* label);
// Vec3 à composantes colorées ; cliquer sur X/Y/Z remet la composante à resetValue
bool Vec3Property(const char* label, Vec3& v, float speed, float resetValue);

// -------------------------------------------------------
// Section de l'inspecteur pour un type de composant
// -------------------------------------------------------
class ComponentBase
{
public:
    virtual ~ComponentBase() = default;

    // Ne dessine rien si l'entité ne possède pas le composant
    virtual void Draw(Scene& scene, EntityID id) = 0;
};

// En-tête repliable + tableau de propriétés ; la dérivée ne remplit que les lignes
template<typename T>
class ComponentPanel : public ComponentBase
{
public:
    explicit ComponentPanel(const char* name) : m_name(name) {}

    void Draw(Scene& scene, EntityID id) final
    {
        T* c = scene.GetComponent<T>(id);
        if (!c) return;

        ImGui::PushID(m_name); // IDs des widgets propres à chaque section
        if (ImGui::CollapsingHeader(m_name, ImGuiTreeNodeFlags_DefaultOpen) && BeginProperties("##props"))
        {
            DrawProperties(id, *c);
            ImGui::EndTable();
        }
        ImGui::PopID();
    }

protected:
    virtual void DrawProperties(EntityID id, T& c) = 0;

private:
    const char* m_name;
};

class TransformPanel : public ComponentPanel<TransformComponent>
{
public:
    TransformPanel() : ComponentPanel("Transform") {}

protected:
    void DrawProperties(EntityID id, TransformComponent& t) override;

private:
    // Euler affichés, recalculés seulement si la rotation change ailleurs
    // (évite l'instabilité d'une reconversion quat -> Euler à chaque frame)
    EntityID m_eulerEntity = NULL_ENTITY;
    Quat     m_eulerSource = {};
    Vec3     m_euler = {};
};

class MeshPanel : public ComponentPanel<MeshComponent>
{
public:
    MeshPanel() : ComponentPanel("Mesh") {}

protected:
    void DrawProperties(EntityID id, MeshComponent& m) override;
};

class CameraPanel : public ComponentPanel<CameraComponent>
{
public:
    CameraPanel() : ComponentPanel("Camera") {}

protected:
    void DrawProperties(EntityID id, CameraComponent& cam) override;
};

class LightPanel : public ComponentPanel<LightComponent>
{
public:
    LightPanel() : ComponentPanel("Light") {}

protected:
    void DrawProperties(EntityID id, LightComponent& l) override;
};
