#include "pch.h"
#include "SceneSerializer.h"

namespace
{
    // -------------------------------------------------------
    // Erreurs
    // -------------------------------------------------------
    struct ParseError
    {
        int         line;
        std::string message;
    };

    [[noreturn]] void Fail(int line, const std::string& message)
    {
        throw ParseError{ line, message };
    }

    void Warn(int line, const std::string& message)
    {
        OutputDebugStringA(("SceneSerializer: line " + std::to_string(line) + ": " + message + "\n").c_str());
    }

    // -------------------------------------------------------
    // Arbre du sous-ensemble YAML
    // -------------------------------------------------------
    struct Node
    {
        enum class Kind { Scalar, Map, List };

        Kind              kind = Kind::Scalar;
        int               line = 0;
        std::string       key;   // clé, si entrée de map
        std::string       value; // scalaire, guillemets retirés
        std::vector<Node> items; // entrées de map ou éléments de liste, dans l'ordre du fichier
    };

    struct Line
    {
        int         number;
        int         indent;
        std::string text; // sans indentation ni commentaire
    };

    std::string_view Trim(std::string_view s)
    {
        const size_t begin = s.find_first_not_of(' ');
        if (begin == std::string_view::npos) return {};
        return s.substr(begin, s.find_last_not_of(' ') - begin + 1);
    }

    bool IsListItem(const std::string& text) { return text == "-" || text.starts_with("- "); }

    std::vector<Line> SplitLines(const std::string& text)
    {
        std::vector<Line> lines;
        std::istringstream in(text);
        std::string raw;
        for (int number = 1; std::getline(in, raw); ++number)
        {
            if (!raw.empty() && raw.back() == '\r') raw.pop_back();

            // Commentaire : '#' en début de ligne ou après un espace, hors chaîne
            bool quoted = false;
            for (size_t i = 0; i < raw.size(); ++i)
            {
                if (quoted && raw[i] == '\\') ++i;
                else if (raw[i] == '"') quoted = !quoted;
                else if (!quoted && raw[i] == '#' && (i == 0 || raw[i - 1] == ' '))
                {
                    raw.resize(i);
                    break;
                }
            }

            const size_t indent = raw.find_first_not_of(' ');
            if (indent == std::string::npos) continue; // ligne vide
            if (raw[indent] == '\t') Fail(number, "tabs are not allowed for indentation");
            lines.push_back({ number, static_cast<int>(indent), std::string(Trim(raw)) });
        }
        return lines;
    }

    class Parser
    {
    public:
        explicit Parser(const std::string& text) : m_lines(SplitLines(text)) {}

        Node ParseDocument()
        {
            if (m_lines.empty()) Fail(1, "empty document");
            if (m_lines[0].indent != 0 || IsListItem(m_lines[0].text))
                Fail(m_lines[0].number, "document root must be a map at column 0");
            return ParseMap(0);
        }

    private:
        bool AtIndent(int indent) const { return m_pos < m_lines.size() && m_lines[m_pos].indent == indent; }

        // Fin de bloc : la ligne suivante doit revenir à une indentation inférieure
        void ExpectBlockEnd(int indent) const
        {
            if (m_pos < m_lines.size() && m_lines[m_pos].indent >= indent)
                Fail(m_lines[m_pos].number, "unexpected indentation");
        }

        Node ParseBlock(int indent)
        {
            return IsListItem(m_lines[m_pos].text) ? ParseList(indent) : ParseMap(indent);
        }

        Node ParseMap(int indent)
        {
            Node map{ Node::Kind::Map, m_lines[m_pos].number };
            while (AtIndent(indent) && !IsListItem(m_lines[m_pos].text))
            {
                const Line& line = m_lines[m_pos++];
                const size_t colon = line.text.find(':');
                if (colon == std::string::npos || colon == 0 ||
                    (colon + 1 < line.text.size() && line.text[colon + 1] != ' '))
                    Fail(line.number, "expected 'key: value'");

                const std::string key = line.text.substr(0, colon);
                for (const Node& item : map.items)
                    if (item.key == key) Fail(line.number, "duplicate key '" + key + "'");

                const std::string_view rest = Trim(std::string_view(line.text).substr(colon + 1));
                Node child;
                if (!rest.empty())
                    child = ParseInline(rest, line.number);
                else if (m_pos < m_lines.size() && m_lines[m_pos].indent > indent)
                    child = ParseBlock(m_lines[m_pos].indent);
                else
                    Fail(line.number, "missing value for '" + key + "'");

                child.key = key;
                map.items.push_back(std::move(child));
            }
            ExpectBlockEnd(indent);
            return map;
        }

