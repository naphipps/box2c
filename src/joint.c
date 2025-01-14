// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#include "joint.h"

#include "body.h"
#include "contact.h"
#include "core.h"
#include "shape.h"
#include "solver_data.h"
#include "world.h"

// needed for dll export
#include "box2d/box2d.h"
#include "box2d/color.h"
#include "box2d/debug_draw.h"
#include "box2d/joint_types.h"
#include "box2d/joint_util.h"

// Get joint from id with validation
b2Joint* b2GetJointCheckType(b2JointId id, b2JointType type)
{
	B2_MAYBE_UNUSED(type);

	b2World* world = b2GetWorldFromIndex(id.world);
	B2_ASSERT(world->locked == false);
	if (world->locked)
	{
		return NULL;
	}

	B2_ASSERT(0 <= id.index && id.index < world->jointPool.capacity);

	b2Joint* joint = world->joints + id.index;
	B2_ASSERT(joint->object.index == joint->object.next);
	B2_ASSERT(joint->object.revision == id.revision);
	B2_ASSERT(joint->type == type);
	return joint;
}

b2Joint* b2GetJoint(b2World* world, b2JointId jointId)
{
	B2_ASSERT(0 <= jointId.index && jointId.index < world->jointPool.capacity);
	b2Joint* joint = world->joints + jointId.index;
	B2_ASSERT(b2ObjectValid(&joint->object));
	B2_ASSERT(joint->object.revision == jointId.revision);
	return joint;
}

void b2LinearStiffness(float* stiffness, float* damping, float frequencyHertz, float dampingRatio, b2BodyId bodyIdA,
					   b2BodyId bodyIdB)
{
	B2_ASSERT(bodyIdA.world == bodyIdB.world);

	b2World* world = b2GetWorldFromIndex(bodyIdA.world);
	B2_ASSERT(0 <= bodyIdA.index && bodyIdA.index < world->bodyPool.capacity);
	B2_ASSERT(0 <= bodyIdB.index && bodyIdB.index < world->bodyPool.capacity);

	b2Body* bodyA = world->bodies + bodyIdA.index;
	b2Body* bodyB = world->bodies + bodyIdB.index;

	float massA = bodyA->mass;
	float massB = bodyB->mass;
	float mass;
	if (massA > 0.0f && massB > 0.0f)
	{
		mass = massA * massB / (massA + massB);
	}
	else if (massA > 0.0f)
	{
		mass = massA;
	}
	else
	{
		mass = massB;
	}

	float omega = 2.0f * b2_pi * frequencyHertz;
	*stiffness = mass * omega * omega;
	*damping = 2.0f * mass * dampingRatio * omega;
}

void b2AngularStiffness(float* stiffness, float* damping, float frequencyHertz, float dampingRatio, b2BodyId bodyIdA,
						b2BodyId bodyIdB)
{
	B2_ASSERT(bodyIdA.world == bodyIdB.world);

	b2World* world = b2GetWorldFromIndex(bodyIdA.world);
	B2_ASSERT(0 <= bodyIdA.index && bodyIdA.index < world->bodyPool.capacity);
	B2_ASSERT(0 <= bodyIdB.index && bodyIdB.index < world->bodyPool.capacity);

	b2Body* bodyA = world->bodies + bodyIdA.index;
	b2Body* bodyB = world->bodies + bodyIdB.index;

	float IA = bodyA->I;
	float IB = bodyB->I;
	float I;
	if (IA > 0.0f && IB > 0.0f)
	{
		I = IA * IB / (IA + IB);
	}
	else if (IA > 0.0f)
	{
		I = IA;
	}
	else
	{
		I = IB;
	}

	float omega = 2.0f * b2_pi * frequencyHertz;
	*stiffness = I * omega * omega;
	*damping = 2.0f * I * dampingRatio * omega;
}

