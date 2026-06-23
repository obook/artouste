/*
 * RingBuffer.cpp
 * Implémentation du tampon circulaire thread-safe (voir RingBuffer.hpp).
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "audio/RingBuffer.hpp"

#include <algorithm>

namespace artouste::audio {

RingBuffer::RingBuffer(std::size_t capacity) : m_buf(capacity) {}

bool RingBuffer::write(const unsigned char* data, std::size_t len) {
    std::size_t written = 0;
    while (written < len) {
        std::unique_lock<std::mutex> lock(m_mutex);
        /* Attend qu'il y ait de la place, ou que le tampon soit fermé. */
        m_notFull.wait(lock, [this] { return m_closed || m_size < m_buf.size(); });
        if (m_closed) {
            return false;
        }
        /* Copie autant que possible d'un coup (jusqu'au bout du tableau ou de la
           place libre, le plus petit des deux). */
        while (written < len && m_size < m_buf.size()) {
            const std::size_t freeToEnd = std::min(m_buf.size() - m_tail,
                                                   m_buf.size() - m_size);
            const std::size_t chunk     = std::min(len - written, freeToEnd);
            std::copy(data + written, data + written + chunk, m_buf.begin() + static_cast<std::ptrdiff_t>(m_tail));
            m_tail = (m_tail + chunk) % m_buf.size();
            m_size += chunk;
            written += chunk;
        }
    }
    return true;
}

std::size_t RingBuffer::read(unsigned char* out, std::size_t len) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::size_t readCount = 0;
    while (readCount < len && m_size > 0) {
        const std::size_t avail     = std::min(m_buf.size() - m_head, m_size);
        const std::size_t chunk     = std::min(len - readCount, avail);
        std::copy(m_buf.begin() + static_cast<std::ptrdiff_t>(m_head),
                  m_buf.begin() + static_cast<std::ptrdiff_t>(m_head + chunk),
                  out + readCount);
        m_head = (m_head + chunk) % m_buf.size();
        m_size -= chunk;
        readCount += chunk;
    }
    if (readCount > 0) {
        m_notFull.notify_one();  /* de la place s'est libérée */
    }
    return readCount;
}

void RingBuffer::close() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_closed = true;
    }
    m_notFull.notify_all();
}

void RingBuffer::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_head   = 0;
    m_tail   = 0;
    m_size   = 0;
    m_closed = false;
}

std::size_t RingBuffer::available() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_size;
}

}  /* namespace artouste::audio */
