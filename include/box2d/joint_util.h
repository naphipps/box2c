// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "api.h"
#include "id.h"

/// Utility to compute linear stiffness values from frequency and damping ratio
B2_API void b2LinearStiffness(float* stiffness, float* damping, float frequencyHertz, float dampingRatio, b2BodyId bodyA,
								 b2BodyId bodyB);

/// Utility to compute angular stiffness values from frequency and damping ratio
B2_API void b2AngularStiffness(float* stiffness, float* damping, float frequencyHertz, float dampingRatio, b2BodyId bodyA,
								  b2BodyId bodyB);