        // Un élément "- clé: valeur" est une map dont la première entrée est sur la ligne du tiret
        Node ParseList(int indent)
        {
            Node list{ Node::Kind::List, m_lines[m_pos].number };
            while (AtIndent(indent) && IsListItem(m_lines[m_pos].text))
            {
                Line& line = m_lines[m_pos];
                if (line.text == "-") Fail(line.number, "list item must start with 'key: value'");
                line.text = std::string(Trim(std::string_view(line.text).substr(2)));
                line.indent = indent + 2;
                list.items.push_back(ParseMap(indent + 2));
            }
            ExpectBlockEnd(indent);
            return list;
        }

        // Valeur sur la ligne de la clé : scalaire, chaîne entre guillemets ou liste [a, b, c]
        static Node ParseInline(std::string_view text, int line)
        {
            Node node{ Node::Kind::Scalar, line };
            if (text.front() == '[')
            {
                if (text.back() != ']') Fail(line, "unterminated '['");
                node.kind = Node::Kind::List;
                const std::string_view inner = Trim(text.substr(1, text.size() - 2));
                for (size_t start = 0; !inner.empty();)
                {
                    const size_t comma = inner.find(',', start);
                    const std::string_view element = Trim(inner.substr(start,
                        comma == std::string_view::npos ? std::string_view::npos : comma - start));
                    if (element.empty() || element.find_first_of("[]{}\"") != std::string_view::npos)
                        Fail(line, "invalid list element");
                    node.items.push_back({ Node::Kind::Scalar, line, {}, std::string(element) });
                    if (comma == std::string_view::npos) break;
                    start = comma + 1;
                }
            }
            else if (text.front() == '"')
            {
                size_t i = 1;
                for (; i < text.size() && text[i] != '"'; ++i)
                {
                    if (text[i] == '\\' && (++i == text.size() || (text[i] != '"' && text[i] != '\\')))
                        Fail(line, "invalid escape (only \\\" and \\\\)");
                    node.value += text[i];
                }
                if (i == text.size()) Fail(line, "unterminated string");
                if (i != text.size() - 1) Fail(line, "unexpected text after string");
            }
            else if (std::string_view("{&*|>!'%@`").find(text.front()) != std::string_view::npos)
                Fail(line, "unsupported YAML syntax");
            else
                node.value = text;
            return node;
        }

        std::vector<Line> m_lines;
        size_t            m_pos = 0;
    };

    // -------------------------------------------------------
    // Lecture typée des noeuds
    // -------------------------------------------------------
    const Node* Find(const Node& map, std::string_view key)
    {
        for (const Node& item : map.items)
            if (item.key == key) return &item;
        return nullptr;
    }

    const std::vector<Node>& Items(const Node& n, Node::Kind kind)
    {
        if (n.kind != kind) Fail(n.line, "'" + n.key + "': expected a " + (kind == Node::Kind::Map ? "map" : "list"));
        return n.items;
    }

    void WarnUnknown(const Node& field)
    {
        Warn(field.line, "unknown field '" + field.key + "' ignored");
    }

    const std::string& ToScalar(const Node& n)
    {
        if (n.kind != Node::Kind::Scalar) Fail(n.line, "'" + n.key + "': expected a value");
        return n.value;
    }

    float ToFloat(const Node& n)
    {
        const std::string& s = ToScalar(n);
        float v = 0.f;
        const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
        if (ec != std::errc() || end != s.data() + s.size()) Fail(n.line, "invalid number '" + s + "'");
        return v;
    }

    bool ToBool(const Node& n)
    {
        const std::string& s = ToScalar(n);
        if (s == "true")  return true;
        if (s == "false") return false;
        Fail(n.line, "expected true or false, got '" + s + "'");
    }

