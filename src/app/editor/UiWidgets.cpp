// UiWidgets.cpp
// Icons, Szenen-Symbole, Buttons, Karten und die obere Navigation.

#include "EditorShared.hpp"

namespace nge {

// ===========================================================================================
// Neue Navigationsebene (Projekt-Hub / Projekt-Konfiguration / Map-Editor-Start /
// Arbeitsbereich) - orientiert an den vom Nutzer bereitgestellten Mockups ("NextGen-Editor").
// Beschriftungen laufen über Localization.hpp (T(...)), siehe dort für den aktuellen Umfang
// der Übersetzung (neue Navigationsebene vollständig, tiefe Funktions-Panels noch Deutsch).
// ===========================================================================================

// Einfache, rein vektorielle Karten-Icons (kein Bild-Asset nötig) - bewusst nur grobe,
// wiedererkennbare Annäherungen an die Mockup-Symbole, keine Pixel-genaue Nachbildung.
void DrawIconGlobe(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddCircle(c, r, col, 32, 2.5f);
    dl->AddLine(ImVec2(c.x - r, c.y), ImVec2(c.x + r, c.y), col, 2.0f);
    dl->AddEllipse(c, ImVec2(r * 0.42f, r), col, 0.0f, 24, 2.0f);
    dl->AddEllipse(c, ImVec2(r, r * 0.42f), col, 0.0f, 24, 2.0f);
}
void DrawIconPencilPaper(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    const ImVec2 p0(c.x - r * 0.55f, c.y - r);
    const ImVec2 p1(c.x + r * 0.55f, c.y + r);
    dl->AddRect(p0, p1, col, 0.0f, 0, 2.5f);
    for (float t = -0.6f; t <= 0.7f; t += 0.35f) {
        dl->AddLine(ImVec2(p0.x + r * 0.15f, c.y + t * r), ImVec2(p1.x - r * 0.15f, c.y + t * r), col, 1.5f);
    }
    dl->AddLine(ImVec2(c.x - r * 0.1f, c.y - r * 1.3f), ImVec2(c.x + r * 1.1f, c.y + r * 0.6f), col, 3.0f);
    dl->AddTriangleFilled(ImVec2(c.x + r * 1.1f, c.y + r * 0.6f), ImVec2(c.x + r * 1.25f, c.y + r * 0.75f),
                           ImVec2(c.x + r * 0.95f, c.y + r * 0.85f), col);
}
void DrawIconBook(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    const ImVec2 spine(c.x, c.y - r * 0.7f);
    const ImVec2 spineBottom(c.x, c.y + r * 0.9f);
    const ImVec2 leftPts[4] = {spine, ImVec2(c.x - r * 1.1f, c.y - r * 0.95f), ImVec2(c.x - r * 1.1f, c.y + r * 0.65f), spineBottom};
    const ImVec2 rightPts[4] = {spine, ImVec2(c.x + r * 1.1f, c.y - r * 0.95f), ImVec2(c.x + r * 1.1f, c.y + r * 0.65f), spineBottom};
    dl->AddPolyline(leftPts, 4, col, ImDrawFlags_None, 2.5f);
    dl->AddPolyline(rightPts, 4, col, ImDrawFlags_None, 2.5f);
    dl->AddLine(spine, spineBottom, col, 2.0f);
}
void DrawIconMonitorEye(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    const ImVec2 p0(c.x - r, c.y - r * 0.75f);
    const ImVec2 p1(c.x + r, c.y + r * 0.45f);
    dl->AddRect(p0, p1, col, 4.0f, 0, 2.5f);
    dl->AddLine(ImVec2(c.x, p1.y), ImVec2(c.x, c.y + r), col, 2.5f);
    dl->AddLine(ImVec2(c.x - r * 0.5f, c.y + r), ImVec2(c.x + r * 0.5f, c.y + r), col, 2.5f);
    const float eyeCY = (p0.y + p1.y) * 0.5f;
    dl->AddEllipse(ImVec2(c.x, eyeCY), ImVec2(r * 0.55f, r * 0.3f), col, 0.0f, 24, 2.0f);
    dl->AddCircleFilled(ImVec2(c.x, eyeCY), r * 0.14f, col);
}
void DrawIconAtom(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    constexpr float kPi = 3.14159265358979323846f;
    dl->AddEllipse(c, ImVec2(r, r * 0.4f), col, 0.0f, 28, 2.0f);
    dl->AddEllipse(c, ImVec2(r, r * 0.4f), col, kPi / 3.0f, 28, 2.0f);
    dl->AddEllipse(c, ImVec2(r, r * 0.4f), col, -kPi / 3.0f, 28, 2.0f);
    dl->AddCircleFilled(c, r * 0.16f, col);
}
void DrawIconClapper(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    const ImVec2 bodyP0(c.x - r, c.y - r * 0.1f);
    const ImVec2 bodyP1(c.x + r, c.y + r);
    dl->AddRect(bodyP0, bodyP1, col, 2.0f, 0, 2.5f);
    const ImVec2 topL(c.x - r * 1.05f, c.y - r * 0.15f);
    const ImVec2 topR(c.x + r * 1.05f, c.y - r * 0.15f);
    dl->AddLine(ImVec2(topL.x, topL.y - r * 0.35f), topR, col, 2.5f);
    dl->AddLine(topL, ImVec2(topR.x, topR.y - r * 0.35f), col, 2.5f);
    for (float x = -0.85f; x < 1.0f; x += 0.4f) {
        dl->AddLine(ImVec2(c.x + x * r, c.y - r * 0.2f), ImVec2(c.x + (x + 0.2f) * r, c.y - r * 0.45f), col, 2.0f);
    }
}
void DrawIconTerrain(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    const ImVec2 a[3] = {ImVec2(c.x-r*1.05f,c.y+r*0.75f), ImVec2(c.x-r*0.2f,c.y-r*0.9f), ImVec2(c.x+r*0.45f,c.y+r*0.75f)};
    const ImVec2 b[3] = {ImVec2(c.x-r*0.15f,c.y+r*0.75f), ImVec2(c.x+r*0.55f,c.y-r*0.45f), ImVec2(c.x+r*1.05f,c.y+r*0.75f)};
    dl->AddPolyline(a,3,col,ImDrawFlags_Closed,2.5f); dl->AddPolyline(b,3,col,ImDrawFlags_Closed,2.5f);
}
void DrawIconBrush(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddLine(ImVec2(c.x+r*0.75f,c.y-r*0.85f), ImVec2(c.x-r*0.25f,c.y+r*0.20f), col, 4.5f);
    dl->AddTriangleFilled(ImVec2(c.x-r*0.15f,c.y+r*0.05f), ImVec2(c.x-r*0.95f,c.y+r*0.55f),
                          ImVec2(c.x-r*0.45f,c.y+r*0.95f), col);
}
void DrawIconLayers(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    for(int i=0;i<3;++i){ const float y=c.y-r*0.55f+i*r*0.55f;
        const ImVec2 p[4]={ImVec2(c.x,y-r*0.35f),ImVec2(c.x+r,y),ImVec2(c.x,y+r*0.35f),ImVec2(c.x-r,y)};
        dl->AddPolyline(p,4,col,ImDrawFlags_Closed,2.2f);
    }
}
void DrawIconGrid(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddRect(ImVec2(c.x-r,c.y-r),ImVec2(c.x+r,c.y+r),col,2.0f,0,2.2f);
    for(int i=-1;i<=1;i+=2){ dl->AddLine(ImVec2(c.x+i*r/3,c.y-r),ImVec2(c.x+i*r/3,c.y+r),col,1.7f);
        dl->AddLine(ImVec2(c.x-r,c.y+i*r/3),ImVec2(c.x+r,c.y+i*r/3),col,1.7f); }
}
void DrawIconCube(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    const ImVec2 top(c.x,c.y-r), l(c.x-r,c.y-r*0.35f), rr(c.x+r,c.y-r*0.35f);
    const ImVec2 bl(c.x-r,c.y+r*0.65f), br(c.x+r,c.y+r*0.65f), bot(c.x,c.y+r);
    dl->AddLine(top,l,col,2.3f); dl->AddLine(top,rr,col,2.3f); dl->AddLine(l,ImVec2(c.x,c.y+r*0.05f),col,2.3f);
    dl->AddLine(rr,ImVec2(c.x,c.y+r*0.05f),col,2.3f); dl->AddLine(l,bl,col,2.3f); dl->AddLine(rr,br,col,2.3f);
    dl->AddLine(bl,bot,col,2.3f); dl->AddLine(br,bot,col,2.3f); dl->AddLine(ImVec2(c.x,c.y+r*0.05f),bot,col,2.3f);
}
void DrawIconPerson(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddCircleFilled(ImVec2(c.x,c.y-r*0.55f),r*0.32f,col);
    dl->AddRectFilled(ImVec2(c.x-r*0.55f,c.y-r*0.05f),ImVec2(c.x+r*0.55f,c.y+r*0.8f),col,r*0.25f);
}
void DrawIconSpawn(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddCircle(c,r*0.80f,col,24,2.2f); dl->AddCircleFilled(c,r*0.20f,col);
    for(int i=0;i<4;++i){ const float a=0.785398f+i*1.570796f;
        dl->AddCircleFilled(ImVec2(c.x+std::cos(a)*r*0.72f,c.y+std::sin(a)*r*0.72f),r*0.15f,col); }
}
void DrawIconShop(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddRect(ImVec2(c.x-r*0.9f,c.y-r*0.15f),ImVec2(c.x+r*0.9f,c.y+r*0.8f),col,2.0f,0,1.9f);
    dl->AddLine(ImVec2(c.x-r,c.y-r*0.15f),ImVec2(c.x-r*0.72f,c.y-r*0.85f),col,1.9f);
    dl->AddLine(ImVec2(c.x-r*0.72f,c.y-r*0.85f),ImVec2(c.x+r*0.72f,c.y-r*0.85f),col,1.9f);
    dl->AddLine(ImVec2(c.x+r*0.72f,c.y-r*0.85f),ImVec2(c.x+r,c.y-r*0.15f),col,1.9f);
    dl->AddLine(ImVec2(c.x-r*0.45f,c.y-r*0.85f),ImVec2(c.x-r*0.45f,c.y-r*0.15f),col,1.4f);
    dl->AddLine(ImVec2(c.x,c.y-r*0.85f),ImVec2(c.x,c.y-r*0.15f),col,1.4f);
    dl->AddLine(ImVec2(c.x+r*0.45f,c.y-r*0.85f),ImVec2(c.x+r*0.45f,c.y-r*0.15f),col,1.4f);
}
void DrawIconRoute(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    const ImVec2 p0(c.x-r*0.85f,c.y+r*0.62f), p1(c.x-r*0.15f,c.y-r*0.2f), p2(c.x+r*0.75f,c.y+r*0.15f);
    dl->AddLine(p0,p1,col,2.0f); dl->AddLine(p1,p2,col,2.0f);
    dl->AddCircleFilled(p0,r*0.20f,col); dl->AddCircleFilled(p1,r*0.20f,col); dl->AddCircleFilled(p2,r*0.20f,col);
    dl->AddTriangleFilled(ImVec2(p2.x+r*0.05f,p2.y-r*0.38f),ImVec2(p2.x+r*0.45f,p2.y),ImVec2(p2.x-r*0.05f,p2.y+r*0.20f),col);
}
void DrawIconDialog(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddRect(ImVec2(c.x-r,c.y-r*0.7f),ImVec2(c.x+r,c.y+r*0.45f),col,3.0f,0,2.0f);
    const ImVec2 tail[3]={ImVec2(c.x-r*0.35f,c.y+r*0.45f),ImVec2(c.x-r*0.60f,c.y+r),ImVec2(c.x+r*0.05f,c.y+r*0.45f)};
    dl->AddPolyline(tail,3,col,ImDrawFlags_None,2.0f);
    dl->AddLine(ImVec2(c.x-r*0.60f,c.y-r*0.25f),ImVec2(c.x+r*0.55f,c.y-r*0.25f),col,1.5f);
    dl->AddLine(ImVec2(c.x-r*0.60f,c.y+r*0.05f),ImVec2(c.x+r*0.25f,c.y+r*0.05f),col,1.5f);
}
void DrawIconCode(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddLine(ImVec2(c.x-r*0.9f,c.y),ImVec2(c.x-r*0.35f,c.y-r*0.55f),col,2.2f);
    dl->AddLine(ImVec2(c.x-r*0.9f,c.y),ImVec2(c.x-r*0.35f,c.y+r*0.55f),col,2.2f);
    dl->AddLine(ImVec2(c.x+r*0.9f,c.y),ImVec2(c.x+r*0.35f,c.y-r*0.55f),col,2.2f);
    dl->AddLine(ImVec2(c.x+r*0.9f,c.y),ImVec2(c.x+r*0.35f,c.y+r*0.55f),col,2.2f);
    dl->AddLine(ImVec2(c.x+r*0.18f,c.y-r*0.85f),ImVec2(c.x-r*0.18f,c.y+r*0.85f),col,2.0f);
}
void DrawIconMove(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddLine(ImVec2(c.x-r,c.y),ImVec2(c.x+r,c.y),col,2.0f);
    dl->AddLine(ImVec2(c.x,c.y-r),ImVec2(c.x,c.y+r),col,2.0f);
    dl->AddTriangleFilled(ImVec2(c.x+r,c.y),ImVec2(c.x+r*0.55f,c.y-r*0.28f),ImVec2(c.x+r*0.55f,c.y+r*0.28f),col);
    dl->AddTriangleFilled(ImVec2(c.x-r,c.y),ImVec2(c.x-r*0.55f,c.y-r*0.28f),ImVec2(c.x-r*0.55f,c.y+r*0.28f),col);
    dl->AddTriangleFilled(ImVec2(c.x,c.y-r),ImVec2(c.x-r*0.28f,c.y-r*0.55f),ImVec2(c.x+r*0.28f,c.y-r*0.55f),col);
    dl->AddTriangleFilled(ImVec2(c.x,c.y+r),ImVec2(c.x-r*0.28f,c.y+r*0.55f),ImVec2(c.x+r*0.28f,c.y+r*0.55f),col);
}
void DrawIconRotate(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->PathClear(); dl->PathArcTo(c,r*0.72f,-2.55f,2.45f,28); dl->PathStroke(col,0,2.2f);
    dl->AddTriangleFilled(ImVec2(c.x-r*0.78f,c.y-r*0.52f),ImVec2(c.x-r*0.15f,c.y-r*0.62f),ImVec2(c.x-r*0.48f,c.y-r*0.02f),col);
}
void DrawIconScale(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddRect(ImVec2(c.x-r*0.85f,c.y-r*0.85f),ImVec2(c.x+r*0.35f,c.y+r*0.35f),col,1.5f,0,1.8f);
    dl->AddLine(ImVec2(c.x-r*0.15f,c.y+r*0.15f),ImVec2(c.x+r*0.85f,c.y-r*0.85f),col,2.2f);
    dl->AddTriangleFilled(ImVec2(c.x+r*0.85f,c.y-r*0.85f),ImVec2(c.x+r*0.25f,c.y-r*0.78f),ImVec2(c.x+r*0.78f,c.y-r*0.25f),col);
}
void DrawIconSnap(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->PathClear(); dl->PathArcTo(ImVec2(c.x,c.y+r*0.05f),r*0.65f,0.0f,3.14159265f,20); dl->PathStroke(col,0,2.5f);
    dl->AddLine(ImVec2(c.x-r*0.65f,c.y+r*0.05f),ImVec2(c.x-r*0.65f,c.y+r*0.85f),col,2.5f);
    dl->AddLine(ImVec2(c.x+r*0.65f,c.y+r*0.05f),ImVec2(c.x+r*0.65f,c.y+r*0.85f),col,2.5f);
    dl->AddLine(ImVec2(c.x-r*0.65f,c.y+r*0.85f),ImVec2(c.x-r*0.25f,c.y+r*0.85f),col,2.5f);
    dl->AddLine(ImVec2(c.x+r*0.25f,c.y+r*0.85f),ImVec2(c.x+r*0.65f,c.y+r*0.85f),col,2.5f);
}
void DrawIconPortal(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddEllipse(c,ImVec2(r*0.82f,r),col,0.0f,28,3.0f);
    dl->AddEllipse(c,ImVec2(r*0.42f,r*0.58f),col,0.0f,24,2.0f);
    dl->AddCircleFilled(c,r*0.12f,col);
}
void DrawIconSave(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddRect(ImVec2(c.x-r,c.y-r),ImVec2(c.x+r,c.y+r),col,2.0f,0,2.2f);
    dl->AddRect(ImVec2(c.x-r*0.55f,c.y-r),ImVec2(c.x+r*0.45f,c.y-r*0.25f),col,1.0f,0,2.0f);
    dl->AddRect(ImVec2(c.x-r*0.55f,c.y+r*0.15f),ImVec2(c.x+r*0.55f,c.y+r*0.75f),col,1.0f,0,2.0f);
}
void DrawIconUndo(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->PathClear(); dl->PathArcTo(ImVec2(c.x+r*0.1f,c.y+r*0.15f),r*0.78f,-2.7f,0.65f,24); dl->PathStroke(col,0,2.6f);
    dl->AddTriangleFilled(ImVec2(c.x-r*0.95f,c.y-r*0.1f),ImVec2(c.x-r*0.35f,c.y-r*0.55f),ImVec2(c.x-r*0.35f,c.y+r*0.35f),col);
}
void DrawIconRedo(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->PathClear(); dl->PathArcTo(ImVec2(c.x-r*0.1f,c.y+r*0.15f),r*0.78f,2.5f,5.85f,24); dl->PathStroke(col,0,2.6f);
    dl->AddTriangleFilled(ImVec2(c.x+r*0.95f,c.y-r*0.1f),ImVec2(c.x+r*0.35f,c.y-r*0.55f),ImVec2(c.x+r*0.35f,c.y+r*0.35f),col);
}
void DrawIconTable(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    DrawIconGrid(dl,c,r,col);
    dl->AddRectFilled(ImVec2(c.x-r,c.y-r),ImVec2(c.x+r,c.y-r*0.48f),col,1.5f);
}
void DrawIconBolt(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    const ImVec2 p[6]={ImVec2(c.x+r*0.10f,c.y-r),ImVec2(c.x-r*0.55f,c.y+r*0.05f),ImVec2(c.x-r*0.10f,c.y+r*0.05f),
                       ImVec2(c.x-r*0.25f,c.y+r),ImVec2(c.x+r*0.60f,c.y-r*0.15f),ImVec2(c.x+r*0.12f,c.y-r*0.15f)};
    dl->AddConvexPolyFilled(p,6,col);
}
void DrawIconEye(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->PathClear();
    dl->PathLineTo(ImVec2(c.x-r,c.y));
    dl->PathBezierCubicCurveTo(ImVec2(c.x-r*0.5f,c.y-r*0.7f),ImVec2(c.x+r*0.5f,c.y-r*0.7f),ImVec2(c.x+r,c.y));
    dl->PathBezierCubicCurveTo(ImVec2(c.x+r*0.5f,c.y+r*0.7f),ImVec2(c.x-r*0.5f,c.y+r*0.7f),ImVec2(c.x-r,c.y));
    dl->PathStroke(col,ImDrawFlags_Closed,1.8f);
    dl->AddCircleFilled(c,r*0.28f,col);
}
void DrawIconLock(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddRect(ImVec2(c.x-r*0.72f,c.y-r*0.05f),ImVec2(c.x+r*0.72f,c.y+r*0.85f),col,2.0f,0,1.8f);
    dl->PathClear();
    dl->PathArcTo(ImVec2(c.x,c.y-r*0.05f),r*0.48f,3.14159265f,6.2831853f,16);
    dl->PathStroke(col,0,1.8f);
}
void DrawIconDuplicate(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddRect(ImVec2(c.x-r,c.y-r*0.7f),ImVec2(c.x+r*0.55f,c.y+r*0.85f),col,1.5f,0,1.8f);
    dl->AddRect(ImVec2(c.x-r*0.5f,c.y-r),ImVec2(c.x+r,c.y+r*0.55f),col,1.5f,0,1.8f);
}
void DrawIconDelete(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddLine(ImVec2(c.x-r*0.7f,c.y-r*0.45f),ImVec2(c.x+r*0.7f,c.y-r*0.45f),col,2.0f);
    dl->AddRect(ImVec2(c.x-r*0.52f,c.y-r*0.3f),ImVec2(c.x+r*0.52f,c.y+r*0.85f),col,1.0f,0,1.8f);
    dl->AddLine(ImVec2(c.x-r*0.28f,c.y-r*0.7f),ImVec2(c.x+r*0.28f,c.y-r*0.7f),col,2.0f);
}
void DrawIconGear(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddCircle(c, r*0.62f, col, 20, 2.0f);
    dl->AddCircle(c, r*0.22f, col, 16, 2.0f);
    for (int i=0;i<8;++i) {
        const float a=static_cast<float>(i)*3.14159265f/4.0f;
        const ImVec2 a0(c.x+std::cos(a)*r*0.70f,c.y+std::sin(a)*r*0.70f);
        const ImVec2 a1(c.x+std::cos(a)*r,c.y+std::sin(a)*r);
        dl->AddLine(a0,a1,col,3.0f);
    }
}
void DrawIconSword(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    const ImVec2 tip(c.x+r*0.72f,c.y-r*0.82f);
    const ImVec2 pommel(c.x-r*0.62f,c.y+r*0.78f);
    dl->AddLine(pommel,tip,col,3.0f);
    dl->AddTriangleFilled(tip,ImVec2(c.x+r*0.28f,c.y-r*0.62f),ImVec2(c.x+r*0.55f,c.y-r*0.30f),col);
    dl->AddLine(ImVec2(c.x-r*0.58f,c.y+r*0.18f),ImVec2(c.x+r*0.10f,c.y+r*0.74f),col,2.6f);
    dl->AddCircleFilled(pommel,r*0.16f,col);
}
void DrawIconGem(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    const ImVec2 p[5]={ImVec2(c.x-r*0.62f,c.y-r*0.68f),ImVec2(c.x+r*0.62f,c.y-r*0.68f),
                       ImVec2(c.x+r*0.92f,c.y-r*0.12f),ImVec2(c.x,c.y+r*0.92f),
                       ImVec2(c.x-r*0.92f,c.y-r*0.12f)};
    dl->AddPolyline(p,5,col,ImDrawFlags_Closed,2.2f);
    dl->AddLine(ImVec2(c.x-r*0.62f,c.y-r*0.68f),ImVec2(c.x,c.y+r*0.92f),col,1.4f);
    dl->AddLine(ImVec2(c.x+r*0.62f,c.y-r*0.68f),ImVec2(c.x,c.y+r*0.92f),col,1.4f);
}
void DrawIconCoin(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddCircle(c,r*0.82f,col,24,2.4f);
    dl->AddCircle(c,r*0.55f,col,20,1.4f);
    dl->AddLine(ImVec2(c.x-r*0.18f,c.y-r*0.42f),ImVec2(c.x+r*0.18f,c.y+r*0.42f),col,2.0f);
}
void DrawIconDice(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddRect(ImVec2(c.x-r*0.82f,c.y-r*0.82f),ImVec2(c.x+r*0.82f,c.y+r*0.82f),col,2.5f,0,2.1f);
    const float p=r*0.43f, q=r*0.12f;
    dl->AddCircleFilled(ImVec2(c.x-p,c.y-p),q,col);
    dl->AddCircleFilled(c,q,col);
    dl->AddCircleFilled(ImVec2(c.x+p,c.y+p),q,col);
}
void DrawIconBanner(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddLine(ImVec2(c.x-r*0.65f,c.y-r),ImVec2(c.x-r*0.65f,c.y+r),col,2.4f);
    const ImVec2 flag[4]={ImVec2(c.x-r*0.60f,c.y-r*0.86f),ImVec2(c.x+r*0.80f,c.y-r*0.62f),
                          ImVec2(c.x+r*0.35f,c.y-r*0.10f),ImVec2(c.x-r*0.60f,c.y-r*0.28f)};
    dl->AddPolyline(flag,4,col,ImDrawFlags_Closed,2.0f);
}
void DrawIconPack(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddCircleFilled(ImVec2(c.x-r*0.48f,c.y-r*0.42f),r*0.26f,col);
    dl->AddCircleFilled(ImVec2(c.x+r*0.48f,c.y-r*0.42f),r*0.26f,col);
    dl->AddCircleFilled(ImVec2(c.x,c.y-r*0.68f),r*0.30f,col);
    dl->AddEllipseFilled(ImVec2(c.x,c.y+r*0.30f),ImVec2(r*0.92f,r*0.52f),col);
}
SceneSemanticIcon ResolveNpcSceneIcon(const std::string& role, const std::string& arg) {
    SceneSemanticIcon out;
    out.tooltip = role.empty() ? "NPC" : role;
    if (!arg.empty() && arg != "-") out.tooltip += " · " + arg;

    if (role == "QuestNpc") {
        if (arg == "GBDice") { out.icon=DrawIconDice; out.color=IM_COL32(235,165,255,245); }
        else { out.icon=DrawIconBook; out.color=IM_COL32(255,205,90,245); }
    } else if (role == "Merchant") {
        if (arg == "Weapon" || arg == "WeaponTitle") { out.icon=DrawIconSword; out.color=IM_COL32(255,155,90,245); }
        else if (arg == "Skill") { out.icon=DrawIconBolt; out.color=IM_COL32(95,205,255,245); }
        else if (arg == "SoulStone") { out.icon=DrawIconGem; out.color=IM_COL32(190,125,255,245); }
        else if (arg == "Guild") { out.icon=DrawIconBanner; out.color=IM_COL32(105,175,255,245); }
        else { out.icon=DrawIconShop; out.color=IM_COL32(100,225,160,245); }
    } else if (role == "StoreManager") {
        out.icon=DrawIconShop; out.color=IM_COL32(90,215,150,245);
    } else if (role == "Guard") {
        out.icon=DrawIconLock; out.color=IM_COL32(255,150,90,245);
    } else if (role == "Gate") {
        out.icon=DrawIconPortal; out.color=IM_COL32(185,125,255,245);
    } else if (role == "NPCMenu") {
        if (arg == "Guild") { out.icon=DrawIconBanner; out.color=IM_COL32(105,175,255,245); }
        else if (arg == "ExchangeCoin") { out.icon=DrawIconCoin; out.color=IM_COL32(255,205,90,245); }
        else if (arg == "RandomOption") { out.icon=DrawIconGear; out.color=IM_COL32(210,145,255,245); }
        else { out.icon=DrawIconTable; out.color=IM_COL32(125,190,255,245); }
    }
    return out;
}
SceneSemanticIcon ResolveMobZoneSceneIcon(int speciesCount) {
    if (speciesCount <= 0)
        return {DrawIconSpawn,IM_COL32(125,135,150,220),
                L("Leere Spawn-Zone","Empty spawn zone")};
    if (speciesCount == 1)
        return {DrawIconSpawn,IM_COL32(95,195,255,245),
                L("Spawn-Zone · eine Monsterart","Spawn zone · one monster species")};
    return {DrawIconPack,IM_COL32(255,165,90,245),
            L("Gemischte Mob-Gruppe · mehrere Monsterarten",
              "Mixed mob group · multiple monster species")};
}
int NpcSceneGroupRank(const std::string& role, const std::string& arg) {
    if (role == "QuestNpc") return arg == "GBDice" ? 1 : 0;
    if (role == "Merchant") {
        if (arg == "Weapon" || arg == "WeaponTitle") return 10;
        if (arg == "Skill") return 11;
        if (arg == "SoulStone") return 12;
        if (arg == "Guild") return 13;
        return 14;
    }
    if (role == "StoreManager") return 15;
    if (role == "NPCMenu") {
        if (arg == "Guild") return 20;
        if (arg == "ExchangeCoin") return 21;
        if (arg == "RandomOption") return 22;
        return 23;
    }
    if (role == "Guard") return 24;
    if (role == "Gate") return 30;
    return 40;
}
std::string NpcSceneGroupLabel(const std::string& role, const std::string& arg) {
    if (role == "QuestNpc") return arg == "GBDice" ? L("Quests / Würfel","Quests / Dice") : "Quests";
    if (role == "Merchant") {
        if (arg == "Weapon" || arg == "WeaponTitle") return L("Handel / Waffen","Trade / Weapons");
        if (arg == "Skill") return L("Handel / Skills","Trade / Skills");
        if (arg == "SoulStone") return L("Handel / SoulStone","Trade / SoulStone");
        if (arg == "Guild") return L("Handel / Gilde","Trade / Guild");
        return L("Handel / Händler","Trade / Merchant");
    }
    if (role == "StoreManager") return L("Handel / Lager","Trade / Storage");
    if (role == "NPCMenu") {
        if (arg == "Guild") return L("Service / Gilde","Service / Guild");
        if (arg == "ExchangeCoin") return L("Service / Münztausch","Service / Coin exchange");
        if (arg == "RandomOption") return L("Service / Random Option","Service / Random Option");
        return L("Service / Menü","Service / Menu");
    }
    if (role == "Guard") return L("Service / Wache","Service / Guard");
    if (role == "Gate") return "Gates";
    if (!role.empty() && role != "-") return std::string(L("Sonstige / ","Other / ")) + role;
    return L("Sonstige","Other");
}
int MobSceneGroupRank(int speciesCount) {
    if (speciesCount <= 0) return 0;
    if (speciesCount == 1) return 1;
    return 2;
}
const char* MobSceneGroupLabel(int speciesCount) {
    if (speciesCount <= 0) return L("Leere Zonen","Empty zones");
    if (speciesCount == 1) return L("Eine Monsterart","One monster species");
    return L("Gemischte Gruppen","Mixed groups");
}
bool SceneQuickFilterButton(const char* id,const char* label,bool active) {
    ImGui::PushID(id);
    if(active) {
        ImGui::PushStyleColor(ImGuiCol_Button,UiTheme::AccentDeep);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,UiTheme::AccentBlue);
    }
    const bool clicked=UI::SmallButton(label);
    if(active) ImGui::PopStyleColor(2);
    ImGui::PopID();
    return clicked;
}
void DrawInlineIcon(const char* id, IconDrawFn icon, ImU32 color,
                    const char* tooltip, ImVec2 size,
                    const char* semanticIcon) {
    ImGui::PushID(id);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##inlineIcon", size);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool drewApprovedIcon = false;
    if (semanticIcon && *semanticIcon) {
        if (const std::uint32_t tex = gUiIcons.Texture(semanticIcon, 16); tex != 0) {
            const float iconSize = std::min(16.0f, std::min(size.x - 2.0f, size.y - 2.0f));
            const ImVec2 center(p.x + size.x * 0.5f, p.y + size.y * 0.5f);
            const ImVec2 half(iconSize * 0.5f, iconSize * 0.5f);
            dl->AddImage(static_cast<ImTextureID>(static_cast<intptr_t>(tex)),
                         ImVec2(center.x - half.x, center.y - half.y),
                         ImVec2(center.x + half.x, center.y + half.y),
                         ImVec2(0,1), ImVec2(1,0));
            drewApprovedIcon = true;
        }
    }
    if (!drewApprovedIcon && icon)
        icon(dl, ImVec2(p.x + size.x * 0.5f, p.y + size.y * 0.5f), 6.5f, color);
    if (tooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
    ImGui::PopID();
}
bool DrawSearchInput(const char* id, const char* hint, char* buffer, std::size_t bufferSize,
                     float inputWidth, bool focusInput) {
    ImGui::PushID(id);
    DrawInlineIcon("searchIcon", nullptr, IM_COL32(100,205,255,245), hint,
                   ImVec2(18.0f,18.0f), "panel.search");
    ImGui::SameLine(0.0f,5.0f);
    if (focusInput) ImGui::SetKeyboardFocusHere();
    ImGui::SetNextItemWidth(inputWidth);
    const bool changed=UI::InputTextWithHint("##searchInput",hint,buffer,bufferSize);
    ImGui::PopID();
    return changed;
}
void DrawContextMenuHeader(const char* id, const char* title, IconDrawFn fallbackIcon,
                           const char* semanticIcon, const char* subtitle) {
    ImGui::PushID(id);
    DrawInlineIcon("contextIcon", fallbackIcon, IM_COL32(100,205,255,245), nullptr,
                   ImVec2(18.0f,18.0f), semanticIcon);
    ImGui::SameLine(0.0f,5.0f);
    ImGui::TextUnformatted(title);
    if (subtitle != nullptr && subtitle[0] != '\0') {
        ImGui::SameLine(0.0f,7.0f);
        ImGui::TextDisabled("%s",subtitle);
    }
    ImGui::PopID();
    ImGui::Separator();
}
void DrawPanelHeader(const char* id, const char* title, IconDrawFn fallbackIcon,
                     const char* semanticIcon, const char* subtitle) {
    DrawInlineIcon(id, fallbackIcon, IM_COL32(100,205,255,245), nullptr,
                   ImVec2(18.0f,18.0f), semanticIcon);
    ImGui::SameLine(0.0f, 5.0f);
    ImGui::TextColored(UiTheme::AccentCyan, "%s", title);
    if (subtitle && *subtitle) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", subtitle);
    }

    const ImVec2 min = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    ImGui::Dummy(ImVec2(0.0f, 2.0f));
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(min.x, min.y + 1.0f),
        ImVec2(min.x + std::max(0.0f, width), min.y + 1.0f),
        IM_COL32(25,92,132,145), 1.0f);
    ImGui::Dummy(ImVec2(0.0f, 3.0f));
}
bool DrawTinyIconButton(const char* id, IconDrawFn icon, bool active, const char* tooltip,
                        ImVec2 size, const char* semanticIcon) {
    ImGui::PushID(id);
    const ImVec2 p=ImGui::GetCursorScreenPos();
    const bool clicked=ImGui::InvisibleButton("##tinyIcon",size);
    const bool hovered=ImGui::IsItemHovered();
    ImDrawList* dl=ImGui::GetWindowDrawList();
    if(active || hovered) {
        dl->AddRectFilled(ImVec2(p.x, p.y + 1.0f), ImVec2(p.x+size.x,p.y+size.y+1.0f),
                          IM_COL32(0,4,12,110),5.0f);
        dl->AddRectFilled(p,ImVec2(p.x+size.x,p.y+size.y),
                          active?IM_COL32(8,79,176,245):IM_COL32(14,43,68,245),5.0f);
        dl->AddRect(p,ImVec2(p.x+size.x,p.y+size.y),
                    active?IM_COL32(43,224,247,245):IM_COL32(32,116,174,190),5.0f,0,1.0f);
    }
    if(active)
        dl->AddRectFilled(ImVec2(p.x+4.0f,p.y+size.y-2.0f),ImVec2(p.x+size.x-4.0f,p.y+size.y),
                          IM_COL32(32,221,242,255),1.0f);
    bool drewApprovedIcon = false;
    if (semanticIcon && *semanticIcon) {
        if (const std::uint32_t tex = gUiIcons.Texture(semanticIcon, 16); tex != 0) {
            const float iconSize = std::min(16.0f, std::min(size.x - 4.0f, size.y - 4.0f));
            const ImVec2 center(p.x + size.x * 0.5f, p.y + size.y * 0.5f);
            const ImVec2 half(iconSize * 0.5f, iconSize * 0.5f);
            dl->AddImage(static_cast<ImTextureID>(static_cast<intptr_t>(tex)),
                         ImVec2(center.x - half.x, center.y - half.y),
                         ImVec2(center.x + half.x, center.y + half.y),
                         ImVec2(0,1), ImVec2(1,0));
            drewApprovedIcon = true;
        }
    }
    if(!drewApprovedIcon && icon) icon(dl,ImVec2(p.x+size.x*0.5f,p.y+size.y*0.5f),7.2f,
                  active?IM_COL32(244,251,255,255):hovered?IM_COL32(222,241,255,255):IM_COL32(159,180,201,245));
    if(hovered && tooltip) ImGui::SetTooltip("%s",tooltip);
    ImGui::PopID();
    return clicked;
}
bool DrawIconButton(const char* id, const char* label, IconDrawFn icon, bool active,
                    ImVec2 size, bool enabled,
                    const char* semanticIcon) {
    ImGui::PushID(id);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::BeginDisabled(!enabled);
    const bool clicked = ImGui::InvisibleButton("##iconButton", size);
    ImGui::EndDisabled();
    const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const ImU32 bg = active ? IM_COL32(7, 63, 143, 250)
                     : hovered && enabled ? IM_COL32(13, 48, 78, 255)
                                          : IM_COL32(10, 27, 43, 255);
    const ImU32 border = active ? IM_COL32(32, 221, 242, 245)
                                : hovered && enabled ? IM_COL32(19, 140, 255, 205)
                                                     : IM_COL32(31, 62, 88, 255);
    const ImU32 fg = enabled ? (active ? IM_COL32(247, 252, 255, 255)
                                       : hovered ? IM_COL32(230, 245, 255, 255)
                                                 : IM_COL32(169, 195, 216, 255))
                             : IM_COL32(88, 108, 126, 145);

    dl->AddRectFilled(ImVec2(p.x, p.y + 2.0f), ImVec2(p.x + size.x, p.y + size.y + 2.0f),
                      IM_COL32(0, 3, 10, active ? 145 : 90), 7.0f);
    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), bg, 7.0f);
    dl->AddRect(p, ImVec2(p.x + size.x, p.y + size.y), border, 7.0f, 0, active ? 1.6f : 1.0f);
    dl->AddLine(ImVec2(p.x + 7.0f, p.y + 1.0f), ImVec2(p.x + size.x - 7.0f, p.y + 1.0f),
                active ? IM_COL32(151, 248, 255, 205) : IM_COL32(55, 101, 135, 105), 1.0f);
    if (active) {
        dl->AddRectFilled(ImVec2(p.x + 9.0f, p.y + size.y - 3.0f),
                          ImVec2(p.x + size.x - 9.0f, p.y + size.y - 1.0f),
                          IM_COL32(32, 221, 242, 255), 1.0f);
    }

    bool drewApprovedIcon = false;
    if (semanticIcon && *semanticIcon) {
        if (const std::uint32_t tex = gUiIcons.Texture(semanticIcon, 24); tex != 0) {
            constexpr float iconSize = 24.0f;
            const ImVec2 center(p.x + size.x * 0.5f, p.y + 21.0f);
            const ImVec2 half(iconSize * 0.5f, iconSize * 0.5f);
            const ImU32 tint = enabled ? IM_COL32(255,255,255,255) : IM_COL32(255,255,255,105);
            dl->AddImage(static_cast<ImTextureID>(static_cast<intptr_t>(tex)),
                         ImVec2(center.x - half.x, center.y - half.y),
                         ImVec2(center.x + half.x, center.y + half.y),
                         ImVec2(0,1), ImVec2(1,0), tint);
            drewApprovedIcon = true;
        }
    }
    if (!drewApprovedIcon && icon)
        icon(dl, ImVec2(p.x + size.x * 0.5f, p.y + 21.0f), 10.8f, fg);
    const ImVec2 ts = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(p.x + (size.x - ts.x) * 0.5f, p.y + size.y - 18.0f), fg, label);
    if (hovered) ImGui::SetTooltip("%s", label);
    ImGui::PopID();
    return clicked && enabled;
}
bool DrawCompactIconTextButton(const char* id, const char* label, IconDrawFn fallbackIcon,
                               const char* semanticIcon, bool enabled,
                               const char* tooltip, bool active) {
    ImGui::PushID(id);
    const ImVec2 textSize = ImGui::CalcTextSize(label);
    const ImVec2 size(textSize.x + 34.0f, 30.0f);
    const ImVec2 p = ImGui::GetCursorScreenPos();

    ImGui::BeginDisabled(!enabled);
    const bool clicked = ImGui::InvisibleButton("##compactIconText", size);
    ImGui::EndDisabled();
    const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 bg = active ? IM_COL32(7,63,143,235)
                      : hovered && enabled ? IM_COL32(13,48,78,235)
                                           : IM_COL32(8,25,40,120);
    const ImU32 border = active ? IM_COL32(32,221,242,220)
                          : hovered && enabled ? IM_COL32(19,140,255,185)
                                               : IM_COL32(31,62,88,160);
    const ImU32 fg = enabled ? (active ? IM_COL32(244,251,255,255)
                                : hovered ? IM_COL32(232,246,255,255)
                                          : IM_COL32(181,204,222,255))
                             : IM_COL32(88,108,126,145);

    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), bg, 5.0f);
    dl->AddRect(p, ImVec2(p.x + size.x, p.y + size.y), border, 5.0f, 0, active ? 1.4f : 1.0f);
    if (active) {
        dl->AddRectFilled(ImVec2(p.x + 6.0f, p.y + size.y - 2.0f),
                          ImVec2(p.x + size.x - 6.0f, p.y + size.y),
                          IM_COL32(32,221,242,255), 1.0f);
    }

    bool drewApprovedIcon = false;
    if (semanticIcon && *semanticIcon) {
        if (const std::uint32_t tex = gUiIcons.Texture(semanticIcon, 24); tex != 0) {
            constexpr float iconSize = 20.0f;
            const ImVec2 iconMin(p.x + 7.0f, p.y + 5.0f);
            dl->AddImage(static_cast<ImTextureID>(static_cast<intptr_t>(tex)),
                         iconMin, ImVec2(iconMin.x + iconSize, iconMin.y + iconSize),
                         ImVec2(0,1), ImVec2(1,0),
                         enabled ? IM_COL32(255,255,255,255) : IM_COL32(255,255,255,105));
            drewApprovedIcon = true;
        }
    }
    if (!drewApprovedIcon && fallbackIcon)
        fallbackIcon(dl, ImVec2(p.x + 17.0f, p.y + 15.0f), 7.0f, fg);

    dl->AddText(ImVec2(p.x + 30.0f, p.y + (size.y - textSize.y) * 0.5f), fg, label);
    if (hovered && tooltip) ImGui::SetTooltip("%s", tooltip);
    ImGui::PopID();
    return clicked && enabled;
}
void DrawTopNav(EditorState& state, const char* breadcrumbTitle) {
    LoadRecentEntries(state);
    LoadShortcutSettings(state);
    LoadViewportSettings(state);
    LoadWorkspaceSettings(state);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, UiTheme::PanelDeep);
    ImGui::PushStyleColor(ImGuiCol_Border, UiTheme::Border);
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, IM_COL32(5,17,29,252));
    ImGui::BeginChild("##nextgenTopBar", ImVec2(0.0f, 76.0f), true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                      ImGuiWindowFlags_MenuBar);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(11.0f, 7.0f));

    ImDrawList* topDl = ImGui::GetWindowDrawList();
    const ImVec2 topMin = ImGui::GetWindowPos();
    const ImVec2 topMax(topMin.x + ImGui::GetWindowWidth(), topMin.y + ImGui::GetWindowHeight());
    topDl->AddRectFilledMultiColor(topMin, topMax,
                                  IM_COL32(5,18,31,255), IM_COL32(8,28,45,255),
                                  IM_COL32(9,34,53,255), IM_COL32(5,20,34,255));
    topDl->AddLine(ImVec2(topMin.x + 1.0f, topMax.y - 2.0f),
                   ImVec2(topMax.x - 1.0f, topMax.y - 2.0f),
                   IM_COL32(19,140,255,120), 1.0f);

    const bool mapWorkspace = state.screen == AppScreen::MapEditorWorkspace;
    const bool mapLoaded = state.hasLegacyIniMeta || state.legacySaveStem[0] != '\0';
    const bool terrainToolsAvailable = mapLoaded && MapSupportsTerrainEditing(state);
    const bool mapCanSave = state.legacySaveDir[0] != '\0' && state.legacySaveStem[0] != '\0';

    auto createNewProject = [&] {
        state.project = ProjectConfig{};
        state.screen = AppScreen::NewProjectConfig;
    };
    auto openProjectFolder = [&] {
#ifdef _WIN32
        if (auto picked = BrowseForFolderWindows(L("Projekt-Ordner wählen","Choose project folder")))
            LoadProjectFolderIntoState(state, *picked);
#endif
    };
    auto saveProjectConfig = [&] {
        std::string err;
        if (SaveProjectConfig(state.project, &err)) {
            TouchRecentProject(state, state.project.projectFolder);
            state.statusMessage = T("newproject.saved");
        } else {
            state.statusMessage = T("newproject.savefailed") + err;
        }
    };
    auto saveCurrentMap = [&] {
        if (!mapCanSave) return;
        if (SaveLegacyMapProject(state) && state.editMode == EditMode::Portals) {
            if (state.townPortalDirty) SaveTownPortalFiles(state);
            if (state.recallCoordDirty) SaveRecallCoordFile(state);
        }
    };

    auto canMapHistory = [&](bool redo) {
        if (!mapWorkspace) return false;
        return redo ? CanRedoMapEdit(state) : CanUndoMapEdit(state);
    };
    auto applyMapHistory = [&](bool redo) {
        if (redo) RedoMapEdit(state);
        else UndoMapEdit(state);
    };
    auto selectMapTool = [&](EditMode mode) {
        if (!mapLoaded) return;
        const bool terrainOnly =
            mode == EditMode::Heightmap || mode == EditMode::TexturePaint || mode == EditMode::BlockWalk;
        if (terrainOnly && !terrainToolsAvailable) {
            state.statusMessage = L(
                "Diese NIF-/Mesh-Karte besitzt kein HTD/HTDG-Terrain. Terrain-, Textur- und Walk-Werkzeuge sind deaktiviert.",
                "This NIF/mesh map has no HTD/HTDG terrain. Terrain, texture and walk tools are disabled.");
            return;
        }
        state.editMode = mode;
        state.screen = AppScreen::MapEditorWorkspace;
    };

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu(L("Datei","File"))) {
            if (ImGui::MenuItem(T("nav.new"))) createNewProject();
            if (ImGui::MenuItem(T("nav.open"))) openProjectFolder();
            if (ImGui::MenuItem(L("Projekt speichern","Save project"), nullptr, false, state.project.hasProject))
                saveProjectConfig();
            ImGui::Separator();
            const std::string saveShortcut = ShortcutLabel(state.shortcutSave);
            if (ImGui::MenuItem(L("Karte speichern","Save map"), saveShortcut.c_str(), false, mapCanSave))
                saveCurrentMap();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("Bearbeiten","Edit"))) {
            if (ImGui::MenuItem(L("Rückgängig","Undo"), L("Strg+Z","Ctrl+Z"), false, canMapHistory(false)))
                applyMapHistory(false);
            if (ImGui::MenuItem(L("Wiederholen","Redo"), L("Strg+Y","Ctrl+Y"), false, canMapHistory(true)))
                applyMapHistory(true);
            ImGui::Separator();
            const bool objectMode = mapWorkspace && state.editMode == EditMode::ObjectPlacement;
            if (ImGui::MenuItem(L("Kopieren","Copy"), L("Strg+C","Ctrl+C"), false,
                                objectMode && !state.selectedObjects.empty()))
                CopySelectedObjects(state);
            if (ImGui::MenuItem(L("Einfügen","Paste"), L("Strg+V","Ctrl+V"), false,
                                objectMode && !state.objectClipboard.empty()))
                PasteObjectClipboard(state);
            if (ImGui::MenuItem(L("Duplizieren","Duplicate"), ShortcutLabel(state.shortcutDuplicate).c_str(), false,
                                objectMode && !state.selectedObjects.empty()))
                DuplicateSelectedObjects(state);
            if (ImGui::MenuItem(L("Löschen","Delete"), ShortcutLabel(state.shortcutDelete).c_str(), false,
                                objectMode && !state.selectedObjects.empty()))
                DeleteSelectedObjects(state);
            ImGui::Separator();
            if (ImGui::MenuItem(L("Alles auswählen","Select all"), nullptr, false, objectMode))
                SelectAllNormalObjects(state);
            if (ImGui::MenuItem(L("Auswahl aufheben","Deselect"), "Esc", false,
                                objectMode && !state.selectedObjects.empty()))
                ClearObjectSelection(state);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("Ansicht","View"))) {
            if (ImGui::BeginMenu(L("Kamera-Ansicht","Camera view"), mapWorkspace)) {
                for (int preset = 0; preset <= 5; ++preset)
                    if (ImGui::MenuItem(ViewportPresetName(preset), nullptr, state.viewportViewPreset == preset))
                        ApplyViewportPreset(state, preset);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(L("Anzeigen (Show Flags)","Show flags"), mapWorkspace)) {
                DrawViewportShowFlagsMenu(state);
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem(L("Unreal-Navigation","Unreal navigation"), nullptr, &state.unrealNavigation))
                SaveViewportSettings(state);
            ImGui::Separator();
            if (ImGui::MenuItem(L("Workspace: Standard","Workspace: Standard")))
                RequestMapWorkspacePreset(state,0);
            if (ImGui::MenuItem(L("Workspace: 3D-Fokus","Workspace: 3D focus")))
                RequestMapWorkspacePreset(state,1);
            if (ImGui::MenuItem(L("Workspace: Terrain / 2D","Workspace: Terrain / 2D")))
                RequestMapWorkspacePreset(state,2);
            if (ImGui::MenuItem(L("Workspace: Daten / Szene","Workspace: Data / scene")))
                RequestMapWorkspacePreset(state,3);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("Map","Map"))) {
            if (ImGui::MenuItem(L("Terrain","Terrain"), nullptr, state.editMode==EditMode::Heightmap, terrainToolsAvailable))
                selectMapTool(EditMode::Heightmap);
            if (ImGui::MenuItem(L("Texturen","Textures"), nullptr, state.editMode==EditMode::TexturePaint, terrainToolsAvailable))
                selectMapTool(EditMode::TexturePaint);
            if (ImGui::MenuItem("Block & Walk", nullptr, state.editMode==EditMode::BlockWalk, terrainToolsAvailable))
                selectMapTool(EditMode::BlockWalk);
            if (ImGui::MenuItem(L("Objekte","Objects"), nullptr, state.editMode==EditMode::ObjectPlacement, mapLoaded))
                selectMapTool(EditMode::ObjectPlacement);
            if (ImGui::MenuItem("NPCs", nullptr, state.editMode==EditMode::Npcs, mapLoaded))
                selectMapTool(EditMode::Npcs);
            if (ImGui::MenuItem("Mobs", nullptr, state.editMode==EditMode::Mobs, mapLoaded))
                selectMapTool(EditMode::Mobs);
            if (ImGui::MenuItem(L("Portale","Portals"), nullptr, state.editMode==EditMode::Portals, mapLoaded))
                selectMapTool(EditMode::Portals);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("Objekte","Objects"))) {
            const bool hasSceneSelection = !state.selectedObjects.empty() ||
                                           state.selectedNpcRecordIdx >= 0 ||
                                           state.selectedMobZoneIdx >= 0 ||
                                           state.selectedPortalIdx >= 0;
            if (ImGui::MenuItem(L("Auswahl fokussieren","Focus selection"),
                                ShortcutLabel(state.shortcutFocus).c_str(), false,
                                mapWorkspace && hasSceneSelection))
                FocusCurrentSceneSelection(state);
            if (ImGui::MenuItem(L("Auf Terrain setzen","Drop to terrain"),
                                ShortcutLabel(state.shortcutGround).c_str(), false,
                                mapWorkspace && state.editMode==EditMode::ObjectPlacement &&
                                !state.selectedObjects.empty()))
                GroundSelectedObjects(state);
            const bool objectTools = mapWorkspace && state.editMode==EditMode::ObjectPlacement;
            const std::size_t selCount = state.selectedObjects.size();
            ImGui::Separator();
            if (ImGui::MenuItem(L("Auswahl ausblenden","Hide selected"), "H", false, objectTools && selCount > 0))
                HideSelectedObjects(state);
            if (ImGui::MenuItem(L("Auswahl isolieren","Isolate selection"), L("Shift+H","Shift+H"), false, objectTools && selCount > 0))
                HideUnselectedObjects(state);
            if (ImGui::MenuItem(L("Alle einblenden","Show all"), L("Strg+H","Ctrl+H"), false,
                                objectTools && CountEditorHiddenObjects(state) > 0))
                ShowAllObjects(state);
            ImGui::Separator();
            if (ImGui::BeginMenu(L("Ausrichten","Align"), objectTools && selCount >= 2)) {
                const char* axisNames[3] = {L("X (Ost)","X (east)"), L("Y (Höhe)","Y (height)"), L("Z (Server-Y)","Z (server Y)")};
                for (int axis = 0; axis < 3; ++axis) {
                    if (ImGui::BeginMenu(axisNames[axis])) {
                        if (ImGui::MenuItem(L("Minimum","Minimum"))) AlignSelectedObjects(state, axis, core::level::AlignTarget::Min);
                        if (ImGui::MenuItem(L("Mitte","Center"))) AlignSelectedObjects(state, axis, core::level::AlignTarget::Center);
                        if (ImGui::MenuItem(L("Maximum","Maximum"))) AlignSelectedObjects(state, axis, core::level::AlignTarget::Max);
                        if (ImGui::MenuItem(L("Aktives Objekt","Active object"))) AlignSelectedObjects(state, axis, core::level::AlignTarget::Active);
                        ImGui::EndMenu();
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(L("Verteilen","Distribute"), objectTools && selCount >= 3)) {
                if (ImGui::MenuItem(L("Entlang X (Ost)","Along X (east)"))) DistributeSelectedObjects(state, 0);
                if (ImGui::MenuItem(L("Entlang Z (Server-Y)","Along Z (server Y)"))) DistributeSelectedObjects(state, 2);
                if (ImGui::MenuItem(L("Entlang Y (Höhe)","Along Y (height)"))) DistributeSelectedObjects(state, 1);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(L("Vom aktiven Objekt übernehmen","Match active object"), objectTools && selCount >= 2)) {
                if (ImGui::MenuItem(L("Rotation","Rotation"))) MatchSelectedObjectsToActive(state, true, false);
                if (ImGui::MenuItem(L("Skalierung","Scale"))) MatchSelectedObjectsToActive(state, false, true);
                if (ImGui::MenuItem(L("Rotation + Skalierung","Rotation + scale"))) MatchSelectedObjectsToActive(state, true, true);
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("Terrain","Terrain"))) {
            if (ImGui::MenuItem(L("Terrain-Werkzeug öffnen","Open terrain tool"), nullptr, false, terrainToolsAvailable))
                selectMapTool(EditMode::Heightmap);
            ImGui::Separator();
            const bool terrainEnabled = terrainToolsAvailable && mapWorkspace && state.editMode==EditMode::Heightmap;
            if (ImGui::MenuItem(L("Anheben","Raise"), nullptr, state.brushMode==core::BrushMode::Raise, terrainEnabled))
                state.brushMode=core::BrushMode::Raise;
            if (ImGui::MenuItem(L("Absenken","Lower"), nullptr, state.brushMode==core::BrushMode::Lower, terrainEnabled))
                state.brushMode=core::BrushMode::Lower;
            if (ImGui::MenuItem(L("Glätten","Smooth"), nullptr, state.brushMode==core::BrushMode::Smooth, terrainEnabled))
                state.brushMode=core::BrushMode::Smooth;
            if (ImGui::MenuItem(L("Einebnen","Flatten"), nullptr, state.brushMode==core::BrushMode::Flatten, terrainEnabled))
                state.brushMode=core::BrushMode::Flatten;
            if (ImGui::MenuItem(L("Rauschen","Noise"), nullptr, state.brushMode==core::BrushMode::Noise, terrainEnabled))
                state.brushMode=core::BrushMode::Noise;
            if (ImGui::MenuItem(L("Terrassen","Terrace"), nullptr, state.brushMode==core::BrushMode::Terrace, terrainEnabled))
                state.brushMode=core::BrushMode::Terrace;
            if (ImGui::MenuItem(L("Erosion","Erode"), nullptr, state.brushMode==core::BrushMode::Erode, terrainEnabled))
                state.brushMode=core::BrushMode::Erode;
            if (ImGui::MenuItem(L("Schärfen","Sharpen"), nullptr, state.brushMode==core::BrushMode::Sharpen, terrainEnabled))
                state.brushMode=core::BrushMode::Sharpen;
            if (ImGui::MenuItem(L("Rampe","Ramp"), nullptr, state.brushMode==core::BrushMode::Ramp, terrainEnabled)) {
                state.brushMode=core::BrushMode::Ramp;
                state.rampStart.reset();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("Layer","Layer"))) {
            if (ImGui::MenuItem(L("Textur-/Layer-Werkzeug öffnen","Open texture/layer tool"), nullptr, false, terrainToolsAvailable))
                selectMapTool(EditMode::TexturePaint);
            ImGui::Separator();
            const int layerCount = static_cast<int>(state.textureStack.LayerCount());
            if (ImGui::MenuItem(L("Vorheriger Layer","Previous layer"), nullptr, false,
                                layerCount>0 && state.selectedLayer>0))
                --state.selectedLayer;
            if (ImGui::MenuItem(L("Nächster Layer","Next layer"), nullptr, false,
                                layerCount>0 && state.selectedLayer>=0 && state.selectedLayer+1<layerCount))
                ++state.selectedLayer;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("Werkzeuge","Tools"))) {
            const std::string paletteShortcut = ShortcutLabel(state.shortcutPalette);
            if (ImGui::MenuItem(L("Befehlspalette","Command palette"), paletteShortcut.c_str())) {
                state.commandPaletteOpen = true;
                state.commandPaletteSelection = 0;
            }
            if (ImGui::MenuItem(L("Einstellungen","Settings"))) state.settingsOpen = true;
            ImGui::Separator();
            if (ImGui::MenuItem(L("Kartenprüfung","Map check"), nullptr, false, mapWorkspace)) {
                state.mapCheckOpen = true;
                state.mapCheckRunRequest = true;
            }
            if (ImGui::MenuItem(state.playtestActive ? L("Spieltest beenden","Stop playtest")
                                                     : L("Spieltest starten","Start playtest"),
                                state.playtestActive ? "Esc" : "Alt+P", false, mapWorkspace))
                TogglePlaytest(state);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("Fenster","Window"))) {
            if (ImGui::MenuItem(L("Standardlayout wiederherstellen","Restore default layout")))
                RequestMapWorkspacePreset(state,0);
            if (ImGui::MenuItem(L("Projektübersicht","Project overview")))
                state.screen=AppScreen::ProjectHub;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("Hilfe","Help"))) {
            if (ImGui::MenuItem(L("Handbuch","Manual"), "F1")) state.manualOpen=true;
            ImGui::Separator();
            if (ImGui::MenuItem("Deutsch", nullptr, app::CurrentLanguage()==app::Language::German))
                app::SetLanguage(app::Language::German);
            if (ImGui::MenuItem("English", nullptr, app::CurrentLanguage()==app::Language::English))
                app::SetLanguage(app::Language::English);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    const ImVec2 brandPos = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(34.0f, 30.0f));
    topDl->AddRectFilled(ImVec2(brandPos.x, brandPos.y), ImVec2(brandPos.x + 34.0f, brandPos.y + 30.0f),
                         IM_COL32(5,37,67,255), 7.0f);
    topDl->AddRect(ImVec2(brandPos.x, brandPos.y), ImVec2(brandPos.x + 34.0f, brandPos.y + 30.0f),
                   IM_COL32(32,221,242,160), 7.0f, 0, 1.0f);
    if (const std::uint32_t brandTex = gUiIcons.Texture("brand.ng", 32); brandTex != 0) {
        topDl->AddImage(static_cast<ImTextureID>(static_cast<intptr_t>(brandTex)),
                        ImVec2(brandPos.x + 2.0f, brandPos.y),
                        ImVec2(brandPos.x + 32.0f, brandPos.y + 30.0f),
                        ImVec2(0,1), ImVec2(1,0));
    } else {
        // Functional fallback for development builds without copied assets.
        topDl->AddText(ImVec2(brandPos.x + 6.0f, brandPos.y + 7.0f),
                       IM_COL32(222,250,255,255), "NG");
    }

    ImGui::SameLine(0.0f, 9.0f);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("NextGen");
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::TextColored(UiTheme::AccentBlue, "Editor");
    if (breadcrumbTitle && breadcrumbTitle[0]) {
        ImGui::SameLine();
        ImGui::TextDisabled("· %s", breadcrumbTitle);
    }
    ImGui::SameLine(225.0f);

    struct PrimaryNav {
        const char* key;
        AppScreen target;
        IconDrawFn fallbackIcon;
        const char* semanticIcon;
        const char* tooltipDe;
        const char* tooltipEn;
    };
    const PrimaryNav primary[] = {
        {"nav.map",        AppScreen::MapEditorLauncher, DrawIconGlobe,       "nav.world",
         "Map Editor: Karten anlegen, öffnen und bearbeiten", "Map editor: create, open and edit maps"},
        {"nav.data",       AppScreen::ShnEditor,         DrawIconTable,       "module.shn.single",
         "Datei Editor: alle Spieldaten-Dateien (SHN, Interface, KFM)", "File editor: all game data files (SHN, interface, KFM)"},
        {"nav.animations", AppScreen::KfmBrowser,        DrawIconClapper,     "module.kfm",
         "KFM/KF Animationen", "KFM/KF animations"},
        {"nav.project",    AppScreen::ProjectHub,        DrawIconPencilPaper, "panel.project",
         "Projektübersicht", "Project overview"},
    };
    for (const auto& item : primary) {
        const bool active =
            (item.target == AppScreen::MapEditorLauncher &&
             (state.screen == AppScreen::MapEditorLauncher || state.screen == AppScreen::MapEditorWorkspace)) ||
            (item.target == AppScreen::ProjectHub &&
             (state.screen == AppScreen::ProjectHub || state.screen == AppScreen::NewProjectConfig)) ||
            state.screen == item.target;
        if (DrawCompactIconTextButton(item.key, T(item.key), item.fallbackIcon, item.semanticIcon,
                                      true, L(item.tooltipDe,item.tooltipEn), active)) {
            state.screen = item.target;
        }
        ImGui::SameLine();
    }

    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    if (DrawCompactIconTextButton("topNew", T("nav.new"), nullptr, "file.new", true,
                                  L("Neues Projekt / neue Karte konfigurieren","Configure new project / map"))) {
        createNewProject();
    }
    ImGui::SameLine();
    if (DrawCompactIconTextButton("topOpen", T("nav.open"), nullptr, "file.open", true,
                                  L("Projektordner öffnen","Open project folder"))) {
        openProjectFolder();
    }
    ImGui::SameLine();
    if (DrawCompactIconTextButton("topSave", T("nav.save"), DrawIconSave, "file.save",
                                  state.project.hasProject,
                                  L("Projektkonfiguration speichern","Save project configuration"))) {
        saveProjectConfig();
    }
    ImGui::SameLine();
    const std::string paletteShortcutLabel = ShortcutLabel(state.shortcutPalette);
    const std::string paletteTooltip = std::string(L("Befehlspalette öffnen","Open command palette")) +
                                       " (" + paletteShortcutLabel + ")";
    if (DrawTinyIconButton("paletteTop", DrawIconTable, false, paletteTooltip.c_str(),
                           ImVec2(22.0f,22.0f), "system.command_palette")) {
        state.commandPaletteOpen = true;
        state.commandPaletteSelection = 0;
    }

    const std::size_t dirtyShn = DirtyShnDocumentCount(state);
    if (state.mapDirty || dirtyShn > 0 || state.questDirty || state.townPortalDirty ||
        state.recallCoordDirty || state.aiScriptDirty || state.dropTableDirty) {
        ImGui::SameLine();
        ImGui::TextColored(UiTheme::Warning, "*");
        if (ImGui::IsItemHovered()) {
            std::string dirtyText = L("Ungespeichert","Unsaved changes");
            if (state.mapDirty) dirtyText += L("\n· Karte geändert","\n· Map modified");
            if (dirtyShn > 0)
                dirtyText += "\n· " + std::to_string(dirtyShn) +
                             L(" SHN-Datei(en) geändert"," SHN file(s) modified");
            if (state.questDirty) dirtyText += L("\n· QuestData.shn geändert","\n· QuestData.shn modified");
            if (state.townPortalDirty) dirtyText += L("\n· TownPortal.shn geändert","\n· TownPortal.shn modified");
            if (state.recallCoordDirty) dirtyText += L("\n· RecallCoord.txt geändert","\n· RecallCoord.txt modified");
            if (state.aiScriptDirty) dirtyText += L("\n· AI-Skript geändert","\n· AI script modified");
            if (state.dropTableDirty) dirtyText += L("\n· ItemDropTable geändert","\n· ItemDropTable modified");
            ImGui::SetTooltip("%s", dirtyText.c_str());
        }
    }

    const float rightWidth = 176.0f;
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - rightWidth);
    if (DrawTinyIconButton("settingsTop", DrawIconGear, state.settingsOpen, L("Einstellungen","Settings"),
                           ImVec2(22.0f,22.0f), "panel.settings"))
        state.settingsOpen = !state.settingsOpen;
    ImGui::SameLine();
    if (DrawTinyIconButton("manualTop", nullptr, state.manualOpen, L("Hilfe öffnen","Open help"),
                           ImVec2(22.0f,22.0f), "panel.help"))
        state.manualOpen = !state.manualOpen;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(64.0f);
    int langIdx = app::CurrentLanguage() == app::Language::German ? 0 : 1;
    const char* langItems[] = {"DE", "EN"};
    if (UI::Combo("##lang", &langIdx, langItems, 2))
        app::SetLanguage(langIdx == 0 ? app::Language::German : app::Language::English);

    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleColor(3);
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
}

} // namespace nge
