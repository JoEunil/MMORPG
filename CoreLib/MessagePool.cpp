#include "pch.h"

#include "MessagePool.h"
#include "Message.h"
#include "Config.h"


namespace Core {
	void MessagePool::Initialize() {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_messages.reserve(MAX_MSGPOOL_SIZE);
		for (int i = 0; i < TARGET_MSGPOOL_SIZE; i++)
		{
			Message* message = new Message(MESSGAGE_LEN);
			m_messages.push_back(message);
		};
	}
	MessagePool::~MessagePool() {
		std::lock_guard<std::mutex> lock(m_mutex);
		for (Message* msg : m_messages) {
			delete msg;
		}
		m_messages.clear();
	}

	void MessagePool::Adjust()
	{
		Decrease();
		Increase();
	}

	void MessagePool::Increase() {
		auto current = m_messages.size();

		if (current <= MIN_MSGPOOL_SIZE) {
			while (current < TARGET_MSGPOOL_SIZE)
			{
				Message* message = new Message(MESSGAGE_LEN);
				m_messages.push_back(message);
				current++;
			}
		}
	}

	void MessagePool::Decrease() {
		auto current = m_messages.size();

		if (current >= MAX_MSGPOOL_SIZE) {
			while (current > TARGET_MSGPOOL_SIZE)
			{
				Message* temp = m_messages.back();
				m_messages.pop_back();
				delete temp;
				current--;
			}
		}
	}

	Message* MessagePool::Acquire()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_messages.empty()) {
			Adjust();
		}
		Message* msg = m_messages.back();
		m_messages.pop_back();
		Adjust();
		return msg;
	}

	void MessagePool::Return(Message* msg) {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_messages.push_back(msg);
		Adjust();
	}

}
