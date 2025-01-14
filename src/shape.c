// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#include "shape.h"

#include "body.h"
#include "broad_phase.h"
#include "contact.h"
#include "world.h"

// needed for dll export
#include "box2d/box2d.h"
#include "box2d/event_types.h"

b2AABB b2ComputeShapeAABB(const b2Shape* shape, b2Transform xf)
{
	switch (shape->type)
	{
		case b2_capsuleShape:
			return b2ComputeCapsuleAABB(&shape->capsule, xf);
		case b2_circleShape:
			return b2ComputeCircleAABB(&shape->circle, xf);
		case b2_polygonShape:
			return b2ComputePolygonAABB(&shape->polygon, xf);
		case b2_segmentShape:
			return b2ComputeSegmentAABB(&shape->segment, xf);
		case b2_smoothSegmentShape:
			return b2ComputeSegmentAABB(&shape->smoothSegment.segment, xf);
		default:
		{
			B2_ASSERT(false);
			b2AABB empty = {xf.p, xf.p};
			return empty;
		}
	}
}

b2Vec2 b2GetShapeCentroid(const b2Shape* shape)
{
	switch (shape->type)
	{
		case b2_capsuleShape:
			return b2Lerp(shape->capsule.point1, shape->capsule.point2, 0.5f);
		case b2_circleShape:
			return shape->circle.point;
		case b2_polygonShape:
			return shape->polygon.centroid;
		case b2_segmentShape:
			return b2Lerp(shape->segment.point1, shape->segment.point2, 0.5f);
		case b2_smoothSegmentShape:
			return b2Lerp(shape->smoothSegment.segment.point1, shape->smoothSegment.segment.point2, 0.5f);
		default:
			return b2Vec2_zero;
	}
}

b2MassData b2ComputeShapeMass(const b2Shape* shape)
{
	switch (shape->type)
	{
		case b2_capsuleShape:
			return b2ComputeCapsuleMass(&shape->capsule, shape->density);
		case b2_circleShape:
			return b2ComputeCircleMass(&shape->circle, shape->density);
		case b2_polygonShape:
			return b2ComputePolygonMass(&shape->polygon, shape->density);
		default:
		{
			return (b2MassData){0};
		}
	}
}

b2ShapeExtent b2ComputeShapeExtent(const b2Shape* shape)
{
	b2ShapeExtent extent = {0};

	switch (shape->type)
	{
		case b2_capsuleShape:
		{
			float radius = shape->capsule.radius;
			extent.minExtent = radius;
			extent.maxExtent = B2_MAX(b2Length(shape->capsule.point1), b2Length(shape->capsule.point2)) + radius;
		}
		break;

		case b2_circleShape:
		{
			float radius = shape->circle.radius;
			extent.minExtent = radius;
			extent.maxExtent = b2Length(shape->circle.point) + radius;
		}
		break;

		case b2_polygonShape:
		{
			const b2Polygon* poly = &shape->polygon;
			float minExtent = b2_huge;
			float maxExtent = 0.0f;
			int32_t count = poly->count;
			for (int32_t i = 0; i < count; ++i)
			{
				float planeOffset = b2Dot(poly->normals[i], b2Sub(poly->vertices[i], poly->centroid));
				minExtent = B2_MIN(minExtent, planeOffset);

				float distanceSqr = b2LengthSquared(poly->vertices[i]);
				maxExtent = B2_MAX(maxExtent, distanceSqr);
			}

			extent.minExtent = minExtent + poly->radius;
			extent.maxExtent = maxExtent + poly->radius;
		}
		break;

		default:
			break;
	}

	return extent;
}

b2RayCastOutput b2RayCastShape(const b2RayCastInput* input, const b2Shape* shape, b2Transform xf)
{
	b2RayCastInput localInput = *input;
	localInput.origin = b2InvTransformPoint(xf, input->origin);
	localInput.translation = b2InvRotateVector(xf.q, input->translation);

	b2RayCastOutput output = {0};
	switch (shape->type)
	{
		case b2_capsuleShape:
			output = b2RayCastCapsule(&localInput, &shape->capsule);
			break;
		case b2_circleShape:
			output = b2RayCastCircle(&localInput, &shape->circle);
			break;
		case b2_polygonShape:
			output = b2RayCastPolygon(&localInput, &shape->polygon);
			break;
		case b2_segmentShape:
			output = b2RayCastSegment(&localInput, &shape->segment, false);
			break;
		case b2_smoothSegmentShape:
			output = b2RayCastSegment(&localInput, &shape->smoothSegment.segment, true);
			break;
		default:
			return output;
	}

	output.point = b2TransformPoint(xf, output.point);
	output.normal = b2RotateVector(xf.q, output.normal);
	return output;
}

