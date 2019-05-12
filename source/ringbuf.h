#pragma once

template <typename T>
class RingBuf
{
	T m_size;
	T m_consumer;
	T m_producer;
	T m_inFlight;
public:
	constexpr RingBuf(T size) noexcept : m_size{size}, m_consumer{}, m_producer{}, m_inFlight{} { }
	constexpr RingBuf() noexcept : RingBuf{0} { }
	void lateInit(T size) noexcept { m_size = size; }

	constexpr T getSize()     const noexcept { return m_size;     }
	constexpr T getConsumer() const noexcept { return m_consumer; }
	constexpr T getProducer() const noexcept { return m_producer; }
	constexpr T getInFlight() const noexcept { return m_inFlight; }

	T reserve(T& pos, T required) noexcept
	{
		T available = m_size - m_inFlight;
		if ((m_producer + available) > m_size)
		{
			T wasted = m_size - m_producer;
			if (available < (required + wasted))
				return 0;
			m_producer = 0;
			m_inFlight += wasted;
			available -= wasted;
		}
		else if (available < required)
			return 0;
		pos = m_producer;
		return available;
	}

	void updateProducer(T producer) noexcept
	{
		if (producer >= m_size)
			producer -= m_size;
		T produced = producer - m_producer;
		if (producer < m_producer)
			produced += m_size;
		m_producer = producer;
		m_inFlight += produced;
	}

	void updateConsumer(T consumer) noexcept
	{
		if (consumer >= m_size)
			consumer -= m_size;
		T consumed = consumer - m_consumer;
		if (consumer < m_consumer)
			consumed += m_size;
		m_consumer = consumer;
		m_inFlight -= consumed;
	}

	bool getFirstInFlight(T& pos) const noexcept
	{
		if (!m_inFlight)
			return false;
		pos = m_consumer;
		return true;
	}

	void consumeOne()
	{
		updateConsumer(m_consumer+1);
	}
};
