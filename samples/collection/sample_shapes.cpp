// SPDX-FileCopyrightText: 2022 Erin Catto
// SPDX-License-Identifier: MIT

#include "sample.h"
#include "settings.h"

#include "box2d/box2d.h"
#include "box2d/geometry.h"
#include "box2d/hull.h"
#include "box2d/math.h"

#include <GLFW/glfw3.h>
#include <imgui.h>

struct RayCastContext
{
	b2Vec2 point;
	b2Vec2 normal;
	bool hit;
};

static float RayCastClosestCallback(b2ShapeId shapeId, b2Vec2 point, b2Vec2 normal, float fraction, void* context)
{
	RayCastContext* rayContext = (RayCastContext*)context;
	rayContext->point = point;
	rayContext->normal = normal;
	rayContext->hit = true;
	return fraction;
}

class ChainShape : public Sample
{
public:
	enum ShapeType
	{
		e_circleShape = 0,
		e_capsuleShape,
		e_boxShape
	};

	ChainShape(const Settings& settings)
		: Sample(settings)
	{
		if (settings.restart == false)
		{
			g_camera.m_center = {0.0f, 0.0f};
			g_camera.m_zoom = 1.75f;
		}

		m_groundId = b2_nullBodyId;
		m_bodyId = b2_nullBodyId;
		m_chainId = b2_nullChainId;
		m_shapeId = b2_nullShapeId;
		m_shapeType = e_circleShape;
		m_restitution = 0.0f;
		m_friction = 0.2f;
		CreateScene();
		Launch();
	}

	void CreateScene()
	{
		if (B2_NON_NULL(m_groundId))
		{
			b2DestroyBody(m_groundId);
		}

		// https://betravis.github.io/shape-tools/path-to-polygon/
		// b2Vec2 points[] = {{-20.58325, 14.54175}, {-21.90625, 15.8645},		 {-24.552, 17.1875},
		//				   {-27.198, 11.89575},	  {-29.84375, 15.8645},		 {-29.84375, 21.15625},
		//				   {-25.875, 23.802},	  {-20.58325, 25.125},		 {-25.875, 29.09375},
		//				   {-20.58325, 31.7395},  {-11.0089998, 23.2290001}, {-8.67700005, 21.15625},
		//				   {-6.03125, 21.15625},  {-7.35424995, 29.09375},	 {-3.38549995, 29.09375},
		//				   {1.90625, 30.41675},	  {5.875, 17.1875},			 {11.16675, 25.125},
		//				   {9.84375, 29.09375},	  {13.8125, 31.7395},		 {21.75, 30.41675},
		//				   {28.3644981, 26.448},  {25.71875, 18.5105},		 {24.3957481, 13.21875},
		//				   {17.78125, 11.89575},  {15.1355, 7.92700005},	 {5.875, 9.25},
		//				   {1.90625, 11.89575},	  {-3.25, 11.89575},		 {-3.25, 9.9375},
		//				   {-4.70825005, 9.25},	  {-8.67700005, 9.25},		 {-11.323, 11.89575},
		//				   {-13.96875, 11.89575}, {-15.29175, 14.54175},	 {-19.2605, 14.54175}};

		b2Vec2 points[] = {
			{-56.885498, 12.8985004},	{-56.885498, 16.2057495},	{56.885498, 16.2057495},   {56.885498, -16.2057514},
			{51.5935059, -16.2057514},	{43.6559982, -10.9139996},	{35.7184982, -10.9139996}, {27.7809982, -10.9139996},
			{21.1664963, -14.2212505},	{11.9059982, -16.2057514},	{0, -16.2057514},		   {-10.5835037, -14.8827496},
			{-17.1980019, -13.5597477}, {-21.1665001, -12.2370014}, {-25.1355019, -9.5909977}, {-31.75, -3.63799858},
			{-38.3644981, 6.2840004},	{-42.3334999, 9.59125137},	{-47.625, 11.5755005},	   {-56.885498, 12.8985004},
		};

		int count = sizeof(points) / sizeof(points[0]);

		// float scale = 0.25f;
		// b2Vec2 lower = {FLT_MAX, FLT_MAX};
		// b2Vec2 upper = {-FLT_MAX, -FLT_MAX};
		// for (int i = 0; i < count; ++i)
		//{
		//	points[i].x = 2.0f * scale * points[i].x;
		//	points[i].y = -scale * points[i].y;

		//	lower = b2Min(lower, points[i]);
		//	upper = b2Max(upper, points[i]);
		//}

		// b2Vec2 center = b2MulSV(0.5f, b2Add(lower, upper));
		// for (int i = 0; i < count; ++i)
		//{
		//	points[i] = b2Sub(points[i], center);
		// }

		// for (int i = 0; i < count / 2; ++i)
		//{
		//	b2Vec2 temp = points[i];
		//	points[i] = points[count - 1 - i];
		//	points[count - 1 - i] = temp;
		// }

		// printf("{");
		// for (int i = 0; i < count; ++i)
		//{
		//	printf("{%.9g, %.9g},", points[i].x, points[i].y);
		// }
		// printf("};\n");

		b2ChainDef chainDef = b2_defaultChainDef;
		chainDef.points = points;
		chainDef.count = count;
		chainDef.loop = true;
		chainDef.friction = 0.2f;

		b2BodyDef bodyDef = b2_defaultBodyDef;
		m_groundId = b2CreateBody(m_worldId, &bodyDef);

		m_chainId = b2CreateChain(m_groundId, &chainDef);
	}