static b2Joint* b2CreateJoint(b2World* world, b2Body* bodyA, b2Body* bodyB)
{
	b2Joint* joint = (b2Joint*)b2AllocObject(&world->jointPool);
	world->joints = (b2Joint*)world->jointPool.memory;

	int32_t jointIndex = joint->object.index;

	// Doubly linked list on bodyA
	joint->edges[0].bodyIndex = bodyA->object.index;
	joint->edges[0].prevKey = B2_NULL_INDEX;
	joint->edges[0].nextKey = bodyA->jointList;

	int32_t keyA = (jointIndex << 1) | 0;
	if (bodyA->jointList != B2_NULL_INDEX)
	{
		b2Joint* jointA = world->joints + (bodyA->jointList >> 1);
		b2JointEdge* edgeA = jointA->edges + (bodyA->jointList & 1);
		edgeA->prevKey = keyA;
	}
	bodyA->jointList = keyA;
	bodyA->jointCount += 1;

	// Doubly linked list on bodyB
	joint->edges[1].bodyIndex = bodyB->object.index;
	joint->edges[1].prevKey = B2_NULL_INDEX;
	joint->edges[1].nextKey = bodyB->jointList;

	int32_t keyB = (jointIndex << 1) | 1;
	if (bodyB->jointList != B2_NULL_INDEX)
	{
		b2Joint* jointB = world->joints + (bodyB->jointList >> 1);
		b2JointEdge* edgeB = jointB->edges + (bodyB->jointList & 1);
		edgeB->prevKey = keyB;
	}
	bodyB->jointList = keyB;
	bodyB->jointCount += 1;

	joint->islandIndex = B2_NULL_INDEX;
	joint->islandPrev = B2_NULL_INDEX;
	joint->islandNext = B2_NULL_INDEX;
	joint->colorIndex = B2_NULL_INDEX;
	joint->colorSubIndex = B2_NULL_INDEX;

	joint->drawSize = 1.0f;
	joint->isMarked = false;

	if ((bodyA->type == b2_dynamicBody || bodyB->type == b2_dynamicBody) && bodyA->isEnabled == true && bodyB->isEnabled == true)
	{
		// Add edge to island graph
		b2LinkJoint(world, joint);

		if (b2IsBodyAwake(world, bodyA) || b2IsBodyAwake(world, bodyB))
		{
			b2AddJointToGraph(world, joint);
		}
	}

	return joint;
}

static void b2DestroyContactsBetweenBodies(b2World* world, b2Body* bodyA, b2Body* bodyB)
{
	int32_t contactKey;
	int32_t otherBodyIndex;

	if (bodyA->contactCount < bodyB->contactCount)
	{
		contactKey = bodyA->contactList;
		otherBodyIndex = bodyB->object.index;
	}
	else
	{
		contactKey = bodyB->contactList;
		otherBodyIndex = bodyA->object.index;
	}

	while (contactKey != B2_NULL_INDEX)
	{
		int32_t contactIndex = contactKey >> 1;
		int32_t edgeIndex = contactKey & 1;

		b2Contact* contact = world->contacts + contactIndex;
		contactKey = contact->edges[edgeIndex].nextKey;

		int32_t otherEdgeIndex = edgeIndex ^ 1;
		if (contact->edges[otherEdgeIndex].bodyIndex == otherBodyIndex)
		{
			// Careful, this removes the contact from the current doubly linked list
			b2DestroyContact(world, contact);
		}
	}
}

b2JointId b2CreateDistanceJoint(b2WorldId worldId, const b2DistanceJointDef* def)
{
	b2World* world = b2GetWorldFromId(worldId);

	B2_ASSERT(world->locked == false);

	if (world->locked)
	{
		return b2_nullJointId;
	}

	B2_ASSERT(b2IsBodyIdValid(world, def->bodyIdA));
	B2_ASSERT(b2IsBodyIdValid(world, def->bodyIdB));

	b2Body* bodyA = world->bodies + def->bodyIdA.index;
	b2Body* bodyB = world->bodies + def->bodyIdB.index;

	b2Joint* joint = b2CreateJoint(world, bodyA, bodyB);

	joint->type = b2_distanceJoint;
	joint->localAnchorA = def->localAnchorA;
	joint->localAnchorB = def->localAnchorB;
	joint->collideConnected = def->collideConnected;

	b2DistanceJoint empty = {0};
	joint->distanceJoint = empty;
	joint->distanceJoint.hertz = def->hertz;
	joint->distanceJoint.dampingRatio = def->dampingRatio;
	joint->distanceJoint.length = def->length;
	joint->distanceJoint.minLength = def->minLength;
	joint->distanceJoint.maxLength = def->maxLength;
	joint->distanceJoint.impulse = 0.0f;
	joint->distanceJoint.lowerImpulse = 0.0f;
	joint->distanceJoint.upperImpulse = 0.0f;

	// If the joint prevents collisions, then destroy all contacts between attached bodies
	if (def->collideConnected == false)
	{
		b2DestroyContactsBetweenBodies(world, bodyA, bodyB);
	}

	b2JointId jointId = {joint->object.index, world->index, joint->object.revision};
	return jointId;
}

