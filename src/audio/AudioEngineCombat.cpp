/*
 * AudioEngineCombat.cpp
 * Sons ponctuels du mode zombie (tir, touché, mort, jet, impact, nouvelle
 * vague, apparition du largueur) : chargement paresseux à la première
 * lecture depuis assets/sounds/combat/, même principe que
 * AudioEngine::playMusic. Fichier absent : silencieux, sans erreur.
 *
 * Tous (sauf wave_start et rale) sont spatiaux : chaque appel crée sa propre
 * instance de lecture (ma_sound_init_copy à partir d'un modèle décodé une
 * seule fois), volume selon la distance source<->hélico, détruite
 * automatiquement dès la fin de la lecture (reapOneShots, appelé chaque image depuis
 * AudioEngine::update). wave_start et rale sont les exceptions : annonces non
 * spatiales, à volume fixe, qui réutilisent chacune un unique son rejoué depuis
 * le début, puisqu'elles n'ont pas de position source pertinente à l'échelle de
 * l'arène.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "audio/AudioEngine.hpp"
#include "audio/AudioEngineImpl.hpp"

#include <algorithm>
#include <cstddef>

namespace artouste::audio {

using namespace audio_detail;

namespace {

/* Charge (au besoin) le modèle décodé de dir/filename dans 'templateSound'.
   Fichier absent ou dossier jamais renseigné (initCombatSounds non appelée) :
   renvoie false, silencieux, sans erreur -- même contrat que playMusic.
   MA_SOUND_FLAG_DECODE (plutôt que STREAM) : les données sont entièrement en
   mémoire, condition requise par ma_sound_init_copy pour partager le décodage
   entre les instances jouées simultanément. */
bool ensureTemplate(ma_engine& engine, ma_sound& templateSound, bool& loaded,
                    const std::filesystem::path& dir, const char* filename) {
    if (dir.empty()) {
        return false;
    }
    if (loaded) {
        return true;
    }
    const std::filesystem::path path = dir / filename;
    if (!std::filesystem::exists(path)) {
        return false;
    }
    if (ma_sound_init_from_file(&engine, path.string().c_str(), MA_SOUND_FLAG_DECODE, nullptr,
                                nullptr, &templateSound) != MA_SUCCESS) {
        return false;
    }
    loaded = true;
    return true;
}

/* Nombre maximal d'exemplaires du MÊME son jouant en même temps. Au-delà, le
   nouvel appel est ignoré : superposer davantage n'ajoute plus d'information
   (on n'entend pas la différence entre huit et douze pneus lancés), mais leurs
   amplitudes s'additionnent et saturent la sortie, en plus de masquer les sons
   utiles -- l'impact sur l'appareil, notamment, qui est le retour de dégâts du
   joueur. */
constexpr std::size_t MAX_SAME_SOUND = 3;

/* Joue une nouvelle instance du modèle à un volume selon la distance de
   sourcePos à listenerPos (voir distanceAttenuation). Rejoint oneShots ;
   silencieuse (aucun objet créé) si le volume résultant est inaudible ou si ce
   son atteint déjà son plafond d'exemplaires simultanés.
   Prend ses paramètres un par un (plutôt qu'AudioEngine::Impl&) : Impl est
   privé, inaccessible depuis cette fonction libre hors de la classe. */
void playPositional(ma_engine& engine, std::list<OneShot>& oneShots, ma_sound& templateSound,
                    bool& loaded, const std::filesystem::path& dir, const char* filename,
                    float baseVolume, const vec3& sourcePos, const vec3& listenerPos,
                    std::size_t maxSimultaneous = MAX_SAME_SOUND) {
    if (!ensureTemplate(engine, templateSound, loaded, dir, filename)) {
        return;
    }
    const float volume = baseVolume * distanceAttenuation(sourcePos, listenerPos);
    if (volume <= 0.001f) {
        return;
    }
    const std::size_t enCours = static_cast<std::size_t>(std::count_if(
        oneShots.begin(), oneShots.end(),
        [&templateSound](const OneShot& o) { return o.source == &templateSound; }));
    if (enCours >= maxSimultaneous) {
        return;
    }

    oneShots.emplace_back();
    OneShot& slot = oneShots.back();
    if (ma_sound_init_copy(&engine, &templateSound, 0, nullptr, &slot.sound) != MA_SUCCESS) {
        oneShots.pop_back();
        return;
    }
    slot.source = &templateSound;
    ma_sound_set_volume(&slot.sound, volume);
    ma_sound_start(&slot.sound);
}

}  /* namespace */

/* Purge les instances de lecture arrivées à leur fin (voir oneShots dans
   AudioEngineImpl.hpp). Appelée chaque image depuis AudioEngine::update. */
void reapOneShots(std::list<OneShot>& oneShots) {
    for (auto it = oneShots.begin(); it != oneShots.end();) {
        if (ma_sound_at_end(&it->sound) == MA_TRUE) {
            ma_sound_uninit(&it->sound);
            it = oneShots.erase(it);
        } else {
            ++it;
        }
    }
}

void AudioEngine::initCombatSounds(const std::filesystem::path& dir) {
    m_impl->combatSoundsDir = dir;
}

