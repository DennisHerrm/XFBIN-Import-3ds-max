// ============================================================
//  XFBIN ANM - Implementierung
//
//  Referenz: br_anm.py, br_nucc.py (BrNuccChunkAnm) und anm.py
//  aus xfbin_lib.
// ============================================================

#include "xfbin_anm.h"

#include <charconv>
#include <cstring>
#include <locale>
#include <map>
#include <ostream>
#include <sstream>

namespace xfbin {

namespace {

class AnmReader {
public:
    AnmReader(const std::vector<uint8_t>& d) : data_(d.data()), size_(d.size()) {}

    bool   failed() const { return failed_; }
    size_t pos()    const { return pos_; }

    void skip(size_t n) { if (Check(n)) pos_ += n; }

    // Auf 4 Bytes ausrichten - nach jeder Kurve faellig.
    void align4() {
        const size_t rest = pos_ % 4;
        if (rest) skip(4 - rest);
    }

    uint8_t u8() {
        if (!Check(1)) return 0;
        return data_[pos_++];
    }
    uint16_t u16() {
        if (!Check(2)) return 0;
        const uint16_t v = static_cast<uint16_t>(
            (static_cast<uint16_t>(data_[pos_]) << 8) |
             static_cast<uint16_t>(data_[pos_ + 1]));
        pos_ += 2;
        return v;
    }
    int16_t i16() { return static_cast<int16_t>(u16()); }
    uint32_t u32() {
        if (!Check(4)) return 0;
        const uint32_t v =
            (static_cast<uint32_t>(data_[pos_])     << 24) |
            (static_cast<uint32_t>(data_[pos_ + 1]) << 16) |
            (static_cast<uint32_t>(data_[pos_ + 2]) <<  8) |
             static_cast<uint32_t>(data_[pos_ + 3]);
        pos_ += 4;
        return v;
    }
    int32_t i32() { return static_cast<int32_t>(u32()); }
    float f32() {
        const uint32_t bits = u32();
        float f = 0.0f;
        std::memcpy(&f, &bits, sizeof(f));
        return f;
    }

private:
    bool Check(size_t n) {
        if (failed_) return false;
        if (pos_ + n > size_) { failed_ = true; return false; }
        return true;
    }
    const uint8_t* data_ = nullptr;
    size_t size_ = 0;
    size_t pos_  = 0;
    bool   failed_ = false;
};

std::string Escape(const RawString& s) {
    std::string out;
    static const char* kHex = "0123456789ABCDEF";
    for (unsigned char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c >= 0x20 && c < 0x7F) out.push_back(static_cast<char>(c));
        else { out += "\\x"; out.push_back(kHex[c >> 4]); out.push_back(kHex[c & 0x0F]); }
    }
    return out;
}

std::string Num(double v, int digits = 6) {
    if (v == 0.0) v = 0.0;
    char buf[64];
    const std::to_chars_result r =
        std::to_chars(buf, buf + sizeof(buf), v, std::chars_format::fixed, digits);
    std::string out = (r.ec == std::errc()) ? std::string(buf, r.ptr) : std::string("0");
    for (char& c : out) if (c == ',') c = '.';
    return out;
}

// Teiler der quantisierten Formate.
constexpr double kDivQuat    = 32768.0;   // 0x8000
constexpr double kDivScale   = 4096.0;    // 0x1000
constexpr double kDivColor   = 255.0;     // 0xFF

// ------------------------------------------------------------
//  Eine Kurve lesen.
//
//  Der Aufbau ist pro Format unterschiedlich; die Trennung
//  laeuft entlang zweier Fragen: bringt der Key seinen eigenen
//  Zeitwert mit, und wie sind die Werte kodiert.
// ------------------------------------------------------------
bool ReadCurve(AnmReader& br, AnmCurve& curve, uint32_t frameSize) {
    const int n = curve.keyframeCount;
    curve.keys.resize(static_cast<size_t>(n));

    auto implicitFrame = [&](int i) {
        return static_cast<int>(frameSize) * i;
    };

    switch (curve.curveFormat) {

    // --- feste Schrittweite, drei floats ---
    case kVector3Fixed:
    case kEulerXyzFixed:
    case kVector3Tbl:
    case kVector3TblNoInterp:
        for (int i = 0; i < n; ++i) {
            AnmKey& k = curve.keys[static_cast<size_t>(i)];
            k.frameRaw = implicitFrame(i);
            k.count = 3;
            for (int c = 0; c < 3; ++c) k.value[c] = br.f32();
        }
        break;

    // --- eigener Zeitwert (int32), drei floats ---
    case kVector3Linear:
    case kVector3Bezier:
        for (int i = 0; i < n; ++i) {
            AnmKey& k = curve.keys[static_cast<size_t>(i)];
            k.frameRaw = br.i32();
            k.count = 3;
            for (int c = 0; c < 3; ++c) k.value[c] = br.f32();
        }
        break;

    // --- Sonderfall: liest drei floats, deutet aber den ERSTEN
    //     als Zeitwert. So macht es die Python-Lib, und beim
    //     Abgleich muss dasselbe herauskommen.
    case kEulerInterpolate:
        for (int i = 0; i < n; ++i) {
            AnmKey& k = curve.keys[static_cast<size_t>(i)];
            const float a = br.f32();
            const float b = br.f32();
            const float c = br.f32();
            k.frameRaw = static_cast<int>(a);
            k.count = 2;
            k.value[0] = b;
            k.value[1] = c;
        }
        break;

    // --- eigener Zeitwert, Quaternion als vier floats ---
    case kQuaternionLinear:
        for (int i = 0; i < n; ++i) {
            AnmKey& k = curve.keys[static_cast<size_t>(i)];
            k.frameRaw = br.i32();
            k.count = 4;
            for (int c = 0; c < 4; ++c) k.value[c] = br.f32();
        }
        break;

    // --- feste Schrittweite, ein float ---
    case kFloatFixed:
    case kFloatTblNi:
    case kFloatTbl:
        for (int i = 0; i < n; ++i) {
            AnmKey& k = curve.keys[static_cast<size_t>(i)];
            k.frameRaw = implicitFrame(i);
            k.count = 1;
            k.value[0] = br.f32();
        }
        break;

    // --- Sonderfall wie kEulerInterpolate: liest Zeitwert und
    //     Wert, legt aber BEIDE als Wert ab und benutzt die
    //     implizite Zeit. Auch das ist so in der Python-Lib.
    case kFloatLinear:
        for (int i = 0; i < n; ++i) {
            AnmKey& k = curve.keys[static_cast<size_t>(i)];
            const int32_t t = br.i32();
            const float   v = br.f32();
            k.frameRaw = implicitFrame(i);
            k.count = 2;
            k.value[0] = static_cast<double>(t);
            k.value[1] = v;
        }
        break;

    // --- feste Schrittweite, ein int16 ---
    case kVector2Fixed:
        for (int i = 0; i < n; ++i) {
            AnmKey& k = curve.keys[static_cast<size_t>(i)];
            k.frameRaw = implicitFrame(i);
            k.count = 1;
            k.value[0] = br.i16();
        }
        break;

    // --- feste Schrittweite, drei int16, aber laufender Index
    //     statt frameSize-Schritt (so in der Python-Lib) ---
    case kVector2Linear:
        for (int i = 0; i < n; ++i) {
            AnmKey& k = curve.keys[static_cast<size_t>(i)];
            k.frameRaw = i;
            k.count = 3;
            for (int c = 0; c < 3; ++c) k.value[c] = br.i16();
        }
        break;

    // --- quantisiert: Deckkraft, ein int16 / 0x8000 ---
    case kOpacityI16Tbl:
    case kOpacityI16TblNoInterp:
        for (int i = 0; i < n; ++i) {
            AnmKey& k = curve.keys[static_cast<size_t>(i)];
            k.frameRaw = implicitFrame(i);
            k.count = 1;
            k.value[0] = br.i16() / kDivQuat;
        }
        break;

    // --- quantisiert: Skalierung, drei int16 / 0x1000 ---
    case kScaleI16Tbl:
        for (int i = 0; i < n; ++i) {
            AnmKey& k = curve.keys[static_cast<size_t>(i)];
            k.frameRaw = implicitFrame(i);
            k.count = 3;
            for (int c = 0; c < 3; ++c) k.value[c] = br.i16() / kDivScale;
        }
        break;

    // --- quantisiert: Quaternion, vier int16 / 0x8000 ---
    case kQuaternionI16Tbl:
    case kQuaternionI16TblNoInterp:
        for (int i = 0; i < n; ++i) {
            AnmKey& k = curve.keys[static_cast<size_t>(i)];
            k.frameRaw = implicitFrame(i);
            k.count = 4;
            for (int c = 0; c < 4; ++c) k.value[c] = br.i16() / kDivQuat;
        }
        break;

    // --- Quaternion als vier floats, feste Schrittweite ---
    case kQuaternionTbl:
        for (int i = 0; i < n; ++i) {
            AnmKey& k = curve.keys[static_cast<size_t>(i)];
            k.frameRaw = implicitFrame(i);
            k.count = 4;
            for (int c = 0; c < 4; ++c) k.value[c] = br.f32();
        }
        break;

    // --- Farbe: drei uint8 / 0xFF ---
    case kColorRgbTbl:
        for (int i = 0; i < n; ++i) {
            AnmKey& k = curve.keys[static_cast<size_t>(i)];
            k.frameRaw = implicitFrame(i);
            k.count = 3;
            for (int c = 0; c < 3; ++c) k.value[c] = br.u8() / kDivColor;
        }
        break;

    // --- int16-Zeitwert, drei floats, dann durch 0x8000 ---
    case kVector3I16Linear:
        for (int i = 0; i < n; ++i) {
            AnmKey& k = curve.keys[static_cast<size_t>(i)];
            k.frameRaw = br.i16();
            k.count = 3;
            for (int c = 0; c < 3; ++c) k.value[c] = br.f32() / kDivQuat;
        }
        break;

    default:
        // Unbekanntes Format: die Kurve wird als leer markiert.
        // Weiterlesen waere Raten - und wuerde den Rest des
        // Eintrags verschieben.
        curve.unsupported = true;
        curve.keys.clear();
        return false;
    }

    return !br.failed();
}

} // namespace