b2JointId b2CreateMotorJoint(b2WorldId worldId, const b2MotorJointDef* def)
{
	b2World* world = b2GetWorldFromId(worldId);

	B2_ASSERT(world->locked == false);

	if (world->locked)
	{
		return b2_nullJointId;
	}

	B2_ASSERT(b2IsBodyIdValid(world, def->bodyIdA));
	B2_ASSERT(b2IsBodyIdValid(world, def->bodyIdB));

	b2Body* bodyA = world->bodies + def->bodyIdA.index;
	b2Body* bodyB = world->bodies + def->bodyIdB.index;

	b2Joint* joint = b2CreateJoint(world, bodyA, bodyB);

	joint->type = b2_motorJoint;
	joint->localAnchorA = (b2Vec2){0.0f, 0.0f};
	joint->localAnchorB = (b2Vec2){0.0f, 0.0f};
	joint->collideConnected = true;

	joint->motorJoint = (b2MotorJoint){0};
	joint->motorJoint.linearOffset = def->linearOffset;
	joint->motorJoint.angularOffset = def->angularOffset;
	joint->motorJoint.maxForce = def->maxForce;
	joint->motorJoint.maxTorque = def->maxTorque;
	joint->motorJoint.correctionFactor = B2_CLAMP(def->correctionFactor, 0.0f, 1.0f);

	b2JointId jointId = {joint->object.index, world->index, joint->object.revision};
	return jointId;
}

b2JointId b2CreateMouseJoint(b2WorldId worldId, const b2MouseJointDef* def)
{
	b2World* world = b2GetWorldFromId(worldId);

	B2_ASSERT(world->locked == false);

	if (world->locked)
	{
		return b2_nullJointId;
	}

	B2_ASSERT(b2IsBodyIdValid(world, def->bodyIdA));
	B2_ASSERT(b2IsBodyIdValid(world, def->bodyIdB));

	b2Body* bodyA = world->bodies + def->bodyIdA.index;
	b2Body* bodyB = world->bodies + def->bodyIdB.index;

	b2Joint* joint = b2CreateJoint(world, bodyA, bodyB);

	joint->type = b2_mouseJoint;
	joint->localAnchorA = b2InvTransformPoint(bodyA->transform, def->target);
	joint->localAnchorB = b2InvTransformPoint(bodyB->transform, def->target);
	joint->collideConnected = true;

	b2MouseJoint empty = {0};
	joint->mouseJoint = empty;
	joint->mouseJoint.targetA = def->target;
	joint->mouseJoint.maxForce = def->maxForce;
	joint->mouseJoint.stiffness = def->stiffness;
	joint->mouseJoint.damping = def->damping;

	b2JointId jointId = {joint->object.index, world->index, joint->object.revision};
	return jointId;
}

b2JointId b2CreateRevoluteJoint(b2WorldId worldId, const b2RevoluteJointDef* def)
{
	b2World* world = b2GetWorldFromId(worldId);

	B2_ASSERT(world->locked == false);

	if (world->locked)
	{
		return b2_nullJointId;
	}

	B2_ASSERT(b2IsBodyIdValid(world, def->bodyIdA));
	B2_ASSERT(b2IsBodyIdValid(world, def->bodyIdB));

	b2Body* bodyA = world->bodies + def->bodyIdA.index;
	b2Body* bodyB = world->bodies + def->bodyIdB.index;

	b2Joint* joint = b2CreateJoint(world, bodyA, bodyB);

	joint->type = b2_revoluteJoint;
	joint->localAnchorA = def->localAnchorA;
	joint->localAnchorB = def->localAnchorB;
	joint->collideConnected = def->collideConnected;
	joint->drawSize = def->drawSize;

	b2RevoluteJoint empty = {0};
	joint->revoluteJoint = empty;

	joint->revoluteJoint.referenceAngle = def->referenceAngle;
	joint->revoluteJoint.linearImpulse = b2Vec2_zero;
	joint->revoluteJoint.axialMass = 0.0f;
	joint->revoluteJoint.motorImpulse = 0.0f;
	joint->revoluteJoint.lowerImpulse = 0.0f;
	joint->revoluteJoint.upperImpulse = 0.0f;
	joint->revoluteJoint.lowerAngle = def->lowerAngle;
	joint->revoluteJoint.upperAngle = def->upperAngle;
	joint->revoluteJoint.maxMotorTorque = def->maxMotorTorque;
	joint->revoluteJoint.motorSpeed = def->motorSpeed;
	joint->revoluteJoint.enableLimit = def->enableLimit;
	joint->revoluteJoint.enableMotor = def->enableMotor;

	// If the joint prevents collisions, then destroy all contacts between attached bodies
	if (def->collideConnected == false)
	{
		b2DestroyContactsBetweenBodies(world, bodyA, bodyB);
	}

	b2JointId jointId = {joint->object.index, world->index, joint->object.revision};
	return jointId;
}

