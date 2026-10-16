// Viewport3D.cpp
// 3D-Picking, Gizmo, Viewport-Leiste, Statistik und 3D-Vorschau.

#include "EditorShared.hpp"

namespace nge {

bool InvertEditMat4(const app::Mat4& m, app::Mat4& out) {
    const float* a=m.m; float inv[16];
    inv[0]=a[5]*a[10]*a[15]-a[5]*a[11]*a[14]-a[9]*a[6]*a[15]+a[9]*a[7]*a[14]+a[13]*a[6]*a[11]-a[13]*a[7]*a[10];
    inv[4]=-a[4]*a[10]*a[15]+a[4]*a[11]*a[14]+a[8]*a[6]*a[15]-a[8]*a[7]*a[14]-a[12]*a[6]*a[11]+a[12]*a[7]*a[10];
    inv[8]=a[4]*a[9]*a[15]-a[4]*a[11]*a[13]-a[8]*a[5]*a[15]+a[8]*a[7]*a[13]+a[12]*a[5]*a[11]-a[12]*a[7]*a[9];
    inv[12]=-a[4]*a[9]*a[14]+a[4]*a[10]*a[13]+a[8]*a[5]*a[14]-a[8]*a[6]*a[13]-a[12]*a[5]*a[10]+a[12]*a[6]*a[9];
    inv[1]=-a[1]*a[10]*a[15]+a[1]*a[11]*a[14]+a[9]*a[2]*a[15]-a[9]*a[3]*a[14]-a[13]*a[2]*a[11]+a[13]*a[3]*a[10];
    inv[5]=a[0]*a[10]*a[15]-a[0]*a[11]*a[14]-a[8]*a[2]*a[15]+a[8]*a[3]*a[14]+a[12]*a[2]*a[11]-a[12]*a[3]*a[10];
    inv[9]=-a[0]*a[9]*a[15]+a[0]*a[11]*a[13]+a[8]*a[1]*a[15]-a[8]*a[3]*a[13]-a[12]*a[1]*a[11]+a[12]*a[3]*a[9];
    inv[13]=a[0]*a[9]*a[14]-a[0]*a[10]*a[13]-a[8]*a[1]*a[14]+a[8]*a[2]*a[13]+a[12]*a[1]*a[10]-a[12]*a[2]*a[9];
    inv[2]=a[1]*a[6]*a[15]-a[1]*a[7]*a[14]-a[5]*a[2]*a[15]+a[5]*a[3]*a[14]+a[13]*a[2]*a[7]-a[13]*a[3]*a[6];
    inv[6]=-a[0]*a[6]*a[15]+a[0]*a[7]*a[14]+a[4]*a[2]*a[15]-a[4]*a[3]*a[14]-a[12]*a[2]*a[7]+a[12]*a[3]*a[6];
    inv[10]=a[0]*a[5]*a[15]-a[0]*a[7]*a[13]-a[4]*a[1]*a[15]+a[4]*a[3]*a[13]+a[12]*a[1]*a[7]-a[12]*a[3]*a[5];
    inv[14]=-a[0]*a[5]*a[14]+a[0]*a[6]*a[13]+a[4]*a[1]*a[14]-a[4]*a[2]*a[13]-a[12]*a[1]*a[6]+a[12]*a[2]*a[5];
    inv[3]=-a[1]*a[6]*a[11]+a[1]*a[7]*a[10]+a[5]*a[2]*a[11]-a[5]*a[3]*a[10]-a[9]*a[2]*a[7]+a[9]*a[3]*a[6];
    inv[7]=a[0]*a[6]*a[11]-a[0]*a[7]*a[10]-a[4]*a[2]*a[11]+a[4]*a[3]*a[10]+a[8]*a[2]*a[7]-a[8]*a[3]*a[6];
    inv[11]=-a[0]*a[5]*a[11]+a[0]*a[7]*a[9]+a[4]*a[1]*a[11]-a[4]*a[3]*a[9]-a[8]*a[1]*a[7]+a[8]*a[3]*a[5];
    inv[15]=a[0]*a[5]*a[10]-a[0]*a[6]*a[9]-a[4]*a[1]*a[10]+a[4]*a[2]*a[9]+a[8]*a[1]*a[6]-a[8]*a[2]*a[5];
    float det=a[0]*inv[0]+a[1]*inv[4]+a[2]*inv[8]+a[3]*inv[12];
    if (std::abs(det)<1.0e-8f) return false;
    det=1.0f/det; for(int i=0;i<16;++i) out.m[i]=inv[i]*det; return true;
}
std::optional<EditVec3> Unproject3D(const EditorState& state, const ImVec2& imagePos,
                                    int w, int h, const ImVec2& mouse, float ndcZ) {
    const app::Mat4 view=state.camera.ViewMatrix();
    const app::Mat4 proj=state.camera.ProjectionMatrix(static_cast<float>(std::max(1,w))/static_cast<float>(std::max(1,h)));
    app::Mat4 inv{};
    if(!InvertEditMat4(proj*view,inv)) return std::nullopt;
    const float x=((mouse.x-imagePos.x)/static_cast<float>(std::max(1,w)))*2.0f-1.0f;
    const float y=1.0f-((mouse.y-imagePos.y)/static_cast<float>(std::max(1,h)))*2.0f;
    const float cx=x,cy=y,cz=ndcZ,cw=1.0f;
    const float ox=inv.m[0]*cx+inv.m[4]*cy+inv.m[8]*cz+inv.m[12]*cw;
    const float oy=inv.m[1]*cx+inv.m[5]*cy+inv.m[9]*cz+inv.m[13]*cw;
    const float oz=inv.m[2]*cx+inv.m[6]*cy+inv.m[10]*cz+inv.m[14]*cw;
    const float ow=inv.m[3]*cx+inv.m[7]*cy+inv.m[11]*cz+inv.m[15]*cw;
    if(std::abs(ow)<1.0e-8f) return std::nullopt;
    return EditVec3{ox/ow,oy/ow,oz/ow};
}
std::optional<EditVec3> PickTerrainFrom3D(EditorState& state, const ImVec2& imagePos,
                                          int w, int h, const ImVec2& mouse) {
    const auto nearP=Unproject3D(state,imagePos,w,h,mouse,-1.0f);
    const auto farP=Unproject3D(state,imagePos,w,h,mouse,1.0f);
    if(!nearP||!farP) return std::nullopt;
    EditVec3 dir{farP->x-nearP->x,farP->y-nearP->y,farP->z-nearP->z};
    const float len=std::sqrt(dir.x*dir.x+dir.y*dir.y+dir.z*dir.z);
    if(len<1.0e-6f||std::abs(dir.y)<1.0e-6f) return std::nullopt;
    dir.x/=len;dir.y/=len;dir.z/=len;

    float planeY=state.camera.TargetY();
    float t=(planeY-nearP->y)/dir.y;
    if(t<0.0f) t=state.camera.Distance();
    const float spanX=static_cast<float>(state.heightmap.Width()>1?state.heightmap.Width()-1:1)*state.heightmap.BlockWidth();
    const float spanZ=static_cast<float>(state.heightmap.Height()>1?state.heightmap.Height()-1:1)*state.heightmap.BlockHeight();

    EditVec3 p{};
    for(int i=0;i<7;++i) {
        p={nearP->x+dir.x*t,nearP->y+dir.y*t,nearP->z+dir.z*t};
        if(p.x<0.0f||p.z<0.0f||p.x>spanX||p.z>spanZ) return std::nullopt;
        const float terrainY=state.heightmap.SampleWorld(p.x,p.z);
        t=(terrainY-nearP->y)/dir.y;
        if(t<0.0f) return std::nullopt;
    }
    p={nearP->x+dir.x*t,nearP->y+dir.y*t,nearP->z+dir.z*t};
    p.y=state.heightmap.SampleWorld(p.x,p.z);
    return p;
}
int PlaceObjectAtWorld(EditorState& state, const std::string& modelPath, float x, float y, float z) {
    if (!RequireWritableMapGeometry(state)) return kNoObjectSelection;
    if(modelPath.empty()) return kNoObjectSelection;
    auto historyBefore = BeginObjectMutation(state);
    core::PlacedObject obj;
    obj.modelPath=modelPath; obj.posX=x; obj.posY=y; obj.posZ=z; obj.scale=state.newObjectScale;
    const float half=state.newObjectRotDeg*3.14159265f/180.0f*0.5f;
    obj.rotY=std::sin(half); obj.rotW=std::cos(half);
    const int id=static_cast<int>(state.placementSet.AddObject(std::move(obj)));
    SyncObjectEditorMetadata(state);
    state.selectedObjects={id}; state.selectedObject=id; state.objectPlaceMode=0;
    state.objectGizmoMatrixValid=false;
    ReloadObjectRenderers(state);
    FinishObjectMutation(state, std::move(historyBefore));
    return id;
}
bool ProjectWorldTo3DView(const EditorState& state, const ImVec2& imagePos, int w, int h,
                          const EditVec3& world, ImVec2& screen) {
    const app::Mat4 vp=state.camera.ProjectionMatrix(static_cast<float>(std::max(1,w))/static_cast<float>(std::max(1,h)))*state.camera.ViewMatrix();
    const float cx=vp.m[0]*world.x+vp.m[4]*world.y+vp.m[8]*world.z+vp.m[12];
    const float cy=vp.m[1]*world.x+vp.m[5]*world.y+vp.m[9]*world.z+vp.m[13];
    const float cw=vp.m[3]*world.x+vp.m[7]*world.y+vp.m[11]*world.z+vp.m[15];
    if(cw<=1.0e-5f) return false;
    const float nx=cx/cw, ny=cy/cw;
    screen=ImVec2(imagePos.x+(nx*0.5f+0.5f)*w,
                  imagePos.y+(1.0f-(ny*0.5f+0.5f))*h);
    return true;
}
float ActiveWorldBrushRadius(const EditorState& state) {
    if(state.editMode==EditMode::Heightmap) return state.brush.radius;
    if(state.editMode==EditMode::TexturePaint) return state.paintSettings.radius;
    if(state.editMode==EditMode::BlockWalk) return state.walkSettings.radius;
    return 0.0f;
}
ImU32 ActiveBrushColor(const EditorState& state, int alpha) {
    if(state.editMode==EditMode::BlockWalk)
        return state.walkBlockMode?IM_COL32(245,85,85,alpha):IM_COL32(80,220,145,alpha);
    if(state.editMode==EditMode::TexturePaint) return IM_COL32(90,190,255,alpha);
    return IM_COL32(75,205,255,alpha);
}
void Draw3DBrushOverlay(EditorState& state, const ImVec2& imagePos, int w, int h, bool hovered) {
    if(!hovered || (state.editMode!=EditMode::Heightmap &&
                    state.editMode!=EditMode::TexturePaint &&
                    state.editMode!=EditMode::BlockWalk)) return;
    const auto hit=PickTerrainFrom3D(state,imagePos,w,h,ImGui::GetMousePos());
    if(!hit) return;
    const float radius=ActiveWorldBrushRadius(state);
    if(radius<=0.0f) return;
    ImDrawList* dl=ImGui::GetWindowDrawList();
    dl->PushClipRect(imagePos,ImVec2(imagePos.x+w,imagePos.y+h),true);
    auto ring=[&](float r,ImU32 col,float thickness) {
        std::array<ImVec2,49> pts{};
        int count=0;
        for(int i=0;i<=48;++i) {
            const float a=static_cast<float>(i)*(2.0f*3.14159265f/48.0f);
            const float x=hit->x+std::cos(a)*r;
            const float z=hit->z+std::sin(a)*r;
            const float y=state.heightmap.SampleWorld(x,z)+3.0f;
            ImVec2 p;
            if(ProjectWorldTo3DView(state,imagePos,w,h,{x,y,z},p)) pts[count++]=p;
        }
        if(count>=3) dl->AddPolyline(pts.data(),count,col,ImDrawFlags_None,thickness);
    };
    ring(radius,ActiveBrushColor(state),2.0f);
    if(state.editMode!=EditMode::BlockWalk)
        ring(radius*0.5f,ActiveBrushColor(state,120),1.0f);
    ImVec2 center;
    if(ProjectWorldTo3DView(state,imagePos,w,h,*hit,center))
        dl->AddCircleFilled(center,3.0f,ActiveBrushColor(state));
    dl->PopClipRect();
}
void DrawMobZones3D(EditorState& state, const ImVec2& imagePos, int w, int h) {
    if(state.editMode!=EditMode::Mobs || !state.mobRegenTextLoaded) return;
    auto* zoneTable=state.mobRegenTextFile.FindTable("MobRegenGroup");
    if(!zoneTable) return;

    ImDrawList* dl=ImGui::GetWindowDrawList();
    dl->PushClipRect(imagePos,ImVec2(imagePos.x+w,imagePos.y+h),true);
    for(std::size_t i=0;i<zoneTable->records.size();++i) {
        const auto& rec=zoneTable->records[i];
        if(rec.values.size()<7) continue;
        const float cx=static_cast<float>(std::atoi(rec.values[2].c_str()));
        const float cz=static_cast<float>(std::atoi(rec.values[3].c_str()));
        const float zw=static_cast<float>(std::atoi(rec.values[4].c_str()));
        const float zh=static_cast<float>(std::atoi(rec.values[5].c_str()));
        const float rangeVal=static_cast<float>(std::atoi(rec.values[6].c_str()));
        const float radius=(zw>0.0f||zh>0.0f)?std::max(zw,zh)*0.5f:rangeVal;
        if(radius<=0.0f) continue;

        const bool selected=state.selectedMobZoneIdx==static_cast<int>(i);
        const ImU32 col=selected?IM_COL32(255,255,255,235):IM_COL32(72,150,235,155);
        std::array<ImVec2,49> pts{};
        int count=0;
        for(int k=0;k<=48;++k) {
            const float a=static_cast<float>(k)*(2.0f*3.14159265f/48.0f);
            const float x=cx+std::cos(a)*radius;
            const float z=cz+std::sin(a)*radius;
            const float y=state.heightmap.SampleWorld(x,z)+5.0f;
            ImVec2 p;
            if(ProjectWorldTo3DView(state,imagePos,w,h,{x,y,z},p)) pts[count++]=p;
        }
        if(count>=3) dl->AddPolyline(pts.data(),count,col,ImDrawFlags_None,selected?2.5f:1.25f);

        ImVec2 center;
        if(ProjectWorldTo3DView(state,imagePos,w,h,{cx,state.heightmap.SampleWorld(cx,cz)+8.0f,cz},center)) {
            dl->AddCircleFilled(center,selected?5.0f:3.0f,col);
            if(selected && !rec.values.empty())
                dl->AddText(ImVec2(center.x+8.0f,center.y-8.0f),col,rec.values[0].c_str());
        }
    }
    dl->PopClipRect();
}
void DrawPortals3D(EditorState& state, const ImVec2& imagePos, int w, int h) {
    if(state.editMode!=EditMode::Portals || state.legacySaveStem[0]=='\0') return;
    const auto markers=CollectPortalMarkers(state);
    if(markers.empty()) return;
    ImDrawList* dl=ImGui::GetWindowDrawList();
    dl->PushClipRect(imagePos,ImVec2(imagePos.x+w,imagePos.y+h),true);
    for(const auto& m:markers) {
        ImVec2 p;
        const float worldY=state.heightmap.SampleWorld(m.x,m.y)+12.0f;
        if(!ProjectWorldTo3DView(state,imagePos,w,h,{m.x,worldY,m.y},p)) continue;
        const bool selected=m.kind==state.selectedPortalKind && static_cast<int>(m.idx)==state.selectedPortalIdx;
        const ImU32 col=selected?IM_COL32(255,255,255,245):IM_COL32(80,185,255,210);
        const float r=selected?8.0f:6.0f;
        if(m.kind==kPortalKindTown) {
            const ImVec2 d[4]={ImVec2(p.x,p.y-r),ImVec2(p.x+r,p.y),ImVec2(p.x,p.y+r),ImVec2(p.x-r,p.y)};
            dl->AddPolyline(d,4,col,ImDrawFlags_Closed,selected?2.5f:1.7f);
        } else if(m.kind==kPortalKindRecall) {
            const ImVec2 t[3]={ImVec2(p.x,p.y-r),ImVec2(p.x+r,p.y+r),ImVec2(p.x-r,p.y+r)};
            dl->AddPolyline(t,3,col,ImDrawFlags_Closed,selected?2.5f:1.7f);
        } else {
            dl->AddRect(ImVec2(p.x-r,p.y-r),ImVec2(p.x+r,p.y+r),col,2.0f,0,selected?2.5f:1.7f);
        }
        if(selected) dl->AddText(ImVec2(p.x+r+5.0f,p.y-r),col,m.label.c_str());
    }
    dl->PopClipRect();
}
// Vertex-Snap (V beim Ziehen des Verschieben-Gizmos, wie in Unreal): nächstgelegene Ecke des
// NIF-Dreiecks unter dem Mauszeiger (ausgewählte Objekte ausgenommen) bzw. der nächste
// HTD-Vertex des Terrains - je nachdem, was entlang des Strahls zuerst getroffen wird.
std::optional<EditVec3> VertexSnapTarget(EditorState& state, const ImVec2& imagePos, int w, int h, const ImVec2& mouse) {
    const auto nearP = Unproject3D(state, imagePos, w, h, mouse, -1.0f);
    const auto farP = Unproject3D(state, imagePos, w, h, mouse, 1.0f);
    if (!nearP || !farP) return std::nullopt;
    std::array<float, 3> origin{nearP->x, nearP->y, nearP->z};
    std::array<float, 3> dir{farP->x - nearP->x, farP->y - nearP->y, farP->z - nearP->z};
    const float len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    if (len < 1.0e-6f) return std::nullopt;
    for (float& v : dir) v /= len;

    const std::unordered_set<int> selected(state.selectedObjects.begin(), state.selectedObjects.end());
    float best = std::numeric_limits<float>::infinity();
    std::optional<EditVec3> target;
    RefreshObjectVisibility(state);
    for (std::size_t i = 0; i < state.placementSet.Count(); ++i) {
        if (selected.contains(static_cast<int>(i)) || IsObjectHidden(state, i) || !state.nifMeshRenderer.HasRealMesh(i)) continue;
        if (const auto hit = state.nifMeshRenderer.RaycastObjectDetailed(state.placementSet, i, state.camera, origin, dir);
            hit && hit->distance < best) {
            best = hit->distance;
            target = EditVec3{hit->nearestVertex[0], hit->nearestVertex[1], hit->nearestVertex[2]};
        }
    }
    RefreshShmdCategoryVisibility(state);
    for (std::size_t i = 0; i < state.shmdCategoryRenderSet.Count(); ++i) {
        if (selected.contains(ShmdSelectionId(i)) || (i < state.shmdCategoryHidden.size() && state.shmdCategoryHidden[i]) ||
            !state.shmdCategoryMeshRenderer.HasRealMesh(i)) continue;
        if (const auto hit = state.shmdCategoryMeshRenderer.RaycastObjectDetailed(state.shmdCategoryRenderSet, i, state.camera, origin, dir);
            hit && hit->distance < best) {
            best = hit->distance;
            target = EditVec3{hit->nearestVertex[0], hit->nearestVertex[1], hit->nearestVertex[2]};
        }
    }
    if (MapSupportsTerrainEditing(state)) {
        if (const auto t = PickTerrainFrom3D(state, imagePos, w, h, mouse)) {
            const float dx = t->x - origin[0], dy = t->y - origin[1], dz = t->z - origin[2];
            if (std::sqrt(dx * dx + dy * dy + dz * dz) < best) {
                const float bw = std::max(state.heightmap.BlockWidth(), 1.0e-3f);
                const float bh = std::max(state.heightmap.BlockHeight(), 1.0e-3f);
                const float vx = std::round(t->x / bw) * bw, vz = std::round(t->z / bh) * bh;
                target = EditVec3{vx, state.heightmap.SampleWorld(vx, vz), vz};
            }
        }
    }
    return target;
}
std::string CurrentGizmoSelectionKey(const EditorState& state) {
    std::string key = std::to_string(state.objectGizmoOperation) + "|" +
                      (state.objectGizmoLocal ? "L|" : "W|");
    for (const int id : state.selectedObjects) key += std::to_string(id) + ",";
    return key;
}
bool DrawObjectTransformGizmo(EditorState& state, const ImVec2& imageScreenPos, int w, int h) {
    if (state.editMode != EditMode::ObjectPlacement || state.selectedObjects.empty() ||
        state.objectGizmoOperation < 0 || state.playtestActive || state.gameView || !state.drawingActiveViewport) {
        state.objectGizmoMatrixValid = false;
        state.objectGizmoWasUsing = false;
        return false;
    }

    const ObjectSelectionPivot pivot = ComputeObjectSelectionPivot(state);
    if (!pivot.valid) return false;

    const std::string selectionKey = CurrentGizmoSelectionKey(state);
    if (!state.objectGizmoMatrixValid || state.objectGizmoSelectionKey != selectionKey ||
        !state.objectGizmoWasUsing) {
        const app::Mat4 start = ObjectEditMatrix(pivot.position, pivot.rotation, pivot.scale);
        std::copy(std::begin(start.m), std::end(start.m), state.objectGizmoMatrix.begin());
        state.objectGizmoMatrixValid = true;
        state.objectGizmoSelectionKey = selectionKey;
    }

    app::Mat4 before{};
    std::copy(state.objectGizmoMatrix.begin(), state.objectGizmoMatrix.end(), std::begin(before.m));

    const app::Mat4 view = state.camera.ViewMatrix();
    const app::Mat4 projection = state.camera.ProjectionMatrix(static_cast<float>(std::max(1,w))/static_cast<float>(std::max(1,h)));

    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetOrthographic(state.camera.IsOrthographic());
    ImGuizmo::SetRect(imageScreenPos.x, imageScreenPos.y, static_cast<float>(w), static_cast<float>(h));
    ImGuizmo::SetID(0x4E47);

    ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
    if (state.objectGizmoOperation == 1) operation = ImGuizmo::ROTATE;
    else if (state.objectGizmoOperation == 2) operation = ImGuizmo::SCALE;

    const ImGuizmo::MODE mode = state.objectGizmoLocal ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
    float snap3[3] = {state.objectMoveSnap, state.objectMoveSnap, state.objectMoveSnap};
    float snap1 = state.objectGizmoOperation == 1 ? state.objectRotateSnap : state.objectScaleSnap;
    const float* snap = nullptr;
    if (state.objectGizmoSnap) snap = state.objectGizmoOperation == 0 ? snap3 : &snap1;

    ImGuizmo::Manipulate(view.m, projection.m, operation, mode,
                         state.objectGizmoMatrix.data(), nullptr, snap);

    const bool usingGizmo = ImGuizmo::IsUsing();
    const bool overGizmo = ImGuizmo::IsOver();

    if (usingGizmo) {
        if (!state.objectGizmoWasUsing) {
            BeginObjectEditTransaction(state);
            // Unreal: Alt beim Ziehen dupliziert die Auswahl an Ort und Stelle; bewegt werden
            // danach die Kopien. Duplikat + Verschiebung bilden einen gemeinsamen Undo-Schritt.
            if (ImGui::GetIO().KeyAlt && state.objectGizmoOperation == 0 && !state.standaloneNifMap) {
                DuplicateSelectedObjects(state, 0.0f);
                state.objectGizmoSelectionKey = CurrentGizmoSelectionKey(state);
                state.statusMessage = std::to_string(state.selectedObjects.size()) +
                    L(" Objekt(e) per Alt+Ziehen dupliziert.", " object(s) duplicated with Alt+drag.");
            }
        }
        app::Mat4 after{};
        std::copy(state.objectGizmoMatrix.begin(), state.objectGizmoMatrix.end(), std::begin(after.m));
        const EditVec3 oldPivot{before.m[12], before.m[13], before.m[14]};

        if (state.objectGizmoOperation == 0) {
            const float dx = after.m[12] - before.m[12];
            const float dy = after.m[13] - before.m[13];
            const float dz = after.m[14] - before.m[14];
            const bool vertexSnap = ImGui::IsKeyDown(ImGuiKey_V) && !ImGui::GetIO().WantTextInput;
            std::optional<EditVec3> snapTarget;
            if (vertexSnap) snapTarget = VertexSnapTarget(state, imageScreenPos, w, h, ImGui::GetMousePos());
            if (snapTarget) {
                // Pivot exakt auf den Zielvertex setzen; der Gizmo folgt im nächsten Frame.
                const float sx = snapTarget->x - before.m[12];
                const float sy = snapTarget->y - before.m[13];
                const float sz = snapTarget->z - before.m[14];
                if (std::abs(sx) + std::abs(sy) + std::abs(sz) > 1.0e-6f) MoveSelectedObjectsBy(state, sx, sy, sz);
                state.objectGizmoMatrix[12] = snapTarget->x;
                state.objectGizmoMatrix[13] = snapTarget->y;
                state.objectGizmoMatrix[14] = snapTarget->z;
                ImVec2 sp;
                if (ProjectWorldTo3DView(state, imageScreenPos, w, h, *snapTarget, sp)) {
                    ImGui::GetWindowDrawList()->AddCircle(sp, 7.0f, IM_COL32(255, 220, 60, 255), 16, 2.0f);
                    ImGui::GetWindowDrawList()->AddCircleFilled(sp, 2.5f, IM_COL32(255, 220, 60, 255));
                }
            } else if (std::abs(dx)+std::abs(dy)+std::abs(dz) > 1.0e-6f) {
                float mx = dx, my = dy, mz = dz;
                if (state.actorSnap) {
                    // Actor-Snap: liegt der neue Pivot innerhalb des Raster-Snap-Radius um den Pivot
                    // eines anderen sichtbaren Objekts, rastet er dort exakt ein.
                    const EditVec3 want{after.m[12], after.m[13], after.m[14]};
                    const float radius = std::max(5.0f, state.objectMoveSnap);
                    const std::unordered_set<int> sel(state.selectedObjects.begin(), state.selectedObjects.end());
                    float bestD = radius * radius;
                    std::optional<EditVec3> snap;
                    for (std::size_t i = 0; i < state.placementSet.Count(); ++i) {
                        if (sel.contains(static_cast<int>(i)) || IsObjectHidden(state, i)) continue;
                        const auto& o = state.placementSet.At(i);
                        const float ex = o.posX - want.x, ey = o.posY - want.y, ez = o.posZ - want.z;
                        const float d = ex * ex + ey * ey + ez * ez;
                        if (d < bestD) { bestD = d; snap = EditVec3{o.posX, o.posY, o.posZ}; }
                    }
                    if (snap) {
                        mx = snap->x - before.m[12]; my = snap->y - before.m[13]; mz = snap->z - before.m[14];
                        state.objectGizmoMatrix[12] = snap->x;
                        state.objectGizmoMatrix[13] = snap->y;
                        state.objectGizmoMatrix[14] = snap->z;
                    }
                }
                MoveSelectedObjectsBy(state, mx, my, mz);
                if (state.surfaceSnap) SnapSelectedObjectsToTerrainSilently(state);
                // Ein frei gesetzter Pivot wandert mit der Auswahl.
                if (state.pivotOverride) {
                    (*state.pivotOverride)[0] += mx; (*state.pivotOverride)[1] += my; (*state.pivotOverride)[2] += mz;
                }
            }
        } else if (state.objectGizmoOperation == 1) {
            const EditQuat beforeQ = MatrixRotationQuat(before);
            const EditQuat afterQ = MatrixRotationQuat(after);
            const EditQuat delta = MulEditQuat(afterQ, ConjugateEditQuat(beforeQ));
            RotateSelectedObjectsAroundPivot(state, oldPivot, delta);
        } else {
            const float beforeScale = MatrixUniformScale(before);
            const float afterScale = MatrixUniformScale(after);
            const float factor = beforeScale > 1.0e-6f ? afterScale / beforeScale : 1.0f;
            if (std::isfinite(factor) && std::abs(factor - 1.0f) > 1.0e-6f)
                ScaleSelectedObjectsAroundPivot(state, oldPivot, factor);
        }
    }

    if (state.objectGizmoWasUsing && !usingGizmo) {
        state.objectGizmoMatrixValid = false;
        CommitObjectEditTransaction(state);
    }
    state.objectGizmoWasUsing = usingGizmo;
    return usingGizmo || overGizmo;
}
 // namespace viewport_ui

void DrawViewportShowFlagsMenu(EditorState& state) {
    ImGui::SeparatorText(L("Szene", "Scene"));
    ImGui::MenuItem(L("Terrain (HTD)", "Terrain (HTD)"), nullptr, &state.showTerrain);
    ImGui::MenuItem(L("Objekt-Meshes (NIF)", "Object meshes (NIF)"), nullptr, &state.showObjectMeshes);
    ImGui::MenuItem(L("Platzhalter ohne Mesh", "Placeholders without mesh"), nullptr, &state.showObjectMarkers);
    ImGui::MenuItem("SHMD Sky", nullptr, &state.showShmdSky);
    ImGui::MenuItem("SHMD Water", nullptr, &state.showShmdWater);
    ImGui::MenuItem("SHMD GroundObject", nullptr, &state.showShmdGroundObject);
    ImGui::MenuItem(L("NPC-Modelle", "NPC models"), nullptr, &state.showNpcModels);
    ImGui::MenuItem(L("NPC-Namen", "NPC labels"), nullptr, &state.showNpcLabels);
    ImGui::MenuItem(L("Patrouillenrouten", "Patrol routes"), nullptr, &state.showRoamRoutes);
    ImGui::MenuItem(L("Drahtgitter", "Wireframe"), nullptr, &state.wireframe);
    ImGui::SeparatorText(L("Darstellung", "Rendering"));
    bool renderChanged = false;
    const char* viewModes[] = {L("Beleuchtet", "Lit"), L("Unbeleuchtet (nur Textur)", "Unlit (texture only)"),
                               L("Nur Licht", "Lighting only"), L("Normalen", "Normals"),
                               L("Vertexfarben", "Vertex colors"), L("UV0", "UV0"), L("Alpha", "Alpha"),
                               L("LOD-Stufen (NiLODNode)", "LOD levels (NiLODNode)")};
    for (int mode = 0; mode < 8; ++mode)
        if (ImGui::MenuItem(viewModes[mode], nullptr, state.viewportViewMode == mode)) {
            state.viewportViewMode = mode;
            renderChanged = true;
        }
    const bool hasVc = state.renderer.HasVertexColorMap();
    renderChanged |= ImGui::MenuItem(L("Terrain-Vertexfarben (Karte)", "Terrain vertex colors (map)"), nullptr,
                                     &state.viewportVertexColors, hasVc);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", hasVc ? state.mapVertexColorPath.c_str()
                                      : L("Diese Karte hat keine lesbare #VerTexColorTexture-Bitmap.",
                                          "This map has no readable #VerTexColorTexture bitmap."));
    const bool hasGlow = state.mapRenderConfig.glow.present;
    renderChanged |= ImGui::MenuItem(L("Glow (<Karte>.conf)", "Glow (<map>.conf)"), nullptr, &state.viewportGlow, hasGlow);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        if (hasGlow)
            ImGui::SetTooltip("%s\nGlowness %.2f · BlendFactor %.2f · GaussFactor %.2f · DownScaling %.0f · NumBlurring %d",
                              state.mapRenderConfigPath.c_str(), state.mapRenderConfig.glow.glowness,
                              state.mapRenderConfig.glow.blendFactor, state.mapRenderConfig.glow.gaussFactor,
                              state.mapRenderConfig.glow.downScaling, state.mapRenderConfig.glow.numBlurring);
        else
            ImGui::SetTooltip("%s", L("Keine [GlowScreenEffect]-Daten für diese Karte.", "No [GlowScreenEffect] data for this map."));
    }
    renderChanged |= ImGui::MenuItem(L("Objekt-Animationen", "Object animations"), nullptr, &state.viewportObjectAnimation);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", L("Im NIF eingebettete Bewegungen abspielen (Windmühlenflügel, Boote, Schilder, Leuchtturm).\n"
                                  "Aus: alle Objekte in der Ruhepose.",
                                  "Play motion embedded in the NIF (windmill blades, boats, signs, lighthouse).\n"
                                  "Off: all objects in their rest pose."));
    const bool hasEnv = state.sceneEnvironmentFromShmd;
    renderChanged |= ImGui::MenuItem(L("Kartenlicht (SHMD)", "Map lighting (SHMD)"), nullptr, &state.viewportMapLighting, hasEnv);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        const auto& env = state.placementSet.environment;
        if (hasEnv)
            ImGui::SetTooltip("GlobalLight %.3f %.3f %.3f\nDirectionLightAmbient %.3f %.3f %.3f\nDirectionLightDiffuse %.3f %.3f %.3f",
                              env.globalLight[0], env.globalLight[1], env.globalLight[2],
                              env.directionLightAmbient[0], env.directionLightAmbient[1], env.directionLightAmbient[2],
                              env.directionLightDiffuse[0], env.directionLightDiffuse[1], env.directionLightDiffuse[2]);
        else
            ImGui::SetTooltip("%s", L("Keine SHMD-Umgebung geladen.", "No SHMD environment loaded."));
    }
    renderChanged |= ImGui::MenuItem(L("Hintergrundfarbe (SHMD)", "Background color (SHMD)"), nullptr,
                                     &state.viewportMapBackground, hasEnv);
    renderChanged |= ImGui::MenuItem(L("Nebel (SHMD Fog/Frustum)", "Fog (SHMD Fog/Frustum)"), nullptr, &state.viewportFog, hasEnv);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && hasEnv) {
        const auto& env = state.placementSet.environment;
        ImGui::SetTooltip(L("Fog %.2f · Farbe %.3f %.3f %.3f · Frustum %.0f\nIm Spieltest immer aktiv, in Achsenansichten aus.",
                            "Fog %.2f · color %.3f %.3f %.3f · frustum %.0f\nAlways on in playtest, off in axis views."),
                          env.fog[0], env.fog[1], env.fog[2], env.fog[3], env.frustumFar);
    }
    if (ImGui::BeginMenu(L("Kantenglättung (MSAA)", "Anti-aliasing (MSAA)"))) {
        for (const int samples : {1, 2, 4, 8}) {
            const std::string label = samples == 1 ? std::string(L("Aus", "Off")) : std::to_string(samples) + "x";
            if (ImGui::MenuItem(label.c_str(), nullptr, state.viewportMsaaSamples == samples,
                                samples <= state.renderer.MaxMsaaSamples())) {
                state.viewportMsaaSamples = samples;
                renderChanged = true;
            }
        }
        ImGui::EndMenu();
    }
    if (renderChanged) state.viewportSettingsDirty = true;
    ImGui::SeparatorText(L("Editor-Helfer", "Editor helpers"));
    bool changed = false;
    changed |= ImGui::MenuItem(L("Raster", "Grid"), nullptr, &state.showViewportGrid);
    changed |= ImGui::MenuItem(L("Achsen-Anzeige", "Axis indicator"), nullptr, &state.showViewportAxes);
    changed |= ImGui::MenuItem(L("Statistik", "Stats"), nullptr, &state.showViewportStats);
    changed |= ImGui::MenuItem(L("SHBD-Kollision um Cursor", "SHBD collision near cursor"), nullptr,
                               &state.showWalkCollision3D);
    ImGui::MenuItem(L("Spielansicht (alle Helfer aus)", "Game view (all helpers off)"), "G", &state.gameView);
    if (changed) state.viewportSettingsDirty = true;
}
void DrawViewportViewMenu(EditorState& state) {
    ImGui::SeparatorText(L("Ansicht", "View"));
    for (int preset = 0; preset <= 5; ++preset)
        if (ImGui::MenuItem(ViewportPresetName(preset), nullptr, state.viewportViewPreset == preset))
            ApplyViewportPreset(state, preset);
    ImGui::TextDisabled("%s", L("Achsenansichten sind orthografisch (Ziehen verschiebt).",
                                "Axis views are orthographic (dragging pans)."));
    ImGui::SeparatorText(L("Viewport-Layout", "Viewport layout"));
    for (const int layout : {1, 2, 4})
        if (ImGui::MenuItem(ViewportLayoutName(layout), nullptr, state.viewportLayout == layout))
            SetViewportLayout(state, layout);
    ImGui::TextDisabled("%s", L("Jeder Viewport hat eigene Kamera, Ansicht und Anzeigen.",
                                "Each viewport has its own camera, view and show flags."));
    ImGui::SeparatorText(L("Kamera-Lesezeichen", "Camera bookmarks"));
    EnsureCameraBookmarksLoaded(state);
    for (int slot = 0; slot < static_cast<int>(core::level::CameraBookmarkSet::kSlots); ++slot) {
        const auto& b = state.cameraBookmarks.slots[static_cast<std::size_t>(slot)];
        ImGui::PushID(slot);
        char label[96];
        if (b.valid) std::snprintf(label, sizeof(label), "%d  ·  %.0f / %.0f", slot, b.targetX, b.targetZ);
        else std::snprintf(label, sizeof(label), "%d  ·  %s", slot, L("leer", "empty"));
        const std::string jumpKey = std::to_string(slot);
        if (ImGui::MenuItem(label, jumpKey.c_str(), false, b.valid)) JumpToCameraBookmark(state, slot);
        ImGui::SameLine(ImGui::GetContentRegionMax().x - 54.0f);
        if (ImGui::SmallButton(L("Setzen", "Set"))) SetCameraBookmark(state, slot);
        ImGui::PopID();
    }
    ImGui::TextDisabled("%s", L("Strg+0..9 setzen · 0..9 anspringen", "Ctrl+0..9 set · 0..9 jump"));
    ImGui::SeparatorText(L("Bedienung", "Controls"));
    if (ImGui::MenuItem(L("Unreal-Navigation", "Unreal navigation"), nullptr, &state.unrealNavigation))
        state.viewportSettingsDirty = true;
    viewport_ui::Tooltip(L("RMB halten + WASD/Q/E fliegen, Mausrad bei RMB = Tempo, Alt+LMB Orbit, Alt+RMB Dolly, "
                           "LMB-Ziehen vor/zurück, W/E/R Gizmo, Leertaste wechselt Gizmo.",
                           "Hold RMB + WASD/Q/E to fly, wheel while RMB = speed, Alt+LMB orbit, Alt+RMB dolly, "
                           "LMB drag forward/back, W/E/R gizmo, Space cycles gizmo."));
    bool crossing = state.marqueeMode == core::level::MarqueeMode::Crossing;
    if (ImGui::MenuItem(L("Rahmenauswahl: Überschneidung", "Marquee: crossing"), nullptr, crossing)) {
        state.marqueeMode = core::level::MarqueeMode::Crossing;
        state.viewportSettingsDirty = true;
    }
    if (ImGui::MenuItem(L("Rahmenauswahl: vollständig innen", "Marquee: fully inside"), nullptr, !crossing)) {
        state.marqueeMode = core::level::MarqueeMode::Inside;
        state.viewportSettingsDirty = true;
    }
    ImGui::TextDisabled("%s", L("Strg+Alt+LMB-Ziehen = Rahmen · +Shift ergänzt",
                                "Ctrl+Alt+LMB drag = marquee · +Shift adds"));
}
// Liefert true, solange die Maus über der Leiste liegt (Klicks dürfen nicht in den Viewport fallen).
bool DrawViewportToolbar(EditorState& state, const ImVec2& imageScreenPos, float width) {
    const ImVec2 restore = ImGui::GetCursorScreenPos();
    bool capturing = false;
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(11, 27, 41, 235));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(18, 67, 104, 245));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(11, 27, 41, 235));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 3.0f));

    // ---- linke Gruppe ----
    ImGui::SetCursorScreenPos(ImVec2(imageScreenPos.x + 10.0f, imageScreenPos.y + 10.0f));
    ImGui::BeginGroup();
    const std::string viewLabel = std::string(ViewportPresetName(state.viewportViewPreset)) + "##vpView";
    if (viewport_ui::GlyphButton(viewLabel.c_str(), viewport_ui::ButtonGlyph::DropDown)) ImGui::OpenPopup("##viewportViewMenu");
    viewport_ui::Tooltip(L("Ansicht, Kamera-Lesezeichen und Navigation", "View, camera bookmarks and navigation"));
    if (ImGui::BeginPopup("##viewportViewMenu")) { DrawViewportViewMenu(state); ImGui::EndPopup(); }
    ImGui::SameLine();
    if (viewport_ui::GlyphButton(L("Anzeigen##vpShow", "Show##vpShow"), viewport_ui::ButtonGlyph::DropDown))
        ImGui::OpenPopup("##viewportShowMenu");
    viewport_ui::Tooltip(L("Show Flags: was der Viewport zeichnet", "Show flags: what the viewport draws"));
    if (ImGui::BeginPopup("##viewportShowMenu")) { DrawViewportShowFlagsMenu(state); ImGui::EndPopup(); }

    // Schmale Viewports: die Snap-/Pivot-Gruppe rückt in eine zweite Zeile, damit sich linke und
    // rechte Gruppe nicht überlappen.
    const bool compactToolbar = width < 1180.0f;
    // Sehr schmale Viewports (2x2-Layout): Transform-Knöpfe in die zweite Zeile, Snap-Werte nur im
    // Eigenschaften-Panel.
    const bool tinyToolbar = width < 640.0f;
    if (state.editMode == EditMode::ObjectPlacement && !state.playtestActive) {
        if (tinyToolbar) ImGui::NewLine();
        else ImGui::SameLine(0.0f, 12.0f);
        auto opButton = [&](const char* id, const char* label, IconDrawFn fallbackIcon,
                            const char* semanticIcon, int op, const char* unrealKey,
                            const EditorState::ShortcutBinding& shortcut) {
            const bool active = state.objectGizmoOperation == op;
            const std::string hint = std::string(label) + " · " +
                (state.unrealNavigation ? std::string(unrealKey) : ShortcutLabel(shortcut));
            if (DrawTinyIconButton(id, fallbackIcon, active, hint.c_str(), ImVec2(24.0f, 22.0f), semanticIcon))
                SetGizmoOperation(state, op);
            ImGui::SameLine(0.0f, 2.0f);
        };
        opButton("viewportMove", L("Verschieben", "Move"), DrawIconMove, "transform.move", 0, "W", state.shortcutGizmoMove);
        opButton("viewportRotate", L("Rotieren", "Rotate"), DrawIconRotate, "transform.rotate", 1, "E", state.shortcutGizmoRotate);
        opButton("viewportScale", L("Skalieren", "Scale"), DrawIconScale, "transform.scale", 2, "R", state.shortcutGizmoScale);
        if (!tinyToolbar) {
        // Schmale Viewports: Koordinatensystem-, Pivot- und Snap-Steuerung in eine zweite Zeile.
        if (compactToolbar) ImGui::NewLine();
        else ImGui::SameLine(0.0f, 6.0f);
        if (ImGui::SmallButton(state.objectGizmoLocal ? "Local##vpSpace" : "World##vpSpace")) {
            state.objectGizmoLocal = !state.objectGizmoLocal;
            state.objectGizmoMatrixValid = false;
        }
        viewport_ui::Tooltip(L("Koordinatensystem des Gizmos (Welt/Lokal)", "Gizmo coordinate space (world/local)"));
        ImGui::SameLine(0.0f, 4.0f);
        if (ImGui::SmallButton(state.pivotAtActiveObject ? L("Pivot: Aktiv##vpPivot", "Pivot: active##vpPivot")
                                                         : L("Pivot: Mitte##vpPivot", "Pivot: center##vpPivot"))) {
            state.pivotAtActiveObject = !state.pivotAtActiveObject;
            state.objectGizmoMatrixValid = false;
            state.viewportSettingsDirty = true;
        }
        viewport_ui::Tooltip(L("Drehen/Skalieren um die Auswahlmitte oder um das aktive (zuletzt gewählte) Objekt",
                               "Rotate/scale around the selection center or the active (last selected) object"));
        ImGui::SameLine(0.0f, 6.0f);
        if (viewport_ui::ToggleChip(L("Boden", "Surface"), &state.surfaceSnap,
                                    L("Surface Snap: verschobene Objekte folgen dem begehbaren Boden (HTD + SHMD-GroundObject)",
                                      "Surface snap: moved objects follow the walkable ground (HTD + SHMD GroundObject)")))
            state.viewportSettingsDirty = true;
        ImGui::SameLine(0.0f, 4.0f);
        if (viewport_ui::ToggleChip(L("Objekt", "Actor"), &state.actorSnap,
                                    L("Actor Snap: Verschieben rastet am Pivot anderer Objekte ein (Radius = Raster-Snap)",
                                      "Actor snap: moving snaps to other objects' pivots (radius = grid snap)")))
            state.viewportSettingsDirty = true;
        ImGui::SameLine(0.0f, 6.0f);
        const bool snapBefore = state.objectGizmoSnap;
        bool rotSnap = state.objectGizmoSnap, scaleSnap = state.objectGizmoSnap;
        bool changed = viewport_ui::SnapCombo("##grid", L("Raster","Grid"), &state.objectGizmoSnap, &state.objectMoveSnap,
                                              core::level::kGridSnapPresets, "",
                                              L("Raster-Snap in Welteinheiten (6.25 = SHBD-Zelle, 50 = HTD-Block)",
                                                "Grid snap in world units (6.25 = SHBD cell, 50 = HTD block)"));
        ImGui::SameLine(0.0f, 4.0f);
        changed |= viewport_ui::SnapCombo("##rot", L("Winkel","Angle"), &rotSnap, &state.objectRotateSnap,
                                          core::level::kRotationSnapPresets, "°",
                                          L("Winkel-Snap", "Rotation snap"));
        ImGui::SameLine(0.0f, 4.0f);
        changed |= viewport_ui::SnapCombo("##scale", L("Skal.","Scale"), &scaleSnap, &state.objectScaleSnap,
                                          core::level::kScaleSnapPresets, "",
                                          L("Skalierungs-Snap", "Scale snap"));
        // Ein gemeinsamer Snap-Schalter (bestehendes Verhalten); jede Chip schaltet ihn um.
        if (rotSnap != snapBefore) state.objectGizmoSnap = rotSnap;
        else if (scaleSnap != snapBefore) state.objectGizmoSnap = scaleSnap;
        if (changed) state.viewportSettingsDirty = true;
        } // Steuerungsgruppe (ggf. zweite Zeile)
    }
    ImGui::EndGroup();
    capturing |= ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), false);

    // ---- rechte Gruppe ----
    char speedLabel[48];
    std::snprintf(speedLabel, sizeof(speedLabel), "%s %d##vpSpeed", L("Tempo", "Speed"), state.cameraSpeedSetting);
    const char* playLabel = state.playtestActive ? L("Stopp##vpPlay", "Stop##vpPlay")
                                                 : L("Spieltest##vpPlay", "Playtest##vpPlay");
    const ImGuiStyle& style = ImGui::GetStyle();
    const auto buttonW = [&](const char* label) {
        return ImGui::CalcTextSize(label, nullptr, true).x + style.FramePadding.x * 2.0f;
    };
    const float glyphPad = ImGui::CalcTextSize("    ").x;
    const float rightW = buttonW(speedLabel) + glyphPad + buttonW("G##vpGame") + buttonW(playLabel) + glyphPad +
                         style.ItemSpacing.x * 2.0f;
    ImGui::SetCursorScreenPos(ImVec2(imageScreenPos.x + std::max(10.0f, width - rightW - 10.0f),
                                     imageScreenPos.y + 10.0f));
    ImGui::BeginGroup();
    if (viewport_ui::GlyphButton(speedLabel, viewport_ui::ButtonGlyph::DropDown)) ImGui::OpenPopup("##viewportSpeedMenu");
    viewport_ui::Tooltip(L("Kameratempo (Stufe 1..8, wie Unreal). Mausrad bei gehaltener RMB ändert es.",
                           "Camera speed (level 1..8, like Unreal). Mouse wheel while holding RMB changes it."));
    if (ImGui::BeginPopup("##viewportSpeedMenu")) {
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::SliderInt("##camSpeed", &state.cameraSpeedSetting, core::level::kMinCameraSpeedSetting,
                             core::level::kMaxCameraSpeedSetting))
            state.viewportSettingsDirty = true;
        ImGui::TextDisabled("× %.3g", core::level::CameraSpeedMultiplier(state.cameraSpeedSetting));
        ImGui::SeparatorText(L("Spieltest", "Playtest"));
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::SliderFloat(L("Laufen##ptSpeed", "Run##ptSpeed"), &state.playtestConfig.runSpeed, 30.0f, 600.0f, "%.0f"))
            state.viewportSettingsDirty = true;
        viewport_ui::Tooltip(L("Editorwert in Welteinheiten/s - kein aus dem Client belegter Wert.",
                               "Editor value in world units/s - not a value verified from the client."));
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    viewport_ui::ToggleChip("G##vpGame", &state.gameView,
                            L("Spielansicht: Editor-Helfer ausblenden (G)", "Game view: hide editor helpers (G)"));
    ImGui::SameLine();
    if (state.playtestActive) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(150, 40, 40, 245));
    else ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(22, 110, 60, 245));
    if (viewport_ui::GlyphButton(playLabel, state.playtestActive ? viewport_ui::ButtonGlyph::Stop
                                                                  : viewport_ui::ButtonGlyph::Play))
        TogglePlaytest(state);
    ImGui::PopStyleColor();
    viewport_ui::Tooltip(L("Spieltest auf dem SHBD-Block&Walk-Gitter (Alt+P, Esc beendet). "
                           "Spawn: MapInfo.shn RegenX/RegenY, sonst Kameraziel.",
                           "Playtest on the SHBD block & walk grid (Alt+P, Esc ends). "
                           "Spawn: MapInfo.shn RegenX/RegenY, otherwise camera target."));
    ImGui::EndGroup();
    capturing |= ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), false);

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    ImGui::SetCursorScreenPos(restore);
    return capturing;
}
// Bodentreffer unter der Maus: HTD-Terrain, für NIF-only-Karten die Ebene auf Höhe des Kameraziels.
std::optional<EditVec3> PickViewportGround(EditorState& state, const ImVec2& imagePos, int w, int h,
                                           const ImVec2& mouse) {
    if (MapSupportsTerrainEditing(state)) return PickTerrainFrom3D(state, imagePos, w, h, mouse);
    const auto nearP = Unproject3D(state, imagePos, w, h, mouse, -1.0f);
    const auto farP = Unproject3D(state, imagePos, w, h, mouse, 1.0f);
    if (!nearP || !farP) return std::nullopt;
    const float dy = farP->y - nearP->y;
    if (std::abs(dy) < 1.0e-6f) return std::nullopt;
    const float t = (state.camera.TargetY() - nearP->y) / dy;
    if (t < 0.0f || t > 1.0f) return std::nullopt;
    return EditVec3{nearP->x + (farP->x - nearP->x) * t, state.camera.TargetY(), nearP->z + (farP->z - nearP->z) * t};
}
// Weltraster auf Höhe des Kameraziels; Abstand = Raster-Snap, automatisch vergröbert.
void DrawViewportGrid(EditorState& state, const ImVec2& imagePos, int w, int h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float spacing = state.objectGizmoSnap ? std::max(1.0f, state.objectMoveSnap) : 50.0f;
    const float extent = std::clamp(state.camera.Distance() * 1.4f, 300.0f, 30000.0f);
    while (extent / spacing > 40.0f) spacing *= 2.0f;
    const float cx = std::floor(state.camera.TargetX() / spacing) * spacing;
    const float cz = std::floor(state.camera.TargetZ() / spacing) * spacing;
    const float y = state.camera.TargetY();
    const int n = static_cast<int>(extent / spacing);
    constexpr int kSegments = 16;
    dl->PushClipRect(imagePos, ImVec2(imagePos.x + w, imagePos.y + h), true);
    for (int axis = 0; axis < 2; ++axis) {
        for (int i = -n; i <= n; ++i) {
            const float lineCoord = (axis == 0 ? cx : cz) + static_cast<float>(i) * spacing;
            const long long gridIndex = static_cast<long long>(std::llround(lineCoord / spacing));
            const bool major = gridIndex % 10 == 0;
            // Dunkle Linien bleiben auf hellem Fiesta-Sand- und Schneeterrain sichtbar.
            const ImU32 col = major ? IM_COL32(10, 40, 70, 170) : IM_COL32(20, 50, 80, 95);
            ImVec2 prev{};
            bool prevOk = false;
            for (int sgm = 0; sgm <= kSegments; ++sgm) {
                const float along = -extent + 2.0f * extent * static_cast<float>(sgm) / kSegments;
                const EditVec3 p = axis == 0 ? EditVec3{lineCoord, y, cz + along} : EditVec3{cx + along, y, lineCoord};
                ImVec2 sp;
                const bool ok = ProjectWorldTo3DView(state, imagePos, w, h, p, sp);
                if (ok && prevOk) dl->AddLine(prev, sp, col, major ? 1.4f : 1.0f);
                prev = sp;
                prevOk = ok;
            }
        }
    }
    dl->PopClipRect();
}
// SHBD-Block&Walk-Zellen (6.25 Einheiten) um einen Weltpunkt als halbtransparente Quads.
void DrawWalkCollision3D(EditorState& state, const ImVec2& imagePos, int w, int h, float centerX, float centerZ) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    constexpr int kRadiusCells = 26;
    const float cs = core::WalkGrid::kCellSize;
    const int ccx = static_cast<int>(std::floor(centerX / cs));
    const int ccz = static_cast<int>(std::floor(centerZ / cs));
    const float fallbackY = state.playtestActive ? state.playtestPawn.y : state.camera.TargetY();
    // Ein GroundObject-Raycast je Frame (Mitte) statt je Zellecke: Plattformen wie das Roumen-
    // Pflaster sind lokal eben; das HTD wird pro Ecke abgetastet.
    const std::optional<float> platformY = GroundObjectHeightAt(state, centerX, centerZ);
    const bool terrain = MapSupportsTerrainEditing(state);
    const auto cornerY = [&](float x, float z) {
        float y = terrain ? state.heightmap.SampleWorld(x, z) : fallbackY;
        if (platformY) y = terrain ? std::max(y, *platformY) : *platformY;
        return y + 1.5f;
    };
    dl->PushClipRect(imagePos, ImVec2(imagePos.x + w, imagePos.y + h), true);
    for (int dz = -kRadiusCells; dz <= kRadiusCells; ++dz) {
        for (int dx = -kRadiusCells; dx <= kRadiusCells; ++dx) {
            if (dx * dx + dz * dz > kRadiusCells * kRadiusCells) continue;
            const int cx = ccx + dx, cz = ccz + dz;
            if (cx < 0 || cz < 0) continue;
            const bool blocked = state.walkGrid.CellBlocked(static_cast<std::uint32_t>(cx), static_cast<std::uint32_t>(cz));
            if (!blocked) continue;
            const float x0 = static_cast<float>(cx) * cs, z0 = static_cast<float>(cz) * cs;
            const float corners[4][2] = {{x0, z0}, {x0 + cs, z0}, {x0 + cs, z0 + cs}, {x0, z0 + cs}};
            ImVec2 quad[4];
            bool ok = true;
            for (int k = 0; k < 4 && ok; ++k) {
                const float gy = cornerY(corners[k][0], corners[k][1]);
                ok = ProjectWorldTo3DView(state, imagePos, w, h, {corners[k][0], gy, corners[k][1]}, quad[k]);
            }
            if (!ok) continue;
            dl->AddQuadFilled(quad[0], quad[1], quad[2], quad[3], IM_COL32(235, 70, 70, 70));
        }
    }
    dl->PopClipRect();
}
void DrawViewportAxisIndicator(EditorState& state, const ImVec2& imagePos, int h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const app::Mat4 view = state.camera.ViewMatrix();
    const ImVec2 origin(imagePos.x + 38.0f, imagePos.y + static_cast<float>(h) - 38.0f);
    constexpr float kLen = 24.0f;
    struct Axis { const char* name; float sx, sy; ImU32 col; };
    // Bildschirmrichtung je Weltachse aus Rechts-/Oben-Zeile der View-Matrix; Welt-Z ist im
    // Anzeigeraum gespiegelt (siehe OrbitCamera::SetTarget).
    const Axis axes[3] = {
        {"X", view.m[0], view.m[1], IM_COL32(235, 80, 80, 255)},
        {"Y", view.m[4], view.m[5], IM_COL32(110, 220, 110, 255)},
        {"Z", -view.m[8], -view.m[9], IM_COL32(90, 150, 255, 255)},
    };
    dl->AddCircleFilled(origin, 32.0f, IM_COL32(8, 16, 24, 150));
    for (const auto& a : axes) {
        const ImVec2 tip(origin.x + a.sx * kLen, origin.y - a.sy * kLen);
        dl->AddLine(origin, tip, a.col, 2.0f);
        dl->AddText(ImVec2(tip.x - 3.0f, tip.y - 7.0f), a.col, a.name);
    }
}
void DrawViewportStats(EditorState& state, const ImVec2& imagePos, int w) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    std::vector<std::string> lines;
    char buf[256];
    const ImGuiIO& io = ImGui::GetIO();
    std::snprintf(buf, sizeof(buf), "%.0f FPS  ·  %.2f ms", io.Framerate, io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f);
    lines.emplace_back(buf);
    const std::size_t objects = state.placementSet.Count() + state.shmdCategoryRenderSet.Count();
    std::snprintf(buf, sizeof(buf), L("Objekte %zu  ·  Meshes %zu  ·  ausgeblendet %zu", "Objects %zu  ·  meshes %zu  ·  hidden %zu"),
                  objects, state.nifMeshRenderer.RealMeshCount() + state.shmdCategoryMeshRenderer.RealMeshCount(),
                  CountEditorHiddenObjects(state));
    lines.emplace_back(buf);
    {
        const auto& a = state.nifMeshRenderer.LastFrameStats();
        const auto& b = state.shmdCategoryMeshRenderer.LastFrameStats();
        std::snprintf(buf, sizeof(buf), L("Teile gezeichnet %zu  ·  außerhalb Sicht %zu  ·  Objekte %.1f ms CPU",
                                          "Parts drawn %zu  ·  off-screen %zu  ·  objects %.1f ms CPU"),
                      a.drawnParts + b.drawnParts, a.culledParts + b.culledParts, a.cpuMs + b.cpuMs);
        lines.emplace_back(buf);
    }
    std::snprintf(buf, sizeof(buf), L("NPCs %zu  ·  Auswahl %zu", "NPCs %zu  ·  selection %zu"),
                  state.npcRenderSet.Count(), state.selectedObjects.size());
    lines.emplace_back(buf);
    std::snprintf(buf, sizeof(buf), L("Kamera %.0f / %.0f / %.0f  ·  Abstand %.0f", "Camera %.0f / %.0f / %.0f  ·  distance %.0f"),
                  state.camera.TargetX(), state.camera.TargetY(), state.camera.TargetZ(), state.camera.Distance());
    lines.emplace_back(buf);
    if (state.viewportCursorWorld) {
        const auto& c = *state.viewportCursorWorld;
        const bool blocked = core::level::WalkBlockedAtWorld(state.walkGrid, c[0], c[2]);
        std::snprintf(buf, sizeof(buf), L("Cursor X %.0f  Y(Server) %.0f  Höhe %.1f  ·  SHBD %d,%d %s",
                                          "Cursor X %.0f  Y(server) %.0f  height %.1f  ·  SHBD %d,%d %s"),
                      c[0], c[2], c[1],
                      static_cast<int>(std::floor(c[0] / core::WalkGrid::kCellSize)),
                      static_cast<int>(std::floor(c[2] / core::WalkGrid::kCellSize)),
                      blocked ? L("blockiert", "blocked") : L("begehbar", "walkable"));
        lines.emplace_back(buf);
    }
    float maxW = 0.0f;
    for (const auto& l : lines) maxW = std::max(maxW, ImGui::CalcTextSize(l.c_str()).x);
    const float lineH = ImGui::GetTextLineHeight();
    // Unter der Viewport-Leiste (in schmalen Viewports zweizeilig, siehe DrawViewportToolbar).
    const bool twoToolbarRows = state.editMode == EditMode::ObjectPlacement && !state.playtestActive && w < 1180;
    const ImVec2 p0(imagePos.x + static_cast<float>(w) - maxW - 22.0f, imagePos.y + (twoToolbarRows ? 70.0f : 42.0f));
    const ImVec2 p1(p0.x + maxW + 12.0f, p0.y + lineH * static_cast<float>(lines.size()) + 10.0f);
    dl->AddRectFilled(p0, p1, IM_COL32(6, 14, 22, 175), 5.0f);
    float y = p0.y + 5.0f;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const ImU32 col = i == 0 ? IM_COL32(120, 230, 160, 255) : IM_COL32(205, 220, 232, 235);
        dl->AddText(ImVec2(p0.x + 6.0f, y), col, lines[i].c_str());
        y += lineH;
    }
}
// Spielermarker (Kapsel, Boden-Kreis, Blickrichtung) + HUD.
void DrawPlaytestOverlay(EditorState& state, const ImVec2& imagePos, int w, int h, bool showHud) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const auto& pawn = state.playtestPawn;
    const float r = state.playtestConfig.radius;
    const bool flash = state.playtestBlockedFlash > 0.0f;
    const ImU32 col = flash ? IM_COL32(255, 90, 80, 255) : IM_COL32(60, 225, 255, 255);
    ImVec2 base, top, rim, nose;
    if (ProjectWorldTo3DView(state, imagePos, w, h, {pawn.x, pawn.y + 1.0f, pawn.z}, base) &&
        ProjectWorldTo3DView(state, imagePos, w, h, {pawn.x, pawn.y + kPlaytestPawnHeight, pawn.z}, top)) {
        float screenR = 6.0f;
        if (ProjectWorldTo3DView(state, imagePos, w, h, {pawn.x + r, pawn.y + 1.0f, pawn.z}, rim))
            screenR = std::max(4.0f, std::hypot(rim.x - base.x, rim.y - base.y));
        dl->AddCircle(base, screenR, col, 24, 2.0f);
        dl->AddLine(base, top, col, std::max(3.0f, screenR * 0.8f));
        dl->AddCircleFilled(top, std::max(3.0f, screenR * 0.55f), col);
        const float fx = std::sin(pawn.facingYaw), fz = std::cos(pawn.facingYaw);
        if (ProjectWorldTo3DView(state, imagePos, w, h, {pawn.x + fx * r * 3.0f, pawn.y + 1.0f, pawn.z + fz * r * 3.0f}, nose))
            dl->AddLine(base, nose, IM_COL32(255, 220, 90, 255), 2.5f);
    }
    if (!showHud) return;
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  L("SPIELTEST  ·  X %.0f  Y(Server) %.0f  ·  SHBD %s  ·  %s",
                    "PLAYTEST  ·  X %.0f  Y(server) %.0f  ·  SHBD %s  ·  %s"),
                  pawn.x, pawn.z,
                  state.playtestConfig.collideWithWalkGrid ? L("Kollision an", "collision on")
                                                           : L("Kollision aus", "collision off"),
                  L("WASD · Shift gehen · RMB/LMB drehen · Rad Zoom · C Kollision · Esc Ende",
                    "WASD · Shift walk · RMB/LMB turn · wheel zoom · C collision · Esc end"));
    const ImVec2 ts = ImGui::CalcTextSize(buf);
    const ImVec2 p0(imagePos.x + (static_cast<float>(w) - ts.x) * 0.5f - 10.0f, imagePos.y + 40.0f);
    dl->AddRectFilled(p0, ImVec2(p0.x + ts.x + 20.0f, p0.y + ts.y + 12.0f), IM_COL32(6, 14, 22, 200), 6.0f);
    dl->AddRect(p0, ImVec2(p0.x + ts.x + 20.0f, p0.y + ts.y + 12.0f), col, 6.0f, 0, 1.5f);
    dl->AddText(ImVec2(p0.x + 10.0f, p0.y + 6.0f), IM_COL32(230, 245, 255, 255), buf);
}
// Spieltest-Steuerung: ersetzt im Viewport die Editor-Kamera, solange der Spieltest läuft.
void UpdatePlaytest(EditorState& state, bool viewportHovered) {
    ImGuiIO& io = ImGui::GetIO();
    if (viewportHovered) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            state.camera.OrbitBy(io.MouseDelta.x * 0.008f, -io.MouseDelta.y * 0.006f);
        if (io.MouseWheel != 0.0f) {
            state.camera.ZoomSteps(io.MouseWheel);
            state.camera.SetDistance(std::clamp(state.camera.Distance(), 90.0f, 3000.0f));
        }
    }
    core::level::PlaytestInput in;
    if (!io.WantTextInput) {
        if (ImGui::IsKeyDown(ImGuiKey_W) || ImGui::IsKeyDown(ImGuiKey_UpArrow)) in.forward += 1.0f;
        if (ImGui::IsKeyDown(ImGuiKey_S) || ImGui::IsKeyDown(ImGuiKey_DownArrow)) in.forward -= 1.0f;
        if (ImGui::IsKeyDown(ImGuiKey_D) || ImGui::IsKeyDown(ImGuiKey_RightArrow)) in.right += 1.0f;
        if (ImGui::IsKeyDown(ImGuiKey_A) || ImGui::IsKeyDown(ImGuiKey_LeftArrow)) in.right -= 1.0f;
        if (ImGui::IsKeyPressed(ImGuiKey_C, false)) {
            if (state.playtestWalkGridUsable) {
                state.playtestConfig.collideWithWalkGrid = !state.playtestConfig.collideWithWalkGrid;
            } else {
                state.statusMessage = L("Kein begehbares SHBD-Gitter geladen - Kollision bleibt aus.",
                                        "No walkable SHBD grid loaded - collision stays off.");
            }
        }
    }
    in.walk = io.KeyShift;
    // Kamera-Yaw -> Weltrichtung: vorwärts = (-sin yaw, +cos yaw), also viewYaw = -yaw.
    in.viewYaw = -state.camera.Yaw();
    const auto blocked = [&state](float x, float z) { return core::level::WalkBlockedAtWorld(state.walkGrid, x, z); };
    const float fallbackY = state.playtestPawn.y;
    const auto height = [&state, fallbackY](float x, float z) { return PlaytestGroundHeight(state, x, z, fallbackY); };
    core::level::StepPlaytest(state.playtestPawn, in, io.DeltaTime, state.playtestConfig, blocked, height);
    if (state.playtestPawn.lastStepBlocked) state.playtestBlockedFlash = 0.25f;
    state.playtestBlockedFlash = std::max(0.0f, state.playtestBlockedFlash - io.DeltaTime);
    state.camera.SetTarget(state.playtestPawn.x, state.playtestPawn.y + kPlaytestEyeHeight, state.playtestPawn.z);
}
// Rahmenauswahl (Strg+Alt+LMB): Treffer über die projizierten Ecken der Objekt-Bounds
// (NIF-Footprint-Bounds), für Modelle ohne Bounds über den Objektursprung.
std::vector<int> CollectMarqueeHits(EditorState& state, const ImVec2& imagePos, int w, int h,
                                    const core::level::ScreenRect& rect) {
    std::vector<int> hits;
    RefreshObjectVisibility(state);
    RefreshShmdCategoryVisibility(state);
    auto testObject = [&](int id, const core::PlacedObject& obj) {
        std::vector<core::level::ScreenPoint> pts;
        const auto& fp = GetOrComputeFootprint(state, obj.modelPath);
        if (fp.valid) {
            const float angle = 2.0f * std::atan2(obj.rotY, obj.rotW);
            const float c = std::cos(angle), sn = std::sin(angle);
            for (float lx : {fp.minX, fp.maxX}) for (float ly : {fp.minY, fp.maxY}) for (float lz : {fp.minZ, fp.maxZ}) {
                const float sx = lx * obj.scale, sy = ly * obj.scale, sz = lz * obj.scale;
                ImVec2 p;
                if (ProjectWorldTo3DView(state, imagePos, w, h,
                                         {obj.posX + sx * c + sz * sn, obj.posY + sy, obj.posZ - sx * sn + sz * c}, p))
                    pts.push_back({p.x, p.y});
            }
            if (pts.size() != 8 && state.marqueeMode == core::level::MarqueeMode::Inside) return; // teilweise hinter der Kamera
        } else {
            ImVec2 p;
            if (ProjectWorldTo3DView(state, imagePos, w, h, {obj.posX, obj.posY, obj.posZ}, p)) pts.push_back({p.x, p.y});
        }
        if (core::level::MarqueeHits(rect, pts, state.marqueeMode)) hits.push_back(id);
    };
    for (std::size_t i = 0; i < state.placementSet.Count(); ++i) {
        if (IsObjectHidden(state, i) || IsObjectEditorLocked(state, static_cast<int>(i))) continue;
        testObject(static_cast<int>(i), state.placementSet.At(i));
    }
    for (std::size_t i = 0; i < state.shmdCategoryRenderSet.Count(); ++i) {
        const int id = ShmdSelectionId(i);
        if ((i < state.shmdCategoryHidden.size() && state.shmdCategoryHidden[i]) || IsObjectEditorLocked(state, id) ||
            IsObjectEditorHidden(state, id)) continue;
        testObject(id, state.shmdCategoryRenderSet.At(i));
    }
    return hits;
}
void DrawPreview3DContent(EditorState& state) {
    if (state.focusLoadedNifScenePending) {
        FocusLoadedNifSceneFromBounds(state);
        state.focusLoadedNifScenePending = false;
    }
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const int w = std::max(1, static_cast<int>(avail.x));
    const int h = std::max(1, static_cast<int>(avail.y));

    if (state.meshDirty) {
        state.renderer.RebuildMesh(state.heightmap);
        UpdatePreviewTexture(state);
        state.meshDirty = false;
    }

    const ImVec2 imageScreenPos = ImGui::GetCursorScreenPos();
    // Karten-Renderdaten und Darstellungsoptionen (Ansicht-/Anzeigen-Menü der Viewport-Leiste).
    state.renderer.SetGroundDirectionalLight(state.mapRenderConfig.groundDirectionalLight);
    state.renderer.SetGlowSettings(state.mapRenderConfig.glow);
    state.renderer.SetGlowEnabled(state.viewportGlow && state.viewportViewMode == 0);
    state.renderer.SetVertexColorEnabled(state.viewportVertexColors);
    state.renderer.SetMsaaSamples(state.viewportMsaaSamples);
    state.renderer.SetViewMode(state.viewportViewMode);
    state.nifMeshRenderer.SetViewMode(state.viewportViewMode);
    state.shmdCategoryMeshRenderer.SetViewMode(state.viewportViewMode);
    state.nifMeshRenderer.SetObjectAnimationEnabled(state.viewportObjectAnimation);
    state.shmdCategoryMeshRenderer.SetObjectAnimationEnabled(state.viewportObjectAnimation);
    state.npcMeshRenderer.SetViewMode(state.viewportViewMode);
    {
        // SHMD-Umgebung -> Licht, Nebel, Hintergrund (Datenherkunft: docs/RENDERER.md).
        app::SceneLighting lighting;
        if (state.sceneEnvironmentFromShmd) {
            const app::SceneLighting env = app::SceneLightingFromEnvironment(
                state.placementSet.environment, state.placementSet.hasLightingFooter);
            if (state.viewportMapLighting) lighting = env;
            if (state.viewportMapBackground) {
                std::copy(std::begin(env.background), std::end(env.background), std::begin(lighting.background));
                lighting.backgroundFromMapData = true;
            }
            std::copy(std::begin(env.fogColor), std::end(env.fogColor), std::begin(lighting.fogColor));
            lighting.fogStart = env.fogStart;
            lighting.fogEnd = env.fogEnd;
            lighting.fogEnabled = (state.viewportFog || state.playtestActive) && !state.camera.IsOrthographic();
        }
        state.renderer.SetSceneLighting(lighting);
        state.nifMeshRenderer.SetSceneLighting(lighting);
        state.shmdCategoryMeshRenderer.SetSceneLighting(lighting);
        state.npcMeshRenderer.SetSceneLighting(lighting);
    }
    // Assistent "Neue Karte": Minimap erst jetzt erzeugen (Terrain-Mesh, Modelle und Licht sind geladen).
    ProcessPendingMinimap(state);
    state.renderer.BeginScene(state.camera, w, h, state.wireframe);
    // Immer neu aufbauen (statt Dirty-Tracking über alle Objekt-Mutationsstellen hinweg) -
    // bei ein paar Tausend Instanzen unproblematisch, aber garantiert nie veraltet (z.B. nach
    // Auswahl-Wechsel für den Highlight-Farbton). Platzhalter-Pyramide wird für Objekte
    // übersprungen, für die bereits ein echtes Mesh geladen werden konnte (siehe
    // NifMeshRenderer) - sonst doppelte Darstellung.
    RefreshObjectVisibility(state);
    state.renderer.SetTerrainVisible(state.showTerrain);
    for (std::size_t li = 0; li < state.textureStack.LayerCount(); ++li) {
        state.renderer.SetLayerVisible(static_cast<int>(li), li >= state.layerHidden.size() || state.layerHidden[li] == 0);
    }
    const std::unordered_set<int> selectedPlacementIds(state.selectedObjects.begin(), state.selectedObjects.end());
    // Spielansicht (G) und Spieltest blenden Editor-Helfer aus - Platzhalter-Pyramiden gehören dazu.
    const bool editorHelpers = !state.gameView && !state.playtestActive;
    state.objectMarkerRenderer.RebuildInstances(state.placementSet, state.selectedObjects,
        [&state, &selectedPlacementIds, editorHelpers](std::size_t i) {
            const bool selected = selectedPlacementIds.contains(static_cast<int>(i));
            return IsObjectHidden(state, i) || !editorHelpers ||
                   (!state.showObjectMarkers && !selected) ||
                   (state.nifMeshRenderer.HasRealMesh(i) && !selected);
        });
    state.objectMarkerRenderer.Draw(state.camera, w, h);
    if (state.editMode == EditMode::Portals && editorHelpers) {
        // Portal-Ziele als grosse Marker (wie die Objekt-Platzhalter, aber 3-4x so gross): TownPortal
        // etwas groesser als Schriftrollen-Ziele; das gewaehlte Ziel wird weiss. Jeden Frame neu
        // aufgebaut (nur wenige Eintraege).
        EnsurePortalDataLoaded(state);
        core::ObjectPlacementSet portalSet;
        int selectedIdx = -1;
        for (const auto& m : CollectPortalMarkers(state)) {
            core::PlacedObject marker;
            marker.posX = m.x;
            marker.posZ = m.y;
            marker.posY = state.heightmap.SampleWorld(m.x, m.y) + 150.0f;
            marker.scale = m.kind == kPortalKindTown ? 4.0f : 3.0f;
            const std::size_t at = portalSet.AddObject(std::move(marker));
            if (m.kind == state.selectedPortalKind && static_cast<int>(m.idx) == state.selectedPortalIdx) selectedIdx = static_cast<int>(at);
        }
        state.portalMarkerRenderer.RebuildInstances(portalSet, selectedIdx);
        state.portalMarkerRenderer.Draw(state.camera, w, h);
    }
    if (state.showObjectMeshes) state.nifMeshRenderer.Draw(state.placementSet, state.camera, w, h, &state.objectHidden);
    RefreshShmdCategoryVisibility(state);
    if (state.showObjectMeshes && state.shmdCategoryRenderSet.Count() > 0) {
        state.shmdCategoryMeshRenderer.Draw(state.shmdCategoryRenderSet, state.camera, w, h, &state.shmdCategoryHidden);
    }
    EnsureNpcModelsLoaded(state);
    if (state.showNpcModels) state.npcMeshRenderer.Draw(state.npcRenderSet, state.camera, w, h);
    const GLuint tex = state.renderer.EndScene();
    if (tex != 0) {
        ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(tex)), ImVec2(static_cast<float>(w), static_cast<float>(h)),
                     ImVec2(0, 1), ImVec2(1, 0)); // FBO-Textur ist vertikal gespiegelt -> UVs tauschen
    }
    const bool viewImageHovered = ImGui::IsItemHovered();
    if (state.editMode == EditMode::ObjectPlacement && ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload=ImGui::AcceptDragDropPayload("NEXTGEN_NIF_ASSET")) {
            const char* model=static_cast<const char*>(payload->Data);
            if (const auto hit=PickTerrainFrom3D(state,imageScreenPos,w,h,ImGui::GetMousePos())) {
                PlaceObjectAtWorld(state,model,hit->x,hit->y,hit->z);
                state.statusMessage=std::string("Objekt im 3D-Viewport platziert: ")+model;
            }
        }
        ImGui::EndDragDropTarget();
    }
    state.viewport3dPos = imageScreenPos;
    state.viewport3dSize = ImVec2(static_cast<float>(w), static_cast<float>(h));
    // Bodentreffer unter der Maus (Statistik-Overlay, SHBD-Kollisionsanzeige).
    state.viewportCursorWorld.reset();
    if (viewImageHovered) {
        if (const auto hit = PickViewportGround(state, imageScreenPos, w, h, ImGui::GetMousePos()))
            state.viewportCursorWorld = std::array<float, 3>{hit->x, hit->y, hit->z};
    }
    if (editorHelpers) {
        if (state.showViewportGrid) DrawViewportGrid(state, imageScreenPos, w, h);
        Draw3DBrushOverlay(state,imageScreenPos,w,h,viewImageHovered);
        DrawMobZones3D(state,imageScreenPos,w,h);
        DrawPortals3D(state,imageScreenPos,w,h);
        DrawRoamRoutes3D(state,imageScreenPos,w,h);
    }
    if (state.showWalkCollision3D && !state.gameView) {
        if (state.playtestActive)
            DrawWalkCollision3D(state, imageScreenPos, w, h, state.playtestPawn.x, state.playtestPawn.z);
        else if (state.viewportCursorWorld)
            DrawWalkCollision3D(state, imageScreenPos, w, h, (*state.viewportCursorWorld)[0], (*state.viewportCursorWorld)[2]);
    }
    const bool gizmoCapturing = DrawObjectTransformGizmo(state, imageScreenPos, w, h);
    if (editorHelpers) DrawNpcOverlay3D(state, imageScreenPos, w, h);
    if (state.playtestActive) // Spielfigur in allen Viewports, Hinweisleiste nur im aktiven
        DrawPlaytestOverlay(state, imageScreenPos, w, h, state.drawingActiveViewport);
    if (editorHelpers && state.showViewportAxes) DrawViewportAxisIndicator(state, imageScreenPos, h);
    if (state.showViewportStats && !state.gameView && !state.playtestActive) DrawViewportStats(state, imageScreenPos, w);
    const bool gizmoToolbarCapturing = DrawViewportToolbar(state, imageScreenPos, avail.x);

    // Viewport-Overlays liegen absichtlich über dem Image. Ihre Hit-Flächen müssen deshalb
    // vor Picking/Kamera explizit berücksichtigt werden, sonst kann ein Button-Klick in die
    // darunterliegende 3D-Auswahl oder Orbit-Steuerung "durchfallen".
    const ImVec2 zoomBtnSize(26.0f, 26.0f);
    const ImVec2 zoomPos(imageScreenPos.x + avail.x - zoomBtnSize.x - 10.0f,
                         imageScreenPos.y + avail.y - zoomBtnSize.y * 2.0f - 16.0f);
    const ImVec2 zoomMax(zoomPos.x + zoomBtnSize.x,
                         zoomPos.y + zoomBtnSize.y * 2.0f + 4.0f);
    const bool zoomControlsCapturing =
        ImGui::IsMouseHoveringRect(zoomPos, zoomMax, false);
    const bool viewportUiCapturing =
        gizmoCapturing || gizmoToolbarCapturing || zoomControlsCapturing;

    // Direktes 3D-Picking: echte NIF-Dreiecke haben Vorrang. Nur Objekte ohne
    // ladbares Mesh fallen weiterhin auf den projizierten Ursprung/Footprint zurück.
    // Frei gesetzter Pivot wie in Unreal: Alt + mittlere Maustaste klicken setzt den Gizmo-Pivot auf
    // den Bodentreffer (HTD bzw. Zielebene); Alt+MMB-Doppelklick setzt ihn zurück.
    if (state.editMode == EditMode::ObjectPlacement && !state.playtestActive && viewImageHovered &&
        ImGui::GetIO().KeyAlt && !state.selectedObjects.empty()) {
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Middle)) {
            state.pivotOverride.reset();
            state.objectGizmoMatrixValid = false;
            state.statusMessage = L("Pivot zurückgesetzt.", "Pivot reset.");
        } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
            if (const auto hit = PickViewportGround(state, imageScreenPos, w, h, ImGui::GetMousePos())) {
                state.pivotOverride = std::array<float, 3>{hit->x, hit->y, hit->z};
                state.pivotOverrideSelectionKey.clear();
                for (const int id : state.selectedObjects) state.pivotOverrideSelectionKey += std::to_string(id) + ",";
                state.objectGizmoMatrixValid = false;
                state.statusMessage = L("Pivot gesetzt (Alt+MMB-Doppelklick setzt zurück).",
                                        "Pivot set (Alt+MMB double-click resets).");
            }
        }
    }

    // Rahmenauswahl wie in Unreal: Strg+Alt+LMB ziehen (Shift ergänzt die Auswahl).
    const ImGuiIO& pickIo = ImGui::GetIO();
    const bool objectPicking = state.editMode==EditMode::ObjectPlacement && !state.playtestActive;
    if (objectPicking && viewImageHovered && !viewportUiCapturing && pickIo.KeyCtrl && pickIo.KeyAlt &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        state.marqueeActive = true;
        state.marqueeStart = ImGui::GetMousePos();
    }
    if (state.marqueeActive) {
        const ImVec2 cur = ImGui::GetMousePos();
        const auto rect = core::level::ScreenRect::FromCorners({state.marqueeStart.x, state.marqueeStart.y}, {cur.x, cur.y});
        ImDrawList* mdl = ImGui::GetWindowDrawList();
        const bool crossing = state.marqueeMode == core::level::MarqueeMode::Crossing;
        mdl->AddRectFilled(ImVec2(rect.minX, rect.minY), ImVec2(rect.maxX, rect.maxY),
                           crossing ? IM_COL32(80, 200, 120, 40) : IM_COL32(70, 150, 255, 40));
        mdl->AddRect(ImVec2(rect.minX, rect.minY), ImVec2(rect.maxX, rect.maxY),
                     crossing ? IM_COL32(110, 235, 150, 230) : IM_COL32(110, 180, 255, 230), 0.0f, 0, 1.5f);
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            state.marqueeActive = false;
            if (rect.Width() > 3.0f || rect.Height() > 3.0f) {
                const auto hits = CollectMarqueeHits(state, imageScreenPos, w, h, rect);
                const auto mode = pickIo.KeyShift ? core::level::SelectionCombine::Add
                                                  : core::level::SelectionCombine::Replace;
                state.selectedObjects = core::level::CombineSelection(state.selectedObjects, hits, mode);
                state.selectedObject = state.selectedObjects.empty() ? kNoObjectSelection : state.selectedObjects.back();
                state.selectedObjectModelPathFor = kNoObjectSelection;
                state.objectGizmoMatrixValid = false;
                state.objectPlaceMode = 0;
                state.statusMessage = std::to_string(hits.size()) + L(" Objekt(e) per Rahmen gewählt.",
                                                                    " object(s) selected by marquee.");
            }
        }
    }

    if (objectPicking && viewImageHovered && !viewportUiCapturing && !state.marqueeActive &&
        !(state.unrealNavigation && pickIo.KeyAlt) && !(pickIo.KeyCtrl && pickIo.KeyAlt) &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const ImVec2 mouse=ImGui::GetMousePos();
        int bestId=kNoObjectSelection;
        float bestRay=std::numeric_limits<float>::infinity();

        const auto nearP=Unproject3D(state,imageScreenPos,w,h,mouse,-1.0f);
        const auto farP=Unproject3D(state,imageScreenPos,w,h,mouse,1.0f);
        std::array<float,3> rayOrigin{}, rayDir{};
        bool rayValid=false;
        if(nearP&&farP) {
            rayOrigin={nearP->x,nearP->y,nearP->z};
            rayDir={farP->x-nearP->x,farP->y-nearP->y,farP->z-nearP->z};
            const float len=std::sqrt(rayDir[0]*rayDir[0]+rayDir[1]*rayDir[1]+rayDir[2]*rayDir[2]);
            if(len>1.0e-6f) {
                for(float& v:rayDir) v/=len;
                rayValid=true;
            }
        }

        RefreshObjectVisibility(state);
        if(rayValid&&state.showObjectMeshes) {
            for(std::size_t i=0;i<state.placementSet.Count();++i) {
                if(IsObjectHidden(state,i)||IsObjectEditorLocked(state,static_cast<int>(i))||
                   !state.nifMeshRenderer.HasRealMesh(i)) continue;
                if(const auto hit=state.nifMeshRenderer.RaycastObject(
                       state.placementSet,i,state.camera,rayOrigin,rayDir);
                   hit&&*hit<bestRay) {
                    bestRay=*hit;
                    bestId=static_cast<int>(i);
                }
            }
        }

        RefreshShmdCategoryVisibility(state);
        if(rayValid&&state.showObjectMeshes) {
            for(std::size_t i=0;i<state.shmdCategoryRenderSet.Count();++i) {
                const int id=ShmdSelectionId(i);
                if((i<state.shmdCategoryHidden.size()&&state.shmdCategoryHidden[i])||
                   IsObjectEditorLocked(state,id)||!state.shmdCategoryMeshRenderer.HasRealMesh(i)) continue;
                if(const auto hit=state.shmdCategoryMeshRenderer.RaycastObject(
                       state.shmdCategoryRenderSet,i,state.camera,rayOrigin,rayDir);
                   hit&&*hit<bestRay) {
                    bestRay=*hit;
                    bestId=id;
                }
            }
        }

        // Kein Geometrietreffer: Marker-/Ursprungs-Fallback ausschließlich für Modelle,
        // deren NIF nicht geladen werden konnte.
        if(bestId==kNoObjectSelection) {
            float bestScreen=18.0f;
            for(std::size_t i=0;i<state.placementSet.Count();++i) {
                if(IsObjectHidden(state,i)||IsObjectEditorLocked(state,static_cast<int>(i))||
                   state.nifMeshRenderer.HasRealMesh(i)) continue;
                const auto& obj=state.placementSet.At(i);
                ImVec2 p;
                if(!ProjectWorldTo3DView(state,imageScreenPos,w,h,{obj.posX,obj.posY,obj.posZ},p)) continue;
                const float dx=p.x-mouse.x,dy=p.y-mouse.y;
                const float d=std::sqrt(dx*dx+dy*dy);
                if(d<bestScreen){bestScreen=d;bestId=static_cast<int>(i);}
            }
            for(std::size_t i=0;i<state.shmdCategoryRenderSet.Count();++i) {
                const int id=ShmdSelectionId(i);
                if((i<state.shmdCategoryHidden.size()&&state.shmdCategoryHidden[i])||
                   IsObjectEditorLocked(state,id)||state.shmdCategoryMeshRenderer.HasRealMesh(i)) continue;
                const auto poly=ObjectFootprintWorldPolygon(state,state.shmdCategoryRenderSet.At(i));
                if(poly.empty()) continue;
                float x=0.0f,z=0.0f;
                for(const auto& q:poly){x+=q.first;z+=q.second;}
                x/=static_cast<float>(poly.size());z/=static_cast<float>(poly.size());
                ImVec2 p;
                if(!ProjectWorldTo3DView(state,imageScreenPos,w,h,{x,state.heightmap.SampleWorld(x,z),z},p)) continue;
                const float dx=p.x-mouse.x,dy=p.y-mouse.y;
                const float d=std::sqrt(dx*dx+dy*dy);
                if(d<bestScreen){bestScreen=d;bestId=id;}
            }
        }

        if(bestId!=kNoObjectSelection) {
            state.objectPlaceMode=0;
            SelectObjectOnCanvas(state,bestId,ImGui::GetIO().KeyCtrl);
            state.objectGizmoMatrixValid=false;
        } else if(!ImGui::GetIO().KeyCtrl) {
            ClearObjectSelection(state);
        }
    }

    // ---- Pinsel im 3D-Viewport (Terrain, Texturen, Block&Walk) wie Unreals Landscape-Modus:
    //  LMB malt auf dem Terrain unter der Maus, Alt+LMB dreht weiterhin die Kamera, RMB fliegt.
    const bool brushTool3D = (state.editMode == EditMode::Heightmap || state.editMode == EditMode::TexturePaint ||
                              state.editMode == EditMode::BlockWalk ||
                              (state.editMode == EditMode::ObjectPlacement && state.objectPlaceMode == 2)) &&
                             MapSupportsTerrainEditing(state) && !state.playtestActive;
    const bool objectPaint3D = state.editMode == EditMode::ObjectPlacement && state.objectPlaceMode == 2;
    if (brushTool3D && viewImageHovered && !viewportUiCapturing && !ImGui::GetIO().KeyAlt &&
        (!ImGui::GetIO().KeyCtrl || objectPaint3D) && ImGui::IsMouseDown(ImGuiMouseButton_Left) && !state.cameraLooking) {
        if (const auto hit = PickTerrainFrom3D(state, imageScreenPos, w, h, ImGui::GetMousePos()))
            ApplyBrushAtWorld(state, hit->x, hit->z);
    }
    // Prefab einsetzen: Klick auf das Terrain.
    if (state.editMode == EditMode::ObjectPlacement && state.objectPlaceMode == 3 && viewImageHovered && !viewportUiCapturing &&
        !ImGui::GetIO().KeyAlt && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !state.cameraLooking) {
        if (const auto hit = PickTerrainFrom3D(state, imageScreenPos, w, h, ImGui::GetMousePos()))
            PlacePrefabAtWorld(state, hit->x, hit->z);
    }
    const bool lmbPaints3D = brushTool3D && !ImGui::GetIO().KeyAlt;

    // ---- Kamera-Steuerung ----
    //  Klassisch (CHANGELOG [0.44.32]): RMB = umsehen, WASD/Q/E sobald der Viewport gehovert ist,
    //  LMB-Ziehen = Orbit, MMB = schieben, Rad = Zoom.
    //  Unreal-Navigation (Standard): RMB halten = umsehen + WASD/Q/E fliegen (Rad ändert dabei
    //  das Tempo), LMB-Ziehen = vor/zurück + drehen, Alt+LMB = Orbit, Alt+RMB = Dolly,
    //  MMB = schieben, Rad = Zoom, Pfeiltasten bewegen ohne Maustaste.
    {
        ImGuiIO& io = ImGui::GetIO();
        const bool hovered3d = viewImageHovered;
        const float speedMul = core::level::CameraSpeedMultiplier(state.cameraSpeedSetting);
        if (state.playtestActive && state.drawingActiveViewport) {
            state.cameraLooking = false;
            UpdatePlaytest(state, hovered3d && !viewportUiCapturing);
        } else {
            const bool ue = state.unrealNavigation;
            if (hovered3d && !viewportUiCapturing && ImGui::IsMouseClicked(ImGuiMouseButton_Right) &&
                !(ue && io.KeyAlt))
                state.cameraLooking = true;
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) state.cameraLooking = false;
            const bool ortho = state.camera.IsOrthographic();
            // Orthografische Achsenansicht (wie Unreal): Ziehen mit LMB/RMB/MMB verschiebt, Rad zoomt,
            // es wird nicht gedreht. Pixel -> Welteinheiten aus der sichtbaren Ausschnittshöhe.
            const float orthoUnitsPerPixel = 2.0f * state.camera.Distance() *
                std::tan(app::OrbitCamera::kFovY * 0.5f) / std::max(1.0f, static_cast<float>(h));
            if (state.cameraLooking && !ortho) {
                const ImVec2 delta = io.MouseDelta;
                // Maus nach rechts = nach rechts drehen (Yaw sinkt), Maus nach oben = nach oben schauen.
                state.camera.LookBy(-delta.x * 0.0045f, delta.y * 0.0045f);
                if (delta.x != 0.0f || delta.y != 0.0f) LeaveAxisView(state);
            }
            if (ortho && hovered3d && !viewportUiCapturing && !state.marqueeActive &&
                !(ImGui::GetIO().KeyCtrl && ImGui::GetIO().KeyAlt) &&
                ((!lmbPaints3D && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) || ImGui::IsMouseDragging(ImGuiMouseButton_Right) ||
                 ImGui::IsMouseDragging(ImGuiMouseButton_Middle))) {
                const ImVec2 delta = io.MouseDelta;
                state.camera.PanBy(-delta.x * orthoUnitsPerPixel, delta.y * orthoUnitsPerPixel);
            } else if (hovered3d && !viewportUiCapturing && !state.marqueeActive) {
                const ImVec2 delta = io.MouseDelta;
                if (ue) {
                    if (io.KeyAlt && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                        state.camera.OrbitBy(delta.x * 0.01f, -delta.y * 0.01f);
                        LeaveAxisView(state);
                    } else if (io.KeyAlt && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
                        state.camera.ZoomSteps((delta.x - delta.y) * 0.04f);
                    } else if (!lmbPaints3D && !io.KeyAlt && !io.KeyCtrl && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                        const float moveScale = std::max(state.camera.Distance(), 30.0f) * 0.004f * speedMul;
                        state.camera.LookBy(-delta.x * 0.0045f, 0.0f);
                        state.camera.MoveLocal(-delta.y * moveScale, 0.0f, 0.0f);
                        LeaveAxisView(state);
                    }
                } else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && (!lmbPaints3D || io.KeyAlt)) {
                    state.camera.OrbitBy(delta.x * 0.01f, -delta.y * 0.01f);
                    LeaveAxisView(state);
                }
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
                    const float panScale = std::max(state.camera.Distance(), 30.0f) * 0.0015f;
                    state.camera.PanBy(-delta.x * panScale, delta.y * panScale);
                }
                if (io.MouseWheel != 0.0f) {
                    if (ue && state.cameraLooking) {
                        const int before = state.cameraSpeedSetting;
                        state.cameraSpeedSetting = std::clamp(state.cameraSpeedSetting + (io.MouseWheel > 0.0f ? 1 : -1),
                                                              core::level::kMinCameraSpeedSetting,
                                                              core::level::kMaxCameraSpeedSetting);
                        if (before != state.cameraSpeedSetting) {
                            state.viewportSettingsDirty = true;
                            state.statusMessage = std::string(L("Kameratempo ", "Camera speed ")) +
                                                  std::to_string(state.cameraSpeedSetting);
                        }
                    } else {
                        state.camera.ZoomSteps(io.MouseWheel);
                    }
                }
            }
            const bool letterKeys = ue ? state.cameraLooking : ((hovered3d || state.cameraLooking) && !viewportUiCapturing);
            const bool arrowKeys = (hovered3d || state.cameraLooking) && !viewportUiCapturing;
            if ((letterKeys || arrowKeys) && !io.WantTextInput) {
                float fwd = 0.0f, right = 0.0f, up = 0.0f;
                if (letterKeys) {
                    if (ImGui::IsKeyDown(ImGuiKey_W)) fwd += 1.0f;
                    if (ImGui::IsKeyDown(ImGuiKey_S)) fwd -= 1.0f;
                    if (ImGui::IsKeyDown(ImGuiKey_D)) right += 1.0f;
                    if (ImGui::IsKeyDown(ImGuiKey_A)) right -= 1.0f;
                    if (ImGui::IsKeyDown(ImGuiKey_E) || (!ue && ImGui::IsKeyDown(ImGuiKey_Space))) up += 1.0f;
                    if (ImGui::IsKeyDown(ImGuiKey_Q)) up -= 1.0f;
                }
                if (arrowKeys) {
                    if (ImGui::IsKeyDown(ImGuiKey_UpArrow)) fwd += 1.0f;
                    if (ImGui::IsKeyDown(ImGuiKey_DownArrow)) fwd -= 1.0f;
                    if (ImGui::IsKeyDown(ImGuiKey_RightArrow)) right += 1.0f;
                    if (ImGui::IsKeyDown(ImGuiKey_LeftArrow)) right -= 1.0f;
                }
                fwd = std::clamp(fwd, -1.0f, 1.0f);
                right = std::clamp(right, -1.0f, 1.0f);
                if (fwd != 0.0f || right != 0.0f || up != 0.0f) {
                    // Tempo waechst mit der Entfernung (weit weg = grosse Spruenge, nah dran = feinfuehlig).
                    float speed = std::clamp(state.camera.Distance() * 1.0f, 150.0f, 6000.0f) * speedMul;
                    if (io.KeyShift) speed *= 4.0f;
                    if (io.KeyCtrl) speed *= 0.2f;
                    const float step = speed * std::min(io.DeltaTime, 0.1f);
                    state.camera.MoveLocal(fwd * step, right * step, up * step);
                }
            }
        }
        // Deliberately no permanent mouse-help banner inside the 3D viewport. It was
        // perceived as a popup that never disappeared and also competed visually with the
        // transform gizmo. Camera bindings stay available in the manual/settings UI.
        if (state.viewportSettingsDirty && state.drawingActiveViewport) SaveViewportSettings(state);
    }

    // Zoom +/- Knöpfe unten rechts über dem 3D-Bild (siehe Mockup) - zusätzlich zum
    // Mausrad, für Nutzer ohne Maus mit Rad bzw. als deutlicher sichtbarer Zugriffspunkt.
    ImGui::SetCursorScreenPos(zoomPos);
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(28, 34, 42, 230));
    if (UI::Button("+##zoomIn", zoomBtnSize)) state.camera.ZoomSteps(2.0f);
    ImGui::SetCursorScreenPos(ImVec2(zoomPos.x, zoomPos.y + zoomBtnSize.y + 4.0f));
    if (UI::Button("-##zoomOut", zoomBtnSize)) state.camera.ZoomSteps(-2.0f);
    ImGui::PopStyleColor();
}

} // namespace nge
