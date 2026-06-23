/*
 * RingBuffer.hpp
 * Tampon circulaire d'octets, thread-safe a un producteur et un consommateur :
 * le thread réseau écrit le flux, le callback audio le lit. Capacité fixe.
 * write() bloque tant qu'il manque de la place (régule le débit réseau) ; read()
 * ne bloque jamais (le thread audio ne doit pas attendre) et renvoie ce qui est
 * disponible. close() débloque proprement a l'arret.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <vector>

namespace artouste::audio {

class RingBuffer {
public:
    explicit RingBuffer(std::size_t capacity);

    /* Écrit len octets. Bloque tant qu'il n'y a pas la place, sauf si close() a
       été appelé (renvoie alors false sans tout écrire). Renvoie true si tout a
       été écrit. */
    bool write(const unsigned char* data, std::size_t len);

    /* Lit jusqu'a len octets dans out ; renvoie le nombre réellement lu (0 si le
       tampon est vide). Ne bloque jamais. */
    std::size_t read(unsigned char* out, std::size_t len);

    /* Débloque tout write() en attente et fait échouer les suivants. */
    void close();

    /* Vide le tampon et le rouvre (réutilisable pour un nouveau flux). */
    void clear();

    /* Nombre d'octets actuellement lisibles. */
    std::size_t available() const;

private:
    mutable std::mutex         m_mutex;
    std::condition_variable    m_notFull;
    std::vector<unsigned char> m_buf;
    std::size_t                m_head   = 0;  /* prochaine lecture */
    std::size_t                m_tail   = 0;  /* prochaine écriture */
    std::size_t                m_size   = 0;  /* octets présents */
    bool                       m_closed = false;
};

}  /* namespace artouste::audio */