b2RayCastOutput b2ShapeCastShape(const b2ShapeCastInput* input, const b2Shape* shape, b2Transform xf)
{
	b2ShapeCastInput localInput = *input;

	for (int i = 0; i < localInput.count; ++i)
	{
		localInput.points[i] = b2InvTransformPoint(xf, input->points[i]);
	}

	localInput.translation = b2InvRotateVector(xf.q, input->translation);

	b2RayCastOutput output = {0};
	switch (shape->type)
	{
		case b2_capsuleShape:
			output = b2ShapeCastCapsule(&localInput, &shape->capsule);
			break;
		case b2_circleShape:
			output = b2ShapeCastCircle(&localInput, &shape->circle);
			break;
		case b2_polygonShape:
			output = b2ShapeCastPolygon(&localInput, &shape->polygon);
			break;
		case b2_segmentShape:
			output = b2ShapeCastSegment(&localInput, &shape->segment);
			break;
		case b2_smoothSegmentShape:
			output = b2ShapeCastSegment(&localInput, &shape->smoothSegment.segment);
			break;
		default:
			return output;
	}

	output.point = b2TransformPoint(xf, output.point);
	output.normal = b2RotateVector(xf.q, output.normal);
	return output;
}

void b2CreateShapeProxy(b2Shape* shape, b2BroadPhase* bp, b2BodyType type, b2Transform xf)
{
	B2_ASSERT(shape->proxyKey == B2_NULL_INDEX);

	// Create proxies in the broad-phase.
	shape->aabb = b2ComputeShapeAABB(shape, xf);

	// Smaller margin for static bodies. Cannot be zero due to TOI tolerance.
	float margin = type == b2_staticBody ? 4.0f * b2_linearSlop : b2_aabbMargin;
	shape->fatAABB.lowerBound.x = shape->aabb.lowerBound.x - margin;
	shape->fatAABB.lowerBound.y = shape->aabb.lowerBound.y - margin;
	shape->fatAABB.upperBound.x = shape->aabb.upperBound.x + margin;
	shape->fatAABB.upperBound.y = shape->aabb.upperBound.y + margin;

	shape->proxyKey = b2BroadPhase_CreateProxy(bp, type, shape->fatAABB, shape->filter.categoryBits, shape->object.index);
	B2_ASSERT(B2_PROXY_TYPE(shape->proxyKey) < b2_bodyTypeCount);
}

void b2DestroyShapeProxy(b2Shape* shape, b2BroadPhase* bp)
{
	if (shape->proxyKey != B2_NULL_INDEX)
	{
		b2BroadPhase_DestroyProxy(bp, shape->proxyKey);
		shape->proxyKey = B2_NULL_INDEX;
	}
}

b2DistanceProxy b2MakeShapeDistanceProxy(const b2Shape* shape)
{
	switch (shape->type)
	{
		case b2_capsuleShape:
			return b2MakeProxy(&shape->capsule.point1, 2, shape->capsule.radius);
		case b2_circleShape:
			return b2MakeProxy(&shape->circle.point, 1, shape->circle.radius);
		case b2_polygonShape:
			return b2MakeProxy(shape->polygon.vertices, shape->polygon.count, shape->polygon.radius);
		case b2_segmentShape:
			return b2MakeProxy(&shape->segment.point1, 2, 0.0f);
		case b2_smoothSegmentShape:
			return b2MakeProxy(&shape->smoothSegment.segment.point1, 2, 0.0f);
		default:
		{
			B2_ASSERT(false);
			b2DistanceProxy empty = {0};
			return empty;
		}
	}
}