    EntityUUID ToUUID(const Node& n)
    {
        const std::string& s = ToScalar(n);
        EntityUUID v = NULL_UUID;
        const char* end = s.data() + s.size();
        if (!s.starts_with("0x") || std::from_chars(s.data() + 2, end, v, 16).ptr != end || v == NULL_UUID)
            Fail(n.line, "invalid uuid '" + s + "'");
        return v;
    }

    template<size_t N>
    std::array<float, N> ToFloats(const Node& n)
    {
        if (n.kind != Node::Kind::List || n.items.size() != N)
            Fail(n.line, "'" + n.key + "': expected [" + std::to_string(N) + " numbers]");
        std::array<float, N> v;
        for (size_t i = 0; i < N; ++i)
            v[i] = ToFloat(n.items[i]);
        return v;
    }

    Vec3 ToVec3(const Node& n) { const auto v = ToFloats<3>(n); return { v[0], v[1], v[2] }; }
    Vec4 ToVec4(const Node& n) { const auto v = ToFloats<4>(n); return { v[0], v[1], v[2], v[3] }; }
    Quat ToQuat(const Node& n) { const auto v = ToFloats<4>(n); return { v[0], v[1], v[2], v[3] }; }

    // Noms des enums dans le fichier, dans les deux sens
    template<typename E>
    struct EnumName
    {
        const char* name;
        E           value;
    };

    constexpr EnumName<MeshPrimitive> kPrimitives[] = { { "box", MeshPrimitive::Box } };
    constexpr EnumName<LightType>     kLightTypes[] = { { "directional", LightType::Directional },
                                                        { "point", LightType::Point },
                                                        { "spot", LightType::Spot } };
    constexpr EnumName<CullMode>      kCullModes[] = { { "none", CullMode::None },
                                                       { "front", CullMode::Front },
                                                       { "back", CullMode::Back } };
    constexpr EnumName<FillMode>      kFillModes[] = { { "solid", FillMode::Solid },
                                                       { "wireframe", FillMode::Wireframe } };
    constexpr EnumName<BlendMode>     kBlendModes[] = { { "opaque", BlendMode::Opaque },
                                                        { "alphaBlend", BlendMode::AlphaBlend } };

    template<typename E, size_t N>
    E ToEnum(const Node& n, const EnumName<E> (&names)[N])
    {
        const std::string& s = ToScalar(n);
        for (const EnumName<E>& e : names)
            if (s == e.name) return e.value;
        Fail(n.line, "unknown value '" + s + "' for '" + n.key + "'");
    }

    template<typename E, size_t N>
    const char* NameOf(E value, const EnumName<E> (&names)[N])
    {
        for (const EnumName<E>& e : names)
            if (e.value == value) return e.name;
        CE_ASSERT(false && "SceneSerializer: enum value without name");
        return names[0].name;
    }

    // -------------------------------------------------------
    // Lecture
    // -------------------------------------------------------
    using NameMap = std::unordered_map<std::string, uint32_t>;

    uint32_t Lookup(const NameMap& names, const Node& n, const char* what)
    {
        const auto it = names.find(ToScalar(n));
        if (it == names.end()) Fail(n.line, std::string("unknown ") + what + " '" + n.value + "'");
        return it->second;
    }

    class SceneReader
    {
    public:
        SceneReader(Scene& scene, IRenderer& renderer) : m_scene(scene), m_renderer(renderer) {}

        void Read(const Node& root)
        {
            const Node* version = Find(root, "version");
            if (!version) Fail(root.line, "missing 'version'");
            if (ToScalar(*version) != "1") Fail(version->line, "unsupported version '" + version->value + "'");

            for (const Node& section : root.items)
                if (section.key != "version" && section.key != "meshes" &&
                    section.key != "materials" && section.key != "entities")
                    WarnUnknown(section);

            // Ressources avant les entités qui les référencent, quel que soit l'ordre du fichier
            if (const Node* meshes = Find(root, "meshes"))
                for (const Node& item : Items(*meshes, Node::Kind::List))
                    ReadMesh(item);
            if (const Node* materials = Find(root, "materials"))
                for (const Node& item : Items(*materials, Node::Kind::List))
                    ReadMaterial(item);
            if (const Node* entities = Find(root, "entities"))
                for (const Node& item : Items(*entities, Node::Kind::List))
                    ReadEntity(item);
        }