	void Launch()
	{
		if (B2_NON_NULL(m_bodyId))
		{
			b2DestroyBody(m_bodyId);
		}

		b2BodyDef bodyDef = b2_defaultBodyDef;
		bodyDef.type = b2_dynamicBody;
		bodyDef.position = {-55.0f, 13.5f};
		m_bodyId = b2CreateBody(m_worldId, &bodyDef);

		b2ShapeDef shapeDef = b2_defaultShapeDef;
		shapeDef.density = 1.0f;
		shapeDef.friction = m_friction;
		shapeDef.restitution = m_restitution;

		if (m_shapeType == e_circleShape)
		{
			b2Circle circle = {{0.0f, 0.0f}, 0.5f};
			m_shapeId = b2CreateCircleShape(m_bodyId, &shapeDef, &circle);
		}
		else if (m_shapeType == e_capsuleShape)
		{
			b2Capsule capsule = {{-0.5f, 0.0f}, {0.5f, 0.0}, 0.25f};
			m_shapeId = b2CreateCapsuleShape(m_bodyId, &shapeDef, &capsule);
		}
		else
		{
			float h = 0.5f;
			b2Polygon box = b2MakeBox(h, h);
			m_shapeId = b2CreatePolygonShape(m_bodyId, &shapeDef, &box);
		}
	}

	void UpdateUI() override
	{
		ImGui::SetNextWindowPos(ImVec2(10.0f, 200.0f), ImGuiCond_Once);
		ImGui::SetNextWindowSize(ImVec2(280.0f, 125.0f));
		ImGui::Begin("Options", nullptr, ImGuiWindowFlags_NoResize);

		const char* shapeTypes[] = {"Circle", "Capsule", "Box"};
		int shapeType = int(m_shapeType);
		if (ImGui::Combo("Shape", &shapeType, shapeTypes, IM_ARRAYSIZE(shapeTypes)))
		{
			m_shapeType = ShapeType(shapeType);
			Launch();
		}

		if (ImGui::SliderFloat("Friction", &m_friction, 0.0f, 1.0f, "%.2f"))
		{
			b2Shape_SetFriction(m_shapeId, m_friction);
			b2Chain_SetFriction(m_chainId, m_friction);
		}

		if (ImGui::SliderFloat("Restitution", &m_restitution, 0.0f, 2.0f, "%.1f"))
		{
			b2Shape_SetRestitution(m_shapeId, m_restitution);
		}

		if (ImGui::Button("Launch"))
		{
			Launch();
		}

		ImGui::End();
	}

	void Step(Settings& settings) override
	{
		Sample::Step(settings);

		g_draw.DrawSegment(b2Vec2_zero, {0.5f, 0.0f}, b2MakeColor(b2_colorRed, 1.0f));
		g_draw.DrawSegment(b2Vec2_zero, {0.0f, 0.5f}, b2MakeColor(b2_colorGreen, 1.0f));
	}

	static Sample* Create(const Settings& settings)
	{
		return new ChainShape(settings);
	}

	b2BodyId m_groundId;
	b2BodyId m_bodyId;
	b2ChainId m_chainId;
	ShapeType m_shapeType;
	b2ShapeId m_shapeId;
	float m_restitution;
	float m_friction;
};