b2JointId b2CreatePrismaticJoint(b2WorldId worldId, const b2PrismaticJointDef* def)
{
	b2World* world = b2GetWorldFromId(worldId);

	B2_ASSERT(world->locked == false);

	if (world->locked)
	{
		return b2_nullJointId;
	}

	B2_ASSERT(b2IsBodyIdValid(world, def->bodyIdA));
	B2_ASSERT(b2IsBodyIdValid(world, def->bodyIdB));

	b2Body* bodyA = world->bodies + def->bodyIdA.index;
	b2Body* bodyB = world->bodies + def->bodyIdB.index;

	b2Joint* joint = b2CreateJoint(world, bodyA, bodyB);

	joint->type = b2_prismaticJoint;
	joint->localAnchorA = def->localAnchorA;
	joint->localAnchorB = def->localAnchorB;
	joint->collideConnected = def->collideConnected;

	b2PrismaticJoint empty = {0};
	joint->prismaticJoint = empty;

	joint->prismaticJoint.localAxisA = b2Normalize(def->localAxisA);
	joint->prismaticJoint.referenceAngle = def->referenceAngle;
	joint->prismaticJoint.impulse = b2Vec2_zero;
	joint->prismaticJoint.axialMass = 0.0f;
	joint->prismaticJoint.motorImpulse = 0.0f;
	joint->prismaticJoint.lowerImpulse = 0.0f;
	joint->prismaticJoint.upperImpulse = 0.0f;
	joint->prismaticJoint.lowerTranslation = def->lowerTranslation;
	joint->prismaticJoint.upperTranslation = def->upperTranslation;
	joint->prismaticJoint.maxMotorForce = def->maxMotorForce;
	joint->prismaticJoint.motorSpeed = def->motorSpeed;
	joint->prismaticJoint.enableLimit = def->enableLimit;
	joint->prismaticJoint.enableMotor = def->enableMotor;

	// If the joint prevents collisions, then destroy all contacts between attached bodies
	if (def->collideConnected == false)
	{
		b2DestroyContactsBetweenBodies(world, bodyA, bodyB);
	}

	b2JointId jointId = {joint->object.index, world->index, joint->object.revision};
	return jointId;
}

b2JointId b2CreateWeldJoint(b2WorldId worldId, const b2WeldJointDef* def)
{
	b2World* world = b2GetWorldFromId(worldId);

	B2_ASSERT(world->locked == false);

	if (world->locked)
	{
		return b2_nullJointId;
	}

	B2_ASSERT(b2IsBodyIdValid(world, def->bodyIdA));
	B2_ASSERT(b2IsBodyIdValid(world, def->bodyIdB));

	b2Body* bodyA = world->bodies + def->bodyIdA.index;
	b2Body* bodyB = world->bodies + def->bodyIdB.index;

	b2Joint* joint = b2CreateJoint(world, bodyA, bodyB);

	joint->type = b2_weldJoint;
	joint->localAnchorA = def->localAnchorA;
	joint->localAnchorB = def->localAnchorB;
	joint->collideConnected = def->collideConnected;

	b2WeldJoint empty = {0};
	joint->weldJoint = empty;
	joint->weldJoint.referenceAngle = def->referenceAngle;
	joint->weldJoint.linearHertz = def->linearHertz;
	joint->weldJoint.linearDampingRatio = def->linearDampingRatio;
	joint->weldJoint.angularHertz = def->angularHertz;
	joint->weldJoint.angularDampingRatio = def->angularDampingRatio;
	joint->weldJoint.linearImpulse = b2Vec2_zero;
	joint->weldJoint.angularImpulse = 0.0f;

	// If the joint prevents collisions, then destroy all contacts between attached bodies
	if (def->collideConnected == false)
	{
		b2DestroyContactsBetweenBodies(world, bodyA, bodyB);
	}

	b2JointId jointId = {joint->object.index, world->index, joint->object.revision};
	return jointId;
}

