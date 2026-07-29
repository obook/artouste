/*
 * CycleJourNuit.hpp
 * Heure simulée du cycle jour/nuit, avec une nuit plus rapide que le jour.
 *
 * Le calcul vit ici, à part de l'Application, pour deux raisons : il n'a besoin
 * ni de fenêtre ni de contexte graphique, et c'est une fonction pure -- même
 * date, même heure rendue -- donc entièrement vérifiable par les tests. La
 * caméra d'orbite solaire s'en sert d'ailleurs avec des dates arbitraires, en
 * avant comme en arrière : elle ne tolérerait pas un état caché.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#pragma once

namespace artouste::app {

/* Heure simulée, en secondes depuis minuit, dans [0, 86400[.

     baseSecondes : heure de départ (t = 0), en secondes depuis minuit
     t            : secondes réelles écoulées depuis le lancement
     vitesseJour  : facteur d'accélération du temps de jour (1 = temps réel,
                    72 = une journée en 20 min, 0 = temps figé)
     facteurNuit  : multiplicateur appliqué à vitesseJour entre le coucher (18 h)
                    et le lever (6 h). 2 = la nuit passe deux fois plus vite que
                    le jour ; 1 = même vitesse de bout en bout.

   Avec un facteur autre que 1, le temps n'avance pas au même rythme selon
   l'heure : la fonction raisonne donc par cycle complet, dont la durée réelle
   vaut le jour simulé plus la nuit simulée, chacun à sa vitesse. Une vitesse
   nulle fige l'heure, une vitesse négative fait tourner le cycle à l'envers, à
   rythme uniforme (le découpage en deux vitesses n'aurait pas de sens). */
float heureDuJour(float baseSecondes, float t, float vitesseJour, float facteurNuit);

} /* namespace artouste::app */