static int sampleChainShape = RegisterSample("Shapes", "Chain Shape", ChainShape::Create);

// This sample shows how careful creation of compound shapes leads to better simulation and avoids
// objects getting stuck.
// This also shows how to get the combined AABB for the body.
class CompoundShapes : public Sample
{
public:
	CompoundShapes(const Settings& settings)
		: Sample(settings)
	{
		if (settings.restart == false)
		{
			g_camera.m_center = {0.0f, 0.0f};
			g_camera.m_zoom = 0.5f;
		}

		{
			b2BodyId groundId = b2CreateBody(m_worldId, &b2_defaultBodyDef);
			b2Segment segment = {{50.0f, 0.0f}, {-50.0f, 0.0f}};
			b2CreateSegmentShape(groundId, &b2_defaultShapeDef, &segment);
		}

		// Table 1
		{
			b2BodyDef bodyDef = b2_defaultBodyDef;
			bodyDef.type = b2_dynamicBody;
			bodyDef.position = {-15.0f, 1.0f};
			m_table1Id = b2CreateBody(m_worldId, &bodyDef);

			b2Polygon top = b2MakeOffsetBox(3.0f, 0.5f, {0.0f, 3.5f}, 0.0f);
			b2Polygon leftLeg = b2MakeOffsetBox(0.5f, 1.5f, {-2.5f, 1.5f}, 0.0f);
			b2Polygon rightLeg = b2MakeOffsetBox(0.5f, 1.5f, {2.5f, 1.5f}, 0.0f);

			b2CreatePolygonShape(m_table1Id, &b2_defaultShapeDef, &top);
			b2CreatePolygonShape(m_table1Id, &b2_defaultShapeDef, &leftLeg);
			b2CreatePolygonShape(m_table1Id, &b2_defaultShapeDef, &rightLeg);
		}

		// Table 2
		{
			b2BodyDef bodyDef = b2_defaultBodyDef;
			bodyDef.type = b2_dynamicBody;
			bodyDef.position = {-5.0f, 1.0f};
			m_table2Id = b2CreateBody(m_worldId, &bodyDef);

			b2Polygon top = b2MakeOffsetBox(3.0f, 0.5f, {0.0f, 3.5f}, 0.0f);
			b2Polygon leftLeg = b2MakeOffsetBox(0.5f, 2.0f, {-2.5f, 2.0f}, 0.0f);
			b2Polygon rightLeg = b2MakeOffsetBox(0.5f, 2.0f, {2.5f, 2.0f}, 0.0f);

			b2CreatePolygonShape(m_table2Id, &b2_defaultShapeDef, &top);
			b2CreatePolygonShape(m_table2Id, &b2_defaultShapeDef, &leftLeg);
			b2CreatePolygonShape(m_table2Id, &b2_defaultShapeDef, &rightLeg);
		}

		// Spaceship 1
		{
			b2BodyDef bodyDef = b2_defaultBodyDef;
			bodyDef.type = b2_dynamicBody;
			bodyDef.position = {5.0f, 1.0f};
			m_ship1Id = b2CreateBody(m_worldId, &bodyDef);

			b2Vec2 vertices[3];

			vertices[0] = {-2.0f, 0.0f};
			vertices[1] = {0.0f, 4.0f / 3.0f};
			vertices[2] = {0.0f, 4.0f};
			b2Hull hull = b2ComputeHull(vertices, 3);
			b2Polygon left = b2MakePolygon(&hull, 0.0f);

			vertices[0] = {2.0f, 0.0f};
			vertices[1] = {0.0f, 4.0f / 3.0f};
			vertices[2] = {0.0f, 4.0f};
			hull = b2ComputeHull(vertices, 3);
			b2Polygon right = b2MakePolygon(&hull, 0.0f);

			b2CreatePolygonShape(m_ship1Id, &b2_defaultShapeDef, &left);
			b2CreatePolygonShape(m_ship1Id, &b2_defaultShapeDef, &right);
		}

		// Spaceship 2
		{
			b2BodyDef bodyDef = b2_defaultBodyDef;
			bodyDef.type = b2_dynamicBody;
			bodyDef.position = {15.0f, 1.0f};
			m_ship2Id = b2CreateBody(m_worldId, &bodyDef);

			b2Vec2 vertices[3];

			vertices[0] = {-2.0f, 0.0f};
			vertices[1] = {1.0f, 2.0f};
			vertices[2] = {0.0f, 4.0f};
			b2Hull hull = b2ComputeHull(vertices, 3);
			b2Polygon left = b2MakePolygon(&hull, 0.0f);

			vertices[0] = {2.0f, 0.0f};
			vertices[1] = {-1.0f, 2.0f};
			vertices[2] = {0.0f, 4.0f};
			hull = b2ComputeHull(vertices, 3);
			b2Polygon right = b2MakePolygon(&hull, 0.0f);

			b2CreatePolygonShape(m_ship2Id, &b2_defaultShapeDef, &left);
			b2CreatePolygonShape(m_ship2Id, &b2_defaultShapeDef, &right);
		}

		m_drawBodyAABBs = false;
	}