    private:
        void ReadMesh(const Node& item)
        {
            MeshDesc desc;
            for (const Node& f : item.items)
            {
                if      (f.key == "name")      desc.name = ToScalar(f);
                else if (f.key == "primitive") desc.primitive = ToEnum(f, kPrimitives);
                else if (f.key == "size")      desc.size = ToFloat(f);
                else WarnUnknown(f);
            }
            if (desc.name.empty()) Fail(item.line, "mesh without name");
            if (m_meshes.contains(desc.name)) Fail(item.line, "duplicate mesh '" + desc.name + "'");
            m_meshes[desc.name] = m_renderer.RegisterMesh(desc);
        }

        void ReadMaterial(const Node& item)
        {
            MaterialDesc desc;
            for (const Node& f : item.items)
            {
                if      (f.key == "name")      desc.name = ToScalar(f);
                else if (f.key == "baseColor") desc.baseColor = ToVec4(f);
                else if (f.key == "cullMode")  desc.cullMode = ToEnum(f, kCullModes);
                else if (f.key == "fillMode")  desc.fillMode = ToEnum(f, kFillModes);
                else if (f.key == "blendMode") desc.blendMode = ToEnum(f, kBlendModes);
                else WarnUnknown(f);
            }
            if (desc.name.empty()) Fail(item.line, "material without name");
            if (m_materials.contains(desc.name)) Fail(item.line, "duplicate material '" + desc.name + "'");
            // ponytail: un nouveau matériau à chaque chargement, les registres ne libèrent rien
            m_materials[desc.name] = m_renderer.CreateMaterial(desc);
        }

        void ReadEntity(const Node& item)
        {
            const Node* uuidNode = Find(item, "uuid");
            const Node* nameNode = Find(item, "name");
            const EntityUUID uuid = uuidNode ? ToUUID(*uuidNode) : NULL_UUID; // absent : nouvel UUID
            const EntityID id = m_scene.CreateEntity(nameNode ? ToScalar(*nameNode) : "Entity", uuid).id;

            const EntityUUID finalUuid = m_scene.GetEntity(id)->uuid;
            if (!m_entities.emplace(finalUuid, id).second)
                Fail(uuidNode ? uuidNode->line : item.line, "duplicate uuid");

            for (const Node& f : item.items)
            {
                if (f.key == "uuid" || f.key == "name") continue;
                else if (f.key == "active")    m_scene.GetEntity(id)->active = ToBool(f);
                else if (f.key == "parent")    ReadParent(id, f);
                else if (f.key == "transform") ReadTransform(*m_scene.GetComponent<TransformComponent>(id), f);
                else if (f.key == "mesh")      ReadMeshComponent(m_scene.AddComponent<MeshComponent>(id), f);
                else if (f.key == "camera")    ReadCamera(m_scene.AddComponent<CameraComponent>(id), f);
                else if (f.key == "light")     ReadLight(m_scene.AddComponent<LightComponent>(id), f);
                else WarnUnknown(f);
            }
        }

        void ReadParent(EntityID id, const Node& f)
        {
            const auto it = m_entities.find(ToUUID(f));
            if (it == m_entities.end()) Fail(f.line, "parent must be defined before its children");
            m_scene.SetParent(id, it->second);
        }

        static void ReadTransform(TransformComponent& t, const Node& node)
        {
            for (const Node& f : Items(node, Node::Kind::Map))
            {
                if (f.key == "position")   t.position = ToVec3(f);
                else if (f.key == "rotation")
                {
                    t.rotation = ToQuat(f);
                    const Quat& q = t.rotation;
                    const float lengthSq = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
                    if (lengthSq == 0.f) Fail(f.line, "null rotation");
                    // Seulement si nécessaire : renormaliser un quaternion déjà unitaire peut
                    // changer le dernier bit, et casser l'égalité octet pour octet des sauvegardes
                    if (std::fabs(lengthSq - 1.f) > 1e-5f) t.rotation = Normalize(q);
                }
                else if (f.key == "scale") t.scale = ToVec3(f);
                else WarnUnknown(f);
            }
            t.dirty = true;
        }

