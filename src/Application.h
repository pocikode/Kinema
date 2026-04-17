#pragma once
#include <geni.h>

class Application : public Geni::Application
{
  public:
    bool Init() override;
    void Update(float deltaTime) override;
    void Destroy() override;

  private:
    Geni::Scene *m_scene;
};
