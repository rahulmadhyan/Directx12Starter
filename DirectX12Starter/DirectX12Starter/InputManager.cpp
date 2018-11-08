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

bool InputManager::isKeyPressed(const unsigned char keyCode)
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

void InputManager::UpdateController()
{
	DWORD dwResult;
	for (DWORD i = 0; i < 1; i++) // loop 1 since we are using only one controller for now  // in future 'XUSER_MAX_COUNT' if using multiple controllers
	{							
		XINPUT_STATE state;
		ZeroMemory(&state, sizeof(XINPUT_STATE));

		// Simply get the state of the controller from XInput.
		dwResult = XInputGetState(i, &state);

		if (dwResult == ERROR_SUCCESS)
		{
			if (previousControllerState != state.dwPacketNumber)
			{
				gameController = state.Gamepad;
			}
		}
		else
		{
			// Controller is not connected 
		}
	}
}

bool InputManager::isControllerButtonPressed(WORD keyCode)
{
	if (gameController.wButtons & keyCode)
	{
		return true;
	}

	return false;
}

SHORT InputManager::getLeftStickX()
{
	if (gameController.sThumbLX > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE || gameController.sThumbLX < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
		return gameController.sThumbLX;
	else
		return 0;
}

SHORT InputManager::getLeftStickY()
{
	if (gameController.sThumbLY > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE || gameController.sThumbLY < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
		return gameController.sThumbLY;
	else
		return 0;
}
SHORT InputManager::getRightStickX()
{
	if (gameController.sThumbRX > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE || gameController.sThumbRX < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
		return gameController.sThumbRX;
	else
		return 0;
}

SHORT InputManager::getRightStickY()
{
	if (gameController.sThumbRY > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE || gameController.sThumbRY < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
		return gameController.sThumbRY;
	else
		return 0;
}