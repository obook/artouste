/*
 * ApplicationSceneSoleil.cpp
 * Heure de départ et vitesse du soleil pour la carte chargée.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"

#include <cstdio>
#include <cstdlib>
#include <ctime>

namespace artouste::app {

void Application::applySunSchedule() {
    /* Cycle jour/nuit : vitesse du temps (clé `soleil_vitesse` de la config, 1 =
       temps réel). m_soleil.heureDepart est l'heure d'origine du soleil (s depuis
       minuit), voir Application::sunDirection :
         - en temps réel (échelle 1) on part de l'heure locale du PC ;
         - sinon (temps accéléré ou figé) on part de 8h du matin, pour démarrer sur
           une belle lumière plutôt qu'en pleine nuit selon l'heure du PC. Avec une
           échelle nulle, le soleil reste donc figé à 8h. */
    m_soleil.vitesse = m_config.sunTimeScale;
    /* Le temps du soleil repart d'ici : l'heure fixée plus bas est celle qu'il sera
       MAINTENANT, et non celle qu'il aurait fallu au lancement du programme (voir
       m_soleil.origine). Chaque carte lancée dans la session commence donc à son
       heure de départ, et pas au milieu de la nuit. */
    m_soleil.origine = m_animTime;
    /* Nuit accélérée (clé `lune_vitesse`) : la course du soleil garde la vitesse
       ci-dessus le jour et la multiplie par ce facteur entre le coucher et le
       lever, de sorte qu'une nuit dure deux fois moins longtemps qu'un jour avec
       la valeur par défaut (voir timeOfDaySeconds, ApplicationSun.cpp). */
    m_soleil.vitesseNuit = m_config.nightSpeedFactor;
    if (m_soleil.vitesse == 1.0f) {
        const std::time_t now = std::time(nullptr);
        std::tm local{};
#if defined(_WIN32)
        localtime_s(&local, &now);
#else
        localtime_r(&now, &local);
#endif
        m_soleil.heureDepart = static_cast<float>(local.tm_hour) * 3600.0f +
                           static_cast<float>(local.tm_min) * 60.0f +
                           static_cast<float>(local.tm_sec);
        std::printf("[scène] cycle jour/nuit : temps réel, heure locale au lancement %02d:%02d.\n",
                    local.tm_hour,
                    local.tm_min);
    } else {
        constexpr float EIGHT_AM = 8.0f * 3600.0f; /* 8h du matin */
        m_soleil.heureDepart = EIGHT_AM;
        if (m_soleil.vitesse == 0.0f) {
            std::printf("[scène] cycle jour/nuit : temps figé à 8h00.\n");
        } else {
            std::printf(
                "[scène] cycle jour/nuit : temps accéléré (x%g le jour, x%g la nuit), départ "
                "à 8h00.\n",
                static_cast<double>(m_soleil.vitesse),
                static_cast<double>(m_soleil.vitesse * m_soleil.vitesseNuit));
        }
    }

    /* Arène dédiée au mode zombie (Happy DeathHour, zombie_only.txt) : nuit figée
       plutôt que le réglage ci-dessus -- ambiance de combat nocturne constante,
       sans cycle jour/nuit dans une arène fermée. 19h00 place la lune à ~14°
       au-dessus de l'horizon dans l'axe du cap de départ (voir sunDirection,
       ApplicationSun.cpp) : bien visible depuis le cockpit sans lever la tête
       (vérifié par capture -- au-delà de ~20°, la vue cockpit, inclinée vers le
       bas, la sort du cadre). Testé sur m_combat.active() : sans ce garde-fou,
       quitter cette arène pour une carte normale garderait le temps figé
       indéfiniment, faute d'être jamais réévalué depuis la config. */
    if (m_combat.active() &&
        std::filesystem::exists(m_assetsDir / "terrain" / m_terrainName / "zombie_only.txt")) {
        constexpr float NIGHT_HOUR_S = 19.0f * 3600.0f;
        m_soleil.heureDepart = NIGHT_HOUR_S;
        m_soleil.vitesse = 0.0f;
    }
}


} /* namespace artouste::app */
