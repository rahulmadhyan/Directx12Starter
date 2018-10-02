#include "InputManager.h"
   
InputManager::InputManager()
{
	for (int i = 0; i < 256; i++)
	{
		keyStates[i] = false;
	}
}

InputManager::~InputManager()
{

}

bool InputManager::KeyIsPressed(const unsigned char keyCode)
{
	return keyStates[keyCode];
}

bool InputManager::KeyBufferEmpty()
{
	return keyBuffer.empty();
}

KeyboardEvent InputManager::ReadKey()
{
	if (keyBuffer.empty())
	{
		return KeyboardEvent();
	}
	else
	{
		KeyboardEvent e = keyBuffer.front();
		keyBuffer.pop();
		return e;
	}
}

void InputManager::OnKeyPressed(const unsigned char key)
{
	keyStates[key] = true;
	keyBuffer.push(KeyboardEvent(KeyboardEvent::EventType::Press, key));
}

void InputManager::OnKeyReleased(const unsigned char key)
{
	keyStates[key] = false;
	keyBuffer.push(KeyboardEvent(KeyboardEvent::EventType::Release, key));
}

void InputManager::EnableAutoRepeatKeys()
{
	autoRepeatKeys = true;
}

void InputManager::DisableAutoRepeatKeys()
{
	autoRepeatKeys = false;
}

bool InputManager::isKeysAutoRepeat()
{
	return autoRepeatKeys;
}