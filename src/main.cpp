#include "Application.h"
#include <geni.h>

int main()
{
    Application *app = new Application();
    Geni::Engine &engine = Geni::Engine::GetInstance();

    engine.SetWindowTitle("Kinema");
    engine.SetApplication(app);

    if (engine.Init(1280, 720))
    {
        engine.Run();
    }

    engine.Destroy();
    return 0;
}