        void ReadMeshComponent(MeshComponent& m, const Node& node)
        {
            for (const Node& f : Items(node, Node::Kind::Map))
            {
                if      (f.key == "mesh")        m.meshIndex = Lookup(m_meshes, f, "mesh");
                else if (f.key == "material")    m.materialIndex = Lookup(m_materials, f, "material");
                else if (f.key == "visible")     m.visible = ToBool(f);
                else if (f.key == "castShadows") m.castShadows = ToBool(f);
                else WarnUnknown(f);
            }
        }

        static void ReadCamera(CameraComponent& c, const Node& node)
        {
            for (const Node& f : Items(node, Node::Kind::Map))
            {
                if      (f.key == "fovY")      c.fovY = ToFloat(f);
                else if (f.key == "nearPlane") c.nearPlane = ToFloat(f);
                else if (f.key == "farPlane")  c.farPlane = ToFloat(f);
                else if (f.key == "isMain")    c.isMain = ToBool(f);
                else WarnUnknown(f);
            }
        }

        static void ReadLight(LightComponent& l, const Node& node)
        {
            for (const Node& f : Items(node, Node::Kind::Map))
            {
                if      (f.key == "type")      l.type = ToEnum(f, kLightTypes);
                else if (f.key == "color")     l.color = ToVec3(f);
                else if (f.key == "intensity") l.intensity = ToFloat(f);
                else if (f.key == "range")     l.range = ToFloat(f);
                else if (f.key == "spotAngle") l.spotAngle = ToFloat(f);
                else if (f.key == "enabled")   l.enabled = ToBool(f);
                else WarnUnknown(f);
            }
        }

        Scene&     m_scene;
        IRenderer& m_renderer;
        NameMap    m_meshes;    // nom → index runtime, pour ce fichier
        NameMap    m_materials;
        std::unordered_map<EntityUUID, EntityID> m_entities;
    };

    // -------------------------------------------------------
    // Écriture
    // -------------------------------------------------------
    std::string Float(float v)
    {
        // Représentation la plus courte qui se relit à l'identique, indépendante de la locale
        char buf[32];
        return std::string(buf, std::to_chars(buf, buf + sizeof(buf), v).ptr);
    }

    std::string Floats(std::initializer_list<float> values)
    {
        std::string s = "[";
        for (float v : values)
            s += (s.size() > 1 ? ", " : "") + Float(v);
        return s + "]";
    }

    std::string Quoted(const std::string& s)
    {
        std::string q = "\"";
        for (char c : s)
        {
            if (c == '"' || c == '\\') q += '\\';
            q += c;
        }
        return q + "\"";
    }

    std::string UUIDString(EntityUUID uuid)
    {
        char buf[19];
        snprintf(buf, sizeof(buf), "0x%016llx", static_cast<unsigned long long>(uuid));
        return buf;
    }

    const char* Bool(bool b) { return b ? "true" : "false"; }

    struct Writer
    {
        std::string out;

        // value vide : clé d'un bloc imbriqué
        void Field(int indent, std::string_view key, std::string_view value = {})
        {
            out.append(indent, ' ');
            out += key;
            out += ':';
            if (!value.empty())
            {
                out += ' ';
                out += value;
            }
            out += '\n';
        }

        // Première entrée d'un élément de liste ("- clé: valeur")
        void Item(int indent, std::string_view key, std::string_view value)
        {
            out.append(indent, ' ');
            out += "- ";
            Field(0, key, value);
        }
    };

    class SceneWriter
    {
    public:
        SceneWriter(Scene& scene, const IRenderer& renderer) : m_scene(scene), m_renderer(renderer) {}