b2Shape* b2GetShape(b2World* world, b2ShapeId shapeId)
{
	B2_ASSERT(0 <= shapeId.index && shapeId.index < world->shapePool.capacity);
	b2Shape* shape = world->shapes + shapeId.index;
	B2_ASSERT(b2ObjectValid(&shape->object));
	B2_ASSERT(shape->object.revision == shapeId.revision);
	return shape;
}

b2BodyId b2Shape_GetBody(b2ShapeId shapeId)
{
	b2World* world = b2GetWorldFromIndex(shapeId.world);
	b2Shape* shape = b2GetShape(world, shapeId);

	b2Body* body = world->bodies + shape->bodyIndex;
	B2_ASSERT(b2ObjectValid(&body->object));

	b2BodyId bodyId = {body->object.index, shapeId.world, body->object.revision};
	return bodyId;
}

void* b2Shape_GetUserData(b2ShapeId shapeId)
{
	b2World* world = b2GetWorldFromIndex(shapeId.world);
	b2Shape* shape = b2GetShape(world, shapeId);
	return shape->userData;
}

bool b2Shape_IsSensor(b2ShapeId shapeId)
{
	b2World* world = b2GetWorldFromIndex(shapeId.world);
	b2Shape* shape = b2GetShape(world, shapeId);
	return shape->isSensor;
}

bool b2Shape_TestPoint(b2ShapeId shapeId, b2Vec2 point)
{
	b2World* world = b2GetWorldFromIndex(shapeId.world);
	B2_ASSERT(0 <= shapeId.index && shapeId.index < world->shapePool.capacity);
	b2Shape* shape = world->shapes + shapeId.index;
	B2_ASSERT(b2ObjectValid(&shape->object));

	B2_ASSERT(0 <= shape->bodyIndex && shape->bodyIndex < world->bodyPool.capacity);
	b2Body* body = world->bodies + shape->bodyIndex;
	B2_ASSERT(b2ObjectValid(&body->object));

	b2Vec2 localPoint = b2InvTransformPoint(body->transform, point);

	switch (shape->type)
	{
		case b2_capsuleShape:
			return b2PointInCapsule(localPoint, &shape->capsule);

		case b2_circleShape:
			return b2PointInCircle(localPoint, &shape->circle);

		case b2_polygonShape:
			return b2PointInPolygon(localPoint, &shape->polygon);

		default:
			return false;
	}
}

void b2Shape_SetDensity(b2ShapeId shapeId, float density)
{
	B2_ASSERT(b2IsValid(density) && density >= 0.0f);

	b2World* world = b2GetWorldFromIndex(shapeId.world);
	B2_ASSERT(world->locked == false);
	if (world->locked)
	{
		return;
	}
	
	b2Shape* shape = b2GetShape(world, shapeId);
	if (density == shape->density)
	{
		// early return to avoid expensive function
		return;
	}

	shape->density = density;

	b2Body* body = world->bodies + shape->bodyIndex;
	B2_ASSERT(b2ObjectValid(&body->object));

	b2UpdateBodyMassData(world, body);
}

float b2Shape_GetDensity(b2ShapeId shapeId)
{
	b2World* world = b2GetWorldFromIndex(shapeId.world);
	b2Shape* shape = b2GetShape(world, shapeId);
	return shape->density;
}

void b2Shape_SetFriction(b2ShapeId shapeId, float friction)
{
	B2_ASSERT(b2IsValid(friction) && friction >= 0.0f);

	b2World* world = b2GetWorldFromIndex(shapeId.world);
	B2_ASSERT(world->locked == false);
	if (world->locked)
	{
		return;
	}

	b2Shape* shape = b2GetShape(world, shapeId);
	shape->friction = friction;
}

float b2Shape_GetFriction(b2ShapeId shapeId)
{
	b2World* world = b2GetWorldFromIndex(shapeId.world);
	b2Shape* shape = b2GetShape(world, shapeId);
	return shape->friction;
}

void b2Shape_SetRestitution(b2ShapeId shapeId, float restitution)
{
	B2_ASSERT(b2IsValid(restitution) && restitution >= 0.0f);

	b2World* world = b2GetWorldFromIndex(shapeId.world);
	B2_ASSERT(world->locked == false);
	if (world->locked)
	{
		return;
	}

	b2Shape* shape = b2GetShape(world, shapeId);
	shape->restitution = restitution;
}