AnmBoneChannel BoneChannelOf(uint16_t curveIndex, uint16_t curveFormat) {
    int idx = curveIndex;

    // Die beiden Euler-Formate verschieben den Index um 10 -
    // dadurch wird aus der Rotation (1) die Euler-Rotation (11).
    if (curveFormat == kEulerXyzFixed || curveFormat == kEulerInterpolate) {
        idx += 10;
    }

    switch (idx) {
    case 0:  return kChannelLocation;
    case 1:  return kChannelRotationQuat;
    case 2:  return kChannelScale;
    case 3:  return kChannelOpacity;
    case 11: return kChannelRotationEuler;
    default: return kChannelUnknown;
    }
}

AnmMaterialChannel MaterialChannelOf(uint16_t curveIndex) {
    if (curveIndex <= 21) return static_cast<AnmMaterialChannel>(curveIndex);
    return kMatUnknown;
}

size_t Anm::KeyframeCount() const {
    size_t n = 0;
    for (const AnmEntry& e : entries)
        for (const AnmCurve& c : e.curves) n += c.keys.size();
    return n;
}

bool Anm::HasBoneEntries() const {
    for (const AnmEntry& e : entries) {
        if (e.entryFormat == kEntryBone) return true;
    }
    return false;
}

// ============================================================
//  Parsen
// ============================================================

