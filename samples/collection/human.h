// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "box2d/types.h"

struct Bone
{
	enum
	{
		e_hip = 0,
		e_torso = 1,
		e_head = 2,
		e_upperLeftLeg = 3,
		e_lowerLeftLeg = 4,
		e_upperRightLeg = 5,
		e_lowerRightLeg = 6,
		e_upperLeftArm = 7,
		e_lowerLeftArm = 8,
		e_upperRightArm = 9,
		e_lowerRightArm = 10,
		e_count = 11,
	};

	b2BodyId bodyId;
	b2JointId jointId;
	int parentIndex;
};

class Human
{
  public:
	Human();

	void Spawn(b2WorldId worldId, b2Vec2 position, float scale, int groupIndex);
	void Despawn();

	b2WorldId m_worldId;
	Bone m_bones[Bone::e_count];
	bool m_isSpawned;
};