float b2Shape_GetRestitution(b2ShapeId shapeId)
{
	b2World* world = b2GetWorldFromIndex(shapeId.world);
	b2Shape* shape = b2GetShape(world, shapeId);
	return shape->restitution;
}

b2Filter b2Shape_GetFilter(b2ShapeId shapeId)
{
	b2World* world = b2GetWorldFromIndex(shapeId.world);
	b2Shape* shape = b2GetShape(world, shapeId);
	return shape->filter;
}

void b2Shape_SetFilter(b2ShapeId shapeId, b2Filter filter)
{
	b2World* world = b2GetWorldFromIndex(shapeId.world);
	b2Shape* shape = b2GetShape(world, shapeId);
	shape->filter = filter;

	b2Body* body = world->bodies + shape->bodyIndex;
	B2_ASSERT(b2ObjectValid(&body->object));

	// Destroy any contacts associated with the shape
	int32_t contactKey = body->contactList;
	while (contactKey != B2_NULL_INDEX)
	{
		int32_t contactIndex = contactKey >> 1;
		int32_t edgeIndex = contactKey & 1;

		b2Contact* contact = world->contacts + contactIndex;
		contactKey = contact->edges[edgeIndex].nextKey;

		if (contact->shapeIndexA == shapeId.index || contact->shapeIndexB == shapeId.index)
		{
			b2DestroyContact(world, contact);
		}
	}

	if (body->isEnabled)
	{
		b2DestroyShapeProxy(shape, &world->broadPhase);
		b2CreateShapeProxy(shape, &world->broadPhase, body->type, body->transform);
	}
	else
	{
		B2_ASSERT(shape->proxyKey == B2_NULL_INDEX);
	}
}

b2ShapeType b2Shape_GetType(b2ShapeId shapeId)
{
	b2World* world = b2GetWorldFromIndex(shapeId.world);
	b2Shape* shape = b2GetShape(world, shapeId);
	return shape->type;
}

const b2Circle* b2Shape_GetCircle(b2ShapeId shapeId)
{
	b2World* world = b2GetWorldFromIndex(shapeId.world);
	b2Shape* shape = b2GetShape(world, shapeId);
	B2_ASSERT(shape->type == b2_circleShape);
	return &shape->circle;
}

const b2Segment* b2Shape_GetSegment(b2ShapeId shapeId)
{
	b2World* world = b2GetWorldFromIndex(shapeId.world);
	b2Shape* shape = b2GetShape(world, shapeId);
	B2_ASSERT(shape->type == b2_segmentShape);
	return &shape->segment;
}

const b2SmoothSegment* b2Shape_GetSmoothSegment(b2ShapeId shapeId)
{
	b2World* world = b2GetWorldFromIndex(shapeId.world);
	b2Shape* shape = b2GetShape(world, shapeId);
	B2_ASSERT(shape->type == b2_smoothSegmentShape);
	return &shape->smoothSegment;
}

const b2Capsule* b2Shape_GetCapsule(b2ShapeId shapeId)
{
	b2World* world = b2GetWorldFromIndex(shapeId.world);
	b2Shape* shape = b2GetShape(world, shapeId);
	B2_ASSERT(shape->type == b2_capsuleShape);
	return &shape->capsule;
}

const b2Polygon* b2Shape_GetPolygon(b2ShapeId shapeId)
{
	b2World* world = b2GetWorldFromIndex(shapeId.world);
	b2Shape* shape = b2GetShape(world, shapeId);
	B2_ASSERT(shape->type == b2_polygonShape);
	return &shape->polygon;
}

b2ChainId b2Shape_GetParentChain(b2ShapeId shapeId)
{
	b2World* world = b2GetWorldFromIndex(shapeId.world);
	b2Shape* shape = b2GetShape(world, shapeId);
	if (shape->type == b2_smoothSegmentShape)
	{
		int32_t chainIndex = shape->smoothSegment.chainIndex;
		if (chainIndex != B2_NULL_INDEX)
		{
			B2_ASSERT(0 <= chainIndex && chainIndex < world->chainPool.capacity);
			b2ChainShape* chain = world->chains + chainIndex;
			B2_ASSERT(b2ObjectValid(&chain->object));
			b2ChainId chainId = {chainIndex, shapeId.world, chain->object.revision};
			return chainId;
		}
	}

	return b2_nullChainId;
}