bool ParseAnims(const XfbinFile& file, std::vector<Anm>& out,
                std::string& error, std::string& warnings) {
    out.clear();
    error.clear();
    warnings.clear();

    std::ostringstream warn;
    bool allOk = true;

    const ChunkTable& tbl = file.table;

    // Namen einer Referenz aufloesen.
    //
    // Eine Referenz besteht aus zwei Indizes: einem in die
    // Namenstabelle (so heisst die Referenz) und einem in die
    // Chunk-Map (dorthin zeigt sie). Fuer die Zuordnung zu einem
    // Bone brauchen wir den Namen des Ziel-Chunks.
    auto refTargetName = [&](const XfbinPage& page, uint32_t refIndex) -> RawString {
        if (refIndex < page.pageReferences.size()) {
            const uint32_t mapIdx = page.pageReferences[refIndex].mapIndex;
            if (mapIdx < tbl.maps.size()) {
                const uint32_t nameIdx = tbl.maps[mapIdx].nameIndex;
                if (nameIdx < tbl.names.size()) return tbl.names[nameIdx];
            }
            return RawString();
        }

        // Rueckfallebene ohne Referenztabelle: der Index zeigt
        // direkt in die Chunkliste der Page. Genau diesen Weg
        // nimmt auch die Python-Lib, wenn chunk_refs zu kurz ist.
        if (refIndex < page.pageMapIndices.size()) {
            const uint32_t mapIdx = page.pageMapIndices[refIndex];
            if (mapIdx < tbl.maps.size()) {
                const uint32_t nameIdx = tbl.maps[mapIdx].nameIndex;
                if (nameIdx < tbl.names.size()) return tbl.names[nameIdx];
            }
        }
        return RawString();
    };

    for (const XfbinPage& page : file.pages) {
        for (const XfbinChunk& chunk : page.chunks) {
            if (!chunk.type || *chunk.type != "nuccChunkAnm") continue;

            Anm anm;
            anm.name    = chunk.name ? *chunk.name : RawString();
            anm.version = chunk.version;

            AnmReader br(chunk.data);

            anm.frameCountRaw = br.u32();
            anm.frameSize     = (chunk.version > 101) ? br.u32() : 100u;
            if (anm.frameSize == 0) anm.frameSize = 100;
            anm.frameCount = static_cast<double>(anm.frameCountRaw) /
                             static_cast<double>(anm.frameSize);

            const uint16_t entryCount = br.u16();
            anm.loopFlag              = br.u16();
            const uint16_t clumpCount = br.u16();
            anm.otherEntryCount       = br.u16();
            anm.otherIndexCount       = br.u16();
            const uint16_t coordCount = br.u16();

            if (br.failed()) {
                warn << "Anm '" << Escape(anm.name) << "': Kopf abgeschnitten.\n";
                allOk = false;
                continue;
            }

            anm.clumps.resize(clumpCount);
            for (uint16_t c = 0; c < clumpCount && !br.failed(); ++c) {
                AnmClumpRef& cr = anm.clumps[c];
                cr.clumpIndex = br.u32();
                const uint16_t boneCount  = br.u16();
                const uint16_t modelCount = br.u16();

                cr.clumpName = refTargetName(page, cr.clumpIndex);

                cr.boneNames.reserve(boneCount);
                for (uint16_t b = 0; b < boneCount && !br.failed(); ++b) {
                    cr.boneNames.push_back(refTargetName(page, br.u32()));
                }
                cr.modelNames.reserve(modelCount);
                for (uint16_t m = 0; m < modelCount && !br.failed(); ++m) {
                    cr.modelNames.push_back(refTargetName(page, br.u32()));
                }
            }

            // Chunkindizes fuer Kamera, Licht und Ambient.
            std::vector<uint32_t> otherIndices;
            const uint32_t otherTotal =
                static_cast<uint32_t>(anm.otherEntryCount) + anm.otherIndexCount;
            for (uint32_t i = 0; i < otherTotal && !br.failed(); ++i) {
                otherIndices.push_back(br.u32());
            }

            anm.coordParents.resize(coordCount);
            for (uint16_t i = 0; i < coordCount && !br.failed(); ++i) {
                AnmCoordParent& cp = anm.coordParents[i];
                cp.parentClump = br.i16();
                cp.parentCoord = br.u16();
                cp.childClump  = br.i16();
                cp.childCoord  = br.u16();
            }

            if (br.failed()) {
                warn << "Anm '" << Escape(anm.name) << "': Tabellen abgeschnitten.\n";
                allOk = false;
                out.push_back(std::move(anm));
                continue;
            }

            anm.entries.resize(entryCount);
            bool broke = false;

            for (uint16_t e = 0; e < entryCount && !broke; ++e) {
                AnmEntry& entry = anm.entries[e];

                entry.clumpIndex  = br.i16();
                entry.boneIndex   = br.u16();
                entry.entryFormat = br.u16();
                const uint16_t curveCount = br.u16();

                if (br.failed()) { broke = true; break; }

                entry.curves.resize(curveCount);
                for (uint16_t c = 0; c < curveCount; ++c) {
                    AnmCurve& cur = entry.curves[c];
                    cur.curveIndex    = br.u16();
                    cur.curveFormat   = br.u16();
                    cur.keyframeCount = br.u16();
                    cur.curveFlags    = br.i16();
                }

                if (br.failed()) { broke = true; break; }

                for (uint16_t c = 0; c < curveCount; ++c) {
                    AnmCurve& cur = entry.curves[c];
                    if (!ReadCurve(br, cur, anm.frameSize)) {
                        if (cur.unsupported) {
                            warn << "Anm '" << Escape(anm.name)
                                 << "': Kurvenformat " << cur.curveFormat
                                 << " wird nicht unterstuetzt - ab hier ist der "
                                    "Eintrag nicht mehr lesbar.\n";
                            allOk = false;
                            broke = true;
                            break;
                        }
                        broke = true;
                        break;
                    }
                    // Nach JEDER Kurve ausrichten.
                    br.align4();
                }

                if (broke) break;

                // Zeitwerte in Frames umrechnen.
                for (AnmCurve& cur : entry.curves) {
                    for (AnmKey& k : cur.keys) {
                        k.frame = static_cast<double>(k.frameRaw) /
                                  static_cast<double>(anm.frameSize);
                    }
                }

                // Ziel benennen.
                if (entry.clumpIndex >= 0 &&
                    static_cast<size_t>(entry.clumpIndex) < anm.clumps.size()) {
                    const AnmClumpRef& cr =
                        anm.clumps[static_cast<size_t>(entry.clumpIndex)];
                    entry.clumpName = cr.clumpName;
                    if (entry.boneIndex < cr.boneNames.size()) {
                        entry.targetName = cr.boneNames[entry.boneIndex];
                    }
                } else if (entry.boneIndex < otherIndices.size()) {
                    // Andere Eintraege - Kamera, Licht, Ambient - loesen
                    // sich NICHT ueber die Referenztabelle auf, sondern
                    // ueber die Indexliste der Page: der Wert in
                    // otherIndices ist eine Position in dieser Liste,
                    // und erst der Eintrag dort ist ein Chunk-Map-Index.
                    //
                    // Ueber die Referenztabelle gelesen kommen hier
                    // Bone-Namen heraus statt "camera001" oder
                    // "direct01" - der Eintrag landet dann am falschen
                    // Ziel.
                    const uint32_t slot = otherIndices[entry.boneIndex];
                    if (slot < page.pageMapIndices.size()) {
                        const uint32_t mapIdx = page.pageMapIndices[slot];
                        if (mapIdx < tbl.maps.size()) {
                            const uint32_t nameIdx = tbl.maps[mapIdx].nameIndex;
                            if (nameIdx < tbl.names.size()) {
                                entry.targetName = tbl.names[nameIdx];
                            }
                        }
                    }
                }
            }

            if (broke) {
                warn << "Anm '" << Escape(anm.name)
                     << "': Eintraege abgeschnitten.\n";
                allOk = false;
            }

            out.push_back(std::move(anm));
        }
    }

    warnings = warn.str();

    if (out.empty()) {
        error = "Kein nuccChunkAnm in der Datei.";
        return false;
    }
    return allOk;
}