        std::string Write()
        {
            // Parcours de la hiérarchie (jamais l'ordre des unordered_map) : parent avant enfants
            for (EntityID root : m_scene.GetRootEntities())
                WriteEntity(root);

            // Seuls les meshes/matériaux référencés, dans l'ordre de première référence
            Writer w;
            w.out = "# CoriumEngine scene, format: doc/SceneFormat.md\n";
            w.Field(0, "version", "1");

            w.Field(0, "meshes", m_meshes.empty() ? "[]" : "");
            for (uint32_t index : m_meshes)
            {
                const MeshDesc& d = *m_renderer.GetMeshDesc(index);
                w.Item(2, "name", Quoted(d.name));
                w.Field(4, "primitive", NameOf(d.primitive, kPrimitives));
                w.Field(4, "size", Float(d.size));
            }

            w.Field(0, "materials", m_materials.empty() ? "[]" : "");
            for (uint32_t index : m_materials)
            {
                const MaterialDesc& d = *m_renderer.GetMaterialDesc(index);
                w.Item(2, "name", Quoted(d.name));
                w.Field(4, "baseColor", Floats({ d.baseColor.x, d.baseColor.y, d.baseColor.z, d.baseColor.w }));
                w.Field(4, "cullMode", NameOf(d.cullMode, kCullModes));
                w.Field(4, "fillMode", NameOf(d.fillMode, kFillModes));
                w.Field(4, "blendMode", NameOf(d.blendMode, kBlendModes));
            }

            w.Field(0, "entities", m_entities.out.empty() ? "[]" : "");
            return w.out + m_entities.out;
        }

    private:
        static void Reference(std::vector<uint32_t>& list, uint32_t index)
        {
            if (std::find(list.begin(), list.end(), index) == list.end())
                list.push_back(index);
        }

        void WriteEntity(EntityID id)
        {
            const Entity* e = m_scene.GetEntity(id);
            const auto* h = m_scene.GetComponent<HierarchyComponent>(id);
            Writer& w = m_entities;

            w.Item(2, "uuid", UUIDString(e->uuid));
            w.Field(4, "name", Quoted(e->name));
            w.Field(4, "active", Bool(e->active));
            if (h && h->HasParent())
                w.Field(4, "parent", UUIDString(m_scene.GetEntity(h->parent)->uuid));

            if (const auto* t = m_scene.GetComponent<TransformComponent>(id))
            {
                w.Field(4, "transform");
                w.Field(6, "position", Floats({ t->position.x, t->position.y, t->position.z }));
                w.Field(6, "rotation", Floats({ t->rotation.x, t->rotation.y, t->rotation.z, t->rotation.w }));
                w.Field(6, "scale", Floats({ t->scale.x, t->scale.y, t->scale.z }));
            }

            if (const auto* m = m_scene.GetComponent<MeshComponent>(id))
            {
                w.Field(4, "mesh");
                // Mesh absent : non écrit. Matériau 0 (« Default », créé par le renderer) : implicite.
                if (const MeshDesc* d = m_renderer.GetMeshDesc(m->meshIndex))
                {
                    w.Field(6, "mesh", Quoted(d->name));
                    Reference(m_meshes, m->meshIndex);
                }
                const MaterialDesc* mat = m_renderer.GetMaterialDesc(m->materialIndex);
                if (m->materialIndex != 0 && mat)
                {
                    w.Field(6, "material", Quoted(mat->name));
                    Reference(m_materials, m->materialIndex);
                }
                w.Field(6, "visible", Bool(m->visible));
                w.Field(6, "castShadows", Bool(m->castShadows));
            }

            if (const auto* c = m_scene.GetComponent<CameraComponent>(id))
            {
                w.Field(4, "camera");
                w.Field(6, "fovY", Float(c->fovY));
                w.Field(6, "nearPlane", Float(c->nearPlane));
                w.Field(6, "farPlane", Float(c->farPlane));
                w.Field(6, "isMain", Bool(c->isMain));
            }

            if (const auto* l = m_scene.GetComponent<LightComponent>(id))
            {
                w.Field(4, "light");
                w.Field(6, "type", NameOf(l->type, kLightTypes));
                w.Field(6, "color", Floats({ l->color.x, l->color.y, l->color.z }));
                w.Field(6, "intensity", Float(l->intensity));
                w.Field(6, "range", Float(l->range));
                w.Field(6, "spotAngle", Float(l->spotAngle));
                w.Field(6, "enabled", Bool(l->enabled));
            }

            if (h)
                for (EntityID child : h->children)
                    WriteEntity(child);
        }

        Scene&                m_scene;
        const IRenderer&      m_renderer;
        Writer                m_entities;
        std::vector<uint32_t> m_meshes;
        std::vector<uint32_t> m_materials;
    };

