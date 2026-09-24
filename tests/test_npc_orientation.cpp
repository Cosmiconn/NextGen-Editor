// test_npc_orientation.cpp
// Verankert die per Block&Walk-Analyse ermittelte NPC-Blickrichtung (CHANGELOG [0.44.34]) als
// Regressionstest: baut ein winziges, synthetisches WalkGrid mit einer Wand auf einer Seite und
// prueft, dass die AKTUELLEN Standardwerte (state.npcDirSign/npcDirOffsetDeg) den NPC vom
// synthetischen Nachbarn weg und damit von der Wand weg blicken lassen - UND dass die reine
// Formel (NpcYawRadians -> Blickvektor) fuer explizite Vorzeichen/Versatz-Werte arithmetisch
// stimmt. Schlaegt fehl, wenn das Vorzeichen kuenftig versehentlich wieder umgedreht wird.
//
// main.cpp direkt einbinden (wie die Headless-Testharnesses waehrend der Entwicklung) - main.cpp
// braucht GLFW/ImGui/glad zum Kompilieren, ist aber dank g_disableGlUploads GL-frei lauffaehig.

#define main app_main_unused
#include "../src/app/main.cpp"
#undef main

#include <cstdio>

using namespace theseed::mapeditor;

namespace {
int g_failures = 0;
void Check(bool ok, const std::string& what) {
    if (ok) std::printf("[ok]     %s\n", what.c_str());
    else { std::fprintf(stderr, "[FEHLER] %s\n", what.c_str()); ++g_failures; }
}
} // namespace

