#pragma once
#include "Core.h"

struct CameraBuffer
{
	glm::mat4 Projection		= glm::mat4(1.0f);
	glm::mat4 View				= glm::mat4(1.0f);
	glm::mat4 LastProjection	= glm::mat4(1.0f);
	glm::mat4 LastView			= glm::mat4(1.0f);
	glm::mat4 InvView			= glm::mat4(1.0f);
	glm::mat4 InvProjection		= glm::mat4(1.0f);
	glm::vec4 Position			= glm::vec4(0.0f);
	glm::vec4 Right				= glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
	glm::vec4 Up				= glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
};

class Camera
{
public:
	Camera();
	~Camera() = default;

	void setDirection(const glm::vec3& direction);
	void setPosition(const glm::vec3& position);
	void setRotation(const glm::vec3& rotation);
	void setProjection(float fovDegrees, float width, float height, float nearPlane, float farPlane);

	void rotate(const glm::vec3& rotation);
	void translate(const glm::vec3& translation);

	void update();

	const glm::mat4& getProjectionMat() const { return m_Projection; }
	const glm::mat4& getProjectionInvMat() const { return m_ProjectionInv; }
	const glm::mat4& getViewMat() const { return m_View; }
	const glm::mat4& getViewInvMat() const { return m_ViewInv; }
	const glm::vec3& getPosition() const { return m_Position; }
	const glm::vec3& getRotation() const { return m_Rotation; }
	const glm::vec3& getRightVec() const { return m_Right; }
	const glm::vec3& getUpVec() const { return m_Up; }

private:
	void calculateVectors();

private:
	glm::mat4 m_Projection;
	glm::mat4 m_ProjectionInv;
	glm::mat4 m_View;
	glm::mat4 m_ViewInv;

	glm::vec3 m_Position;
	glm::vec3 m_Rotation;

	glm::vec3 m_Direction;
	glm::vec3 m_Right;
	glm::vec3 m_Up;

	bool m_IsDirty;
};