	void Spawn()
	{
		// Table 1 obstruction
		{
			b2BodyDef bodyDef = b2_defaultBodyDef;
			bodyDef.type = b2_dynamicBody;
			bodyDef.position = b2Body_GetPosition(m_table1Id);
			bodyDef.angle = b2Body_GetAngle(m_table1Id);
			b2BodyId bodyId = b2CreateBody(m_worldId, &bodyDef);

			b2Polygon box = b2MakeOffsetBox(4.0f, 0.1f, {0.0f, 3.0f}, 0.0f);
			b2CreatePolygonShape(bodyId, &b2_defaultShapeDef, &box);
		}

		// Table 2 obstruction
		{
			b2BodyDef bodyDef = b2_defaultBodyDef;
			bodyDef.type = b2_dynamicBody;
			bodyDef.position = b2Body_GetPosition(m_table2Id);
			bodyDef.angle = b2Body_GetAngle(m_table2Id);
			b2BodyId bodyId = b2CreateBody(m_worldId, &bodyDef);

			b2Polygon box = b2MakeOffsetBox(4.0f, 0.1f, {0.0f, 3.0f}, 0.0f);
			b2CreatePolygonShape(bodyId, &b2_defaultShapeDef, &box);
		}

		// Ship 1 obstruction
		{
			b2BodyDef bodyDef = b2_defaultBodyDef;
			bodyDef.type = b2_dynamicBody;
			bodyDef.position = b2Body_GetPosition(m_ship1Id);
			bodyDef.angle = b2Body_GetAngle(m_ship1Id);
			//bodyDef.gravityScale = 0.0f;
			b2BodyId bodyId = b2CreateBody(m_worldId, &bodyDef);

			b2Circle circle = {{0.0f, 2.0f}, 0.5f};
			b2CreateCircleShape(bodyId, &b2_defaultShapeDef, &circle);
		}

		// Ship 2 obstruction
		{
			b2BodyDef bodyDef = b2_defaultBodyDef;
			bodyDef.type = b2_dynamicBody;
			bodyDef.position = b2Body_GetPosition(m_ship2Id);
			bodyDef.angle = b2Body_GetAngle(m_ship2Id);
			//bodyDef.gravityScale = 0.0f;
			b2BodyId bodyId = b2CreateBody(m_worldId, &bodyDef);

			b2Circle circle = {{0.0f, 2.0f}, 0.5f};
			b2CreateCircleShape(bodyId, &b2_defaultShapeDef, &circle);
		}
	}

	void UpdateUI() override
	{
		ImGui::SetNextWindowPos(ImVec2(10.0f, 100.0f));
		ImGui::SetNextWindowSize(ImVec2(200.0f, 100.0f));
		ImGui::Begin("Compound Shapes", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

		if (ImGui::Button("Intrude"))
		{
			Spawn();
		}

		ImGui::Checkbox("Body AABBs", &m_drawBodyAABBs);

		ImGui::End();
	}

	void Step(Settings& settings) override
	{
		Sample::Step(settings);

		if (m_drawBodyAABBs)
		{
			b2AABB aabb;

			b2Color yellow = b2MakeColor(b2_colorYellow3, 0.5f);

			aabb = b2Body_ComputeAABB(m_table1Id);
			g_draw.DrawAABB(aabb, yellow);
			
			aabb = b2Body_ComputeAABB(m_table2Id);
			g_draw.DrawAABB(aabb, yellow);
			
			aabb = b2Body_ComputeAABB(m_ship1Id);
			g_draw.DrawAABB(aabb, yellow);
			
			aabb = b2Body_ComputeAABB(m_ship2Id);
			g_draw.DrawAABB(aabb, yellow);
		}
	}



	static Sample* Create(const Settings& settings)
	{
		return new CompoundShapes(settings);
	}

	b2BodyId m_table1Id;
	b2BodyId m_table2Id;
	b2BodyId m_ship1Id;
	b2BodyId m_ship2Id;
	bool m_drawBodyAABBs;
};

static int sampleCompoundShape = RegisterSample("Shapes", "Compound Shapes", CompoundShapes::Create);

class ShapeFilter : public Sample
{
public:

	enum CollisionBits
	{
		GROUND = 0x00000001,
		TEAM1 = 0x00000002,
		TEAM2 = 0x00000004,
		TEAM3 = 0x00000008,

		ALL_BITS = (~0u)
	};

	ShapeFilter(const Settings& settings)
		: Sample(settings)
	{
		if (settings.restart == false)
		{
			g_camera.m_zoom = 0.5f;
			g_camera.m_center = {0.0f, 5.0f};
		}

		b2BodyId groundId;
		{
			groundId = b2CreateBody(m_worldId, &b2_defaultBodyDef);
			b2Segment segment = {{-20.0f, 0.0f}, {20.0f, 0.0f}};

			b2ShapeDef shapeDef = b2_defaultShapeDef;
			shapeDef.filter.categoryBits = GROUND;
			shapeDef.filter.maskBits = ALL_BITS;

			b2CreateSegmentShape(groundId, &b2_defaultShapeDef, &segment);
		}

		// Define motorized body
		{
			b2BodyDef bodyDef = b2_defaultBodyDef;
			bodyDef.type = b2_dynamicBody;
			
			bodyDef.position = {0.0f, 4.0f};
			m_player1Id = b2CreateBody(m_worldId, &bodyDef);

			bodyDef.position = {0.0f, 8.0f};
			m_player2Id = b2CreateBody(m_worldId, &bodyDef);

			bodyDef.position = {0.0f, 12.0f};
			m_player3Id = b2CreateBody(m_worldId, &bodyDef);
			
			b2Polygon box = b2MakeBox(2.0f, 1.0f);

			b2ShapeDef shapeDef = b2_defaultShapeDef;

			shapeDef.filter.categoryBits = TEAM1;
			shapeDef.filter.maskBits = GROUND | TEAM2 | TEAM3;
			m_shape1Id = b2CreatePolygonShape(m_player1Id, &shapeDef, &box);

			shapeDef.filter.categoryBits = TEAM2;
			shapeDef.filter.maskBits = GROUND | TEAM1 | TEAM3;
			m_shape2Id = b2CreatePolygonShape(m_player2Id, &shapeDef, &box);

			shapeDef.filter.categoryBits = TEAM3;
			shapeDef.filter.maskBits = GROUND | TEAM1 | TEAM2;
			m_shape3Id = b2CreatePolygonShape(m_player3Id, &shapeDef, &box);
		}
	}

