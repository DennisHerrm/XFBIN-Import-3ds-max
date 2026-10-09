// ============================================================
//  XFBIN Particle - Implementierung. Siehe xfbin_particle.h.
// ============================================================

#include "xfbin_particle.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>

namespace xfbin {

namespace {

uint16_t U16(const std::vector<uint8_t>& b, size_t o) { return o + 2 <= b.size() ? static_cast<uint16_t>((b[o] << 8) | b[o + 1]) : 0; }
uint32_t U32(const std::vector<uint8_t>& b, size_t o) {
    return o + 4 <= b.size() ? (static_cast<uint32_t>(b[o]) << 24) | (static_cast<uint32_t>(b[o + 1]) << 16) |
                                   (static_cast<uint32_t>(b[o + 2]) << 8) | b[o + 3]
                             : 0;
}
int16_t I16(const std::vector<uint8_t>& b, size_t o) { return static_cast<int16_t>(U16(b, o)); }
float F32(const std::vector<uint8_t>& b, size_t o) {
    const uint32_t u = U32(b, o);
    float f;
    std::memcpy(&f, &u, 4);
    return std::isfinite(f) ? f : 0.0f;
}

} // namespace

RawString ResolveLocalName(const XfbinFile& file, size_t pageIndex, uint32_t localIndex, RawString* type) {
    if (pageIndex >= file.pages.size()) return RawString();
    const XfbinPage& pg = file.pages[pageIndex];
    if (localIndex >= pg.pageMapIndices.size()) return RawString();
    const uint32_t gi = pg.pageMapIndices[localIndex];
    if (gi >= file.table.maps.size()) return RawString();
    const ChunkMap& m = file.table.maps[gi];
    if (type != nullptr) *type = m.typeIndex < file.table.types.size() ? file.table.types[m.typeIndex] : RawString();
    return m.nameIndex < file.table.names.size() ? file.table.names[m.nameIndex] : RawString();
}

bool ParseParticles(const XfbinFile& file, std::vector<ParticleSystem>& out, std::string& warnings) {
    std::ostringstream warn;
    for (size_t pi = 0; pi < file.pages.size(); ++pi) {
        for (const XfbinChunk& c : file.pages[pi].chunks) {
            if (c.type == nullptr || *c.type != "nuccChunkParticle") continue;
            const std::vector<uint8_t>& b = c.data;
            if (b.size() < 40) continue;

            // Abschnittskoepfe: Anzahl und Gesamtgroesse
            uint16_t count[5], bytes[5];
            size_t start[5];
            size_t o = 40;
            for (int k = 0; k < 5; ++k) {
                count[k] = U16(b, static_cast<size_t>(k) * 8 + 4);
                bytes[k] = U16(b, static_cast<size_t>(k) * 8 + 6);
                start[k] = o;
                o += bytes[k];
            }
            // Die Zeitleisten haben keine feste Groesse; der Rest der
            // Chunk-Laenge muss genau aufgehen.
            if (o != b.size()) {
                warn << "nuccChunkParticle '" << (c.name ? *c.name : "") << "': Abschnitte passen nicht zur Laenge ("
                     << o << " / " << b.size() << ") - uebersprungen\n";
                continue;
            }
            auto groesse = [&](int k) -> size_t { return count[k] ? bytes[k] / count[k] : 0; };

            ParticleSystem sys;
            sys.name = c.name ? *c.name : RawString();

            // 0: Emitter
            for (uint16_t i = 0; i < count[0]; ++i) {
                const size_t e = start[0] + i * groesse(0);
                if (groesse(0) < 196) break;
                ParticleEmitter em;
                em.entry        = U32(b, e + 4);
                em.stretchLoop  = b[e + 16];
                em.attachType   = b[e + 17];
                em.attachToBone = b[e + 19];
                em.randomRoll   = b[e + 20];
                em.frameRate    = b[e + 21] ? b[e + 21] : 30;
                em.start        = b[e + 27];
                em.end          = I16(b, e + 28);
                em.count        = F32(b, e + 32);
                em.radiusStart  = F32(b, e + 36);
                em.radiusEnd    = F32(b, e + 40);
                em.life         = U16(b, e + 44);
                em.blowout      = F32(b, e + 52);
                for (int k = 0; k < 4; ++k) em.rotation[k] = F32(b, e + 56 + 4 * static_cast<size_t>(k));
                em.transparency = F32(b, e + 76);
                for (int s = 0; s < 4; ++s)
                    for (int k = 0; k < 3; ++k) em.scale[s][k] = F32(b, e + 92 + 12 * static_cast<size_t>(s) + 4 * static_cast<size_t>(k));
                em.scaleBlend = F32(b, e + 140);
                for (int s = 0; s < 3; ++s)
                    for (int k = 0; k < 4; ++k) em.color[s][k] = F32(b, e + 144 + 16 * static_cast<size_t>(s) + 4 * static_cast<size_t>(k));
                em.colorBlend = F32(b, e + 192);
                sys.emitters.push_back(em);
            }
            auto zuEintrag = [&](uint32_t entry) -> ParticleEmitter* {
                for (ParticleEmitter& em : sys.emitters) if (em.entry == entry) return &em;
                return nullptr;
            };

            // 1: Material
            for (uint16_t i = 0; i < count[1]; ++i) {
                const size_t e = start[1] + i * groesse(1);
                if (ParticleEmitter* em = zuEintrag(U32(b, e + 4))) {
                    em->materialName = ResolveLocalName(file, pi, U32(b, e));
                    em->materialKind = U32(b, e + 28);
                }
            }
            // 2: Andockknochen - der Chunk, der den Coord bewegt, steht
            // in den letzten acht Bytes (Eintraege sind 48 oder 56 lang)
            for (uint16_t i = 0; i < count[2]; ++i) {
                const size_t e = start[2] + i * groesse(2);
                if (groesse(2) < 16) break;
                if (ParticleEmitter* em = zuEintrag(U32(b, e + 4))) {
                    em->attachBone = ResolveLocalName(file, pi, U32(b, e));
                    em->attachOwner = ResolveLocalName(file, pi, U32(b, e + groesse(2) - 8));
                }
            }
            // 3: Kraefte
            for (uint16_t i = 0; i < count[3]; ++i) {
                const size_t e = start[3] + i * groesse(3);
                if (groesse(3) < 72) break;
                if (ParticleEmitter* em = zuEintrag(U32(b, e + 4))) {
                    ParticleForce f;
                    for (int k = 0; k < 3; ++k) f.direction[k] = F32(b, e + 16 + 4 * static_cast<size_t>(k));
                    f.kind = U32(b, e + 48);
                    f.duration = F32(b, e + 56);
                    f.strength = F32(b, e + 64);
                    em->forces.push_back(f);
                }
            }
            // 4: Zeitleisten, je Emitter in Reihenfolge
            size_t t = start[4];
            for (uint16_t i = 0; i < count[4] && t + 4 <= b.size(); ++i) {
                const uint32_t n = U32(b, t);
                t += 4;
                std::vector<std::pair<bool, int>> tl;
                for (uint32_t k = 0; k < n && t + 4 <= b.size(); ++k, t += 4) tl.emplace_back(U16(b, t) != 0, U16(b, t + 2));
                if (i < sys.emitters.size()) sys.emitters[i].timeline = std::move(tl);
            }
            out.push_back(std::move(sys));
        }
    }
    warnings += warn.str();
    return true;
}

bool ParseBillboards(const XfbinFile& file, std::vector<Billboard>& out, std::string& warnings) {
    (void)warnings;
    for (size_t pi = 0; pi < file.pages.size(); ++pi) {
        for (const XfbinChunk& c : file.pages[pi].chunks) {
            if (c.type == nullptr || *c.type != "nuccChunkBillboard") continue;
            const std::vector<uint8_t>& b = c.data;
            if (b.size() < 16) continue;
            Billboard bb;
            bb.name = c.name ? *c.name : RawString();
            bb.modelName = ResolveLocalName(file, pi, U32(b, 0));
            const uint32_t flags = U32(b, 4);
            bb.frames = std::max<int>(0, I16(b, 8));
            // Spuren je Frame, in dieser Reihenfolge, vorhanden wenn ihr
            // Zwei-Bit-Feld in den Flags gesetzt ist.
            static const struct { int shift; int floats; } kSpur[] = {
                { 1, 3 }, { 3, 3 }, { 5, 3 }, { 7, 1 }, { 9, 2 }, { 11, 2 },
            };
            size_t o = 16;
            float uvBreite = 0, uvHoehe = 0;
            for (const auto& s : kSpur) {
                if (((flags >> s.shift) & 3) == 0) continue;
                if (s.shift == 11 && bb.frames > 0 && o + 8 <= b.size()) {   // WH1: Groesse der UV-Zelle
                    uvBreite = F32(b, o);
                    uvHoehe  = F32(b, o + 4);
                }
                o += static_cast<size_t>(bb.frames) * static_cast<size_t>(s.floats) * 4;
            }
            if (uvBreite > 0.01f) bb.cols = std::clamp(static_cast<int>(std::lround(1.0f / uvBreite)), 1, 16);
            if (uvHoehe > 0.01f) bb.rows = std::clamp(static_cast<int>(std::lround(1.0f / uvHoehe)), 1, 16);
            out.push_back(std::move(bb));
        }
    }
    return true;
}

} // namespace xfbin
