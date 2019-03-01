#pragma once
#include "SystemData.h"
#include "Entity.h"

class Waypoints
{
public:
	Waypoints();
	Waypoints(SystemData *systemData);
	~Waypoints();

private:
	SystemData* systemData;
};