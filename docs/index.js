/* ==========================================================================
   Artouste - scripts de la page de présentation (docs/index.html)
   Deux comportements indépendants, chacun isolé dans sa propre IIFE pour
   éviter toute fuite de variables entre eux.
   ========================================================================== */

/* --------------------------------------------------------------------------
   Menu mobile : panneau déroulant accessible (clavier et lecteur d'écran).
   -------------------------------------------------------------------------- */
(function () {
  var toggle = document.getElementById('navToggle');
  var menu = document.getElementById('navmenu');
  if (!toggle || !menu) { return; }

  function setOuvert(ouvert) {
    toggle.setAttribute('aria-expanded', ouvert ? 'true' : 'false');
    toggle.setAttribute('aria-label', ouvert ? 'Fermer le menu' : 'Ouvrir le menu');
    menu.classList.toggle('open', ouvert);
  }
  toggle.addEventListener('click', function () {
    setOuvert(toggle.getAttribute('aria-expanded') !== 'true');
  });
  /* Une fois un lien choisi, on referme le panneau. */
  menu.addEventListener('click', function (e) {
    if (e.target.tagName === 'A') { setOuvert(false); }
  });
  document.addEventListener('keydown', function (e) {
    if (e.key === 'Escape' && toggle.getAttribute('aria-expanded') === 'true') {
      setOuvert(false); toggle.focus();
    }
  });
  /* Retour en grand écran : on repart d'un état propre. */
  window.matchMedia('(min-width: 761px)').addEventListener('change', function (mq) {
    if (mq.matches) { setOuvert(false); }
  });
})();

/* --------------------------------------------------------------------------
   Vidéo : la façade (affiche + bouton) laisse place au lecteur YouTube au
   premier clic seulement. Tant qu'on n'a pas cliqué, la page n'émet aucune
   requête vers YouTube, donc aucun cookie tiers n'est déposé. Sans
   JavaScript, la façade reste un lien ordinaire vers la vidéo.
   -------------------------------------------------------------------------- */
(function () {
  var facade = document.getElementById('videoFacade');
  if (!facade) { return; }

  facade.addEventListener('click', function (e) {
    var embed = facade.getAttribute('data-embed');
    if (!embed) { return; }  /* pas d'URL de lecteur : on suit le lien normal */
    e.preventDefault();
    var lecteur = document.createElement('iframe');
    lecteur.src = embed;
    lecteur.title = 'Vidéo de présentation d\'Artouste';
    lecteur.allow = 'accelerometer; autoplay; clipboard-write; encrypted-media; ' +
                    'gyroscope; picture-in-picture; web-share';
    lecteur.referrerPolicy = 'strict-origin-when-cross-origin';
    lecteur.allowFullscreen = true;
    facade.replaceWith(lecteur);
    lecteur.focus();
  });
})();

/* --------------------------------------------------------------------------
   Galerie : modale (lightbox) affichant une capture en grand.
   -------------------------------------------------------------------------- */
(function () {
  var lb = document.getElementById('lightbox');
  var lbImg = document.getElementById('lbImg');
  var lbCap = document.getElementById('lbCap');
  var lbClose = document.getElementById('lbClose');
  var lastFocus = null;

  function ouvrir(shot) {
    lastFocus = shot;
    var img = shot.querySelector('img');
    lbImg.src = shot.getAttribute('data-full');
    lbImg.alt = img ? img.alt : '';
    lbCap.textContent = shot.getAttribute('data-cap') || '';
    lb.hidden = false;
    document.body.style.overflow = 'hidden';
    lbClose.focus();
  }
  function fermer() {
    lb.hidden = true;
    lbImg.src = '';
    document.body.style.overflow = '';
    if (lastFocus) { lastFocus.focus(); }
  }

  var shots = document.querySelectorAll('.shot');
  for (var i = 0; i < shots.length; i++) {
    shots[i].addEventListener('click', function () { ouvrir(this); });
  }
  lbClose.addEventListener('click', fermer);
  lb.addEventListener('click', function (e) { if (e.target === lb) { fermer(); } });
  document.addEventListener('keydown', function (e) {
    if (e.key === 'Escape' && !lb.hidden) { fermer(); }
  });
})();