	void UpdateUI() override
	{
		ImGui::SetNextWindowPos(ImVec2(10.0f, 100.0f));
		ImGui::SetNextWindowSize(ImVec2(250.0f, 240.0f));
		ImGui::Begin("Shape Filter", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

		ImGui::Text("Player 1 Collides With");
		{
			b2Filter filter1 = b2Shape_GetFilter(m_shape1Id);
			bool team2 = (filter1.maskBits & TEAM2) == TEAM2;
			if (ImGui::Checkbox("Team 2##1", &team2))
			{
				if (team2)
				{
					filter1.maskBits |= TEAM2;
				}
				else
				{
					filter1.maskBits &= ~TEAM2;
				}

				b2Shape_SetFilter(m_shape1Id, filter1);
			}

			bool team3 = (filter1.maskBits & TEAM3) == TEAM3;
			if (ImGui::Checkbox("Team 3##1", &team3))
			{
				if (team3)
				{
					filter1.maskBits |= TEAM3;
				}
				else
				{
					filter1.maskBits &= ~TEAM3;
				}

				b2Shape_SetFilter(m_shape1Id, filter1);
			}
		}

		ImGui::Separator();

		ImGui::Text("Player 2 Collides With");
		{
			b2Filter filter2 = b2Shape_GetFilter(m_shape2Id);
			bool team1 = (filter2.maskBits & TEAM1) == TEAM1;
			if (ImGui::Checkbox("Team 1##2", &team1))
			{
				if (team1)
				{
					filter2.maskBits |= TEAM1;
				}
				else
				{
					filter2.maskBits &= ~TEAM1;
				}

				b2Shape_SetFilter(m_shape2Id, filter2);
			}

			bool team3 = (filter2.maskBits & TEAM3) == TEAM3;
			if (ImGui::Checkbox("Team 3##2", &team3))
			{
				if (team3)
				{
					filter2.maskBits |= TEAM3;
				}
				else
				{
					filter2.maskBits &= ~TEAM3;
				}

				b2Shape_SetFilter(m_shape2Id, filter2);
			}
		}

		ImGui::Separator();

		ImGui::Text("Player 3 Collides With");
		{
			b2Filter filter3 = b2Shape_GetFilter(m_shape3Id);
			bool team1 = (filter3.maskBits & TEAM1) == TEAM1;
			if (ImGui::Checkbox("Team 1##3", &team1))
			{
				if (team1)
				{
					filter3.maskBits |= TEAM1;
				}
				else
				{
					filter3.maskBits &= ~TEAM1;
				}

				b2Shape_SetFilter(m_shape3Id, filter3);
			}

			bool team2 = (filter3.maskBits & TEAM2) == TEAM2;
			if (ImGui::Checkbox("Team 2##3", &team2))
			{
				if (team2)
				{
					filter3.maskBits |= TEAM2;
				}
				else
				{
					filter3.maskBits &= ~TEAM2;
				}

				b2Shape_SetFilter(m_shape3Id, filter3);
			}
		}

		ImGui::End();
	}

	void Step(Settings& settings) override
	{
		Sample::Step(settings);

		b2Vec2 p1 = b2Body_GetPosition(m_player1Id);
		g_draw.DrawString({p1.x - 0.5f, p1.y}, "player 1");

		b2Vec2 p2 = b2Body_GetPosition(m_player2Id);
		g_draw.DrawString({p2.x - 0.5f, p2.y}, "player 2");

		b2Vec2 p3 = b2Body_GetPosition(m_player3Id);
		g_draw.DrawString({p3.x - 0.5f, p3.y}, "player 3");
	}

	static Sample* Create(const Settings& settings)
	{
		return new ShapeFilter(settings);
	}

	b2BodyId m_player1Id;
	b2BodyId m_player2Id;
	b2BodyId m_player3Id;

	b2ShapeId m_shape1Id;
	b2ShapeId m_shape2Id;
	b2ShapeId m_shape3Id;
};

static int sampleShapeFilter = RegisterSample("Shapes", "Filter", ShapeFilter::Create);

// Restitution is approximate since Box2D uses speculative collision
class Restitution : public Sample
{
public:
	enum
	{
		e_count = 40
	};

	enum ShapeType
	{
		e_circleShape = 0,
		e_boxShape
	};

	Restitution(const Settings& settings)
		: Sample(settings)
	{
		{
			b2BodyDef bodyDef = b2_defaultBodyDef;
			b2BodyId groundId = b2CreateBody(m_worldId, &bodyDef);

			float h = 1.0f * e_count;
			b2Segment segment = {{-h, 0.0f}, {h, 0.0f}};
			b2ShapeDef shapeDef = b2_defaultShapeDef;
			b2CreateSegmentShape(groundId, &shapeDef, &segment);
		}

		for (int32_t i = 0; i < e_count; ++i)
		{
			m_bodyIds[i] = b2_nullBodyId;
		}

		m_shapeType = e_circleShape;

		CreateBodies();
	}

