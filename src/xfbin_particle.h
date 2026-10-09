// ============================================================
//  XFBIN Particle - nuccChunkParticle und nuccChunkBillboard
//
//  Das Effektsystem der Storm-Spiele. Zu einer Animation gehoert
//  ein gleichnamiger nuccChunkParticle; jeder seiner Emitter
//  startet zu bestimmten Zeiten etwas an einem Andockknochen:
//
//    Material-Art 2  eine EFFEKT-ANIMATION (das Rasengan selbst:
//                    1nrtskl1_atk1 startet 1efb_rsn_demo01)
//    Material-Art 1  einen Clump als Mesh-Partikel (Ringe)
//    Material-Art 5  ein Billboard: Sprite mit Bildfolge (Rauch)
//
//  Effekt-Animationen haben selbst wieder Partikel - das
//  Rasengan startet Ringe, Rauch und weitere Animationen.
//
//  Aufbau nach der 010-Editor-Vorlage nuccChunkParticle.bt aus
//  github.com/Al-Hydra/Storm-Engine-NUCC (MIT), an 1.600 echten
//  Chunks aus Storm 4 nachgeprueft:
//
//    5 Abschnittskoepfe { u32 ?, u16 Anzahl, u16 Bytes gesamt }
//    dann die Abschnitte lueckenlos hintereinander:
//      0  Emitter (208 Bytes je Eintrag)
//      1  Material: Verweis + Art (1 Clump, 2 Anm, 3/4 Textur,
//         5 Billboard)
//      2  Andockknochen: Coord + der Chunk, der ihn bewegt
//      3  Kraefte (Schwerkraft, Wind, Wirbel - nur teilweise
//         verstanden)
//      4  Zeitleisten: je Emitter (an/aus, Zeit in 1/100 Frame)
//  Emitter, Material, Knochen und Kraefte gehoeren ueber die
//  gemeinsame Eintragsnummer zusammen.
//
//  Ohne Bindung an das 3ds Max SDK, wie die anderen Parser.
// ============================================================

#pragma once

#include "xfbin_reader.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace xfbin {

enum ParticleMaterialKind : uint32_t {
    kParticleClump     = 1,
    kParticleAnm       = 2,
    kParticleTexture   = 3,
    kParticleTexture2  = 4,
    kParticleBillboard = 5,
};

struct ParticleForce {
    float    direction[3] = { 0, 0, 0 };
    uint32_t kind = 0;
    float    duration = 0;      // 1/100 Frame
    float    strength = 0;
};

struct ParticleEmitter {
    uint32_t entry = 0;

    // Zuordnung
    uint32_t  materialKind = 0;   // ParticleMaterialKind
    RawString materialName;       // Clump / Anm / Billboard / Textur
    RawString attachBone;         // Coord-Name
    RawString attachOwner;        // Anm oder Clump, der den Coord bewegt

    // Verhalten (Bedeutung teils aus der Vorlage, teils geschaetzt)
    uint8_t stretchLoop  = 0;     // 1 = alle Partikel auf einmal
    uint8_t attachType   = 0;
    uint8_t attachToBone = 0;     // 1 = Partikel folgen dem Knochen
    uint8_t randomRoll   = 0;
    uint8_t frameRate    = 30;
    int     start = 0;
    int     end   = -1;           // -1 = bis zum Ende
    float   count = 0;            // Partikel je Ausstoss
    float   radiusStart = 0, radiusEnd = 0;   // Spawnradius, cm
    int     life = 0;             // Lebensdauer in Frames ("FadeTime")
    float   blowout = 0;          // Ausstossgeschwindigkeit
    float   rotation[4] = { 0, 0, 0, 1 };
    float   transparency = 0;
    float   scale[4][3] = {};     // Anfang, Mitte, Ende, Grundgroesse
    float   scaleBlend = 0;
    float   color[3][4] = {};     // RGBA Anfang, Mitte, Ende
    float   colorBlend = 0;

    std::vector<ParticleForce> forces;

    // (an, Zeit in 1/100 Frame) - aus Zeitleiste 4
    std::vector<std::pair<bool, int>> timeline;
};

struct ParticleSystem {
    RawString name;               // = Name der Animation
    std::vector<ParticleEmitter> emitters;
};

// Alle nuccChunkParticle einer Datei. warnings bekommt Chunks,
// deren Abschnittsgroessen nicht zur Chunk-Laenge passen.
bool ParseParticles(const XfbinFile& file, std::vector<ParticleSystem>& out, std::string& warnings);

// Billboard: Sprite mit Bildfolge.
struct Billboard {
    RawString name;
    RawString modelName;          // das Rechteck, auf dem das Sprite liegt
    int frames = 0;
    int cols = 1, rows = 1;       // Raster der Bildfolge (aus der UV-Zellgroesse)
};
bool ParseBillboards(const XfbinFile& file, std::vector<Billboard>& out, std::string& warnings);

// Name eines Chunks ueber seinen page-lokalen Index.
RawString ResolveLocalName(const XfbinFile& file, size_t pageIndex, uint32_t localIndex, RawString* type = nullptr);

} // namespace xfbin
