/*
 * AudioEngineCombat.cpp
 * Sons ponctuels du mode zombie (tir, touché, mort, jet, impact, nouvelle
 * vague, apparition de la pondeuse) : chargement paresseux à la première
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

/* Joue une nouvelle instance du modèle à un volume selon la distance de
   sourcePos à listenerPos (voir distanceAttenuation). Rejoint oneShots ;
   silencieuse (aucun objet créé) si le volume résultant est inaudible.
   Prend ses paramètres un par un (plutôt qu'AudioEngine::Impl&) : Impl est
   privé, inaccessible depuis cette fonction libre hors de la classe. */
void playPositional(ma_engine& engine, std::list<ma_sound>& oneShots, ma_sound& templateSound,
                    bool& loaded, const std::filesystem::path& dir, const char* filename,
                    float baseVolume, const vec3& sourcePos, const vec3& listenerPos) {
    if (!ensureTemplate(engine, templateSound, loaded, dir, filename)) {
        return;
    }
    const float volume = baseVolume * distanceAttenuation(sourcePos, listenerPos);
    if (volume <= 0.001f) {
        return;
    }
    oneShots.emplace_back();
    ma_sound& instance = oneShots.back();
    if (ma_sound_init_copy(&engine, &templateSound, 0, nullptr, &instance) != MA_SUCCESS) {
        oneShots.pop_back();
        return;
    }
    ma_sound_set_volume(&instance, volume);
    ma_sound_start(&instance);
}

}  /* namespace */

/* Purge les instances de lecture arrivées à leur fin (voir oneShots dans
   AudioEngineImpl.hpp). Appelée chaque image depuis AudioEngine::update. */
void reapOneShots(std::list<ma_sound>& oneShots) {
    for (auto it = oneShots.begin(); it != oneShots.end();) {
        if (ma_sound_at_end(&*it) == MA_TRUE) {
            ma_sound_uninit(&*it);
            it = oneShots.erase(it);
        } else {
            ++it;
        }
    }
}

void AudioEngine::initCombatSounds(const std::filesystem::path& dir) {
    m_impl->combatSoundsDir = dir;
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

void AudioEngine::playToxicThrow(const vec3& sourcePos, const vec3& listenerPos) {
    if (!m_impl->engineInit) {
        return;
    }
    playPositional(m_impl->engine, m_impl->oneShots, m_impl->toxicThrowSound,
                  m_impl->toxicThrowLoaded, m_impl->combatSoundsDir, "toxic_throw.wav", 0.6f,
                  sourcePos, listenerPos);
}

void AudioEngine::playToxicImpact(const vec3& sourcePos, const vec3& listenerPos) {
    if (!m_impl->engineInit) {
        return;
    }
    playPositional(m_impl->engine, m_impl->oneShots, m_impl->toxicImpactSound,
                  m_impl->toxicImpactLoaded, m_impl->combatSoundsDir, "toxic_impact.wav", 0.8f,
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