	void CreateBodies()
	{
		for (int32_t i = 0; i < e_count; ++i)
		{
			if (B2_NON_NULL(m_bodyIds[i]))
			{
				b2DestroyBody(m_bodyIds[i]);
				m_bodyIds[i] = b2_nullBodyId;
			}
		}

		b2Circle circle = {0};
		circle.radius = 0.5f;

		b2Polygon box = b2MakeBox(0.5f, 0.5f);

		b2ShapeDef shapeDef = b2_defaultShapeDef;
		shapeDef.density = 1.0f;
		shapeDef.restitution = 0.0f;

		b2BodyDef bodyDef = b2_defaultBodyDef;
		bodyDef.type = b2_dynamicBody;

		float dr = 1.0f / (e_count > 1 ? e_count - 1 : 1);
		float x = -1.0f * (e_count - 1);
		float dx = 2.0f;

		for (int32_t i = 0; i < e_count; ++i)
		{
			bodyDef.position = {x, 40.0f};
			b2BodyId bodyId = b2CreateBody(m_worldId, &bodyDef);

			m_bodyIds[i] = bodyId;

			if (m_shapeType == e_circleShape)
			{
				b2CreateCircleShape(bodyId, &shapeDef, &circle);
			}
			else
			{
				b2CreatePolygonShape(bodyId, &shapeDef, &box);
			}

			shapeDef.restitution += dr;
			x += dx;
		}
	}

	void UpdateUI() override
	{
		ImGui::SetNextWindowPos(ImVec2(10.0f, 300.0f), ImGuiCond_Once);
		ImGui::SetNextWindowSize(ImVec2(240.0f, 230.0f));
		ImGui::Begin("Restitution", nullptr, ImGuiWindowFlags_NoResize);

		bool changed = false;
		const char* shapeTypes[] = {"Circle", "Box"};

		int shapeType = int(m_shapeType);
		changed = changed || ImGui::Combo("Shape", &shapeType, shapeTypes, IM_ARRAYSIZE(shapeTypes));
		m_shapeType = ShapeType(shapeType);

		changed = changed || ImGui::Button("Reset");

		if (changed)
		{
			CreateBodies();
		}

		ImGui::End();
	}

	static Sample* Create(const Settings& settings)
	{
		return new Restitution(settings);
	}

	b2BodyId m_bodyIds[e_count];
	ShapeType m_shapeType;
};

static int sampleIndex = RegisterSample("Shapes", "Restitution", Restitution::Create);

class Friction : public Sample
{
public:
	Friction(const Settings& settings)
		: Sample(settings)
	{

		{
			b2BodyDef bodyDef = b2_defaultBodyDef;
			b2BodyId groundId = b2CreateBody(m_worldId, &bodyDef);

			b2ShapeDef shapeDef = b2_defaultShapeDef;
			shapeDef.friction = 0.2f;

			b2Segment segment = {{-40.0f, 0.0f}, {40.0f, 0.0f}};
			b2CreateSegmentShape(groundId, &shapeDef, &segment);

			b2Polygon box = b2MakeOffsetBox(13.0f, 0.25f, {-4.0f, 22.0f}, -0.25f);
			b2CreatePolygonShape(groundId, &shapeDef, &box);

			box = b2MakeOffsetBox(0.25f, 1.0f, {10.5f, 19.0f}, 0.0f);
			b2CreatePolygonShape(groundId, &shapeDef, &box);

			box = b2MakeOffsetBox(13.0f, 0.25f, {4.0f, 14.0f}, 0.25f);
			b2CreatePolygonShape(groundId, &shapeDef, &box);

			box = b2MakeOffsetBox(0.25f, 1.0f, {-10.5f, 11.0f}, 0.0f);
			b2CreatePolygonShape(groundId, &shapeDef, &box);

			box = b2MakeOffsetBox(13.0f, 0.25f, {-4.0f, 6.0f}, -0.25f);
			b2CreatePolygonShape(groundId, &shapeDef, &box);
		}

		{
			b2Polygon box = b2MakeBox(0.5f, 0.5f);

			b2ShapeDef shapeDef = b2_defaultShapeDef;
			shapeDef.density = 25.0f;

			float friction[5] = {0.75f, 0.5f, 0.35f, 0.1f, 0.0f};

			for (int i = 0; i < 5; ++i)
			{
				b2BodyDef bodyDef = b2_defaultBodyDef;
				bodyDef.type = b2_dynamicBody;
				bodyDef.position = {-15.0f + 4.0f * i, 28.0f};
				b2BodyId bodyId = b2CreateBody(m_worldId, &bodyDef);

				shapeDef.friction = friction[i];
				b2CreatePolygonShape(bodyId, &shapeDef, &box);
			}
		}
	}

	static Sample* Create(const Settings& settings)
	{
		return new Friction(settings);
	}
};

static int sampleIndex3 = RegisterSample("Shapes", "Friction", Friction::Create);
