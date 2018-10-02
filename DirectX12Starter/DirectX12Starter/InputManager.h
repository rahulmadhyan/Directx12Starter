#pragma once
#include <queue>
#include "KeyboardEvent.h"
 
class InputManager
{
public:
	InputManager();
	~InputManager();

	static InputManager& getInstance()
	{
		static InputManager* instance = NULL;
		if (instance == NULL)
		{
			instance = new InputManager;
		}
		_ASSERT(instance);
		
		return *instance;
	}

	bool KeyIsPressed(const unsigned char keyCode);
	bool KeyBufferEmpty();
	KeyboardEvent ReadKey();
	void OnKeyPressed(const unsigned char key);
	void OnKeyReleased(const unsigned char key);
	void EnableAutoRepeatKeys();
	void DisableAutoRepeatKeys();
	bool isKeysAutoRepeat();

private:
	bool autoRepeatKeys = false;
	bool keyStates[256];
	std::queue<KeyboardEvent> keyBuffer;
};