int main() {
    g_disableGlUploads = true;

    std::printf("== Formel: NpcYawRadians + Blickvektor (fx=-sin, fz=-cos) ==\n");
    {
        EditorState st;
        // sign=+1, offset=0: Direct=0 -> Blick nach -Z (Suden im Editor-Bezugsrahmen).
        st.npcDirSign = 1;
        st.npcDirOffsetDeg = 0;
        float yaw = NpcYawRadians(st, 0);
        Check(std::fabs(-std::sin(yaw) - 0.0f) < 1e-4f && std::fabs(-std::cos(yaw) - (-1.0f)) < 1e-4f,
              "sign=+1,offset=0,Direct=0 -> Blick (0,-1)");
        // Direct=90 -> Blick nach -X.
        yaw = NpcYawRadians(st, 90);
        Check(std::fabs(-std::sin(yaw) - (-1.0f)) < 1e-4f && std::fabs(-std::cos(yaw) - 0.0f) < 1e-4f,
              "sign=+1,offset=0,Direct=90 -> Blick (-1,0)");
    }

    std::printf("\n== Regression: Standardwerte zeigen vom synthetischen Nachbarn (Wand) weg ==\n");
    {
        // Aufbau: eine 40x40-Zellen-Karte (250x250 Einheiten). Wand = blockierte Zellen fuer x<20,
        // offen fuer x>=20. NPC steht bei x=21 (knapp oestlich der Wand), Direct so gewaehlt, dass er
        // bei KORREKTEM Vorzeichen/Versatz nach Osten (weg von der Wand) blicken sollte: mit
        // sign=+1,offset=0 zeigt Direct=90 nach -X (Westen) und Direct=270 nach +X (Osten) - siehe
        // Formel-Test oben. Der NPC bekommt also Direct=270.
        EditorState st;
        st.walkGrid.Resize(4, 40); // 4 Woerter * 16 Zellen = 64 Spalten, 40 Zeilen - reicht fuer x=0..63
        for (std::uint32_t cz = 0; cz < 40; ++cz)
            for (std::uint32_t cx = 0; cx < 20; ++cx) st.walkGrid.SetCellBlocked(cx, cz, true);
        st.heightmap.Resize(10, 10); st.heightmap.SetBlockSize(25.0f, 25.0f);
        std::snprintf(st.legacySaveStem, sizeof(st.legacySaveStem), "TestMap");

        core::legacy::ShineTable table;
        table.name = "ShineNPC";
        for (const char* col : {"MobName", "Map", "Coord-X", "Coord-Y", "Direct", "NPCMenu", "Role", "RoleArg0"})
            table.columns.push_back({col, "STRING"});
        core::legacy::ShineRecord rec;
        rec.values = {"TestNpc", "TestMap", "131", "125", "270", "1", "Guard", "-"}; // x=21 Zellen*6.25=131.25
        table.records.push_back(rec);
        st.npcTextFile.tables.push_back(std::move(table));
        st.npcTextLoaded = true;

        // Standardwerte (unveraendert aus dem State-Konstruktor) verwenden - genau das soll die Regel verankern.
        Check(st.npcDirSign == 1 && st.npcDirOffsetDeg == 0, "Standardwerte sind sign=+1, offset=0 Grad");

        const float yaw = NpcYawRadians(st, 270);
        const float fx = -std::sin(yaw), fz = -std::cos(yaw);
        Check(fx > 0.9f, "Mit den Standardwerten blickt Direct=270 nach Osten (+X, von der Westwand weg)");

        core::PlacedObject obj;
        ApplyNpcRecordToObject(st, st.npcTextFile.tables[0].records[0], obj);
        const float half = std::atan2(obj.rotY, obj.rotW);
        Check(std::fabs(2.0f * half - yaw) < 1e-3f, "ApplyNpcRecordToObject: Quaternion entspricht NpcYawRadians(Direct)");

        // EstimateNpcOrientation auf genau diesem Szenario: muss auf ein sign/offset kommen, dessen
        // Blickvektor ebenfalls ueberwiegend nach Osten zeigt (perfekte Rekonstruktion ist bei nur 1
        // NPC und 10-Grad-Raster nicht garantiert, wohl aber "ungefaehr richtig").
        EstimateNpcOrientation(st);
        const float yaw2 = NpcYawRadians(st, 270);
        const float fx2 = -std::sin(yaw2);
        Check(fx2 > 0.5f, "EstimateNpcOrientation findet auf dem Testfall ebenfalls 'nach Osten' (fx=" + std::to_string(fx2) + ")");
    }

    std::printf("\n== Objekt-Transform: Mehrfachauswahl, Lock, Pivot ==\n");
    {
        EditorState st;
        core::PlacedObject a; a.modelPath="a.nif"; a.posX=0.0f; a.scale=1.0f;
        core::PlacedObject b; b.modelPath="b.nif"; b.posX=10.0f; b.scale=1.0f;
        st.placementSet.AddObject(a);
        st.placementSet.AddObject(b);
        st.selectedObjects={0,1};
        st.selectedObject=1;
        SyncObjectEditorMetadata(st);

        MoveSelectedObjectsBy(st,5.0f,2.0f,-3.0f);
        Check(std::fabs(st.placementSet.At(0).posX-5.0f)<1e-4f &&
              std::fabs(st.placementSet.At(1).posX-15.0f)<1e-4f,
              "Gruppenbewegung wendet dasselbe Delta auf alle ausgewählten Objekte an");

        st.objectEditorLocked[1]=1;
        MoveSelectedObjectsBy(st,5.0f,0.0f,0.0f);
        Check(std::fabs(st.placementSet.At(0).posX-10.0f)<1e-4f &&
              std::fabs(st.placementSet.At(1).posX-15.0f)<1e-4f,
              "Gesperrte Objekte werden bei Gruppenbewegung nicht verändert");
        st.objectEditorLocked[1]=0;

        const auto pivot=ComputeObjectSelectionPivot(st);
        Check(pivot.valid && std::fabs(pivot.position.x-12.5f)<1e-4f,
              "Gruppenpivot liegt im Mittelpunkt der Auswahl");

        const float half=3.14159265f*0.25f;
        const EditQuat quarterTurn{0.0f,std::sin(half),0.0f,std::cos(half)};
        RotateSelectedObjectsAroundPivot(st,pivot.position,quarterTurn);
        const float dx=st.placementSet.At(0).posX-st.placementSet.At(1).posX;
        const float dz=st.placementSet.At(0).posZ-st.placementSet.At(1).posZ;
        Check(std::fabs(dx)<1e-3f && std::fabs(std::fabs(dz)-5.0f)<1e-3f,
              "90-Grad-Gruppenrotation dreht Positionen um den gemeinsamen Pivot");

        const auto pivot2=ComputeObjectSelectionPivot(st);
        const float beforeDist=std::hypot(st.placementSet.At(0).posX-pivot2.position.x,
                                          st.placementSet.At(0).posZ-pivot2.position.z);
        ScaleSelectedObjectsAroundPivot(st,pivot2.position,2.0f);
        const float afterDist=std::hypot(st.placementSet.At(0).posX-pivot2.position.x,
                                         st.placementSet.At(0).posZ-pivot2.position.z);
        Check(std::fabs(afterDist-beforeDist*2.0f)<1e-3f &&
              std::fabs(st.placementSet.At(0).scale-2.0f)<1e-4f &&
              std::fabs(st.placementSet.At(1).scale-2.0f)<1e-4f,
              "Gruppenskalierung skaliert Abstand zum Pivot und Objektmaß gemeinsam");
    }

    std::printf("\n%d Fehler.\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