b2JointId b2CreateWheelJoint(b2WorldId worldId, const b2WheelJointDef* def)
{
	b2World* world = b2GetWorldFromId(worldId);

	B2_ASSERT(world->locked == false);

	if (world->locked)
	{
		return b2_nullJointId;
	}

	B2_ASSERT(b2IsBodyIdValid(world, def->bodyIdA));
	B2_ASSERT(b2IsBodyIdValid(world, def->bodyIdB));

	b2Body* bodyA = world->bodies + def->bodyIdA.index;
	b2Body* bodyB = world->bodies + def->bodyIdB.index;

	b2Joint* joint = b2CreateJoint(world, bodyA, bodyB);

	joint->type = b2_wheelJoint;
	joint->localAnchorA = def->localAnchorA;
	joint->localAnchorB = def->localAnchorB;
	joint->collideConnected = def->collideConnected;

	// todo test this
	joint->wheelJoint = (b2WheelJoint){0};

	joint->wheelJoint.localAxisA = b2Normalize(def->localAxisA);
	joint->wheelJoint.perpMass = 0.0f;
	joint->wheelJoint.axialMass = 0.0f;
	joint->wheelJoint.motorImpulse = 0.0f;
	joint->wheelJoint.lowerImpulse = 0.0f;
	joint->wheelJoint.upperImpulse = 0.0f;
	joint->wheelJoint.lowerTranslation = def->lowerTranslation;
	joint->wheelJoint.upperTranslation = def->upperTranslation;
	joint->wheelJoint.maxMotorTorque = def->maxMotorTorque;
	joint->wheelJoint.motorSpeed = def->motorSpeed;
	joint->wheelJoint.stiffness = def->stiffness;
	joint->wheelJoint.damping = def->damping;
	joint->wheelJoint.enableLimit = def->enableLimit;
	joint->wheelJoint.enableMotor = def->enableMotor;

	// If the joint prevents collisions, then destroy all contacts between attached bodies
	if (def->collideConnected == false)
	{
		b2DestroyContactsBetweenBodies(world, bodyA, bodyB);
	}

	b2JointId jointId = {joint->object.index, world->index, joint->object.revision};
	return jointId;
}

void b2DestroyJoint(b2JointId jointId)
{
	b2World* world = b2GetWorldFromIndex(jointId.world);
	B2_ASSERT(world->locked == false);

	if (world->locked)
	{
		return;
	}

	B2_ASSERT(0 <= jointId.index && jointId.index < world->jointPool.capacity);

	b2Joint* joint = world->joints + jointId.index;

	B2_ASSERT(0 <= joint->edges[0].bodyIndex && joint->edges[0].bodyIndex < world->bodyPool.capacity);
	B2_ASSERT(0 <= joint->edges[1].bodyIndex && joint->edges[1].bodyIndex < world->bodyPool.capacity);

	b2JointEdge* edgeA = joint->edges + 0;
	b2JointEdge* edgeB = joint->edges + 1;

	b2Body* bodyA = world->bodies + edgeA->bodyIndex;
	b2Body* bodyB = world->bodies + edgeB->bodyIndex;

	// Remove from body A
	if (edgeA->prevKey != B2_NULL_INDEX)
	{
		b2Joint* prevJoint = world->joints + (edgeA->prevKey >> 1);
		b2JointEdge* prevEdge = prevJoint->edges + (edgeA->prevKey & 1);
		prevEdge->nextKey = edgeA->nextKey;
	}

	if (edgeA->nextKey != B2_NULL_INDEX)
	{
		b2Joint* nextJoint = world->joints + (edgeA->nextKey >> 1);
		b2JointEdge* nextEdge = nextJoint->edges + (edgeA->nextKey & 1);
		nextEdge->prevKey = edgeA->prevKey;
	}

	int32_t edgeKeyA = (joint->object.index << 1) | 0;
	if (bodyA->jointList == edgeKeyA)
	{
		bodyA->jointList = edgeA->nextKey;
	}

	bodyA->jointCount -= 1;

	// Remove from body B
	if (edgeB->prevKey != B2_NULL_INDEX)
	{
		b2Joint* prevJoint = world->joints + (edgeB->prevKey >> 1);
		b2JointEdge* prevEdge = prevJoint->edges + (edgeB->prevKey & 1);
		prevEdge->nextKey = edgeB->nextKey;
	}

	if (edgeB->nextKey != B2_NULL_INDEX)
	{
		b2Joint* nextJoint = world->joints + (edgeB->nextKey >> 1);
		b2JointEdge* nextEdge = nextJoint->edges + (edgeB->nextKey & 1);
		nextEdge->prevKey = edgeB->prevKey;
	}

	int32_t edgeKeyB = (joint->object.index << 1) | 1;
	if (bodyB->jointList == edgeKeyB)
	{
		bodyB->jointList = edgeB->nextKey;
	}

	bodyB->jointCount -= 1;

	b2UnlinkJoint(world, joint);

	b2RemoveJointFromGraph(world, joint);

	b2FreeObject(&world->jointPool, &joint->object);
}

