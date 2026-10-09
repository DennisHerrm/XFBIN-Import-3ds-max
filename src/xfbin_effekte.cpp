// ============================================================
//  xfbin_effekte.cpp - das Effektsystem der Storm-Spiele in Max
//
//  Siehe xfbin_particle.h fuer das Format. Hier wird es benutzt:
//
//  BEIM IMPORT
//    1. Partikel der eigenen Animationen lesen. Jeder Emitter
//       startet eine Effekt-Animation, einen Clump oder ein
//       Billboard an einem Andockknochen.
//    2. Gestartete Effekt-Animationen aus data/effect holen
//       (1efb_rsn_demo01 steht in 1efb_rsn.xfbin) - samt ihren
//       eigenen Partikeln, bis vier Ebenen tief.
//    3. Nach dem Bauen: die Clumps einer gestarteten Animation an
//       den Andockknochen haengen, Sprite- und Mesh-Schwaerme als
//       WhiteoutDex "Particle 2" anlegen (wenn WhiteoutDex da ist).
//
//  BEIM SETZEN EINER ANIMATION
//    4. Jede gestartete Effekt-Animation zeitversetzt mitsetzen,
//       Emissionsraten nach den Zeitleisten schalten.
//    5. Am Ende die Sichtbarkeit aller Effekte in einem Durchlauf:
//       sichtbar, wenn die Figurenanimation den Effekt selbst
//       bewegt ODER ein Emitter ihn gerade gestartet hat. Step-Keys
//       mit einem Key an jedem Sequenzrand, so wie WhiteoutDex sie
//       liest. Im WC3-Modus zusaetzlich aus, wo das Spiel den
//       Effekt auf fast 0 schrumpft.
// ============================================================
#include "xfbinimport.h"
#include "xfbin_app.h"
#include "xfbin_particle.h"
#include "xfbin_nud.h"
#include "xfbin_tex.h"

#include <iparamb2.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace xfi {

