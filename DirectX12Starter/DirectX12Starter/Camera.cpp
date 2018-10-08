#include "Camera.h"

Camera::Camera()
{
}

Camera::Camera(unsigned int width, unsigned int height)
{
	XMVECTOR pos = XMVectorSet(0, 0, -5, 0);
	XMVECTOR dir = XMVectorSet(0, 0, 1, 0);
	XMVECTOR up = XMVectorSet(0, 1, 0, 0);

	XMMATRIX V = XMMatrixLookToLH(
		pos,
		dir,
		up);
	XMStoreFloat4x4(&viewMatrix, (V));

	SetProjectionMatrix(width, height);

	XMStoreFloat3(&position, pos);
	XMStoreFloat3(&direction, dir);
}

Camera::~Camera()
{
}

XMFLOAT4X4 Camera::GetViewMatrix()
{
	return viewMatrix;
}

XMFLOAT4X4 Camera::GetProjectionMatrix()
{
	return projectionMatrix;
}

void Camera::SetXRotation(float amount)
{
	xRotation += amount * 0.001f;
}

void Camera::SetYRotation(float amount)
{
	yRotation += amount * 0.001f;
}

void Camera::SetProjectionMatrix(unsigned int newWidth, unsigned int newHeight)
{
	XMMATRIX P = XMMatrixPerspectiveFovLH(45.0f * (3.14f / 180.0f), (float)newWidth / (float)newHeight, 0.1f, 1000.0f);
	XMStoreFloat4x4(&projectionMatrix, (P));
}

void Camera::Update()
{
	XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(xRotation, yRotation, 0.0f);
	XMVECTOR defaultVector = XMVectorSet(0.0, 0.0, 1.0, 0.0);

	XMVECTOR newDirection = XMVector3Transform(defaultVector, rotationMatrix);
	XMStoreFloat3(&direction, newDirection);

	XMVECTOR up = XMVectorSet(0, 1, 0, 0);

	XMVECTOR lrVector = XMVector3Cross(newDirection, up);

	XMVECTOR pos = XMLoadFloat3(&position);

	float moveRate = 0.001f;

	if (InputManager::getInstance()->KeyIsPressed('W'))
	{
		pos += (newDirection * moveRate);
	}

	if (InputManager::getInstance()->KeyIsPressed('S'))
	{
		pos += (-newDirection * moveRate);
	}

	if (InputManager::getInstance()->KeyIsPressed('A'))
	{
		pos += (lrVector * moveRate);
	}

	if (InputManager::getInstance()->KeyIsPressed('D'))
	{
		pos += (-lrVector * moveRate);
	}

	if (InputManager::getInstance()->KeyIsPressed(VK_SPACE))
	{
		pos += (up * moveRate);
	}

	if (InputManager::getInstance()->KeyIsPressed('X'))
	{
		pos += (-up * moveRate);
	}

	XMStoreFloat3(&position, pos);

	XMMATRIX V = XMMatrixLookToLH(
		pos,
		newDirection,
		up);
	XMStoreFloat4x4(&viewMatrix, (V));
}

void Camera::ResetCamera()
{
	XMVECTOR pos = XMVectorSet(0, 0, -5, 0);
	XMStoreFloat3(&position, pos);
	xRotation = 0.0f;
	yRotation = 0.0f;
}

void Camera::CreateMatrices()
{
	
}