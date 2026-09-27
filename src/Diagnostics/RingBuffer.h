#pragma once

#include "LogEntry.h"
#include <vector>
#include <mutex>

namespace TSA::Diagnostics
{

/**
 * @brief Tampon circulaire thread-safe conservant les N derniers événements en mémoire.
 * Utilisé pour reconstruire la séquence exacte d'actions avant un crash.
 */
template <size_t Capacity = 100, typename T = LogEntry>
class RingBuffer
{
public:
    RingBuffer()
    {
        m_buffer.resize(Capacity);
    }

    void push(const T& entry)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_buffer[m_head] = entry;
        m_head = (m_head + 1) % Capacity;
        if (m_count < Capacity)
        {
            m_count++;
        }
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_head = 0;
        m_count = 0;
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_count == 0;
    }

    size_t size() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_count;
    }

    static constexpr size_t capacity()
    {
        return Capacity;
    }

    /**
     * @brief Retourne une copie ordonnée chronologiquement des éléments présents.
     */
    std::vector<T> snapshot() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<T> result;
        result.reserve(m_count);

        if (m_count < Capacity)
        {
            for (size_t i = 0; i < m_count; ++i)
            {
                result.push_back(m_buffer[i]);
            }
        }
        else
        {
            // Le buffer a débordé : commencer à m_head (le plus ancien) jusqu'à la fin
            for (size_t i = 0; i < Capacity; ++i)
            {
                size_t idx = (m_head + i) % Capacity;
                result.push_back(m_buffer[idx]);
            }
        }
        return result;
    }

private:
    std::vector<T> m_buffer;
    size_t m_head = 0;
    size_t m_count = 0;
    mutable std::mutex m_mutex;
};

} // namespace TSA::Diagnostics