void b2Chain_SetFriction(b2ChainId chainId, float friction)
{
	b2World* world = b2GetWorldFromIndex(chainId.world);
	B2_ASSERT(world->locked == false);
	if (world->locked)
	{
		return;
	}

	B2_ASSERT(0 <= chainId.index && chainId.index < world->chainPool.count);

	b2ChainShape* chainShape = world->chains + chainId.index;
	B2_ASSERT(chainShape->object.revision == chainId.revision);

	int32_t count = chainShape->count;

	for (int32_t i = 0; i < count; ++i)
	{
		int32_t shapeIndex = chainShape->shapeIndices[i];
		B2_ASSERT(0 <= shapeIndex && shapeIndex < world->shapePool.count);
		b2Shape* shape = world->shapes + shapeIndex;
		shape->friction = friction;
	}
}

void b2Chain_SetRestitution(b2ChainId chainId, float restitution)
{
	b2World* world = b2GetWorldFromIndex(chainId.world);
	B2_ASSERT(world->locked == false);
	if (world->locked)
	{
		return;
	}

	B2_ASSERT(0 <= chainId.index && chainId.index < world->chainPool.count);

	b2ChainShape* chainShape = world->chains + chainId.index;
	B2_ASSERT(chainShape->object.revision == chainId.revision);

	int32_t count = chainShape->count;

	for (int32_t i = 0; i < count; ++i)
	{
		int32_t shapeIndex = chainShape->shapeIndices[i];
		B2_ASSERT(0 <= shapeIndex && shapeIndex < world->shapePool.count);
		b2Shape* shape = world->shapes + shapeIndex;
		shape->restitution = restitution;
	}
}

int32_t b2Shape_GetContactCapacity(b2ShapeId shapeId)
{
	b2World* world = b2GetWorldFromIndex(shapeId.world);
	B2_ASSERT(world->locked == false);
	if (world->locked)
	{
		return 0;
	}

	b2Shape* shape = b2GetShape(world, shapeId);
	b2Body* body = world->bodies + shape->bodyIndex;

	// Conservative and fast
	return body->contactCount;
}

int32_t b2Shape_GetContactData(b2ShapeId shapeId, b2ContactData* contactData, int32_t capacity)
{
	b2World* world = b2GetWorldFromIndex(shapeId.world);
	B2_ASSERT(world->locked == false);
	if (world->locked)
	{
		return 0;
	}

	b2Shape* shape = b2GetShape(world, shapeId);

	b2Body* body = world->bodies + shape->bodyIndex;
	int32_t contactKey = body->contactList;
	int32_t index = 0;
	while (contactKey != B2_NULL_INDEX && index < capacity)
	{
		int32_t contactIndex = contactKey >> 1;
		int32_t edgeIndex = contactKey & 1;

		b2Contact* contact = world->contacts + contactIndex;

		// Does contact involve this shape and is it touching?
		if ((contact->shapeIndexA == shapeId.index || contact->shapeIndexB == shapeId.index) &&
			(contact->flags & b2_contactTouchingFlag) != 0)
		{
			b2Shape* shapeA = world->shapes + contact->shapeIndexA;
			b2Shape* shapeB = world->shapes + contact->shapeIndexB;

			contactData[index].shapeIdA = (b2ShapeId){shapeA->object.index, shapeId.world, shapeA->object.revision};
			contactData[index].shapeIdB = (b2ShapeId){shapeB->object.index, shapeId.world, shapeB->object.revision};
			contactData[index].manifold = contact->manifold;
			index += 1;
		}

		contactKey = contact->edges[edgeIndex].nextKey;
	}

	B2_ASSERT(index < capacity);

	return index;
}

b2AABB b2Shape_GetAABB(b2ShapeId shapeId)
{
	b2World* world = b2GetWorldFromIndexLocked(shapeId.world);
	if (world == NULL)
	{
		return (b2AABB){0};
	}

	b2Shape* shape = b2GetShape(world, shapeId);
	return shape->aabb;
}