// ============================================================
//  Dump
// ============================================================

void WriteAnimDump(const std::vector<Anm>& anims, const std::string& label,
                   std::ostream& o, bool includeKeys) {
    o.imbue(std::locale::classic());

    o << "# XFBIN anim dump v1\n";
    o << "file " << label << "\n";
    o << "anims " << anims.size() << "\n";

    for (size_t ai = 0; ai < anims.size(); ++ai) {
        const Anm& a = anims[ai];

        o << "anim[" << ai << "] name=" << Escape(a.name)
          << " frames="    << Num(a.frameCount, 2)
          << " frameSize=" << a.frameSize
          << " loop="      << a.loopFlag
          << " clumps="    << a.clumps.size()
          << " coords="    << a.coordParents.size()
          << " entries="   << a.entries.size()
          << " other="     << a.otherEntryCount << "/" << a.otherIndexCount
          << "\n";

        for (size_t ci = 0; ci < a.clumps.size(); ++ci) {
            const AnmClumpRef& cr = a.clumps[ci];
            o << "clumpref[" << ai << "." << ci << "] name=" << Escape(cr.clumpName)
              << " bones="  << cr.boneNames.size()
              << " models=" << cr.modelNames.size() << "\n";
        }

        for (size_t ei = 0; ei < a.entries.size(); ++ei) {
            const AnmEntry& e = a.entries[ei];

            o << "entry[" << ai << "." << ei << "]"
              << " clump="  << e.clumpIndex
              << " bone="   << e.boneIndex
              << " format=" << e.entryFormat
              << " curves=" << e.curves.size()
              << " target=" << Escape(e.targetName)
              << "\n";

            for (size_t ki = 0; ki < e.curves.size(); ++ki) {
                const AnmCurve& c = e.curves[ki];

                o << "curve[" << ai << "." << ei << "." << ki << "]"
                  << " idx="   << c.curveIndex
                  << " fmt="   << c.curveFormat
                  << " keys="  << c.keyframeCount
                  << " flags=" << c.curveFlags
                  << " ch="    << static_cast<int>(
                         BoneChannelOf(c.curveIndex, c.curveFormat))
                  << "\n";

                if (!includeKeys) continue;

                for (const AnmKey& k : c.keys) {
                    o << "  k " << Num(k.frame, 4);
                    for (int v = 0; v < k.count; ++v) {
                        o << " " << Num(k.value[v]);
                    }
                    o << "\n";
                }
            }
        }
    }

    o << "end\n";
}

std::string MakeAnimSummary(const std::vector<Anm>& anims) {
    std::ostringstream s;
    s.imbue(std::locale::classic());

    size_t entries = 0, curves = 0, keys = 0;
    std::map<int, size_t> formats;

    for (const Anm& a : anims) {
        entries += a.entries.size();
        for (const AnmEntry& e : a.entries) {
            curves += e.curves.size();
            for (const AnmCurve& c : e.curves) {
                keys += c.keys.size();
                ++formats[c.curveFormat];
            }
        }
    }

    s << "Animationen: " << anims.size()
      << " | Entries: " << entries
      << " | Kurven: " << curves
      << " | Keyframes: " << keys << "\n";

    s << "  Kurvenformate:";
    for (const auto& kv : formats) s << " " << kv.first << "=" << kv.second;
    s << "\n";

    return s.str();
}

} // namespace xfbin
