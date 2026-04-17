#include "Application.h"
#include "glm/ext/quaternion_float.hpp"
#include "glm/ext/vector_float3.hpp"
#include "physics/Collider.h"
#include "physics/RigidBody.h"
#include "scene/components/MeshComponent.h"
#include "scene/components/PhysicsComponent.h"
#include <iostream>
#include <memory>
#include <ostream>

bool Application::Init()
{
    std::cout << "Hello World" << std::endl;
    return true;
}

void Application::Update(float deltaTime)
{
    m_scene->Update(deltaTime);
}

void Application::Destroy()
{
}