namespace {

const Class_ID kParticle2(0xD9F33BC9, 0x7A0DA37A);
constexpr ULONG kP2Pfad = 0x7B3C8D10, kP2Praefix = 0x7B3C8D11;
enum : ParamID {
    P2_COUNT = 0, P2_SPEED = 1, P2_VARIATION = 2, P2_LIFE = 3, P2_WIDTH = 4, P2_HEIGHT = 5, P2_RATE = 6, P2_CONE = 7,
    P2_MIDTIME = 8, P2_COLOR_START = 9, P2_COLOR_MID = 10, P2_COLOR_END = 11, P2_ALPHA_START = 12, P2_ALPHA_MID = 13,
    P2_ALPHA_END = 14, P2_SCALE_START = 15, P2_SCALE_MID = 16, P2_SCALE_END = 17, P2_HEAD_LIFE_START = 18,
    P2_HEAD_LIFE_REPEAT = 19, P2_HEAD_LIFE_END = 20, P2_TYPE = 25, P2_ROWS = 26, P2_COLS = 27, P2_SQUIRT = 34,
    P2_BLEND = 35, P2_GRAVITY = 36, P2_UNSHADED = 39, P2_MODELSPACE = 43, P2_XYQUAD = 44
};

// Was ein Sprite braucht: Texturdatei, Groesse, Raster.
struct Sprite {
    std::wstring textur;   // DDS im Texturordner
    float groesse = 10.0f; // Durchmesser des Modells, cm
    int rows = 1, cols = 1;
};

std::map<std::string, xfbin::ParticleSystem> g_systeme;   // Name der Animation -> Partikel
std::map<std::string, Sprite>                g_sprites;   // Billboard/Clump -> Sprite
std::map<std::string, ULONG>                 g_p2;        // "<system>#<eintrag>" -> Particle-2-Knoten
std::vector<std::string>                     g_spriteDateien;

std::string Schluessel(const std::string& sys, uint32_t entry) { return sys + "#" + std::to_string(entry); }

bool Particle2Da() {
    static int da = -1;
    if (da < 0) {
        ClassDirectory& cd = GetCOREInterface()->GetDllDir().ClassDir();
        da = cd.FindClass(GEOMOBJECT_CLASS_ID, kParticle2) != nullptr ? 1 : 0;
    }
    return da == 1;
}

std::string SicherDatei(const std::string& s) {
    std::string out;
    for (unsigned char c : s)
        out.push_back((c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|' || c < 0x20 || c >= 0x7F)
                          ? '_' : static_cast<char>(c));
    return out.empty() ? std::string("unbenannt") : out;
}

std::string OhneEndung(const std::string& n) {
    const size_t p = n.rfind('.');
    return p == std::string::npos ? n : n.substr(0, p);
}

// Sprites einer Datei: Billboards ueber ihr Modell, Clumps ueber
// ihr erstes Modell - jeweils das erste Material und seine erste
// Textur.
void SammleSprites(const xfbin::XfbinFile& file, const std::set<std::string>& gesucht, const std::wstring& texOrdner) {
    std::vector<xfbin::NudModel> modelle;
    std::vector<xfbin::XfbinMaterial> materialien;
    std::vector<xfbin::Billboard> billboards;
    std::string e, w;
    xfbin::ParseModels(file, modelle, e, w);
    xfbin::ParseMaterials(file, materialien, e, w);
    xfbin::ParseBillboards(file, billboards, w);
    auto spriteFuer = [&](const xfbin::NudModel& m) {
        Sprite s;
        s.groesse = std::max(1.0f, 2.0f * m.boundingSphere[3]);
        const std::vector<xfbin::RawString> mats = xfbin::ResolveModelMaterials(file, m.pageIndex, m.materialIndices);
        for (const xfbin::RawString& mn : mats) {
            for (const xfbin::XfbinMaterial& mt : materialien) {
                if (mt.name != mn || mt.groups.empty() || mt.groups[0].textureNames.empty()) continue;
                s.textur = texOrdner + L"\\" + Breit(SicherDatei(mt.groups[0].textureNames[0])) + L".dds";
                return s;
            }
        }
        return s;
    };
    for (const xfbin::Billboard& bb : billboards) {
        if (!gesucht.count(bb.name) || g_sprites.count(bb.name)) continue;
        for (const xfbin::NudModel& m : modelle) {
            if (m.name != bb.modelName) continue;
            Sprite s = spriteFuer(m);
            s.rows = bb.rows;
            s.cols = bb.cols;
            g_sprites[bb.name] = s;
            break;
        }
    }
    for (const xfbin::NudModel& m : modelle) {
        const xfbin::RawString clump = xfbin::ResolveLocalName(file, m.pageIndex, m.clumpIndex);
        if (!gesucht.count(clump) || g_sprites.count(clump)) continue;
        g_sprites[clump] = spriteFuer(m);
    }
}

// Zeitfenster eines Emitters in Frames, begrenzt auf [start, ende].
std::vector<std::pair<double, double>> Fenster(const xfbin::ParticleEmitter& em, double start, double ende) {
    std::vector<std::pair<double, double>> f;
    if (em.timeline.empty()) {
        f.emplace_back(start, ende);
    } else {
        double an = -1.0;
        for (const auto& z : em.timeline) {
            const double t = start + z.second / 100.0;
            if (z.first) { if (an < 0.0) an = t; }
            else if (an >= 0.0) { f.emplace_back(an, t); an = -1.0; }
        }
        if (an >= 0.0) f.emplace_back(an, ende);
    }
    std::vector<std::pair<double, double>> aus;
    for (auto w : f) {
        w.first = std::max(w.first, start);
        w.second = std::min(w.second, ende);
        if (w.second > w.first) aus.push_back(w);
    }
    return aus;
}

std::set<std::string> ClumpsDer(const xfbin::Anm& a) {
    std::set<std::string> s;
    for (const xfbin::AnmClumpRef& c : a.clumps) s.insert(c.clumpName);
    return s;
}

} // namespace

// ============================================================
//  Import
// ============================================================
void EffekteZuruecksetzen() {
    g_systeme.clear();
    g_sprites.clear();
    g_p2.clear();
    g_spriteDateien.clear();
}

void EffekteVorbereiten(XfbinImportInterface& x, const xfbin::Spieldaten& d, const std::vector<std::string>& animDateien,
                        EffektVorbereitung& v) {
    std::set<std::string> offen, geladen, eigene;
    std::string w;
    auto sammle = [&](const xfbin::XfbinFile& file, const std::set<std::string>* nur) {
        std::vector<xfbin::ParticleSystem> ps;
        xfbin::ParseParticles(file, ps, w);
        for (xfbin::ParticleSystem& s : ps) {
            if (nur != nullptr && !nur->count(s.name)) continue;
            for (const xfbin::ParticleEmitter& em : s.emitters) {
                ++v.emitter;
                if (em.materialKind == xfbin::kParticleAnm && !geladen.count(em.materialName)) offen.insert(em.materialName);
                if (em.materialKind == xfbin::kParticleClump || em.materialKind == xfbin::kParticleBillboard) v.sprites.insert(em.materialName);
                if (em.materialKind == xfbin::kParticleClump && em.count <= 1.0f) v.clumps.insert(em.materialName);
            }
            g_systeme[s.name] = std::move(s);
        }
    };

    // 1. eigene Animationsdateien
    for (const std::string& p : animDateien) {
        std::vector<uint8_t> b;
        std::string f;
        xfbin::XfbinFile file;
        if (!d.Lies(p, b, f) || !xfbin::ReadXfbinBuffer(b.data(), b.size(), file).ok) continue;
        for (const xfbin::Anm& a : x.Anims()) eigene.insert(a.name);
        sammle(file, &eigene);
    }

    // 2. gestartete Effekt-Animationen, bis vier Ebenen tief
    std::vector<std::string> effektDateien;
    for (const std::string& p : d.Pfade())
        if (p.compare(0, 12, "data/effect/") == 0 && p.find('/', 12) == std::string::npos) effektDateien.push_back(p);
    auto dateiFuer = [&](const std::string& anim) -> std::string {
        // die laengste Datei, deren Name der Anfang des Animationsnamens ist
        std::string best;
        for (const std::string& p : effektDateien) {
            const std::string n = OhneEndung(xfbin::DateiName(p));
            if (n.size() > OhneEndung(xfbin::DateiName(best)).size() && anim.compare(0, n.size(), n) == 0 &&
                (anim.size() == n.size() || anim[n.size()] == '_'))
                best = p;
        }
        if (best.empty() && anim.size() > 4 && anim.compare(1, 3, "efc") == 0) {
            const std::string cmn = "data/effect/" + anim.substr(0, 1) + "efcmn.xfbin";
            if (d.Gibt(cmn)) best = cmn;
            else if (d.Gibt("data/effect/1efcmn.xfbin")) best = "data/effect/1efcmn.xfbin";
        }
        return best;
    };

    for (int ebene = 0; ebene < 4 && !offen.empty(); ++ebene) {
        std::map<std::string, std::set<std::string>> jeDatei;
        for (const std::string& a : offen) {
            const std::string p = dateiFuer(a);
            if (!p.empty()) jeDatei[p].insert(a);
            geladen.insert(a);
        }
        offen.clear();
        for (const auto& kv : jeDatei) {
            std::vector<uint8_t> b;
            std::string f;
            xfbin::XfbinFile file;
            if (!d.Lies(kv.first, b, f) || !xfbin::ReadXfbinBuffer(b.data(), b.size(), file).ok) continue;
            if (x.OpenBuffer(b, Breit(kv.first)) < 0) continue;
            const size_t von = x.Anims().size();
            x.ParseAnimsAppend();
            const size_t n = x.KeepAnimsNamed(von, kv.second);
            x.TagAnims(von, "#fx");
            v.animsFx += n;
            for (const std::string& c : x.AnimClumpNames(von)) v.clumps.insert(c);
            if (std::find(v.dateien.begin(), v.dateien.end(), kv.first) == v.dateien.end()) v.dateien.push_back(kv.first);
            sammle(file, &kv.second);
            Log("  Effektdatei %s: %zu Effekt-Animationen", kv.first.c_str(), n);
        }
    }
    // Clumps und Billboards der Partikel stehen in denselben
    // Effektdateien oder in der gemeinsamen.
    for (const char* cmn : { "data/effect/1efcmn.xfbin" })
        if (d.Gibt(cmn) && std::find(v.dateien.begin(), v.dateien.end(), cmn) == v.dateien.end() && !v.sprites.empty()) v.dateien.push_back(cmn);
    g_spriteDateien = v.dateien;
    if (!w.empty()) Log("  Partikel-Hinweise: %s", w.c_str());
}

std::vector<ULONG> EffekteAnbinden(XfbinImportInterface& x, const xfbin::Spieldaten& d, const std::wstring& texOrdner, float massstab,
                                   std::set<std::string>& effektClumps, std::string& log) {
    Interface* ip = GetCOREInterface();
    std::vector<ULONG> p2Knoten;
    if (g_systeme.empty() || ip == nullptr) return p2Knoten;

    // Sprites: Texturen exportieren und Billboards/Clumps zuordnen
    std::set<std::string> gesucht;
    for (const auto& kv : g_systeme)
        for (const xfbin::ParticleEmitter& em : kv.second.emitters)
            if (em.materialKind == xfbin::kParticleBillboard || (em.materialKind == xfbin::kParticleClump && em.count > 1.0f))
                gesucht.insert(em.materialName);
    const bool p2 = Particle2Da();
    if (p2 && !gesucht.empty() && !texOrdner.empty()) {
        for (const std::string& p : g_spriteDateien) {
            std::vector<uint8_t> b;
            std::string f;
            xfbin::XfbinFile file;
            if (!d.Lies(p, b, f) || !xfbin::ReadXfbinBuffer(b.data(), b.size(), file).ok) continue;
            const size_t vorher = g_sprites.size();
            SammleSprites(file, gesucht, texOrdner);
            if (g_sprites.size() != vorher && x.OpenBuffer(b, Breit(p)) >= 0) x.ExportTextures(texOrdner.c_str());
        }
    }

    size_t angehaengt = 0, partikel = 0, ohneTextur = 0;
    for (const auto& kv : g_systeme) {
        for (const xfbin::ParticleEmitter& em : kv.second.emitters) {
            INode* anker = nullptr;
            if (const ULONG h = x.FindBone(em.attachBone)) anker = ip->GetINodeByHandle(h);

            // Effekt-Animation oder einzelnes Mesh: Clumps an den Andockknochen
            std::set<std::string> clumps;
            if (em.materialKind == xfbin::kParticleAnm) {
                const int i = x.AnimIndex(em.materialName);
                if (i >= 0) clumps = ClumpsDer(x.Anims()[static_cast<size_t>(i)]);
            } else if (em.materialKind == xfbin::kParticleClump && em.count <= 1.0f) {
                clumps.insert(em.materialName);
            }
            for (const std::string& c : clumps) {
                if (anker == nullptr) break;
                for (ULONG r : x.ClumpRoots(c)) {
                    INode* rn = ip->GetINodeByHandle(r);
                    if (rn == nullptr || rn == anker || rn->GetParentNode() == nullptr || !rn->GetParentNode()->IsRootNode()) continue;
                    // Lokale Lage behalten: die Keys der Effekt-Animation
                    // gelten relativ zum Startpunkt, also zum Andockknochen.
                    anker->AttachChild(rn, 0);
                    ++angehaengt;
                }
                effektClumps.insert(c);
            }

            // Schwaerme: WhiteoutDex Particle 2
            const bool schwarm = em.materialKind == xfbin::kParticleBillboard || (em.materialKind == xfbin::kParticleClump && em.count > 1.0f);
            if (!schwarm || !p2) continue;
            const auto sp = g_sprites.find(em.materialName);
            if (sp == g_sprites.end() || sp->second.textur.empty()) { ++ohneTextur; continue; }
            Object* obj = static_cast<Object*>(ip->CreateInstance(GEOMOBJECT_CLASS_ID, kParticle2));
            IParamBlock2* pb = obj ? obj->GetParamBlockByID(0) : nullptr;
            if (pb == nullptr) continue;
            const Sprite& s = sp->second;
            const float leben = std::max(1, em.life) / 30.0f;
            pb->SetValue(P2_COUNT, 0, std::clamp(static_cast<int>(std::ceil(em.count * 2.0f)), 1, 500));
            pb->SetValue(P2_SPEED, 0, em.blowout * 30.0f * massstab);
            pb->SetValue(P2_VARIATION, 0, 0.3f);
            pb->SetValue(P2_LIFE, 0, leben);
            pb->SetValue(P2_WIDTH, 0, std::max(0.1f, 2.0f * em.radiusStart * massstab));
            pb->SetValue(P2_HEIGHT, 0, std::max(0.1f, 2.0f * em.radiusStart * massstab));
            pb->SetValue(P2_CONE, 0, em.blowout > 0.05f ? 90.0f : 0.0f);
            pb->SetValue(P2_MIDTIME, 0, 0.5f);
            const ParamID farbe[3] = { P2_COLOR_START, P2_COLOR_MID, P2_COLOR_END };
            const ParamID alpha[3] = { P2_ALPHA_START, P2_ALPHA_MID, P2_ALPHA_END };
            const ParamID groesse[3] = { P2_SCALE_START, P2_SCALE_MID, P2_SCALE_END };
            for (int k = 0; k < 3; ++k) {
                pb->SetValue(farbe[k], 0, Point3(std::clamp(em.color[k][0], 0.f, 1.f), std::clamp(em.color[k][1], 0.f, 1.f),
                                                 std::clamp(em.color[k][2], 0.f, 1.f)));
                pb->SetValue(alpha[k], 0, static_cast<int>(std::lround(std::clamp(em.color[k][3], 0.f, 1.f) * 255.0f)));
                const float sk = std::max({ std::fabs(em.scale[k][0]), std::fabs(em.scale[k][1]), 0.001f });
                pb->SetValue(groesse[k], 0, std::clamp(sk * s.groesse * massstab, 0.01f, 2000.0f));
            }
            pb->SetValue(P2_ROWS, 0, s.rows);
            pb->SetValue(P2_COLS, 0, s.cols);
            pb->SetValue(P2_HEAD_LIFE_START, 0, 0);
            pb->SetValue(P2_HEAD_LIFE_END, 0, std::max(0, s.rows * s.cols - 1));
            pb->SetValue(P2_HEAD_LIFE_REPEAT, 0, 1);
            pb->SetValue(P2_SQUIRT, 0, em.stretchLoop == 1 ? 1 : 0);
            pb->SetValue(P2_BLEND, 0, 1);                           // Additive - so sehen die Storm-Effekte aus
            float schwerkraft = 0.0f;
            for (const xfbin::ParticleForce& fo : em.forces)
                if (fo.direction[2] < -0.5f) schwerkraft += std::fabs(fo.strength) * 30.0f * 30.0f * massstab;
            pb->SetValue(P2_GRAVITY, 0, schwerkraft);
            pb->SetValue(P2_UNSHADED, 0, 1);
            pb->SetValue(P2_MODELSPACE, 0, em.attachToBone ? 1 : 0);
            pb->SetValue(P2_XYQUAD, 0, 0);
            pb->SetValue(P2_RATE, 0, 0.0f);
            if (MSTR* p = static_cast<MSTR*>(obj->GetInterface(kP2Pfad))) *p = MSTR(s.textur.c_str());
            if (MSTR* p = static_cast<MSTR*>(obj->GetInterface(kP2Praefix))) *p = MSTR(_T("Textures\\"));
            INode* n = ip->CreateObjectNode(obj);
            if (n == nullptr) continue;
            const std::wstring name = L"FX " + Breit(kv.first) + L" #" + std::to_wstring(em.entry) + L" " + Breit(em.materialName);
            n->SetName(const_cast<MCHAR*>(name.c_str()));
            if (anker != nullptr) anker->AttachChild(n, 0);
            n->SetVisibility(0, 0.0f);
            n->SetUserPropInt(_T("Wc3P2Intervals"), 1);
            g_p2[Schluessel(kv.first, em.entry)] = n->GetHandle();
            p2Knoten.push_back(n->GetHandle());
            ++partikel;
        }
    }
    char z[200];
    std::snprintf(z, sizeof z, "%zu Effekt-Clumps an Andockknochen, %zu Particle 2%s%s", angehaengt, partikel,
                  ohneTextur ? (", " + std::to_string(ohneTextur) + " Schwaerme ohne Textur").c_str() : "",
                  p2 ? "" : ", WhiteoutDex fehlt - keine Partikel");
    log = z;
    return p2Knoten;
}

// ============================================================
//  Setzen
// ============================================================
void EffektKeysLoeschen() {
    Interface* ip = GetCOREInterface();
    for (const auto& kv : g_p2) {
        INode* n = ip->GetINodeByHandle(kv.second);
        Object* o = n ? n->GetObjectRef() : nullptr;
        IParamBlock2* pb = o ? o->GetParamBlockByID(0) : nullptr;
        if (pb == nullptr) continue;
        if (Control* c = pb->GetControllerByID(P2_RATE)) c->DeleteKeys(TRACK_DOALL);
        pb->SetValue(P2_RATE, 0, 0.0f);
    }
}

void SpawnsSetzen(XfbinImportInterface& x, int index, double start, double ende, float massstab, EffektFenster& f, int tiefe) {
    if (tiefe > 4 || index < 0 || static_cast<size_t>(index) >= x.Anims().size()) return;
    const auto it = g_systeme.find(x.Anims()[static_cast<size_t>(index)].name);
    if (it == g_systeme.end()) return;
    Interface* ip = GetCOREInterface();
    const TimeValue tpf = GetTicksPerFrame();
    for (const xfbin::ParticleEmitter& em : it->second.emitters) {
        const std::vector<std::pair<double, double>> fenster = Fenster(em, start, ende);
        if (fenster.empty()) continue;

        if (em.materialKind == xfbin::kParticleAnm) {
            // Eine Instanz je Effekt-Animation: beim ersten Einschalten.
            const int e = x.AnimIndex(em.materialName);
            if (e < 0) continue;
            const double a = fenster.front().first;
            const double b = std::min(a + x.AnimFrames(e), ende);
            if (b <= a) continue;
            x.BuildAnimAt(e, static_cast<float>(a), 7, massstab);
            for (const XfbinImportInterface::MeshBone& mb : x.MeshBones(ClumpsDer(x.Anims()[static_cast<size_t>(e)])))
                f.fenster[mb.mesh].emplace_back(a, b);
            SpawnsSetzen(x, e, a, b, massstab, f, tiefe + 1);
        } else if (em.materialKind == xfbin::kParticleClump && em.count <= 1.0f) {
            // Ein einzelnes Mesh (Ring, Schockwelle): sichtbar fuer seine
            // Lebensdauer, Groesse von Anfang ueber Mitte zum Ende.
            const double a = fenster.front().first;
            const double b = std::min(a + std::max(1, em.life), ende);
            for (const XfbinImportInterface::MeshBone& mb : x.MeshBones({ em.materialName })) f.fenster[mb.mesh].emplace_back(a, b);
            SuspendAnimate();
            AnimateOn();
            for (ULONG r : x.ClumpRoots(em.materialName)) {
                INode* rn = ip->GetINodeByHandle(r);
                Control* sc = (rn && rn->GetTMController()) ? rn->GetTMController()->GetScaleController() : nullptr;
                if (sc == nullptr) continue;
                const double zeit[3] = { a, (a + b) / 2.0, b };
                for (int k = 0; k < 3; ++k) {
                    ScaleValue sv(Point3(std::max(em.scale[k][0], 0.001f), std::max(em.scale[k][1], 0.001f), std::max(em.scale[k][2], 0.001f)),
                                  Quat(0.0f, 0.0f, 0.0f, 1.0f));
                    sc->SetValue(static_cast<TimeValue>(zeit[k] * tpf), &sv, TRUE, CTRL_ABSOLUTE);
                }
            }
            AnimateOff();
            ResumeAnimate();
        } else {
            // Schwarm: Emissionsrate an/aus
            const auto p = g_p2.find(Schluessel(it->first, em.entry));
            if (p == g_p2.end()) continue;
            INode* n = ip->GetINodeByHandle(p->second);
            Object* o = n ? n->GetObjectRef() : nullptr;
            IParamBlock2* pb = o ? o->GetParamBlockByID(0) : nullptr;
            if (pb == nullptr) continue;
            SuspendAnimate();
            AnimateOn();
            for (const auto& w : fenster) {
                const double dauer = std::max(1.0, w.second - w.first);
                const float rate = em.stretchLoop == 1 ? std::max(1.0f, em.count) : static_cast<float>(em.count / dauer * 30.0);
                pb->SetValue(P2_RATE, static_cast<TimeValue>((w.first - 1.0) * tpf), 0.0f);
                pb->SetValue(P2_RATE, static_cast<TimeValue>(w.first * tpf), rate);
                const double aus = em.stretchLoop == 1 ? w.first + 1.0 : w.second;
                pb->SetValue(P2_RATE, static_cast<TimeValue>(aus * tpf), em.stretchLoop == 1 ? 0.0f : rate);
                pb->SetValue(P2_RATE, static_cast<TimeValue>((aus + 1.0) * tpf), 0.0f);
                // sichtbar, solange Partikel leben
                f.fenster[p->second].emplace_back(w.first, std::min(ende, w.second + std::max(1, em.life)));
            }
            AnimateOff();
            ResumeAnimate();
        }
    }
}

size_t EffektSichtbarkeit(XfbinImportInterface& x, const std::vector<Abschnitt>& abschnitte, const EffektFenster& f,
                          const std::set<std::string>& effektClumps, bool wc3, bool bindPose) {
    Interface* ip = GetCOREInterface();
    if (ip == nullptr || abschnitte.empty()) return 0;
    const TimeValue tpf = GetTicksPerFrame();
    const std::vector<XfbinImportInterface::MeshBone> mbs = x.MeshBones(effektClumps);

    std::map<ULONG, std::vector<std::pair<TimeValue, float>>> keys;
    auto imFenster = [&](ULONG h, int t) {
        const auto it = f.fenster.find(h);
        if (it == f.fenster.end()) return false;
        for (const auto& w : it->second) if (t >= w.first && t < w.second) return true;
        return false;
    };
    auto schreibe = [&](ULONG h, const Abschnitt& a, auto&& an) {
        auto& kl = keys[h];
        bool vorher = false;
        for (int t = a.start; t <= a.ende; ++t) {
            const bool v = an(t);
            if (t == a.start || t == a.ende || v != vorher) kl.emplace_back(t * tpf, v ? 1.0f : 0.0f);
            vorher = v;
        }
    };
    for (const XfbinImportInterface::MeshBone& mb : mbs) {
        if (bindPose) keys[mb.mesh].emplace_back(0, 0.0f);
    }
    for (const auto& kv : g_p2) if (bindPose) keys[kv.second].emplace_back(0, 0.0f);

    for (const Abschnitt& a : abschnitte) {
        std::set<std::string> drin;
        if (a.index >= 0 && static_cast<size_t>(a.index) < x.Anims().size()) drin = ClumpsDer(x.Anims()[static_cast<size_t>(a.index)]);
        for (const XfbinImportInterface::MeshBone& mb : mbs) {
            INode* mesh = ip->GetINodeByHandle(mb.mesh);
            INode* bone = mb.bone ? ip->GetINodeByHandle(mb.bone) : nullptr;
            const bool selbst = drin.count(mb.clump) > 0;
            schreibe(mb.mesh, a, [&](int t) {
                // Hat das Mesh keine Sichtbarkeits-Keys (Schalter "Visibility"
                // aus), gilt ein Effekt, den die Animation bewegt, als sichtbar.
                Control* vc = mesh != nullptr ? mesh->GetVisController() : nullptr;
                const bool basis = selbst && mesh != nullptr && (vc == nullptr || vc->NumKeys() == 0 || mesh->GetVisibility(t * tpf) > 0.5f);
                bool v = imFenster(mb.mesh, t) || basis;
                if (v && wc3 && bone != nullptr) {
                    const Matrix3 tm = bone->GetNodeTM(t * tpf);
                    v = std::max({ Length(tm.GetRow(0)), Length(tm.GetRow(1)), Length(tm.GetRow(2)) }) > 0.02f;
                }
                return v;
            });
        }
        for (const auto& kv : g_p2) schreibe(kv.second, a, [&](int t) { return imFenster(kv.second, t); });
    }
    for (const auto& kv : keys) x.SetStepVisibility(kv.first, kv.second);
    return keys.size();
}

} // namespace xfi
