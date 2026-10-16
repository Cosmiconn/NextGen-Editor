// Editor2DView.cpp
// 2D-Ansicht, Routen und NPC-Overlays.

#include "EditorShared.hpp"

namespace nge {

void DrawEditor2DContent(EditorState& state) {
    const bool texMode = state.editMode == EditMode::TexturePaint;
    const bool walkMode = state.editMode == EditMode::BlockWalk;
    const bool objectMode = state.editMode == EditMode::ObjectPlacement;

    if (texMode && (state.selectedLayer < 0 || static_cast<std::size_t>(state.selectedLayer) >= state.textureStack.LayerCount())) {
        ImGui::TextDisabled("Kein Layer ausgewählt - links in den Werkzeugen einen wählen oder anlegen.");
        return;
    }

    if (texMode && state.layerPreviewDirty) {
        UpdateLayerPreviewTexture(state);
        state.layerPreviewDirty = false;
    }
    if (walkMode && state.walkPreviewDirty) {
        UpdateWalkPreviewTexture(state);
        state.walkPreviewDirty = false;
    }

    // Objekt-Placement/Heightmap/Texturing nutzen jetzt alle dieselbe echte, texturierte
    // Draufsicht als Hintergrund (statt reiner Graustufen) - siehe
    // HeightmapRenderer::BeginTopDownScene. Bei Block&Walk wird die Heatmap zusätzlich
    // halbtransparent rot darübergelegt, statt sie allein anzuzeigen.
    const std::uint32_t gridW = walkMode ? state.walkGrid.Width() : (texMode ? state.textureStack.Width() : state.heightmap.Width());
    const std::uint32_t gridH = walkMode ? state.walkGrid.Height() : (texMode ? state.textureStack.Height() : state.heightmap.Height());
    if (gridW == 0 || gridH == 0) {
        if (objectMode && state.heightmap.Width() == 0) {
            ImGui::TextWrapped("%s", L(
                "Diese Karte besitzt kein HTD/HTDG-Terrain. Die NIF-Szenengeometrie wird vollständig im 3D-Viewport gerendert; eine 2D-Terrainfläche wird bewusst nicht erfunden.",
                "This map has no HTD/HTDG terrain. Its NIF scene geometry is rendered in the 3D viewport; no fake 2D terrain surface is invented."));
        } else {
            ImGui::TextDisabled("%s", L("Für dieses Werkzeug sind keine Kartendaten vorhanden.",
                                         "No map data is available for this tool."));
        }
        return;
    }

    // WICHTIG: Die angezeigte Fläche orientiert sich IMMER an der Form der Heightmap, NICHT an
    // der nativen Pixel-Form des jeweiligen Rohgitters. Block&Walk hat z.B. ein extrem
    // längliches Rohformat (Seitenverhältnis 1:16, siehe docs/MAP_FORMAT.md) - trotzdem soll der
    // Nutzer auf einer Fläche malen, die wie die Heightmap aussieht ("wo auf der Karte"), nicht
    // auf der krummen Rohform. Die Grafikkarte streckt/staucht die Textur beim Zeichnen
    // automatisch auf die Zielgröße (wie eine Textur auf ein andersförmiges Quad) - Speichern
    // konvertiert unsichtbar zurück ins Original-Rohformat (siehe SaveLegacyMap), der Nutzer
    // bekommt davon nie etwas mit.
    const float spanX = static_cast<float>(state.heightmap.Width() > 1 ? state.heightmap.Width() - 1 : 1) * state.heightmap.BlockWidth();
    const float spanZ = static_cast<float>(state.heightmap.Height() > 1 ? state.heightmap.Height() - 1 : 1) * state.heightmap.BlockHeight();

    const float availW = ImGui::GetContentRegionAvail().x;
    const float aspect = spanX > 0.0f ? (spanZ / spanX) : 1.0f;
    // Sichtbarer Bereich (Viewport) = ganze Kartenbreite; Zoom/Verschiebung (CHANGELOG [0.44.32]):
    // `imageSize`/`cursorScreenPos` beschreiben ab hier das VIRTUELLE, gezoomte Kartenbild (Kartenrand
    // links oben = cursorScreenPos, Groesse = Viewport * Zoom) - alle Marker- und Mausumrechnungen
    // unten (u = (Maus - cursorScreenPos) / imageSize) gelten damit unveraendert auch gezoomt. Der
    // Viewport zeigt nur den passenden Ausschnitt (Renderer-Fenster + Clip-Rechteck).
    const ImVec2 viewSize(availW, availW * aspect);
    const ImVec2 viewMin = ImGui::GetCursorScreenPos();
    const float zoom = std::clamp(state.view2dZoom, 1.0f, 40.0f);
    state.view2dZoom = zoom;
    state.view2dCenterU = std::clamp(state.view2dCenterU, 0.5f / zoom, 1.0f - 0.5f / zoom);
    state.view2dCenterV = std::clamp(state.view2dCenterV, 0.5f / zoom, 1.0f - 0.5f / zoom);
    const ImVec2 imageSize(viewSize.x * zoom, viewSize.y * zoom);
    const ImVec2 cursorScreenPos(viewMin.x + viewSize.x * 0.5f - state.view2dCenterU * imageSize.x,
                                 viewMin.y + viewSize.y * 0.5f - state.view2dCenterV * imageSize.y);

    // Weltfenster: u -> X, Bildzeile v -> Z = (1 - v) * spanZ.
    const float winCenterX = state.view2dCenterU * spanX;
    const float winCenterZ = (1.0f - state.view2dCenterV) * spanZ;
    state.renderer.SetTopDownWindow(winCenterX, winCenterZ, spanX * 0.5f / zoom, spanZ * 0.5f / zoom);
    state.renderer.BeginTopDownScene(static_cast<int>(viewSize.x), static_cast<int>(viewSize.y));
    if (walkMode) {
        // Das Walk-Gitter ist quadratisch (Zelle 6.25) - bei nicht-quadratischen Karten deckt die Karte
        // nur einen Teil ab: UV-Bereich = Kartenspanne / Gitterspanne.
        const float gridSpanX = std::max(1.0f, static_cast<float>(state.walkGrid.Cols()) * core::WalkGrid::kCellSize);
        const float gridSpanZ = std::max(1.0f, static_cast<float>(state.walkGrid.Rows()) * core::WalkGrid::kCellSize);
        const float mapU = std::min(1.0f, spanX / gridSpanX), mapV = std::min(1.0f, spanZ / gridSpanZ);
        // Overlay-UV = Kartenausschnitt (normiert) * Anteil der Textur, den die Karte belegt.
        const float x0n = state.view2dCenterU - 0.5f / zoom, z0n = (1.0f - state.view2dCenterV) - 0.5f / zoom;
        if (state.walkProposal) {
            if (state.walkDiffDirty) UpdateWalkDiffTexture(state);
            state.renderer.DrawTopDownOverlay(state.walkDiffTex, 0.85f, mapU / zoom, mapV / zoom, x0n * mapU, z0n * mapV, 1);
        } else {
            state.renderer.DrawTopDownOverlay(state.walkPreviewTex, 0.75f, mapU / zoom, mapV / zoom, x0n * mapU, z0n * mapV);
        }
    }
    const GLuint tex = state.renderer.EndTopDownScene();
    state.renderer.ResetTopDownWindow();

    // WICHTIG (CHANGELOG [0.44.27]): Norden (groesseres Legacy-Y = groesseres Z) liegt im Bild
    // OBEN, wie bei einer echten Karte und wie die Kartenbitmaps (BMP-Zeile 0 = unten). Die FBO-
    // Textur hat Z=0 unten (OpenGL), daher hier die UVs NICHT vertauschen. Alle Marker/Maus-
    // Umrechnungen unten nutzen deshalb v = 1 - z/spanZ. Der Grund: die Achsenvertauschung
    // Legacy->Editor ist eine Spiegelung, ohne diese Umkehr waere die Karte im Bild kopfueber.
    ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(tex)), viewSize,
                 ImVec2(0, 0), ImVec2(1, 1));
    const bool hoveredView = ImGui::IsItemHovered();
    if (objectMode && ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload=ImGui::AcceptDragDropPayload("NEXTGEN_NIF_ASSET")) {
            const char* model=static_cast<const char*>(payload->Data);
            const ImVec2 mouse=ImGui::GetMousePos();
            const float u=(mouse.x-cursorScreenPos.x)/imageSize.x;
            const float v=(mouse.y-cursorScreenPos.y)/imageSize.y;
            const float worldX=std::clamp(u,0.0f,1.0f)*spanX;
            const float worldZ=(1.0f-std::clamp(v,0.0f,1.0f))*spanZ;
            PlaceObjectAtWorld(state,model,worldX,state.heightmap.SampleWorld(worldX,worldZ),worldZ);
            state.statusMessage=std::string("Objekt in 2D platziert: ")+model;
        }
        ImGui::EndDragDropTarget();
    }
    {
        // Zoom (Mausrad, um den Mauszeiger) und Verschieben (mittlere/rechte Taste ziehen).
        ImGuiIO& io2 = ImGui::GetIO();
        if (hoveredView && io2.MouseWheel != 0.0f) {
            const ImVec2 mouse = ImGui::GetMousePos();
            const float mu = (mouse.x - cursorScreenPos.x) / imageSize.x;
            const float mv = (mouse.y - cursorScreenPos.y) / imageSize.y;
            const float newZoom = std::clamp(zoom * std::pow(1.2f, io2.MouseWheel), 1.0f, 40.0f);
            const ImVec2 newSize(viewSize.x * newZoom, viewSize.y * newZoom);
            // Der Kartenpunkt unter dem Mauszeiger bleibt unter dem Mauszeiger.
            state.view2dCenterU = (viewMin.x + viewSize.x * 0.5f - mouse.x) / newSize.x + mu;
            state.view2dCenterV = (viewMin.y + viewSize.y * 0.5f - mouse.y) / newSize.y + mv;
            state.view2dZoom = newZoom;
        }
        if (hoveredView && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f) || ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f))) {
            state.view2dCenterU -= io2.MouseDelta.x / imageSize.x;
            state.view2dCenterV -= io2.MouseDelta.y / imageSize.y;
        }
    }
    // Alles Folgende (Marker, Fussabdruecke, Pinsel) wird auf den Viewport beschnitten.
    ImGui::GetWindowDrawList()->PushClipRect(viewMin, ImVec2(viewMin.x + viewSize.x, viewMin.y + viewSize.y), true);
    struct ClipPop { ~ClipPop() { ImGui::GetWindowDrawList()->PopClipRect(); } } clipPop;

    DrawRoamRoutes2D(state, ImGui::GetWindowDrawList(), cursorScreenPos, imageSize, spanX, spanZ);

    if (hoveredView && (state.editMode==EditMode::Heightmap || texMode || walkMode)) {
        const float radius=ActiveWorldBrushRadius(state);
        const ImVec2 mouse=ImGui::GetMousePos();
        const float rx=spanX>0.0f ? radius/spanX*imageSize.x : 0.0f;
        const float ry=spanZ>0.0f ? radius/spanZ*imageSize.y : rx;
        const float rp=std::max(2.0f,(rx+ry)*0.5f);
        ImDrawList* dl=ImGui::GetWindowDrawList();
        dl->AddCircle(mouse,rp,ActiveBrushColor(state),64,2.0f);
        if(!walkMode) dl->AddCircle(mouse,rp*0.5f,ActiveBrushColor(state,110),48,1.0f);
        dl->AddCircleFilled(mouse,2.5f,ActiveBrushColor(state));
    }

    if (objectMode || walkMode) {
        // Marker für alle platzierten Objekte einzeichnen - im Objekt-Modus normal (weiß =
        // ausgewählt, blau = übrige), im Block/Walk-Modus zusätzlich als dezente REFERENZ
        // (halbtransparent, keine Auswahl/Bearbeitung hier) - Nutzerwunsch: man muss sehen,
        // wo Gebäude stehen, um die Lauf-Sperren drumherum korrekt zu setzen, siehe CHANGELOG
        // [0.44.23]. Objekte bleiben im Block/Walk-Modus rein informativ, nicht anklickbar -
        // versehentliches Verschieben beim Grid-Malen wäre schlimmer als gar keine Anzeige.
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        RefreshObjectVisibility(state);
        for (std::size_t i = 0; i < state.placementSet.Count() && state.showObjects2D; ++i) {
            if (IsObjectHidden(state, i)) continue;
            const auto& obj = state.placementSet.At(i);
            const float ou = spanX > 0.0f ? obj.posX / spanX : 0.0f;
            const float ov = 1.0f - (spanZ > 0.0f ? obj.posZ / spanZ : 0.0f);
            const ImVec2 screenPos(cursorScreenPos.x + ou * imageSize.x, cursorScreenPos.y + ov * imageSize.y);
            const bool selected = std::find(state.selectedObjects.begin(), state.selectedObjects.end(),
                                            static_cast<int>(i)) != state.selectedObjects.end();
            const ImU32 edgeColor = walkMode
                ? IM_COL32(230, 230, 235, 190)
                : (selected ? IM_COL32(255,255,255,235) : IM_COL32(70,160,225,205));
            const bool hasExactContact = DrawObjectFootprint2D(
                state, obj, drawList, cursorScreenPos, imageSize, spanX, spanZ,
                edgeColor, selected ? 2.4f : 1.5f);

            // Echte Kontaktkontur vorhanden: unselektierte Objekte werden absichtlich NUR
            // über die Geometrie gezeigt. Der alte Origin-Kreis machte die 2D-Ansicht zu einer
            // Markerkarte statt zu einer orthografischen Editoransicht. Pivot/Marker bleibt
            // für Auswahl bzw. als Fallback bei nicht ableitbarer NIF-Kontur erhalten.
            if (!hasExactContact || selected) {
                drawList->AddCircleFilled(
                    screenPos, selected ? 4.5f : 3.0f,
                    selected ? IM_COL32(255,255,255,255)
                             : (walkMode ? IM_COL32(230,230,235,150)
                                         : IM_COL32(70,135,210,220)));
                if (walkMode && !hasExactContact)
                    drawList->AddCircle(screenPos, 7.0f, IM_COL32(230,230,235,90), 16, 1.0f);
            }
        }

        // Sky/Water/GroundObject besitzen keinen SHMD-Transform, aber ihre NIF-Geometrie kann
        // trotzdem bereits an Weltkoordinaten liegen. Deshalb werden ihre echten Grundflächen
        // ebenfalls im 2D-Editor gezeichnet und können über die Liste ausgewählt werden.
        RefreshShmdCategoryVisibility(state);
        if (state.showObjects2D) {
            for (std::size_t i = 0; i < state.shmdCategoryRenderSet.Count(); ++i) {
                if (i < state.shmdCategoryHidden.size() && state.shmdCategoryHidden[i]) continue;
                const auto& obj = state.shmdCategoryRenderSet.At(i);
                const int id = ShmdSelectionId(i);
                const bool selected = std::find(state.selectedObjects.begin(), state.selectedObjects.end(), id) != state.selectedObjects.end();
                const bool hasExactContact = DrawObjectFootprint2D(
                    state, obj, drawList, cursorScreenPos, imageSize, spanX, spanZ,
                    selected ? IM_COL32(255,255,255,235) : IM_COL32(70,190,210,190),
                    selected ? 2.5f : 1.5f);
                const auto polygon = ObjectFootprintWorldPolygon(state, obj);
                if ((!hasExactContact || selected) && !polygon.empty()) {
                    float cx = 0.0f, cz = 0.0f;
                    for (const auto& p : polygon) { cx += p.first; cz += p.second; }
                    cx /= static_cast<float>(polygon.size());
                    cz /= static_cast<float>(polygon.size());
                    const float ou = spanX > 0.0f ? cx / spanX : 0.0f;
                    const float ov = 1.0f - (spanZ > 0.0f ? cz / spanZ : 0.0f);
                    const ImVec2 center(cursorScreenPos.x + ou * imageSize.x, cursorScreenPos.y + ov * imageSize.y);
                    const float radius = selected ? 5.0f : 3.5f;
                    drawList->AddRectFilled(ImVec2(center.x - radius, center.y - radius),
                                            ImVec2(center.x + radius, center.y + radius),
                                            selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(70, 190, 210, 230));
                }
            }
        }
    }
    if (walkMode && state.walkFootprintPreviewActive) {
        DrawWalkFootprintPreview(state, ImGui::GetWindowDrawList(),
                                 cursorScreenPos, imageSize, spanX, spanZ);
    }
    if (state.editMode == EditMode::Npcs && state.npcTextLoaded && state.legacySaveStem[0] != '\0') {
        // NPC-Marker als Quadrate (statt Kreise wie bei Objekten) - selbe Weiß/Blau-Logik,
        // aber optisch unterscheidbar, ohne eine neue Farbe außerhalb des Themes einzuführen
        // (siehe CHANGELOG [0.44.12]). Koordinaten aus World/NPC.txt (Coord-X/Coord-Y) nutzen
        // dieselbe Weltraum-Konvention wie posX/posZ der regulären Objekt-Platzierung.
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        auto* table = state.npcTextFile.FindTable("ShineNPC");
        if (table) {
            for (std::size_t idx : NpcRecordsForCurrentMap(state)) {
                const auto& rec = table->records[idx];
                if (rec.values.size() < 4) continue;
                const float npcX = static_cast<float>(std::atoi(rec.values[2].c_str()));
                const float npcY = static_cast<float>(std::atoi(rec.values[3].c_str()));
                const float ou = spanX > 0.0f ? npcX / spanX : 0.0f;
                const float ov = 1.0f - (spanZ > 0.0f ? npcY / spanZ : 0.0f);
                const ImVec2 c(cursorScreenPos.x + ou * imageSize.x, cursorScreenPos.y + ov * imageSize.y);
                const bool selected = state.selectedNpcRecordIdx == static_cast<int>(idx);
                const float r = selected ? 5.0f : 3.5f;
                drawList->AddRectFilled(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r),
                                         selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(70, 135, 210, 220));
                if (rec.values.size() >= 5) {
                    // Blickrichtung: Modell schaut bei Winkel 0 nach -Z (Charaktermodelle), gedreht um die Hochachse.
                    const float yaw = NpcYawRadians(state, std::atoi(rec.values[4].c_str()));
                    const float fx = -std::sin(yaw), fz = -std::cos(yaw);
                    const float len = selected ? 22.0f : 15.0f;
                    const ImVec2 tip(c.x + fx * len, c.y - fz * len); // Norden = oben im Bild
                    drawList->AddLine(c, tip, selected ? IM_COL32(255, 210, 90, 255) : IM_COL32(255, 210, 90, 170), selected ? 2.5f : 1.5f);
                    drawList->AddCircleFilled(tip, selected ? 3.0f : 2.0f, IM_COL32(255, 210, 90, 255));
                }
            }
        }
    }
    if (state.editMode == EditMode::Mobs && state.mobRegenTextLoaded) {
        // Mob-Spawn-Zonen: kleines Icon (gefüllter Kreis) in der Mitte + dünner Umriss-Kreis
        // für den Spawn-Radius (Nutzerwunsch, siehe CHANGELOG [0.44.17]). Radius-Näherung: bei
        // Width/Height > 0 (rechteckig/länglich wirkende Zonen) die größere der beiden Seiten
        // halbiert, sonst (Width=Height=0, häufigster Fall) RangeDegree direkt als Radius -
        // letzteres Feld ist trotz seines Namens in diesen Fällen ein Weltraum-Radius, kein
        // Winkel (Werte wie 507 passen nicht in 0-360°, siehe docs/SHN_DEPENDENCIES.md wäre
        // der richtige Ort für eine spätere, genauere Form-Unterscheidung).
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        auto* zoneTable = state.mobRegenTextFile.FindTable("MobRegenGroup");
        if (zoneTable) {
            for (std::size_t i = 0; i < zoneTable->records.size(); ++i) {
                const auto& rec = zoneTable->records[i];
                if (rec.values.size() < 7) continue;
                const float cx = static_cast<float>(std::atoi(rec.values[2].c_str()));
                const float cy = static_cast<float>(std::atoi(rec.values[3].c_str()));
                const float w = static_cast<float>(std::atoi(rec.values[4].c_str()));
                const float h = static_cast<float>(std::atoi(rec.values[5].c_str()));
                const float rangeVal = static_cast<float>(std::atoi(rec.values[6].c_str()));
                const float radius = (w > 0.0f || h > 0.0f) ? std::max(w, h) * 0.5f : rangeVal;
                const float ou = spanX > 0.0f ? cx / spanX : 0.0f;
                const float ov = 1.0f - (spanZ > 0.0f ? cy / spanZ : 0.0f);
                const ImVec2 c(cursorScreenPos.x + ou * imageSize.x, cursorScreenPos.y + ov * imageSize.y);
                const float screenRadius = spanX > 0.0f ? std::max((radius / spanX) * imageSize.x, 2.0f) : 2.0f;
                const bool selected = state.selectedMobZoneIdx == static_cast<int>(i);
                const ImU32 col = selected ? IM_COL32(255, 255, 255, 220) : IM_COL32(70, 135, 210, 180);
                drawList->AddCircle(c, screenRadius, col, 32, selected ? 2.0f : 1.25f);
                drawList->AddCircleFilled(c, selected ? 5.0f : 3.5f, selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(70, 135, 210, 220));
            }
        }
    }

    DrawSpline2D(state, ImGui::GetWindowDrawList(), cursorScreenPos, imageSize, spanX, spanZ);
    if (state.editMode == EditMode::Portals && state.legacySaveStem[0] != '\0') {
        DrawPortalMarkers2D(state, cursorScreenPos, imageSize, spanX, spanZ);
    }

    const bool hovered = hoveredView;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && state.objectDragActive) {
        state.objectDragActive = false;
        CommitObjectEditTransaction(state);
    }

    // Shift + Ziehen = Rechteckauswahl. Alt+Shift + Ziehen = Lasso.
    // Strg erweitert jeweils die bestehende Auswahl.
    bool suppressObjectClickForBox = false;
    if (objectMode && !state.objectPlaceMode) {
        const ImGuiIO& ioBox=ImGui::GetIO();
        if (hovered && ioBox.KeyShift && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            state.objectDragActive=false;
            if (ioBox.KeyAlt) {
                state.objectLassoSelectActive=true;
                state.objectBoxSelectActive=false;
                state.objectLassoPoints.clear();
                state.objectLassoPoints.push_back(ImGui::GetMousePos());
            } else {
                state.objectBoxSelectActive=true;
                state.objectLassoSelectActive=false;
                state.objectLassoPoints.clear();
                state.objectBoxSelectStart=ImGui::GetMousePos();
            }
        }

        auto screenPointFromWorldXZ = [&](float wx, float wz) {
            const float u = spanX > 0.0f ? wx / spanX : 0.0f;
            const float v = 1.0f - (spanZ > 0.0f ? wz / spanZ : 0.0f);
            return ImVec2(cursorScreenPos.x + u * imageSize.x,
                          cursorScreenPos.y + v * imageSize.y);
        };
        auto segmentIntersectsSegment = [](const ImVec2& a, const ImVec2& b,
                                           const ImVec2& c, const ImVec2& d) {
            auto orient = [](const ImVec2& p, const ImVec2& q, const ImVec2& r) {
                return (q.x - p.x) * (r.y - p.y) -
                       (q.y - p.y) * (r.x - p.x);
            };
            auto onSegment = [](const ImVec2& p, const ImVec2& q, const ImVec2& r) {
                constexpr float eps = 1.0e-4f;
                return q.x >= std::min(p.x,r.x)-eps && q.x <= std::max(p.x,r.x)+eps &&
                       q.y >= std::min(p.y,r.y)-eps && q.y <= std::max(p.y,r.y)+eps;
            };
            constexpr float eps = 1.0e-4f;
            const float o1=orient(a,b,c), o2=orient(a,b,d);
            const float o3=orient(c,d,a), o4=orient(c,d,b);
            if (((o1 > eps && o2 < -eps) || (o1 < -eps && o2 > eps)) &&
                ((o3 > eps && o4 < -eps) || (o3 < -eps && o4 > eps)))
                return true;
            if (std::abs(o1)<=eps && onSegment(a,c,b)) return true;
            if (std::abs(o2)<=eps && onSegment(a,d,b)) return true;
            if (std::abs(o3)<=eps && onSegment(c,a,d)) return true;
            if (std::abs(o4)<=eps && onSegment(c,b,d)) return true;
            return false;
        };

        auto objectCrossesArea = [&](const core::PlacedObject& obj,
                                     const auto& inside,
                                     const auto& segmentCrosses,
                                     bool allowOriginFallback) {
            const auto worldEdges=ObjectGroundContactWorldSegments(state,obj);
            // Gleiche Rahmen-Semantik wie im 3D-Viewport: "vollständig innen" verlangt, dass die
            // gesamte sichtbare Kontaktkontur im Rahmen/Lasso liegt.
            if (state.marqueeMode==core::level::MarqueeMode::Inside) {
                if (worldEdges.empty())
                    return allowOriginFallback && inside(screenPointFromWorldXZ(obj.posX,obj.posZ));
                for (const auto& edge:worldEdges)
                    if (!inside(screenPointFromWorldXZ(edge.x0,edge.z0)) ||
                        !inside(screenPointFromWorldXZ(edge.x1,edge.z1))) return false;
                return true;
            }
            if (!worldEdges.empty()) {
                for (const auto& edge:worldEdges) {
                    const ImVec2 a=screenPointFromWorldXZ(edge.x0,edge.z0);
                    const ImVec2 b=screenPointFromWorldXZ(edge.x1,edge.z1);
                    if (inside(a)||inside(b)||segmentCrosses(a,b)) return true;
                }
                // A small marquee entirely inside a large closed footprint might not cross
                // an outer edge. Including the authored pivot as an additional probe handles
                // that case without reverting to pivot-only selection.
                return inside(screenPointFromWorldXZ(obj.posX,obj.posZ));
            }
            return allowOriginFallback && inside(screenPointFromWorldXZ(obj.posX,obj.posZ));
        };

        auto applyAreaSelection = [&](const auto& inside, const auto& segmentCrosses,
                                      const char* kind) {
            std::vector<int> hits;
            if (state.showObjects2D) {
                RefreshObjectVisibility(state);
                for(std::size_t i=0;i<state.placementSet.Count();++i) {
                    if(IsObjectHidden(state,i)||IsObjectEditorLocked(state,static_cast<int>(i))) continue;
                    const auto& obj=state.placementSet.At(i);
                    if(objectCrossesArea(obj,inside,segmentCrosses,true))
                        hits.push_back(static_cast<int>(i));
                }

                RefreshShmdCategoryVisibility(state);
                for(std::size_t i=0;i<state.shmdCategoryRenderSet.Count();++i) {
                    if(i<state.shmdCategoryHidden.size()&&state.shmdCategoryHidden[i]) continue;
                    const int id=ShmdSelectionId(i);
                    if(IsObjectEditorLocked(state,id)) continue;
                    const auto& obj=state.shmdCategoryRenderSet.At(i);
                    if(objectCrossesArea(obj,inside,segmentCrosses,false)) {
                        hits.push_back(id);
                        continue;
                    }

                    // SHMD scene models may already contain absolute world geometry while their
                    // placement origin remains zero. Only models without an exact contact contour
                    // fall back to the legacy footprint center.
                    if(!ObjectGroundContactWorldSegments(state,obj).empty()) continue;
                    const auto poly=ObjectFootprintWorldPolygon(state,obj);
                    if(poly.empty()) continue;
                    float cx=0.0f,cz=0.0f;
                    for(const auto& p:poly){cx+=p.first;cz+=p.second;}
                    cx/=static_cast<float>(poly.size());cz/=static_cast<float>(poly.size());
                    if(inside(screenPointFromWorldXZ(cx,cz))) hits.push_back(id);
                }
            }

            if(!ioBox.KeyCtrl) state.selectedObjects.clear();
            for(const int id:hits)
                if(std::find(state.selectedObjects.begin(),state.selectedObjects.end(),id)==state.selectedObjects.end())
                    state.selectedObjects.push_back(id);
            state.selectedObject=state.selectedObjects.empty()?kNoObjectSelection:state.selectedObjects.back();
            state.selectedObjectModelPathFor=kNoObjectSelection;
            state.objectGizmoMatrixValid=false;
            state.statusMessage=std::to_string(hits.size())+" "+
                L("Objekt(e) per ","object(s) selected by ")+kind+
                L(" ausgewählt.",".");
        };

        if (state.objectBoxSelectActive) {
            suppressObjectClickForBox=true;
            const ImVec2 cur=ImGui::GetMousePos();
            const ImVec2 lo(std::min(state.objectBoxSelectStart.x,cur.x),std::min(state.objectBoxSelectStart.y,cur.y));
            const ImVec2 hi(std::max(state.objectBoxSelectStart.x,cur.x),std::max(state.objectBoxSelectStart.y,cur.y));
            ImDrawList* dl=ImGui::GetWindowDrawList();
            dl->AddRectFilled(lo,hi,IM_COL32(40,150,225,30));
            dl->AddRect(lo,hi,IM_COL32(80,190,255,235),0.0f,0,1.5f);

            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                auto insideRect=[&](const ImVec2& p){
                    return p.x>=lo.x&&p.x<=hi.x&&p.y>=lo.y&&p.y<=hi.y;
                };
                auto crossesRect=[&](const ImVec2& a,const ImVec2& b){
                    if(insideRect(a)||insideRect(b)) return true;
                    const ImVec2 tl(lo.x,lo.y), tr(hi.x,lo.y);
                    const ImVec2 br(hi.x,hi.y), bl(lo.x,hi.y);
                    return segmentIntersectsSegment(a,b,tl,tr)||
                           segmentIntersectsSegment(a,b,tr,br)||
                           segmentIntersectsSegment(a,b,br,bl)||
                           segmentIntersectsSegment(a,b,bl,tl);
                };
                applyAreaSelection(insideRect,crossesRect,L("Rechteck","rectangle"));
                state.objectBoxSelectActive=false;
            }
        }

        if (state.objectLassoSelectActive) {
            suppressObjectClickForBox=true;
            const ImVec2 cur=ImGui::GetMousePos();
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                if (state.objectLassoPoints.empty()) {
                    state.objectLassoPoints.push_back(cur);
                } else {
                    const ImVec2 last=state.objectLassoPoints.back();
                    const float dx=cur.x-last.x, dy=cur.y-last.y;
                    if (dx*dx+dy*dy>=25.0f) state.objectLassoPoints.push_back(cur);
                }
            }

            ImDrawList* dl=ImGui::GetWindowDrawList();
            if (state.objectLassoPoints.size()>=2)
                dl->AddPolyline(state.objectLassoPoints.data(),static_cast<int>(state.objectLassoPoints.size()),
                                IM_COL32(80,210,255,245),ImDrawFlags_None,2.0f);
            if (!state.objectLassoPoints.empty())
                dl->AddLine(state.objectLassoPoints.back(),cur,IM_COL32(80,210,255,180),1.4f);

            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                if (state.objectLassoPoints.size()>=3) {
                    const auto points=state.objectLassoPoints;
                    auto inside=[&](const ImVec2& p) {
                        bool result=false;
                        std::size_t j=points.size()-1;
                        for(std::size_t i=0;i<points.size();j=i++) {
                            const ImVec2& a=points[i];
                            const ImVec2& b=points[j];
                            const bool crosses=((a.y>p.y)!=(b.y>p.y)) &&
                                (p.x < (b.x-a.x)*(p.y-a.y)/(b.y-a.y)+a.x);
                            if(crosses) result=!result;
                        }
                        return result;
                    };
                    auto crossesLasso=[&](const ImVec2& a,const ImVec2& b) {
                        if(inside(a)||inside(b)) return true;
                        for(std::size_t i=0,j=points.size()-1;i<points.size();j=i++)
                            if(segmentIntersectsSegment(a,b,points[j],points[i])) return true;
                        return false;
                    };
                    applyAreaSelection(inside,crossesLasso,"Lasso");
                } else {
                    state.statusMessage="Lasso verworfen: zu wenige Punkte.";
                }
                state.objectLassoSelectActive=false;
                state.objectLassoPoints.clear();
            }
        }
    }

    bool suppressWalkClickForRect=false;
    if (walkMode) {
        const ImGuiIO& ioWalk=ImGui::GetIO();
        if (hovered && ioWalk.KeyShift && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            state.walkRectActive=true;
            state.walkRectStart=ImGui::GetMousePos();
        }
        if (state.walkRectActive) {
            suppressWalkClickForRect=true;
            const ImVec2 cur=ImGui::GetMousePos();
            const ImVec2 lo(std::min(state.walkRectStart.x,cur.x),std::min(state.walkRectStart.y,cur.y));
            const ImVec2 hi(std::max(state.walkRectStart.x,cur.x),std::max(state.walkRectStart.y,cur.y));
            ImDrawList* dl=ImGui::GetWindowDrawList();
            const ImU32 col=state.walkBlockMode?IM_COL32(245,85,85,235):IM_COL32(80,220,145,235);
            dl->AddRectFilled(lo,hi,state.walkBlockMode?IM_COL32(245,85,85,28):IM_COL32(80,220,145,28));
            dl->AddRect(lo,hi,col,0.0f,0,1.5f);
            if(!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                auto toWorld=[&](const ImVec2& p) {
                    const float u=std::clamp((p.x-cursorScreenPos.x)/imageSize.x,0.0f,1.0f);
                    const float v=std::clamp((p.y-cursorScreenPos.y)/imageSize.y,0.0f,1.0f);
                    return std::pair<float,float>{u*spanX,(1.0f-v)*spanZ};
                };
                const auto a=toWorld(lo), b=toWorld(hi);
                const float x0=std::min(a.first,b.first), x1=std::max(a.first,b.first);
                const float z0=std::min(a.second,b.second), z1=std::max(a.second,b.second);
                std::vector<std::pair<float,float>> polygon={{x0,z0},{x1,z0},{x1,z1},{x0,z1}};
                core::WalkUndoPatch patch;
                std::vector<std::uint64_t> seenWords;
                core::ApplyWalkConvexPolygon(state.walkGrid,polygon,state.walkBlockMode,patch,seenWords);
                if(!patch.entries.empty()) {
                    PushWalkHistory(state, std::move(patch));
                    state.walkPreviewDirty=true;
                    state.mapDirty=true;
                }
                state.walkRectActive=false;
                state.statusMessage=state.walkBlockMode?"Walk-Rechteck gesperrt.":"Walk-Rechteck freigegeben.";
            }
        }
    }

    // Object placement benötigt sowohl den initialen Klick als auch die folgenden
    // Mouse-Down-Frames für flüssiges Verschieben. Die übrigen Modi behalten ihr
    // bisheriges Klick-/Drag-Verhalten.
    const bool clickTrigger = objectMode
                                   ? (!suppressObjectClickForBox && (state.objectPlaceMode == 2
                                          ? ImGui::IsMouseDown(ImGuiMouseButton_Left)
                                          : state.objectPlaceMode
                                          ? ImGui::IsMouseClicked(ImGuiMouseButton_Left)
                                          : (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
                                             ImGui::IsMouseDown(ImGuiMouseButton_Left))))
                                   : ((state.editMode == EditMode::Npcs || state.editMode == EditMode::Mobs ||
                                       state.editMode == EditMode::Portals)
                                          ? ImGui::IsMouseClicked(ImGuiMouseButton_Left)
                                          : (!suppressWalkClickForRect && ImGui::IsMouseDown(ImGuiMouseButton_Left)));
    if (hovered && clickTrigger && imageSize.x > 0 && imageSize.y > 0) {
        const ImVec2 mouse = ImGui::GetMousePos();
        const float u = (mouse.x - cursorScreenPos.x) / imageSize.x;
        const float v = (mouse.y - cursorScreenPos.y) / imageSize.y;

        if (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f) {
            // Weltraum-Ausdehnung immer aus der Heightmap ableiten (alle Gitter decken dieselbe
            // physische Kartenfläche ab, nur mit unterschiedlicher Auflösung) - so bleibt die
            // Maus-Zuordnung über alle Bearbeitungsmodi hinweg konsistent.
            const float worldX = u * spanX;
            const float worldZ = (1.0f - v) * spanZ;

            if (objectMode) {
                // Bereits ausgewählte Objekte mit der linken Maustaste verschieben. Die
                // Bildschirmbewegung wird direkt in Weltkoordinaten umgerechnet, sodass
                // Mehrfachauswahl gemeinsam bewegt werden kann.
                if (!state.objectPlaceMode && state.objectDragActive && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
                    ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.0f) && !state.selectedObjects.empty()) {
                    const ImVec2 delta = ImGui::GetIO().MouseDelta;
                    const float dx = imageSize.x > 0.0f ? delta.x * spanX / imageSize.x : 0.0f;
                    const float dz = imageSize.y > 0.0f ? -delta.y * spanZ / imageSize.y : 0.0f;
                    // 2D-Drag verändert nur X/Z. Y bleibt bewusst unverändert, damit mehrere
                    // Objekte ihre individuellen Höhen/Offsets nicht beim ersten Pixel verlieren.
                    MoveSelectedObjectsBy(state, dx, 0.0f, dz);
                }
                if (state.objectPlaceMode == 2) {
                    ScatterAtWorld(state, worldX, worldZ);
                } else if (state.objectPlaceMode == 3) {
                    PlacePrefabAtWorld(state, worldX, worldZ);
                } else if (state.objectPlaceMode && MapSupportsObjectGeometryEditing(state)) {
                    auto historyBefore = BeginObjectMutation(state);
                    core::PlacedObject obj;
                    obj.modelPath = state.newObjectModelPath;
                    obj.posX = worldX;
                    obj.posZ = worldZ;
                    obj.posY = state.heightmap.SampleWorld(worldX, worldZ);
                    const float rad = state.newObjectRotDeg * 3.14159265f / 180.0f / 2.0f;
                    obj.rotY = std::sin(rad);
                    obj.rotW = std::cos(rad);
                    obj.scale = state.newObjectScale;
                    state.selectedObject = static_cast<int>(state.placementSet.AddObject(std::move(obj)));
                    state.objectEditorHidden.push_back(0);
                    state.objectEditorLocked.push_back(0);
                    state.selectedObjects = {state.selectedObject};
                    state.selectedObjectModelPathFor = kNoObjectSelection;
                    state.objectListRangeAnchor = -1;
                    ReloadObjectRenderers(state);
                    FinishObjectMutation(state, std::move(historyBefore));
                } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    // Auswahl wird ausschließlich auf dem initialen Klick geändert. Danach darf
                    // der gehaltene Klick nur noch die bereits ausgewählten Objekte verschieben.
                    // So bleibt Ctrl-Mehrfachauswahl stabil und Dragging überschreibt sie nicht.
                    // Orthografisches Picking folgt derselben Geometrie, die der Nutzer
                    // sieht: Kontaktkonturen statt eines großen Radius um den Objektursprung.
                    // Acht Bildschirmpixel bleiben bei jedem Zoom ungefähr gleich gut anklickbar.
                    const float worldPerPixelX = imageSize.x > 0.0f ? spanX / imageSize.x : 0.0f;
                    const float worldPerPixelZ = imageSize.y > 0.0f ? spanZ / imageSize.y : 0.0f;
                    const float tolerance = std::max(1.0f, 8.0f * std::max(worldPerPixelX, worldPerPixelZ));
                    float bestDist = tolerance;
                    int bestIdx = kNoObjectSelection;
                    if (state.showObjects2D) {
                        RefreshObjectVisibility(state);
                        for (std::size_t i = 0; i < state.placementSet.Count(); ++i) {
                            if (IsObjectHidden(state, i) ||
                                IsObjectEditorLocked(state, static_cast<int>(i))) continue;
                            const auto& obj = state.placementSet.At(i);
                            float dist = DistanceToObjectGroundContactXZ(state, obj, worldX, worldZ);
                            if (!std::isfinite(dist))
                                dist = std::hypot(obj.posX - worldX, obj.posZ - worldZ);
                            if (dist < bestDist) {
                                bestDist = dist;
                                bestIdx = static_cast<int>(i);
                            }
                        }

                        RefreshShmdCategoryVisibility(state);
                        for (std::size_t i = 0; i < state.shmdCategoryRenderSet.Count(); ++i) {
                            const int id = ShmdSelectionId(i);
                            if ((i < state.shmdCategoryHidden.size() && state.shmdCategoryHidden[i]) ||
                                IsObjectEditorLocked(state, id)) continue;
                            const auto& obj = state.shmdCategoryRenderSet.At(i);
                            float dist = DistanceToObjectGroundContactXZ(state, obj, worldX, worldZ);
                            if (!std::isfinite(dist)) {
                                const auto polygon = ObjectFootprintWorldPolygon(state, obj);
                                if (!polygon.empty()) {
                                    float cx = 0.0f, cz = 0.0f;
                                    for (const auto& p : polygon) { cx += p.first; cz += p.second; }
                                    cx /= static_cast<float>(polygon.size());
                                    cz /= static_cast<float>(polygon.size());
                                    dist = std::hypot(cx - worldX, cz - worldZ);
                                }
                            }
                            if (dist < bestDist) {
                                bestDist = dist;
                                bestIdx = id;
                            }
                        }
                    }
                    if (bestIdx != kNoObjectSelection) {
                        SelectObjectOnCanvas(state, bestIdx, ImGui::GetIO().KeyCtrl);
                    } else if (!ImGui::GetIO().KeyCtrl) {
                        ClearObjectSelection(state);
                    }
                    if (bestIdx != kNoObjectSelection) {
                        state.objectDragActive =
                            std::find(state.selectedObjects.begin(), state.selectedObjects.end(), bestIdx) != state.selectedObjects.end();
                        if (state.objectDragActive) BeginObjectEditTransaction(state);
                    }
                }
            } else if (state.editMode == EditMode::Npcs && state.npcTextLoaded) {
                // Nächsten NPC innerhalb einer kleinen Toleranz suchen und auswählen (analog zur
                // Objekt-Platzierung oben) - Klicken in diesem Modus darf NICHT die Heightmap
                // bearbeiten (siehe "else if (!texMode && !walkMode)" unten).
                auto* table = state.npcTextFile.FindTable("ShineNPC");
                if (table) {
                    const float tolerance = spanX * 0.02f / zoom;
                    float bestDist = tolerance;
                    int bestIdx = -1;
                    for (std::size_t idx : NpcRecordsForCurrentMap(state)) {
                        const auto& rec = table->records[idx];
                        if (rec.values.size() < 4) continue;
                        const float dx = static_cast<float>(std::atoi(rec.values[2].c_str())) - worldX;
                        const float dz = static_cast<float>(std::atoi(rec.values[3].c_str())) - worldZ;
                        const float dist = std::sqrt(dx * dx + dz * dz);
                        if (dist < bestDist) { bestDist = dist; bestIdx = static_cast<int>(idx); }
                    }
                    state.selectedNpcRecordIdx = bestIdx;
                }
            } else if (state.editMode == EditMode::Mobs && state.mobRegenTextLoaded) {
                // Nächste Spawn-Zone auswählen (Toleranz etwas großzügiger als bei NPCs, da
                // Zonen typischerweise ausgedehnter sind).
                auto* zoneTable = state.mobRegenTextFile.FindTable("MobRegenGroup");
                if (zoneTable) {
                    const float tolerance = spanX * 0.04f / zoom;
                    float bestDist = tolerance;
                    int bestIdx = -1;
                    for (std::size_t i = 0; i < zoneTable->records.size(); ++i) {
                        const auto& rec = zoneTable->records[i];
                        if (rec.values.size() < 4) continue;
                        const float dx = static_cast<float>(std::atoi(rec.values[2].c_str())) - worldX;
                        const float dz = static_cast<float>(std::atoi(rec.values[3].c_str())) - worldZ;
                        const float dist = std::sqrt(dx * dx + dz * dz);
                        if (dist < bestDist) { bestDist = dist; bestIdx = static_cast<int>(i); }
                    }
                    state.selectedMobZoneIdx = bestIdx;
                }
            } else if (state.editMode == EditMode::Portals) {
                // Klick setzt entweder die Position des gewaehlten Ziels (Pick-Modus) oder
                // waehlt das naechste Ziel aus. Darf NIE die Heightmap bearbeiten.
                if (state.portalPickMode &&
                    (state.selectedPortalKind == kPortalKindTown || state.selectedPortalKind == kPortalKindRecall)) {
                    SetSelectedPortalPosition(state, std::llround(worldX), std::llround(worldZ));
                } else {
                    const float tolerance = spanX * 0.02f / zoom;
                    float bestDist = tolerance;
                    int bestKind = kPortalKindNone, bestIdx = -1;
                    for (const auto& m : CollectPortalMarkers(state)) {
                        const float dx = m.x - worldX, dz = m.y - worldZ;
                        const float dist = std::sqrt(dx * dx + dz * dz);
                        if (dist < bestDist) { bestDist = dist; bestKind = m.kind; bestIdx = static_cast<int>(m.idx); }
                    }
                    state.selectedPortalKind = bestKind;
                    state.selectedPortalIdx = bestIdx;
                }
            } else if (state.editMode == EditMode::Heightmap || texMode || walkMode) {
                ApplyBrushAtWorld(state, worldX, worldZ);
            }
        }
    }

    // Zoom-Knoepfe oben rechts im Viewport (Mausrad geht ebenfalls) - nach ALLEN Klick-Auswertungen,
    // damit "zuletzt gezeichnetes Element" fuer die Hover-Logik oben nicht verfaelscht wird.
    {
        const ImVec2 after = ImGui::GetCursorScreenPos();
        const ImVec2 btn(28.0f, 26.0f);
        ImGui::SetCursorScreenPos(ImVec2(viewMin.x + viewSize.x - btn.x - 10.0f, viewMin.y + 10.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(28, 34, 42, 230));
        if (UI::Button("+##zoom2dIn", btn)) state.view2dZoom = std::clamp(zoom * 1.5f, 1.0f, 40.0f);
        ImGui::SetCursorScreenPos(ImVec2(viewMin.x + viewSize.x - btn.x - 10.0f, viewMin.y + 10.0f + btn.y + 4.0f));
        if (UI::Button("-##zoom2dOut", btn)) state.view2dZoom = std::clamp(zoom / 1.5f, 1.0f, 40.0f);
        ImGui::SetCursorScreenPos(ImVec2(viewMin.x + viewSize.x - btn.x - 10.0f, viewMin.y + 10.0f + (btn.y + 4.0f) * 2.0f));
        if (UI::Button("1:1##zoom2dFit", btn)) { state.view2dZoom = 1.0f; state.view2dCenterU = state.view2dCenterV = 0.5f; }
        ImGui::PopStyleColor();
        ImGui::SetCursorScreenPos(after);
        // No permanent mouse-help overlay here: it looked like a stuck popup over the
        // viewport. The explicit +/-/1:1 controls remain visible and the manual documents
        // wheel/middle-button navigation.
    }
}
std::optional<EditorState::RoamOverlayRoute> LoadRoamOverlayRoute(
    const EditorState& state, const std::string& name) {
    if (name.empty() || state.shnServerRoot.empty()) return std::nullopt;
    const auto sourcePath = std::filesystem::path(state.shnServerRoot) / "MobRoam" / (name + ".txt");
    const auto path = PreferProjectOverride(state.project, core::ProjectOutputSide::Server, sourcePath);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return std::nullopt;
    auto loaded = core::legacy::LoadShineTextFile(path);
    if (!loaded) return std::nullopt;
    const auto* table = loaded->FindTable("Roaming");
    if (!table) return std::nullopt;

    EditorState::RoamOverlayRoute route;
    route.name = name;
    for (const auto& rec : table->records) {
        if (rec.values.size() < 3) continue;
        const float x = static_cast<float>(std::atof(rec.values[1].c_str()));
        const float z = static_cast<float>(std::atof(rec.values[2].c_str()));
        route.points.emplace_back(x, z);
        if (rec.values.size() >= 4 && LowerAscii(rec.values[3]) == "return")
            route.returnsToStart = true;
    }
    if (route.points.empty()) return std::nullopt;
    return route;
}
void RefreshRoamOverlayRoutes(EditorState& state) {
    std::vector<std::string> names;
    std::unordered_set<std::string> unique;
    const auto addName = [&](const std::string& name) {
        if (!name.empty() && unique.insert(name).second) names.push_back(name);
    };

    std::string key = state.shnServerRoot + "|" + std::to_string(static_cast<int>(state.editMode)) +
                      "|" + (state.showAllRoamRoutes ? "all" : "selected") + "|";

    if (state.editMode == EditMode::Npcs && state.npcTextLoaded) {
        if (auto* table = state.npcTextFile.FindTable("ShineNPC")) {
            if (state.showAllRoamRoutes) {
                for (const std::size_t idx : NpcRecordsForCurrentMap(state)) {
                    if (idx < table->records.size() && !table->records[idx].values.empty())
                        addName(table->records[idx].values[0]);
                }
            } else if (state.selectedNpcRecordIdx >= 0) {
                const auto idx = static_cast<std::size_t>(state.selectedNpcRecordIdx);
                if (idx < table->records.size() && !table->records[idx].values.empty())
                    addName(table->records[idx].values[0]);
            }
        }
    } else if (state.editMode == EditMode::Mobs && state.mobRegenTextLoaded) {
        auto* zones = state.mobRegenTextFile.FindTable("MobRegenGroup");
        auto* spawns = state.mobRegenTextFile.FindTable("MobRegen");
        if (spawns) {
            if (state.showAllRoamRoutes) {
                for (const auto& rec : spawns->records) {
                    if (rec.values.size() >= 2) addName(rec.values[1]);
                }
            } else if (state.selectedMobZoneIdx >= 0) {
                const auto zi = static_cast<std::size_t>(state.selectedMobZoneIdx);
                if (zones && zi < zones->records.size() && !zones->records[zi].values.empty()) {
                    const std::string zoneName = zones->records[zi].values[0];
                    for (const auto& rec : spawns->records) {
                        if (rec.values.size() < 2 || rec.values[0] != zoneName) continue;
                        addName(rec.values[1]);
                    }
                }
            }
        }
    }

    std::sort(names.begin(), names.end());
    for (const auto& n : names) key += "|" + n;

    if (key == state.roamOverlayKey) return;
    state.roamOverlayKey = key;
    state.roamOverlayRoutes.clear();
    for (const auto& name : names) {
        if (auto route = LoadRoamOverlayRoute(state, name))
            state.roamOverlayRoutes.push_back(std::move(*route));
    }
}
void DrawRoamRoutes2D(EditorState& state, ImDrawList* drawList,
                      const ImVec2& mapOrigin, const ImVec2& mapSize,
                      float spanX, float spanZ) {
    if (!drawList || !state.showRoamRoutes ||
        (state.editMode != EditMode::Npcs && state.editMode != EditMode::Mobs) ||
        spanX <= 0.0f || spanZ <= 0.0f)
        return;

    RefreshRoamOverlayRoutes(state);
    if (state.roamOverlayRoutes.empty()) return;

    static const ImU32 kRouteColors[] = {
        IM_COL32(80, 220, 255, 235),
        IM_COL32(255, 190, 80, 235),
        IM_COL32(170, 120, 255, 235),
        IM_COL32(90, 235, 150, 235),
    };

    auto toScreen = [&](float x, float z) {
        const float u = x / spanX;
        const float v = 1.0f - z / spanZ;
        return ImVec2(mapOrigin.x + u * mapSize.x,
                      mapOrigin.y + v * mapSize.y);
    };

    for (std::size_t ri = 0; ri < state.roamOverlayRoutes.size(); ++ri) {
        const auto& route = state.roamOverlayRoutes[ri];
        const ImU32 col = kRouteColors[ri % std::size(kRouteColors)];
        std::vector<ImVec2> points;
        points.reserve(route.points.size() + (route.returnsToStart ? 1u : 0u));
        for (const auto& [x,z] : route.points) points.push_back(toScreen(x,z));
        if (route.returnsToStart && points.size() > 1) points.push_back(points.front());

        if (points.size() >= 2)
            drawList->AddPolyline(points.data(), static_cast<int>(points.size()),
                                  col, ImDrawFlags_None, 2.0f);

        const std::size_t realCount = route.points.size();
        for (std::size_t pi = 0; pi < realCount; ++pi) {
            const float radius = pi == 0 ? 4.5f : 3.0f;
            drawList->AddCircleFilled(points[pi], radius, col);
            if (pi + 1 < realCount) {
                const ImVec2 a = points[pi], b = points[pi + 1];
                const float dx = b.x - a.x, dy = b.y - a.y;
                const float len = std::sqrt(dx*dx + dy*dy);
                if (len > 16.0f) {
                    const float ux = dx / len, uy = dy / len;
                    const ImVec2 mid(a.x + dx * 0.5f, a.y + dy * 0.5f);
                    const ImVec2 tip(mid.x + ux * 5.0f, mid.y + uy * 5.0f);
                    const ImVec2 left(mid.x - ux * 4.0f - uy * 3.0f,
                                      mid.y - uy * 4.0f + ux * 3.0f);
                    const ImVec2 right(mid.x - ux * 4.0f + uy * 3.0f,
                                       mid.y - uy * 4.0f - ux * 3.0f);
                    drawList->AddTriangleFilled(tip, left, right, col);
                }
            }
        }

        if (!points.empty()) {
            const ImVec2 p(points.front().x + 7.0f, points.front().y - 10.0f);
            const ImVec2 ts = ImGui::CalcTextSize(route.name.c_str());
            drawList->AddRectFilled(ImVec2(p.x-3.0f,p.y-2.0f),
                                    ImVec2(p.x+ts.x+3.0f,p.y+ts.y+2.0f),
                                    IM_COL32(10,16,24,190),3.0f);
            drawList->AddText(p,col,route.name.c_str());
        }
    }
}
void DrawRoamRoutes3D(EditorState& state, const ImVec2& imagePos, int w, int h) {
    if (!state.showRoamRoutes || (state.editMode != EditMode::Npcs && state.editMode != EditMode::Mobs))
        return;
    RefreshRoamOverlayRoutes(state);
    if (state.roamOverlayRoutes.empty()) return;

    static const ImU32 kRouteColors[] = {
        IM_COL32(80, 220, 255, 235),
        IM_COL32(255, 190, 80, 235),
        IM_COL32(170, 120, 255, 235),
        IM_COL32(90, 235, 150, 235),
    };

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(imagePos, ImVec2(imagePos.x + w, imagePos.y + h), true);
    for (std::size_t ri = 0; ri < state.roamOverlayRoutes.size(); ++ri) {
        const auto& route = state.roamOverlayRoutes[ri];
        const ImU32 col = kRouteColors[ri % std::size(kRouteColors)];
        std::vector<ImVec2> points;
        points.reserve(route.points.size() + (route.returnsToStart ? 1u : 0u));

        for (const auto& [x,z] : route.points) {
            ImVec2 p;
            const float y = state.heightmap.SampleWorld(x,z) + 12.0f;
            if (ProjectWorldTo3DView(state,imagePos,w,h,{x,y,z},p)) points.push_back(p);
        }
        if (route.returnsToStart && points.size() > 1) points.push_back(points.front());
        if (points.size() >= 2) dl->AddPolyline(points.data(), static_cast<int>(points.size()), col, ImDrawFlags_None, 2.2f);

        for (std::size_t pi = 0; pi < points.size() && pi < route.points.size(); ++pi) {
            dl->AddCircleFilled(points[pi], pi == 0 ? 4.5f : 3.0f, col);
            if (pi == 0) {
                const ImVec2 ts = ImGui::CalcTextSize(route.name.c_str());
                const ImVec2 p(points[pi].x + 7.0f, points[pi].y - ts.y * 0.5f);
                dl->AddRectFilled(ImVec2(p.x-3.0f,p.y-2.0f),
                                  ImVec2(p.x+ts.x+3.0f,p.y+ts.y+2.0f),
                                  IM_COL32(10,16,24,190),3.0f);
                dl->AddText(p,col,route.name.c_str());
            }
        }
    }
    dl->PopClipRect();
}
// Zeichnet im 3D-View Namen und Blickpfeile der NPCs (Projektion per ImGui) - damit sich NPCs
// richtig ausrichten lassen (CHANGELOG [0.44.32]).
void DrawNpcOverlay3D(EditorState& state, const ImVec2& imagePos, int w, int h) {
    if (!state.showNpcModels || !state.showNpcLabels || state.editMode != EditMode::Npcs || !state.npcTextLoaded || w <= 0 || h <= 0) return;
    auto* table = state.npcTextFile.FindTable("ShineNPC");
    if (!table) return;
    const app::Mat4 vp = state.camera.ProjectionMatrix(static_cast<float>(w) / static_cast<float>(h)) * state.camera.ViewMatrix();
    auto project = [&](float x, float y, float z, ImVec2& out) {
        const float cx = vp.m[0] * x + vp.m[4] * y + vp.m[8] * z + vp.m[12];
        const float cy = vp.m[1] * x + vp.m[5] * y + vp.m[9] * z + vp.m[13];
        const float cw = vp.m[3] * x + vp.m[7] * y + vp.m[11] * z + vp.m[15];
        if (cw <= 0.01f) return false; // hinter der Kamera
        out = ImVec2(imagePos.x + (cx / cw * 0.5f + 0.5f) * static_cast<float>(w), imagePos.y + (1.0f - (cy / cw * 0.5f + 0.5f)) * static_cast<float>(h));
        return true;
    };
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(imagePos, ImVec2(imagePos.x + static_cast<float>(w), imagePos.y + static_cast<float>(h)), true);
    for (const std::size_t idx : NpcRecordsForCurrentMap(state)) {
        const auto& rec = table->records[idx];
        if (rec.values.size() < 5) continue;
        const float x = static_cast<float>(std::atoi(rec.values[2].c_str()));
        const float z = static_cast<float>(std::atoi(rec.values[3].c_str()));
        const float y = state.heightmap.SampleWorld(x, z);
        const bool selected = state.selectedNpcRecordIdx == static_cast<int>(idx);
        const float yaw = NpcYawRadians(state, std::atoi(rec.values[4].c_str()));
        const float fx = -std::sin(yaw), fz = -std::cos(yaw);
        ImVec2 base, tip, head;
        if (!project(x, y + 25.0f, z, base) || !project(x + fx * 70.0f, y + 25.0f, z + fz * 70.0f, tip) || !project(x, y + 75.0f, z, head)) continue;
        const ImU32 col = selected ? IM_COL32(255, 210, 90, 255) : IM_COL32(120, 200, 255, 220);
        dl->AddLine(base, tip, col, selected ? 3.0f : 2.0f);
        dl->AddCircleFilled(tip, selected ? 5.0f : 3.5f, col);
        dl->AddCircleFilled(base, 3.0f, col);
        const std::string label = rec.values[0];
        const ImVec2 ts = ImGui::CalcTextSize(label.c_str());
        dl->AddRectFilled(ImVec2(head.x - ts.x * 0.5f - 3.0f, head.y - ts.y - 3.0f), ImVec2(head.x + ts.x * 0.5f + 3.0f, head.y + 1.0f), IM_COL32(12, 15, 20, 180), 3.0f);
        dl->AddText(ImVec2(head.x - ts.x * 0.5f, head.y - ts.y - 1.0f), selected ? IM_COL32(255, 235, 170, 255) : IM_COL32(215, 225, 240, 240), label.c_str());
    }
    dl->PopClipRect();
}

} // namespace nge