b2JointType b2Joint_GetType(b2JointId jointId)
{
	b2World* world = b2GetWorldFromIndex(jointId.world);
	b2Joint* joint = b2GetJoint(world, jointId);
	return joint->type;
}

b2BodyId b2Joint_GetBodyA(b2JointId jointId)
{
	b2World* world = b2GetWorldFromIndex(jointId.world);
	b2Joint* joint = b2GetJoint(world, jointId);

	int32_t bodyIndex = joint->edges[0].bodyIndex;
	B2_ASSERT(0 <= bodyIndex && bodyIndex < world->bodyPool.capacity);
	b2Body* body = world->bodies + bodyIndex;
	b2BodyId bodyId = {bodyIndex, jointId.world, body->object.revision};
	return bodyId;
}

b2BodyId b2Joint_GetBodyB(b2JointId jointId)
{
	b2World* world = b2GetWorldFromIndex(jointId.world);
	b2Joint* joint = b2GetJoint(world, jointId);

	int32_t bodyIndex = joint->edges[1].bodyIndex;
	B2_ASSERT(0 <= bodyIndex && bodyIndex < world->bodyPool.capacity);
	b2Body* body = world->bodies + bodyIndex;
	b2BodyId bodyId = {bodyIndex, jointId.world, body->object.revision};
	return bodyId;
}

extern void b2PrepareDistanceJoint(b2Joint* base, b2StepContext* context);
extern void b2PrepareMotorJoint(b2Joint* base, b2StepContext* context);
extern void b2PrepareMouseJoint(b2Joint* base, b2StepContext* context);
extern void b2PreparePrismaticJoint(b2Joint* base, b2StepContext* context);
extern void b2PrepareRevoluteJoint(b2Joint* base, b2StepContext* context);
extern void b2PrepareWeldJoint(b2Joint* base, b2StepContext* context);
extern void b2PrepareWheelJoint(b2Joint* base, b2StepContext* context);

void b2PrepareJoint(b2Joint* joint, b2StepContext* context)
{
	switch (joint->type)
	{
		case b2_distanceJoint:
			b2PrepareDistanceJoint(joint, context);
			break;

		case b2_motorJoint:
			b2PrepareMotorJoint(joint, context);
			break;

		case b2_mouseJoint:
			b2PrepareMouseJoint(joint, context);
			break;

		case b2_prismaticJoint:
			b2PreparePrismaticJoint(joint, context);
			break;

		case b2_revoluteJoint:
			b2PrepareRevoluteJoint(joint, context);
			break;

		case b2_weldJoint:
			b2PrepareWeldJoint(joint, context);
			break;

		case b2_wheelJoint:
			b2PrepareWheelJoint(joint, context);
			break;

		default:
			B2_ASSERT(false);
	}
}

extern void b2WarmStartDistanceJoint(b2Joint* base, b2StepContext* context);
extern void b2WarmStartMotorJoint(b2Joint* base, b2StepContext* context);
extern void b2WarmStartMouseJoint(b2Joint* base, b2StepContext* context);
extern void b2WarmStartPrismaticJoint(b2Joint* base, b2StepContext* context);
extern void b2WarmStartRevoluteJoint(b2Joint* base, b2StepContext* context);
extern void b2WarmStartWeldJoint(b2Joint* base, b2StepContext* context);
extern void b2WarmStartWheelJoint(b2Joint* base, b2StepContext* context);

void b2WarmStartJoint(b2Joint* joint, b2StepContext* context)
{
	switch (joint->type)
	{
		case b2_distanceJoint:
			b2WarmStartDistanceJoint(joint, context);
			break;

		case b2_motorJoint:
			b2WarmStartMotorJoint(joint, context);
			break;

		case b2_mouseJoint:
			b2WarmStartMouseJoint(joint, context);
			break;

		case b2_prismaticJoint:
			b2WarmStartPrismaticJoint(joint, context);
			break;

		case b2_revoluteJoint:
			b2WarmStartRevoluteJoint(joint, context);
			break;

		case b2_weldJoint:
			b2WarmStartWeldJoint(joint, context);
			break;

		case b2_wheelJoint:
			b2WarmStartWheelJoint(joint, context);
			break;

		default:
			B2_ASSERT(false);
	}
}

