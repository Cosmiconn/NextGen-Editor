// ObjectEditing.cpp
// Objekt-Transformationen, Auswahl, Ausrichten/Verteilen, Kopieren, Löschen, Modellpfade.

#include "EditorShared.hpp"

namespace nge {

EditQuat NormalizeEditQuat(EditQuat q) {
    const float len = std::sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
    if (len < 1.0e-8f) return {};
    return {q.x/len, q.y/len, q.z/len, q.w/len};
}
EditQuat MulEditQuatRaw(const EditQuat& a, const EditQuat& b) {
    return {
        a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
        a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
        a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w,
        a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z
    };
}
EditQuat MulEditQuat(const EditQuat& a, const EditQuat& b) {
    return NormalizeEditQuat(MulEditQuatRaw(a,b));
}
EditQuat ConjugateEditQuat(const EditQuat& q) { return {-q.x,-q.y,-q.z,q.w}; }
EditVec3 RotateEditVec(const EditQuat& qn, const EditVec3& v) {
    const EditQuat q = NormalizeEditQuat(qn);
    const EditQuat p{v.x,v.y,v.z,0.0f};
    const EditQuat r = MulEditQuatRaw(MulEditQuatRaw(q,p), ConjugateEditQuat(q));
    return {r.x,r.y,r.z};
}
app::Mat4 ObjectEditMatrix(const EditVec3& position, const EditQuat& qn, float scale) {
    const EditQuat q = NormalizeEditQuat(qn);
    app::Mat4 m = app::Mat4::Identity();
    m.m[0] = (1.0f - 2.0f*(q.y*q.y + q.z*q.z)) * scale;
    m.m[1] = (2.0f*(q.x*q.y + q.w*q.z)) * scale;
    m.m[2] = (2.0f*(q.x*q.z - q.w*q.y)) * scale;
    m.m[4] = (2.0f*(q.x*q.y - q.w*q.z)) * scale;
    m.m[5] = (1.0f - 2.0f*(q.x*q.x + q.z*q.z)) * scale;
    m.m[6] = (2.0f*(q.y*q.z + q.w*q.x)) * scale;
    m.m[8] = (2.0f*(q.x*q.z + q.w*q.y)) * scale;
    m.m[9] = (2.0f*(q.y*q.z - q.w*q.x)) * scale;
    m.m[10] = (1.0f - 2.0f*(q.x*q.x + q.y*q.y)) * scale;
    m.m[12] = position.x; m.m[13] = position.y; m.m[14] = position.z;
    return m;
}
float MatrixUniformScale(const app::Mat4& m) {
    const auto len = [&](int c) {
        return std::sqrt(m.m[c*4+0]*m.m[c*4+0] + m.m[c*4+1]*m.m[c*4+1] + m.m[c*4+2]*m.m[c*4+2]);
    };
    return std::max(1.0e-6f, (len(0)+len(1)+len(2))/3.0f);
}
EditQuat MatrixRotationQuat(const app::Mat4& m) {
    const float s = MatrixUniformScale(m);
    const float r00=m.m[0]/s, r01=m.m[4]/s, r02=m.m[8]/s;
    const float r10=m.m[1]/s, r11=m.m[5]/s, r12=m.m[9]/s;
    const float r20=m.m[2]/s, r21=m.m[6]/s, r22=m.m[10]/s;
    EditQuat q;
    const float trace = r00+r11+r22;
    if (trace > 0.0f) {
        const float t = std::sqrt(trace+1.0f)*2.0f;
        q.w=0.25f*t; q.x=(r21-r12)/t; q.y=(r02-r20)/t; q.z=(r10-r01)/t;
    } else if (r00 > r11 && r00 > r22) {
        const float t = std::sqrt(1.0f+r00-r11-r22)*2.0f;
        q.w=(r21-r12)/t; q.x=0.25f*t; q.y=(r01+r10)/t; q.z=(r02+r20)/t;
    } else if (r11 > r22) {
        const float t = std::sqrt(1.0f+r11-r00-r22)*2.0f;
        q.w=(r02-r20)/t; q.x=(r01+r10)/t; q.y=0.25f*t; q.z=(r12+r21)/t;
    } else {
        const float t = std::sqrt(1.0f+r22-r00-r11)*2.0f;
        q.w=(r10-r01)/t; q.x=(r02+r20)/t; q.y=(r12+r21)/t; q.z=0.25f*t;
    }
    return NormalizeEditQuat(q);
}
ObjectSelectionPivot ComputeObjectSelectionPivot(const EditorState& state) {
    ObjectSelectionPivot p;
    const core::PlacedObject* orientationSource = nullptr;
    for (const int id : state.selectedObjects) {
        if (IsObjectEditorLocked(state,id)) continue;
        const auto* obj = EditableObject(state,id);
        if (!obj) continue;
        p.position.x += obj->posX; p.position.y += obj->posY; p.position.z += obj->posZ;
        ++p.editableCount;
        if (!orientationSource) orientationSource = obj;
    }
    if (p.editableCount == 0 || !orientationSource) return p;
    const float inv = 1.0f/static_cast<float>(p.editableCount);
    p.position.x*=inv; p.position.y*=inv; p.position.z*=inv;
    if (state.pivotOverride) {
        std::string key;
        for (const int id : state.selectedObjects) key += std::to_string(id) + ",";
        if (key == state.pivotOverrideSelectionKey) {
            p.position = {(*state.pivotOverride)[0], (*state.pivotOverride)[1], (*state.pivotOverride)[2]};
            p.rotation = NormalizeEditQuat({orientationSource->rotX,orientationSource->rotY,
                                            orientationSource->rotZ,orientationSource->rotW});
            p.scale = p.editableCount == 1 ? std::max(0.001f,orientationSource->scale) : 1.0f;
            p.valid = true;
            return p;
        }
    }
    if (state.pivotAtActiveObject) {
        // Wie in Unreal: Pivot und Ausrichtung des aktiven (zuletzt gewählten) Objekts.
        for (auto it = state.selectedObjects.rbegin(); it != state.selectedObjects.rend(); ++it) {
            if (IsObjectEditorLocked(state,*it)) continue;
            if (const auto* active = EditableObject(state,*it)) {
                p.position = {active->posX, active->posY, active->posZ};
                orientationSource = active;
                break;
            }
        }
    }
    p.rotation = NormalizeEditQuat({orientationSource->rotX,orientationSource->rotY,
                                    orientationSource->rotZ,orientationSource->rotW});
    p.scale = p.editableCount == 1 ? std::max(0.001f,orientationSource->scale) : 1.0f;
    p.valid = true;
    return p;
}
void RotateSelectedObjectsAroundPivot(EditorState& state, const EditVec3& pivot, const EditQuat& delta) {
    if (!RequireWritableMapGeometry(state)) return;
    auto historyBefore = BeginObjectMutation(state);
    bool changed = PromoteSelectedShmdObjectsToPlacements(state);
    SyncObjectEditorMetadata(state);
    for (const int id : state.selectedObjects) {
        if (id < 0 || static_cast<std::size_t>(id) >= state.placementSet.Count() || IsObjectEditorLocked(state,id)) continue;
        auto& obj=state.placementSet.At(static_cast<std::size_t>(id));
        const EditVec3 rel{obj.posX-pivot.x,obj.posY-pivot.y,obj.posZ-pivot.z};
        const EditVec3 rr=RotateEditVec(delta,rel);
        obj.posX=pivot.x+rr.x; obj.posY=pivot.y+rr.y; obj.posZ=pivot.z+rr.z;
        const EditQuat oq{obj.rotX,obj.rotY,obj.rotZ,obj.rotW};
        const EditQuat nq=MulEditQuat(delta,oq);
        obj.rotX=nq.x; obj.rotY=nq.y; obj.rotZ=nq.z; obj.rotW=nq.w;
        changed = true;
    }
    FinishObjectMutation(state, std::move(historyBefore), changed);
}
void ScaleSelectedObjectsAroundPivot(EditorState& state, const EditVec3& pivot, float factor) {
    if (!RequireWritableMapGeometry(state)) return;
    if (!std::isfinite(factor) || factor <= 0.0f) return;
    auto historyBefore = BeginObjectMutation(state);
    bool changed = PromoteSelectedShmdObjectsToPlacements(state);
    SyncObjectEditorMetadata(state);
    for (const int id : state.selectedObjects) {
        if (id < 0 || static_cast<std::size_t>(id) >= state.placementSet.Count() || IsObjectEditorLocked(state,id)) continue;
        auto& obj=state.placementSet.At(static_cast<std::size_t>(id));
        obj.posX=pivot.x+(obj.posX-pivot.x)*factor;
        obj.posY=pivot.y+(obj.posY-pivot.y)*factor;
        obj.posZ=pivot.z+(obj.posZ-pivot.z)*factor;
        const float oldScale = obj.scale;
        obj.scale=std::clamp(obj.scale*factor,0.01f,100.0f);
        changed = changed || std::abs(obj.scale-oldScale) > 1.0e-8f;
    }
    FinishObjectMutation(state, std::move(historyBefore), changed);
}
void CopySelectedObjects(EditorState& state) {
    state.objectClipboard.clear();
    state.objectClipboardLabels.clear();
    state.objectClipboardGroups.clear();
    for (const int id : state.selectedObjects) {
        if (const auto* obj=EditableObject(state,id)) {
            state.objectClipboard.push_back(*obj);
            state.objectClipboardLabels.push_back(ObjectEditorLabel(state,id));
            state.objectClipboardGroups.push_back(ObjectEditorGroup(state,id));
        }
    }
    state.statusMessage=state.objectClipboard.empty()
        ? "Keine Objekte kopiert."
        : std::to_string(state.objectClipboard.size())+" Objekt(e) kopiert.";
}
void PasteObjectClipboard(EditorState& state) {
    if (!RequireWritableMapGeometry(state)) return;
    if (state.objectClipboard.empty()) return;
    auto historyBefore = BeginObjectMutation(state);
    SyncObjectEditorMetadata(state);
    const float offset=state.objectGizmoSnap ? std::max(1.0f,state.objectMoveSnap) : 50.0f;
    std::vector<int> ids;
    for (std::size_t i=0;i<state.objectClipboard.size();++i) {
        auto obj=state.objectClipboard[i];
        obj.posX+=offset; obj.posZ+=offset;
        obj.sourceIndex=-1; // Kopie ist ein neues Objekt (keine IDM-Zuordnung)
        const int id=static_cast<int>(state.placementSet.AddObject(std::move(obj)));
        ids.push_back(id);
        state.objectEditorHidden.push_back(0); state.objectEditorLocked.push_back(0);
        state.objectEditorLabels.push_back(i<state.objectClipboardLabels.size()?state.objectClipboardLabels[i]:std::string{});
        state.objectEditorGroups.push_back(i<state.objectClipboardGroups.size()?state.objectClipboardGroups[i]:std::string{});
    }
    state.selectedObjects=std::move(ids);
    state.selectedObject=state.selectedObjects.empty()?kNoObjectSelection:state.selectedObjects.back();
    ReloadObjectRenderers(state);
    state.statusMessage=std::to_string(state.selectedObjects.size())+" Objekt(e) eingefügt.";
    FinishObjectMutation(state, std::move(historyBefore));
}
// offsetOverride: 0 = exakt an Ort und Stelle (Alt+Ziehen mit dem Gizmo wie in Unreal).
void DuplicateSelectedObjects(EditorState& state, std::optional<float> offsetOverride) {
    if (!RequireWritableMapGeometry(state)) return;
    if (state.selectedObjects.empty()) return;
    auto historyBefore = BeginObjectMutation(state);
    struct ObjectCopy { core::PlacedObject object; std::string label; std::string group; };
    std::vector<ObjectCopy> copies;
    for (const int id : state.selectedObjects)
        if (const auto* obj=EditableObject(state,id))
            copies.push_back({*obj,ObjectEditorLabel(state,id),ObjectEditorGroup(state,id)});
    if (copies.empty()) return;
    const float offset=offsetOverride ? *offsetOverride
                                      : (state.objectGizmoSnap ? std::max(1.0f,state.objectMoveSnap) : 50.0f);
    SyncObjectEditorMetadata(state);
    std::vector<int> ids;
    for (auto& copy : copies) {
        copy.object.posX+=offset; copy.object.posZ+=offset;
        copy.object.sourceIndex=-1; // Kopie ist ein neues Objekt (keine IDM-Zuordnung)
        const int id=static_cast<int>(state.placementSet.AddObject(std::move(copy.object)));
        ids.push_back(id);
        state.objectEditorHidden.push_back(0); state.objectEditorLocked.push_back(0);
        state.objectEditorLabels.push_back(copy.label.empty()?std::string{}:copy.label+" Kopie");
        state.objectEditorGroups.push_back(copy.group);
    }
    state.selectedObjects=std::move(ids);
    state.selectedObject=state.selectedObjects.back();
    ReloadObjectRenderers(state);
    state.statusMessage=std::to_string(state.selectedObjects.size())+" Objekt(e) dupliziert.";
    FinishObjectMutation(state, std::move(historyBefore));
}
void GroundSelectedObjects(EditorState& state) {
    if (!RequireWritableMapGeometry(state)) return;
    if (state.selectedObjects.empty()) return;
    if (!MapSupportsTerrainEditing(state)) {
        state.statusMessage = L("Auf Terrain setzen ist für eine NIF-only-Karte ohne HTD nicht verfügbar.",
                                "Drop to terrain is unavailable for a NIF-only map without HTD.");
        return;
    }
    auto historyBefore = BeginObjectMutation(state);
    bool changed = PromoteSelectedShmdObjectsToPlacements(state);
    SyncObjectEditorMetadata(state);
    for (const int id : state.selectedObjects) {
        if (id < 0 || static_cast<std::size_t>(id) >= state.placementSet.Count() || IsObjectEditorLocked(state,id)) continue;
        auto& obj=state.placementSet.At(static_cast<std::size_t>(id));
        const float grounded = state.heightmap.SampleWorld(obj.posX,obj.posZ);
        changed = changed || std::abs(obj.posY-grounded) > 1.0e-6f;
        obj.posY=grounded;
    }
    state.statusMessage="Auswahl auf Terrain gesetzt.";
    FinishObjectMutation(state, std::move(historyBefore), changed);
}
void FocusSelectedObjects(EditorState& state) {
    const auto pivot=ComputeObjectSelectionPivot(state);
    if (!pivot.valid) return;
    float radius=150.0f;
    for (const int id : state.selectedObjects) {
        const auto* obj=EditableObject(state,id);
        if (!obj) continue;
        const float dx=obj->posX-pivot.position.x, dy=obj->posY-pivot.position.y, dz=obj->posZ-pivot.position.z;
        radius=std::max(radius,std::sqrt(dx*dx+dy*dy+dz*dz)+100.0f*std::max(1.0f,obj->scale));
    }
    state.camera.SetTarget(pivot.position.x,pivot.position.y,pivot.position.z);
    const float desired=std::clamp(radius*2.8f,120.0f,25000.0f);
    state.camera.Zoom(desired-state.camera.Distance());
}
// ---- Editor-Sichtbarkeit: Hide Selected (H) / Hide Unselected (Shift+H) / Show All (Strg+H) ----
// Rein editorseitig (objectEditorHidden bzw. shmdEditorHiddenKeys) - SHMD-Export bleibt unberührt.
void SetObjectEditorHiddenFlag(EditorState& state, int id, bool hidden) {
    if (id >= 0) {
        SyncObjectEditorMetadata(state);
        const auto index = static_cast<std::size_t>(id);
        if (index < state.objectEditorHidden.size()) state.objectEditorHidden[index] = hidden ? 1 : 0;
        state.objectVisKey.clear();
        return;
    }
    const std::string key = ShmdEditorObjectKey(state, id);
    if (key.empty()) return;
    if (hidden) state.shmdEditorHiddenKeys.insert(key);
    else state.shmdEditorHiddenKeys.erase(key);
}
std::size_t CountEditorHiddenObjects(const EditorState& state) {
    const auto normal = static_cast<std::size_t>(
        std::count_if(state.objectEditorHidden.begin(), state.objectEditorHidden.end(), [](char c) { return c != 0; }));
    return normal + state.shmdEditorHiddenKeys.size();
}
void HideSelectedObjects(EditorState& state) {
    if (state.selectedObjects.empty()) return;
    const std::size_t count = state.selectedObjects.size();
    for (const int id : state.selectedObjects) SetObjectEditorHiddenFlag(state, id, true);
    state.selectedObjects.clear();
    state.selectedObject = kNoObjectSelection;
    state.objectGizmoMatrixValid = false;
    state.statusMessage = std::to_string(count) + L(" Objekt(e) im Editor ausgeblendet (Strg+H blendet alle ein).",
                                                   " object(s) hidden in the editor (Ctrl+H shows all).");
}
void HideUnselectedObjects(EditorState& state) {
    if (state.selectedObjects.empty()) return;
    const std::unordered_set<int> keep(state.selectedObjects.begin(), state.selectedObjects.end());
    std::size_t hidden = 0;
    for (std::size_t i = 0; i < state.placementSet.Count(); ++i) {
        if (keep.contains(static_cast<int>(i))) continue;
        SetObjectEditorHiddenFlag(state, static_cast<int>(i), true);
        ++hidden;
    }
    for (std::size_t i = 0; i < state.shmdCategoryRenderSet.Count(); ++i) {
        const int id = ShmdSelectionId(i);
        if (keep.contains(id)) continue;
        SetObjectEditorHiddenFlag(state, id, true);
        ++hidden;
    }
    state.statusMessage = std::string(L("Auswahl isoliert: ", "Selection isolated: ")) + std::to_string(hidden) +
                          L(" andere Objekte ausgeblendet.", " other objects hidden.");
}
void ShowAllObjects(EditorState& state) {
    const std::size_t count = CountEditorHiddenObjects(state);
    std::fill(state.objectEditorHidden.begin(), state.objectEditorHidden.end(), 0);
    state.shmdEditorHiddenKeys.clear();
    state.objectVisKey.clear();
    state.statusMessage = std::to_string(count) + L(" ausgeblendete(s) Objekt(e) wieder eingeblendet.",
                                                   " hidden object(s) shown again.");
}
// ---- Ausrichten / Verteilen / Transform übernehmen -------------------------------------------
// axis: 0 = X (Ost), 1 = Y (Höhe), 2 = Z (Server-Y / Nord). Der zuletzt gewählte Eintrag ist
// das "aktive" Objekt (wie in Unreal das zuletzt angeklickte Objekt).
std::vector<int> EditableSelectedPlacementIds(const EditorState& state) {
    std::vector<int> ids;
    for (const int id : state.selectedObjects)
        if (id >= 0 && static_cast<std::size_t>(id) < state.placementSet.Count() && !IsObjectEditorLocked(state, id))
            ids.push_back(id);
    return ids;
}
float& PlacementAxis(core::PlacedObject& object, int axis) {
    return axis == 0 ? object.posX : (axis == 1 ? object.posY : object.posZ);
}
void AlignSelectedObjects(EditorState& state, int axis, core::level::AlignTarget target) {
    if (!RequireWritableMapGeometry(state) || state.selectedObjects.size() < 2) return;
    auto historyBefore = BeginObjectMutation(state);
    bool changed = PromoteSelectedShmdObjectsToPlacements(state);
    const auto ids = EditableSelectedPlacementIds(state);
    if (ids.size() < 2) { FinishObjectMutation(state, std::move(historyBefore), changed); return; }
    std::vector<float> values;
    for (const int id : ids) values.push_back(PlacementAxis(state.placementSet.At(static_cast<std::size_t>(id)), axis));
    const std::size_t active = ids.size() - 1;
    const auto aligned = core::level::AlignValues(values, target, active);
    for (std::size_t i = 0; i < ids.size(); ++i) {
        float& v = PlacementAxis(state.placementSet.At(static_cast<std::size_t>(ids[i])), axis);
        changed = changed || std::abs(v - aligned[i]) > 1.0e-6f;
        v = aligned[i];
    }
    state.objectGizmoMatrixValid = false;
    state.statusMessage = std::to_string(ids.size()) + L(" Objekte ausgerichtet.", " objects aligned.");
    FinishObjectMutation(state, std::move(historyBefore), changed);
}
void DistributeSelectedObjects(EditorState& state, int axis) {
    if (!RequireWritableMapGeometry(state) || state.selectedObjects.size() < 3) return;
    auto historyBefore = BeginObjectMutation(state);
    bool changed = PromoteSelectedShmdObjectsToPlacements(state);
    const auto ids = EditableSelectedPlacementIds(state);
    std::vector<float> values;
    for (const int id : ids) values.push_back(PlacementAxis(state.placementSet.At(static_cast<std::size_t>(id)), axis));
    const auto distributed = core::level::DistributeValues(values);
    for (std::size_t i = 0; i < ids.size(); ++i) {
        float& v = PlacementAxis(state.placementSet.At(static_cast<std::size_t>(ids[i])), axis);
        changed = changed || std::abs(v - distributed[i]) > 1.0e-6f;
        v = distributed[i];
    }
    state.objectGizmoMatrixValid = false;
    state.statusMessage = std::to_string(ids.size()) + L(" Objekte gleichmäßig verteilt.", " objects distributed evenly.");
    FinishObjectMutation(state, std::move(historyBefore), changed);
}
// Rotation und/oder Skalierung des aktiven (zuletzt gewählten) Objekts auf die übrige Auswahl übertragen.
void MatchSelectedObjectsToActive(EditorState& state, bool rotation, bool scale) {
    if (!RequireWritableMapGeometry(state) || state.selectedObjects.size() < 2) return;
    auto historyBefore = BeginObjectMutation(state);
    bool changed = PromoteSelectedShmdObjectsToPlacements(state);
    const auto ids = EditableSelectedPlacementIds(state);
    if (ids.size() < 2) { FinishObjectMutation(state, std::move(historyBefore), changed); return; }
    const core::PlacedObject active = state.placementSet.At(static_cast<std::size_t>(ids.back()));
    for (std::size_t i = 0; i + 1 < ids.size(); ++i) {
        auto& o = state.placementSet.At(static_cast<std::size_t>(ids[i]));
        if (rotation) { o.rotX = active.rotX; o.rotY = active.rotY; o.rotZ = active.rotZ; o.rotW = active.rotW; }
        if (scale) o.scale = active.scale;
        changed = true;
    }
    state.objectGizmoMatrixValid = false;
    state.statusMessage = L("Transform vom aktiven Objekt übernommen.", "Transform matched to the active object.");
    FinishObjectMutation(state, std::move(historyBefore), changed);
}
// Begehbarer Boden einer Fiesta-Karte = HTD-Terrain UND die SHMD-Kategorie "GroundObject"
// (z.B. das Pflaster von Roumen, Rou_ground2_CD.nif, liegt deutlich über dem HTD). Die
// GroundObject-Meshes werden senkrecht von oben per Dreiecks-Raycast abgetastet.
std::optional<float> GroundObjectHeightAt(const EditorState& state, float x, float z) {
    constexpr float kRayTop = 100000.0f;
    std::optional<float> best;
    const std::size_t count = std::min(state.shmdCategoryRenderSet.Count(), state.shmdCategoryRenderKind.size());
    for (std::size_t i = 0; i < count; ++i) {
        if (state.shmdCategoryRenderKind[i] != 2 || !state.shmdCategoryMeshRenderer.HasRealMesh(i)) continue;
        if (const auto t = state.shmdCategoryMeshRenderer.RaycastObject(
                state.shmdCategoryRenderSet, i, state.camera, {x, kRayTop, z}, {0.0f, -1.0f, 0.0f})) {
            const float y = kRayTop - *t;
            if (!best || y > *best) best = y;
        }
    }
    return best;
}
// Höchste begehbare Oberfläche (HTD bzw. GroundObject); `fallback`, wenn keine vorhanden ist.
float SurfaceHeightAt(const EditorState& state, float x, float z, float fallback, bool* found) {
    const bool terrain = MapSupportsTerrainEditing(state);
    float y = terrain ? state.heightmap.SampleWorld(x, z) : fallback;
    bool any = terrain;
    if (const auto ground = GroundObjectHeightAt(state, x, z)) {
        y = any ? std::max(y, *ground) : *ground;
        any = true;
    }
    if (found) *found = any;
    return y;
}
// Surface Snap: verschobene Objekte folgen dem begehbaren Boden (HTD + SHMD-GroundObject).
void SnapSelectedObjectsToTerrainSilently(EditorState& state) {
    for (const int id : state.selectedObjects) {
        if (id < 0 || static_cast<std::size_t>(id) >= state.placementSet.Count() || IsObjectEditorLocked(state, id)) continue;
        auto& obj = state.placementSet.At(static_cast<std::size_t>(id));
        bool found = false;
        const float grounded = SurfaceHeightAt(state, obj.posX, obj.posZ, obj.posY, &found);
        if (!found) continue;
        if (std::abs(obj.posY - grounded) > 1.0e-6f) {
            obj.posY = grounded;
            if (state.pendingObjectEdit) state.pendingObjectEditChanged = true;
        }
    }
}
void SelectObjectId(EditorState& state, int id, bool ctrl) {
    if (ctrl) {
        auto it = std::find(state.selectedObjects.begin(), state.selectedObjects.end(), id);
        if (it == state.selectedObjects.end()) state.selectedObjects.push_back(id);
        else state.selectedObjects.erase(it);
    } else {
        state.selectedObjects = {id};
    }
    state.selectedObject = state.selectedObjects.empty() ? kNoObjectSelection : state.selectedObjects.back();
    state.selectedObjectModelPathFor = kNoObjectSelection;
    state.selectedObjectModelPathDirty = false;
}
void ClearObjectSelection(EditorState& state) {
    state.selectedObjects.clear();
    state.selectedObject = kNoObjectSelection;
    state.selectedObjectModelPathFor = kNoObjectSelection;
    state.selectedObjectModelPathDirty = false;
    state.objectListRangeAnchor = -1;
}
void SelectAllNormalObjects(EditorState& state) {
    state.selectedObjects.clear();
    state.selectedObjects.reserve(state.placementSet.Count());
    for (std::size_t i = 0; i < state.placementSet.Count(); ++i)
        state.selectedObjects.push_back(static_cast<int>(i));
    state.selectedObject = state.selectedObjects.empty() ? kNoObjectSelection : state.selectedObjects.back();
    state.selectedObjectModelPathFor = kNoObjectSelection;
    state.selectedObjectModelPathDirty = false;
    state.objectListRangeAnchor = -1;
}
void SelectObjectFromList(EditorState& state, int id, int ordinal,
                          const std::vector<int>& orderedIds, bool ctrl, bool shift) {
    if (shift && state.objectListRangeAnchor >= 0 &&
        state.objectListRangeAnchor < static_cast<int>(orderedIds.size())) {
        if (!ctrl) state.selectedObjects.clear();
        const int lo = std::min(state.objectListRangeAnchor, ordinal);
        const int hi = std::max(state.objectListRangeAnchor, ordinal);
        for (int i = lo; i <= hi; ++i) {
            const int candidate = orderedIds[static_cast<std::size_t>(i)];
            if (std::find(state.selectedObjects.begin(), state.selectedObjects.end(), candidate) == state.selectedObjects.end())
                state.selectedObjects.push_back(candidate);
        }
        state.selectedObject = id;
        state.selectedObjectModelPathFor = kNoObjectSelection;
        state.selectedObjectModelPathDirty = false;
    } else {
        SelectObjectId(state, id, ctrl);
    }
    state.objectListRangeAnchor = ordinal;
}
// Auf der 2D-Karte darf ein normaler Klick auf EIN BEREITS ausgewähltes Objekt die bestehende
// Mehrfachauswahl nicht zerstören: genau dieser Klick ist der Start eines Gruppen-Drags.
void SelectObjectOnCanvas(EditorState& state, int id, bool ctrl) {
    const auto it = std::find(state.selectedObjects.begin(), state.selectedObjects.end(), id);
    const bool alreadySelected = it != state.selectedObjects.end();
    if (!ctrl && alreadySelected && state.selectedObjects.size() > 1) {
        state.selectedObject = id;
        state.selectedObjectModelPathFor = kNoObjectSelection;
        state.selectedObjectModelPathDirty = false;
        return;
    }
    SelectObjectId(state, id, ctrl);
}
void EraseShmdCategorySources(EditorState& state,
                              std::vector<std::pair<std::size_t, std::size_t>> sources) {
    std::sort(sources.begin(), sources.end(), [](const auto& a, const auto& b) {
        if (a.first != b.first) return a.first > b.first;
        return a.second > b.second;
    });
    sources.erase(std::unique(sources.begin(), sources.end()), sources.end());
    for (const auto [categoryIndex, pathIndex] : sources) {
        if (categoryIndex >= state.placementSet.categories.size()) continue;
        auto& paths = state.placementSet.categories[categoryIndex].modelPaths;
        if (pathIndex < paths.size()) paths.erase(paths.begin() + static_cast<std::ptrdiff_t>(pathIndex));
    }
}
void DeleteSelectedObjects(EditorState& state) {
    if (!RequireWritableMapGeometry(state)) return;
    auto historyBefore = BeginObjectMutation(state);
    std::vector<int> placementIndices;
    std::vector<std::pair<std::size_t, std::size_t>> categorySources;
    std::vector<std::string> categoryEditorKeys;
    for (const int id : state.selectedObjects) {
        if (id >= 0) {
            if (static_cast<std::size_t>(id) < state.placementSet.Count() && !IsObjectEditorLocked(state,id))
                placementIndices.push_back(id);
            continue;
        }
        if (IsObjectEditorLocked(state,id)) continue;
        const auto renderIndex = ShmdRenderIndex(id);
        if (renderIndex && *renderIndex < state.shmdCategorySource.size()) {
            categorySources.push_back(state.shmdCategorySource[*renderIndex]);
            const std::string key=ShmdEditorObjectKey(state,id);
            if(!key.empty()) categoryEditorKeys.push_back(key);
        }
    }

    std::sort(placementIndices.begin(), placementIndices.end(), std::greater<int>());
    placementIndices.erase(std::unique(placementIndices.begin(), placementIndices.end()), placementIndices.end());
    for (const int index : placementIndices) {
        if (index >= 0 && static_cast<std::size_t>(index) < state.placementSet.Count()) {
            state.placementSet.RemoveObject(static_cast<std::size_t>(index));
            if (static_cast<std::size_t>(index) < state.objectEditorHidden.size())
                state.objectEditorHidden.erase(state.objectEditorHidden.begin() + index);
            if (static_cast<std::size_t>(index) < state.objectEditorLocked.size())
                state.objectEditorLocked.erase(state.objectEditorLocked.begin() + index);
            if (static_cast<std::size_t>(index) < state.objectEditorLabels.size())
                state.objectEditorLabels.erase(state.objectEditorLabels.begin() + index);
            if (static_cast<std::size_t>(index) < state.objectEditorGroups.size())
                state.objectEditorGroups.erase(state.objectEditorGroups.begin() + index);
        }
    }
    for(const auto& key:categoryEditorKeys) {
        state.shmdEditorHiddenKeys.erase(key);
        state.shmdEditorLockedKeys.erase(key);
        state.shmdEditorLabels.erase(key);
        state.shmdEditorGroups.erase(key);
    }
    const bool changed = !placementIndices.empty() || !categorySources.empty() || !categoryEditorKeys.empty();
    EraseShmdCategorySources(state, std::move(categorySources));

    ClearObjectSelection(state);
    ReloadObjectRenderers(state);
    FinishObjectMutation(state, std::move(historyBefore), changed);
}
void DeleteAllNormalObjects(EditorState& state) {
    if (!RequireWritableMapGeometry(state)) return;
    auto historyBefore = BeginObjectMutation(state);
    const std::size_t removed = state.placementSet.Count();
    state.placementSet.ClearObjects();
    state.objectEditorHidden.clear();
    state.objectEditorLocked.clear();
    state.objectEditorLabels.clear();
    state.objectEditorGroups.clear();
    ClearObjectSelection(state);
    ReloadObjectRenderers(state);
    state.statusMessage = std::to_string(removed) +
        " normale Placement-Objekte entfernt. Sky/Water/GroundObject bleiben unverändert.";
    FinishObjectMutation(state, std::move(historyBefore), removed > 0);
}
// SHMD-Kategorieeinträge besitzen im Dateiformat KEINEN Transform. Sobald der Benutzer einen
// solchen Eintrag verschiebt/dreht/skaliert, wird er deshalb automatisch aus der Kategorie-
// Pfadliste entfernt und als normales Placement mit exakt diesem Transform gespeichert.
bool PromoteSelectedShmdObjectsToPlacements(EditorState& state, std::optional<int> onlySelection) {
    if (!MapSupportsObjectGeometryEditing(state)) return false;
    struct Promotion {
        int oldId = kNoObjectSelection;
        core::PlacedObject object;
        std::pair<std::size_t, std::size_t> source{};
        std::string editorKey;
        std::string editorLabel;
        std::string editorGroup;
    };
    std::vector<Promotion> promotions;
    for (const int id : state.selectedObjects) {
        if (onlySelection && id != *onlySelection) continue;
        if (IsObjectEditorLocked(state,id)) continue;
        const auto renderIndex = ShmdRenderIndex(id);
        if (!renderIndex || *renderIndex >= state.shmdCategorySource.size() ||
            *renderIndex >= state.shmdCategoryRenderSet.Count()) continue;
        promotions.push_back({id, state.shmdCategoryRenderSet.At(*renderIndex),
                              state.shmdCategorySource[*renderIndex],
                              ShmdEditorObjectKey(state,id),
                              ObjectEditorLabel(state,id),
                              ObjectEditorGroup(state,id)});
    }
    if (promotions.empty()) return false;

    std::unordered_map<int, int> replacement;
    std::vector<std::pair<std::size_t, std::size_t>> sources;
    for (auto& promotion : promotions) {
        const int newId = static_cast<int>(state.placementSet.AddObject(std::move(promotion.object)));
        state.objectEditorHidden.push_back(0);
        state.objectEditorLocked.push_back(0);
        state.objectEditorLabels.push_back(promotion.editorLabel);
        state.objectEditorGroups.push_back(promotion.editorGroup);
        if(!promotion.editorKey.empty()) {
            state.shmdEditorHiddenKeys.erase(promotion.editorKey);
            state.shmdEditorLockedKeys.erase(promotion.editorKey);
            state.shmdEditorLabels.erase(promotion.editorKey);
            state.shmdEditorGroups.erase(promotion.editorKey);
        }
        replacement[promotion.oldId] = newId;
        sources.push_back(promotion.source);
    }
    EraseShmdCategorySources(state, std::move(sources));

    for (int& id : state.selectedObjects) {
        if (const auto it = replacement.find(id); it != replacement.end()) id = it->second;
    }
    if (const auto it = replacement.find(state.selectedObject); it != replacement.end()) {
        state.selectedObject = it->second;
    }
    // Bei einer Einzel-Transformbearbeitung kann das Entfernen eines Kategoriepfads die
    // negativen Render-IDs anderer, weiterhin ausgewählter SHMD-Einträge verschieben.
    // Diese anderen Einträge wurden durch den Regler ohnehin nicht verändert; daher bleibt
    // danach nur das tatsächlich bearbeitete/konvertierte Objekt ausgewählt. Beim Gruppen-
    // Drag (onlySelection==nullopt) werden dagegen alle ausgewählten SHMD-Einträge gemeinsam
    // promoted und die gemischte Mehrfachauswahl bleibt vollständig erhalten.
    if (onlySelection) {
        if (const auto it = replacement.find(*onlySelection); it != replacement.end()) {
            state.selectedObjects = {it->second};
            state.selectedObject = it->second;
        }
    }
    state.selectedObjectModelPathFor = kNoObjectSelection;
    state.selectedObjectModelPathDirty = false;
    state.objectListRangeAnchor = -1;
    ReloadObjectRenderers(state);
    state.statusMessage =
        "SHMD-Szenenmodell in normales Placement umgewandelt: Sky/Water/GroundObject speichern "
        "keine eigenen Transform-Daten; Position/Rotation/Skalierung bleiben so verlustfrei erhalten.";
    state.mapDirty = true;
    return true;
}
void MoveSelectedObjectsBy(EditorState& state, float dx, float dy, float dz) {
    if (!RequireWritableMapGeometry(state)) return;
    if (state.selectedObjects.empty()) return;
    const bool changed = dx != 0.0f || dy != 0.0f || dz != 0.0f;
    if (!changed) return;
    auto historyBefore = BeginObjectMutation(state);
    bool applied = PromoteSelectedShmdObjectsToPlacements(state);
    for (const int id : state.selectedObjects) {
        if (id < 0 || static_cast<std::size_t>(id) >= state.placementSet.Count() || IsObjectEditorLocked(state,id)) continue;
        auto& object = state.placementSet.At(static_cast<std::size_t>(id));
        object.posX += dx;
        object.posY += dy;
        object.posZ += dz;
        applied = true;
    }
    FinishObjectMutation(state, std::move(historyBefore), applied);
}
void RotateSelectedObjectsYawBy(EditorState& state, float deltaRadians) {
    if (!RequireWritableMapGeometry(state)) return;
    if (state.selectedObjects.empty() || std::abs(deltaRadians) < 1.0e-8f) return;
    auto historyBefore = BeginObjectMutation(state);
    bool changed = PromoteSelectedShmdObjectsToPlacements(state);
    const float sy = std::sin(deltaRadians * 0.5f);
    const float cy = std::cos(deltaRadians * 0.5f);
    for (const int id : state.selectedObjects) {
        if (id < 0 || static_cast<std::size_t>(id) >= state.placementSet.Count() || IsObjectEditorLocked(state,id)) continue;
        auto& object = state.placementSet.At(static_cast<std::size_t>(id));
        const float x = object.rotX, y = object.rotY, z = object.rotZ, w = object.rotW;
        object.rotX = cy * x + sy * z;
        object.rotY = cy * y + sy * w;
        object.rotZ = cy * z - sy * x;
        object.rotW = cy * w - sy * y;
        const float length = std::sqrt(object.rotX * object.rotX + object.rotY * object.rotY +
                                       object.rotZ * object.rotZ + object.rotW * object.rotW);
        if (length > 1.0e-8f) {
            object.rotX /= length; object.rotY /= length; object.rotZ /= length; object.rotW /= length;
        }
        changed = true;
    }
    FinishObjectMutation(state, std::move(historyBefore), changed);
}
void ScaleSelectedObjectsBy(EditorState& state, float factor) {
    if (!RequireWritableMapGeometry(state)) return;
    if (state.selectedObjects.empty() || !std::isfinite(factor) || factor <= 0.0f) return;
    auto historyBefore = BeginObjectMutation(state);
    bool changed = PromoteSelectedShmdObjectsToPlacements(state);
    for (const int id : state.selectedObjects) {
        if (id < 0 || static_cast<std::size_t>(id) >= state.placementSet.Count() || IsObjectEditorLocked(state,id)) continue;
        auto& object = state.placementSet.At(static_cast<std::size_t>(id));
        const float oldScale = object.scale;
        object.scale = std::clamp(object.scale * factor, 0.01f, 100.0f);
        changed = changed || std::abs(object.scale-oldScale) > 1.0e-8f;
    }
    FinishObjectMutation(state, std::move(historyBefore), changed);
}
void SyncSelectedObjectModelPath(EditorState& state) {
    if (state.selectedObject == kNoObjectSelection) {
        state.selectedObjectModelPath[0] = '\0';
        state.selectedObjectModelPathFor = kNoObjectSelection;
        state.selectedObjectModelPathDirty = false;
        return;
    }
    if (state.selectedObjectModelPathFor == state.selectedObject) return;
    const auto* object = EditableObject(state, state.selectedObject);
    if (!object) return;
    std::snprintf(state.selectedObjectModelPath, sizeof(state.selectedObjectModelPath), "%s", object->modelPath.c_str());
    state.selectedObjectModelPathFor = state.selectedObject;
    state.selectedObjectModelPathDirty = false;
}
void ApplySelectedObjectModelPath(EditorState& state, const std::string& modelPath) {
    if (!RequireWritableMapGeometry(state)) return;
    if (modelPath.empty() || state.selectedObject == kNoObjectSelection) return;
    auto historyBefore = BeginObjectMutation(state);
    bool changed = false;
    if (state.selectedObject >= 0) {
        if (auto* object = EditableObject(state, state.selectedObject)) {
            changed = object->modelPath != modelPath;
            object->modelPath = modelPath;
        }
        state.nifMeshRenderer.LoadModelsForSet(state.placementSet, CurrentObjectAssetMapDir(state));
    } else {
        const auto renderIndex = ShmdRenderIndex(state.selectedObject);
        if (!renderIndex || *renderIndex >= state.shmdCategorySource.size()) return;
        const std::string oldEditorKey=ShmdEditorObjectKey(state,state.selectedObject);
        const auto [categoryIndex, pathIndex] = state.shmdCategorySource[*renderIndex];
        if (categoryIndex >= state.placementSet.categories.size()) return;
        auto& paths = state.placementSet.categories[categoryIndex].modelPaths;
        if (pathIndex >= paths.size()) return;
        changed = paths[pathIndex] != modelPath;
        paths[pathIndex] = modelPath;
        RebuildShmdCategoryRenderSet(state, CurrentObjectAssetMapDir(state));
        TransferShmdEditorMetadataKey(state,oldEditorKey,
                                      ShmdEditorObjectKey(state,state.selectedObject));
    }
    state.footprintCache.erase(modelPath);
    state.selectedObjectModelPathFor = kNoObjectSelection;
    SyncSelectedObjectModelPath(state);
    FinishObjectMutation(state, std::move(historyBefore), changed);
}
void PlaceSkyAtCamera(EditorState& state, const app::OrbitCamera& cam) {
    // field_sky_01.nif liegt bei +-4164 um den Ursprung (Unterkante +290): eine Kuppel um den
    // Betrachter. An der Kartenecke (0,0) gezeichnet verdeckte sie Teile der Karte.
    const std::size_t count = std::min(state.shmdCategoryRenderSet.Count(), state.shmdCategoryRenderKind.size());
    for (std::size_t i = 0; i < count; ++i) {
        if (state.shmdCategoryRenderKind[i] != 0) continue;
        auto& sky = state.shmdCategoryRenderSet.At(i);
        sky.posX = cam.EyeX();
        sky.posY = cam.EyeY();
        sky.posZ = -cam.EyeZ();
    }
}
void RefreshShmdCategoryVisibility(EditorState& state) {
    state.shmdCategoryHidden.resize(state.shmdCategoryRenderSet.Count(), 0);
    for (std::size_t i = 0; i < state.shmdCategoryRenderSet.Count(); ++i) {
        bool visible = true;
        if (i < state.shmdCategoryRenderKind.size()) {
            switch (state.shmdCategoryRenderKind[i]) {
                // Orthografische Achsenansichten zeigen keinen Himmel (wie Unreal) - die Himmelskuppel
                // würde dort sonst als ebene Fläche das ganze Bild verdecken.
                case 0: visible = state.showShmdSky && !state.camera.IsOrthographic(); break;
                case 1: visible = state.showShmdWater; break;
                case 2: visible = state.showShmdGroundObject; break;
                default: break; // unbekannte generische SHMD-Kategorie bleibt sichtbar
            }
        }
        const int id=ShmdSelectionId(i);
        if (IsObjectEditorHidden(state,id)) visible=false;
        state.shmdCategoryHidden[i] = visible ? 0 : 1;
    }
}

} // namespace nge
