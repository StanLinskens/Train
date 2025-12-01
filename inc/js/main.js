// Responsive main.js v2: improved infinite carousel loop and responsive behaviors
document.addEventListener('DOMContentLoaded', () => {
  // Menu toggle for mobile
  const navToggle = document.querySelector('.nav-toggle');
  const navLinks = document.querySelector('.nav-links');
  if (navToggle && navLinks) {
    navToggle.addEventListener('click', () => {
      const expanded = navToggle.getAttribute('aria-expanded') === 'true';
      navToggle.setAttribute('aria-expanded', String(!expanded));
      navLinks.classList.toggle('open');
    });
  }

  // Smooth scroll for in-page links with accessibility
  document.querySelectorAll('a[href^="#"]').forEach(anchor => {
    anchor.addEventListener('click', function (e) {
      const href = this.getAttribute('href');
      if (!href || href === '#') return;
      const target = document.querySelector(href);
      if (!target) return;
      e.preventDefault();
      target.scrollIntoView({ behavior: 'smooth', block: 'start' });
      target.setAttribute('tabindex', '-1');
      target.focus({ preventScroll: true });
      if (navLinks && navLinks.classList.contains('open')) {
        navLinks.classList.remove('open');
        if (navToggle) navToggle.setAttribute('aria-expanded', 'false');
      }
    });
  });

  // Reveal on scroll using IntersectionObserver
  const reveals = document.querySelectorAll('.reveal');
  if (reveals.length && 'IntersectionObserver' in window) {
    const obs = new IntersectionObserver((entries, observer) => {
      entries.forEach(entry => {
        if (entry.isIntersecting) {
          entry.target.classList.add('show');
          observer.unobserve(entry.target);
        }
      });
    }, { threshold: 0.12 });
    reveals.forEach(r => obs.observe(r));
  } else {
    reveals.forEach(r => r.classList.add('show'));
  }

  // Team carousel: create a seamless infinite loop by cloning originals until wide enough
  const container = document.querySelector('.team-container');
  const carousel = document.querySelector('.team-carousel');
  let rafId = null;
  let running = false;
  let originalCount = 0;
  let originalWidth = 0;

  function getWidths(items) {
    return items.reduce((sum, el) => {
      // include horizontal margin if present
      const style = getComputedStyle(el);
      const marginRight = parseFloat(style.marginRight || 0);
      return sum + el.offsetWidth + marginRight;
    }, 0);
  }

  function prepareCarousel() {
    if (!carousel || !container) return false;
    // capture originals
    const children = Array.from(carousel.children);
    if (children.length === 0) return false;
    // If we previously stored originals, skip capturing again
    if (!carousel.dataset.originalCount) {
      originalCount = children.length;
      carousel.dataset.originalCount = String(originalCount);
    } else {
      originalCount = parseInt(carousel.dataset.originalCount, 10);
    }

    // compute width of original sequence
    const originals = Array.from(carousel.children).slice(0, originalCount);
    originalWidth = getWidths(originals);

    // Ensure there are enough clones so that the carousel's scrollWidth is large enough
    // to avoid gaps: target at least container width + originalWidth (so one loop fits)
    let attempts = 0;
    const maxAttempts = 8;
    while ((carousel.scrollWidth < originalWidth * 2 || carousel.scrollWidth < container.clientWidth * 2) && attempts < maxAttempts) {
      originals.forEach(node => carousel.appendChild(node.cloneNode(true)));
      attempts++;
    }

    // Mark that we've prepared cloning
    carousel.dataset.cloned = 'true';
    return true;
  }

  function startLoop() {
    if (!carousel || !container) return;
    if (running) return;
    // allow auto-loop on all screen sizes (adaptive speed used)

    if (!prepareCarousel()) return;
    // Need originalWidth recalculated after clones
    const originals = Array.from(carousel.children).slice(0, parseInt(carousel.dataset.originalCount, 10));
    originalWidth = getWidths(originals) || originalWidth;
    if (originalWidth === 0) return;

    let position = 0;
    const speed = window.matchMedia('(max-width: 899px)').matches ? 0.9 : 0.35; // adaptive speed (higher on mobile)
    running = true;

    function step() {
      position += speed;
      // modulo to keep position in [0, originalWidth)
      if (position >= originalWidth) position = position % originalWidth;
      carousel.style.transform = `translateX(${-position}px)`;
      rafId = requestAnimationFrame(step);
    }
    rafId = requestAnimationFrame(step);
  }

  function stopLoop() {
    if (rafId) cancelAnimationFrame(rafId);
    rafId = null;
    running = false;
    if (carousel) {
      carousel.style.transform = '';
    }
  }

  function resetClonesIfNeeded() {
    // If screen is small, remove cloned nodes to simplify DOM
    if (!carousel || !carousel.dataset.cloned) return;
    if (window.matchMedia('(min-width: 900px)').matches) return; // keep clones on wide screens

    const originalCount = parseInt(carousel.dataset.originalCount || '0', 20);
    if (originalCount <= 0) return;
    // remove everything and re-append only originals
    const originals = Array.from(carousel.children).slice(0, originalCount);
    carousel.innerHTML = '';
    originals.forEach(n => carousel.appendChild(n));
    delete carousel.dataset.cloned;
  }

  function evaluateCarousel() {
    if (window.matchMedia('(min-width: 900px)').matches) startLoop(); else { stopLoop(); resetClonesIfNeeded(); }
  }

  evaluateCarousel();
  window.addEventListener('resize', () => { evaluateCarousel(); });

  // Team container keyboard + wheel support
  if (container) {
    container.addEventListener('wheel', (e) => {
      if (Math.abs(e.deltaY) > Math.abs(e.deltaX)) {
        e.preventDefault();
        container.scrollLeft += e.deltaY;
      }
    }, { passive: false });
    container.setAttribute('tabindex', '0');
    container.addEventListener('keydown', (e) => {
      const step = 220;
      if (e.key === 'ArrowRight') { container.scrollBy({ left: step, behavior: 'smooth' }); e.preventDefault(); }
      if (e.key === 'ArrowLeft') { container.scrollBy({ left: -step, behavior: 'smooth' }); e.preventDefault(); }
    });
  }

});