extern void b2SolveDistanceJoint(b2Joint* base, b2StepContext* context, bool useBias);
extern void b2SolveMotorJoint(b2Joint* base, b2StepContext* context, bool useBias);
extern void b2SolveMouseJoint(b2Joint* base, b2StepContext* context);
extern void b2SolvePrismaticJoint(b2Joint* base, b2StepContext* context, bool useBias);
extern void b2SolveRevoluteJoint(b2Joint* base, b2StepContext* context, bool useBias);
extern void b2SolveWeldJoint(b2Joint* base, b2StepContext* context, bool useBias);
extern void b2SolveWheelJoint(b2Joint* base, b2StepContext* context, bool useBias);

void b2SolveJoint(b2Joint* joint, b2StepContext* context, bool useBias)
{
	switch (joint->type)
	{
		case b2_distanceJoint:
			b2SolveDistanceJoint(joint, context, useBias);
			break;

		case b2_motorJoint:
			b2SolveMotorJoint(joint, context, useBias);
			break;

		case b2_mouseJoint:
			if (useBias)
			{
				b2SolveMouseJoint(joint, context);
			}
			break;

		case b2_prismaticJoint:
			b2SolvePrismaticJoint(joint, context, useBias);
			break;

		case b2_revoluteJoint:
			b2SolveRevoluteJoint(joint, context, useBias);
			break;

		case b2_weldJoint:
			b2SolveWeldJoint(joint, context, useBias);
			break;

		case b2_wheelJoint:
			b2SolveWheelJoint(joint, context, useBias);
			break;

		default:
			B2_ASSERT(false);
	}
}

void b2PrepareAndWarmStartOverflowJoints(b2SolverTaskContext* context)
{
	b2TracyCZoneNC(prepare_joints, "PrepJoints", b2_colorOldLace, true);

	b2World* world = context->world;
	b2Graph* graph = context->graph;
	b2Joint* joints = world->joints;
	b2StepContext* stepContext = context->stepContext;
	int32_t* jointIndices = graph->overflow.jointArray;
	int32_t jointCount = b2Array(graph->overflow.jointArray).count;
	bool enableWarmStarting = world->enableWarmStarting;

	for (int32_t i = 0; i < jointCount; ++i)
	{
		int32_t index = jointIndices[i];
		B2_ASSERT(0 <= index && index < world->jointPool.capacity);

		b2Joint* joint = joints + index;
		B2_ASSERT(b2ObjectValid(&joint->object) == true);

		b2PrepareJoint(joint, stepContext);

		if (enableWarmStarting)
		{
			b2WarmStartJoint(joint, stepContext);
		}
	}

	b2TracyCZoneEnd(prepare_joints);
}

void b2SolveOverflowJoints(b2SolverTaskContext* context, bool useBias)
{
	b2TracyCZoneNC(solve_joints, "SolveJoints", b2_colorLemonChiffon, true);

	b2World* world = context->world;
	b2Graph* graph = context->graph;
	b2Joint* joints = world->joints;
	b2StepContext* stepContext = context->stepContext;
	int32_t* jointIndices = graph->overflow.jointArray;
	int32_t jointCount = b2Array(graph->overflow.jointArray).count;

	for (int32_t i = 0; i < jointCount; ++i)
	{
		int32_t index = jointIndices[i];
		B2_ASSERT(0 <= index && index < world->jointPool.capacity);

		b2Joint* joint = joints + index;
		B2_ASSERT(b2ObjectValid(&joint->object) == true);

		b2SolveJoint(joint, stepContext, useBias);
	}

	b2TracyCZoneEnd(solve_joints);
}

b2JointId b2Body_GetFirstJoint(b2BodyId bodyId)
{
	b2World* world = b2GetWorldFromIndex(bodyId.world);
	b2Body* body = b2GetBody(world, bodyId);

	if (body->jointList == B2_NULL_INDEX)
	{
		return b2_nullJointId;
	}

	b2Joint* joint = world->joints + body->jointList;
	b2JointId id = {joint->object.index, bodyId.world, joint->object.revision};
	return id;
}