    // -------------------------------------------------------
    // Texte ↔ scène
    // -------------------------------------------------------
    bool Read(const std::string& text, Scene& scene, IRenderer& renderer, std::string* error)
    {
        try
        {
            const Node root = Parser(text).ParseDocument();
            Scene loaded;
            SceneReader(loaded, renderer).Read(root);
            scene = std::move(loaded);
            return true;
        }
        catch (const ParseError& e)
        {
            if (error) *error = "line " + std::to_string(e.line) + ": " + e.message;
            return false;
        }
    }

    std::string Write(Scene& scene, const IRenderer& renderer)
    {
        return SceneWriter(scene, renderer).Write();
    }

    bool ReadFile(const std::wstring& path, std::string& text)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;
        std::ostringstream content;
        content << file.rdbuf();
        text = content.str();
        if (text.starts_with("\xEF\xBB\xBF")) text.erase(0, 3); // BOM UTF-8 (Bloc-notes)
        return true;
    }
}

bool SceneSerializer::Load(const std::wstring& path, Scene& scene, IRenderer& renderer, std::string* error)
{
    std::string text;
    if (!ReadFile(path, text))
    {
        if (error) *error = "cannot open file";
        return false;
    }
    return Read(text, scene, renderer, error);
}

bool SceneSerializer::Save(const std::wstring& path, Scene& scene, const IRenderer& renderer, std::string* error)
{
    // Fichier temporaire puis remplacement : une écriture interrompue ne détruit pas la scène existante
    const std::wstring tmpPath = path + L".tmp";
    {
        std::ofstream file(tmpPath, std::ios::binary | std::ios::trunc);
        file << Write(scene, renderer);
        if (!file)
        {
            if (error) *error = "cannot write file";
            return false;
        }
    }
    if (!MoveFileExW(tmpPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING))
    {
        if (error) *error = "cannot replace file";
        return false;
    }
    return true;
}

void SceneSerializer::SelfCheck(const std::wstring& path, IRenderer& renderer)
{
    // Ce que la scène de démo ne couvre pas : hiérarchie sur 3 niveaux, ordre des enfants,
    // chaîne échappée, commentaires, entité sans uuid, champ inconnu, sections dans le désordre
    const std::string sample = R"(# commentaire
version: 1
entities:
  - uuid: 0x00000000000000a1
    name: "Root \"quoted\" # not a comment"
    active: false
    transform:
      position: [1.5, -2, 0.1]
      rotation: [0, 0.7071068, 0, 0.7071068]
      scale: [1, 2, 3]
    light:
      type: spot
      color: [1, 0.5, 0.25]
      spotAngle: 45
  - uuid: 0x00000000000000b2
    name: "Child B"
    parent: 0x00000000000000a1
    mesh:
      mesh: "selfcheck_box"
      material: "SelfCheck"
      castShadows: false
    futureComponent: 1 # inconnu : avertissement
  - name: "Grandchild without uuid"
    parent: 0x00000000000000b2
    camera:
      fovY: 75
  - uuid: 0x00000000000000c3
    name: "Child A"
    parent: 0x00000000000000a1
materials:
  - name: "SelfCheck"
    baseColor: [0.2, 0.4, 0.6, 0.5]
    cullMode: none
    fillMode: wireframe
    blendMode: alphaBlend
meshes:
  - name: "selfcheck_box"
    primitive: box
    size: 0.5
)";

    const auto roundTrip = [&renderer](const std::string& text)
    {
        Scene first, second;
        std::string error;
        const bool loaded = Read(text, first, renderer, &error);
        if (!loaded)
            OutputDebugStringA(("SceneSerializer::SelfCheck: " + error + "\n").c_str());
        CE_ASSERT(loaded);
        const std::string saved = Write(first, renderer);
        CE_ASSERT(Read(saved, second, renderer, &error));
        CE_ASSERT(Write(second, renderer) == saved);
    };

    std::string demo;
    CE_ASSERT(ReadFile(path, demo));
    roundTrip(demo);
    roundTrip(sample);

    // Erreur de syntaxe : échec avec numéro de ligne, scène inchangée
    Scene untouched;
    std::string error;
    CE_ASSERT(!Read("version: 1\nentities:\n  - name: \"x\"\n      bad: 1\n", untouched, renderer, &error));
    CE_ASSERT(error.starts_with("line 4:"));
    CE_ASSERT(untouched.GetRootEntities().empty());
}