void AudioEngine::stopCombatSounds() {
    if (!m_impl->engineInit) {
        return;
    }
    /* Coupe net toutes les lectures en cours et les libère : setPaused ne les
       connaît pas (il ne suspend que les boucles), et reapOneShots ne tourne plus
       dès que le jeu est figé, faute d'appel à update(). Sans cela, un râle ou
       une queue d'explosion se poursuit derrière le bandeau de fin de partie. */
    for (audio_detail::OneShot& os : m_impl->oneShots) {
        ma_sound_stop(&os.sound);
        ma_sound_uninit(&os.sound);
    }
    m_impl->oneShots.clear();
    /* Les deux annonces non spatiales sont réutilisées, pas copiées : on les
       arrête sans les libérer. */
    if (m_impl->waveStartLoaded) {
        ma_sound_stop(&m_impl->waveStartSound);
    }
    if (m_impl->broodSpawnLoaded) {
        ma_sound_stop(&m_impl->broodSpawnSound);
    }
}

void AudioEngine::playGunfire(const vec3& sourcePos, const vec3& listenerPos) {
    if (!m_impl->engineInit) {
        return;
    }
    playPositional(m_impl->engine, m_impl->oneShots, m_impl->gunfireSound, m_impl->gunfireLoaded,
                  m_impl->combatSoundsDir, "gunfire.wav", 0.35f, sourcePos, listenerPos);
}

void AudioEngine::playExplosion(const vec3& sourcePos, const vec3& listenerPos) {
    if (!m_impl->engineInit) {
        return;
    }
    playPositional(m_impl->engine, m_impl->oneShots, m_impl->explosionSound, m_impl->explosionLoaded,
                  m_impl->combatSoundsDir, "explosion.wav", 0.7f, sourcePos, listenerPos);
}

void AudioEngine::playZombieHit(const vec3& sourcePos, const vec3& listenerPos) {
    if (!m_impl->engineInit) {
        return;
    }
    playPositional(m_impl->engine, m_impl->oneShots, m_impl->zombieHitSound, m_impl->zombieHitLoaded,
                  m_impl->combatSoundsDir, "zombie_hit.wav", 0.6f, sourcePos, listenerPos);
}

void AudioEngine::playZombieDeath(const vec3& sourcePos, const vec3& listenerPos) {
    if (!m_impl->engineInit) {
        return;
    }
    playPositional(m_impl->engine, m_impl->oneShots, m_impl->zombieDeathSound,
                  m_impl->zombieDeathLoaded, m_impl->combatSoundsDir, "zombie_death.wav", 0.7f,
                  sourcePos, listenerPos);
}

/* Deux exemplaires au plus, et à volume modéré : l'échantillon dure sept
   secondes alors que le geste, lui, est instantané, si bien qu'une horde qui
   lance en rafale accumulait une pile de queues de son. */
void AudioEngine::playToxicThrow(const vec3& sourcePos, const vec3& listenerPos) {
    if (!m_impl->engineInit) {
        return;
    }
    playPositional(m_impl->engine, m_impl->oneShots, m_impl->toxicThrowSound,
                  m_impl->toxicThrowLoaded, m_impl->combatSoundsDir, "toxic_throw.wav", 0.35f,
                  sourcePos, listenerPos, 2);
}

/* Plein volume : c'est l'échantillon le plus discret du lot (une quinzaine de
   décibels sous les autres) alors qu'il porte l'information la plus utile au
   joueur, les dégâts encaissés. */
void AudioEngine::playToxicImpact(const vec3& sourcePos, const vec3& listenerPos) {
    if (!m_impl->engineInit) {
        return;
    }
    playPositional(m_impl->engine, m_impl->oneShots, m_impl->toxicImpactSound,
                  m_impl->toxicImpactLoaded, m_impl->combatSoundsDir, "toxic_impact.wav", 1.0f,
                  sourcePos, listenerPos);
}

void AudioEngine::playWaveStart() {
    if (!m_impl->engineInit) {
        return;
    }
    if (m_impl->combatSoundsDir.empty()) {
        return;
    }
    if (!m_impl->waveStartLoaded) {
        const std::filesystem::path path = m_impl->combatSoundsDir / "wave_start.wav";
        if (!std::filesystem::exists(path)) {
            return;
        }
        if (ma_sound_init_from_file(&m_impl->engine, path.string().c_str(), MA_SOUND_FLAG_STREAM,
                                    nullptr, nullptr, &m_impl->waveStartSound) != MA_SUCCESS) {
            return;
        }
        m_impl->waveStartLoaded = true;
    }
    ma_sound_seek_to_pcm_frame(&m_impl->waveStartSound, 0);
    ma_sound_set_volume(&m_impl->waveStartSound, 0.7f);
    ma_sound_start(&m_impl->waveStartSound);
}

void AudioEngine::playBroodSpawn() {
    if (!m_impl->engineInit) {
        return;
    }
    if (m_impl->combatSoundsDir.empty()) {
        return;
    }
    if (!m_impl->broodSpawnLoaded) {
        const std::filesystem::path path = m_impl->combatSoundsDir / "rale.wav";
        if (!std::filesystem::exists(path)) {
            return;
        }
        if (ma_sound_init_from_file(&m_impl->engine, path.string().c_str(), MA_SOUND_FLAG_STREAM,
                                    nullptr, nullptr, &m_impl->broodSpawnSound) != MA_SUCCESS) {
            return;
        }
        m_impl->broodSpawnLoaded = true;
    }
    ma_sound_seek_to_pcm_frame(&m_impl->broodSpawnSound, 0);
    /* Un cran au-dessus de l'annonce de vague : c'est l'événement le plus fort
       de la partie. */
    ma_sound_set_volume(&m_impl->broodSpawnSound, 0.9f);
    ma_sound_start(&m_impl->broodSpawnSound);
}

}  /* namespace artouste::audio */