b2JointId b2Body_GetNextJoint(b2BodyId bodyId, b2JointId jointId)
{
	b2World* world = b2GetWorldFromIndex(bodyId.world);
	b2Body* body = b2GetBody(world, bodyId);
	b2Joint* joint = b2GetJoint(world, jointId);

	if (joint->edges[0].bodyIndex == body->object.index)
	{
		if (joint->edges[0].nextKey == B2_NULL_INDEX)
		{
			return b2_nullJointId;
		}

		joint = world->joints + (joint->edges[0].nextKey >> 1);
	}
	else
	{
		B2_ASSERT(joint->edges[1].bodyIndex == body->object.index);

		if (joint->edges[1].nextKey == B2_NULL_INDEX)
		{
			return b2_nullJointId;
		}

		joint = world->joints + (joint->edges[1].nextKey >> 1);
	}

	b2JointId id = {joint->object.index, bodyId.world, joint->object.revision};
	return id;
}

extern void b2DrawDistance(b2DebugDraw* draw, b2Joint* base, b2Body* bodyA, b2Body* bodyB);
extern void b2DrawPrismatic(b2DebugDraw* draw, b2Joint* base, b2Body* bodyA, b2Body* bodyB);
extern void b2DrawRevolute(b2DebugDraw* draw, b2Joint* base, b2Body* bodyA, b2Body* bodyB);
extern void b2DrawWheelJoint(b2DebugDraw* draw, b2Joint* base, b2Body* bodyA, b2Body* bodyB);

void b2DrawJoint(b2DebugDraw* draw, b2World* world, b2Joint* joint)
{
	b2Body* bodyA = world->bodies + joint->edges[0].bodyIndex;
	b2Body* bodyB = world->bodies + joint->edges[1].bodyIndex;
	if (bodyA->isEnabled == false || bodyB->isEnabled == false)
	{
		return;
	}

	b2Transform xfA = bodyA->transform;
	b2Transform xfB = bodyB->transform;
	b2Vec2 pA = b2TransformPoint(bodyA->transform, joint->localAnchorA);
	b2Vec2 pB = b2TransformPoint(bodyB->transform, joint->localAnchorB);

	b2Color color = {0.5f, 0.8f, 0.8f, 1.0f};

	switch (joint->type)
	{
		case b2_distanceJoint:
			b2DrawDistance(draw, joint, bodyA, bodyB);
			break;

			// case b2_pulleyJoint:
			//{
			//	b2PulleyJoint* pulley = (b2PulleyJoint*)this;
			//	b2Vec2 sA = pulley->GetGroundAnchorA();
			//	b2Vec2 sB = pulley->GetGroundAnchorB();
			//	draw->DrawSegment(sA, pA, color);
			//	draw->DrawSegment(sB, pB, color);
			//	draw->DrawSegment(sA, sB, color);
			// }
			// break;

		case b2_mouseJoint:
		{
			b2Vec2 target = joint->mouseJoint.targetA;

			b2Color c1 = {0.0f, 1.0f, 0.0f, 1.0f};
			draw->DrawPoint(target, 4.0f, c1, draw->context);
			draw->DrawPoint(pB, 4.0f, c1, draw->context);

			b2Color c2 = {0.8f, 0.8f, 0.8f, 1.0f};
			draw->DrawSegment(target, pB, c2, draw->context);
		}
		break;

		case b2_prismaticJoint:
			b2DrawPrismatic(draw, joint, bodyA, bodyB);
			break;

		case b2_revoluteJoint:
			b2DrawRevolute(draw, joint, bodyA, bodyB);
			break;

		case b2_wheelJoint:
			b2DrawWheelJoint(draw, joint, bodyA, bodyB);
			break;

		default:
			draw->DrawSegment(xfA.p, pA, color, draw->context);
			draw->DrawSegment(pA, pB, color, draw->context);
			draw->DrawSegment(xfB.p, pB, color, draw->context);
	}

	if (draw->drawGraphColors)
	{
		b2HexColor colors[b2_graphColorCount + 1] = {
			b2_colorRed,  b2_colorOrange,	 b2_colorYellow,	b2_colorGreen, b2_colorCyan, b2_colorBlue, b2_colorViolet,
			b2_colorPink, b2_colorChocolate, b2_colorGoldenrod, b2_colorCoral, b2_colorAqua, b2_colorBlack};

		if (joint->colorIndex != B2_NULL_INDEX)
		{
			b2Vec2 p = b2Lerp(pA, pB, 0.5f);
			draw->DrawPoint(p, 5.0f, b2MakeColor(colors[joint->colorIndex], 1.0f), draw->context);
		}
	}
}
